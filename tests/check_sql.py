#!/usr/bin/env python3
"""Run module SQL in a disposable MySQL or MariaDB database, without worldserver."""
import argparse
import contextlib
import json
import os
import pathlib
import re
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument('--core', required=True, type=pathlib.Path)
engine = parser.add_mutually_exclusive_group(required=True)
engine.add_argument('--mariadbd', type=pathlib.Path)
engine.add_argument('--mysqld', type=pathlib.Path)
parser.add_argument('--mysql', type=pathlib.Path, help='MySQL client; required with --mysqld')
parser.add_argument('--mysql-initialize-only', action='store_true',
                    help='Run first-install/materialization SQL during MySQL initialization, without a socket')
parser.add_argument('--library-path', type=pathlib.Path)
args = parser.parse_args()
if args.mysql_initialize_only and not args.mysqld:
    parser.error('--mysql-initialize-only requires --mysqld')
if args.mysqld and not args.mysql and not args.mysql_initialize_only:
    parser.error('--mysqld requires --mysql')


@contextlib.contextmanager
def sql_engine(directory):
    env = os.environ.copy()
    if args.library_path:
        env['LD_LIBRARY_PATH'] = str(args.library_path.resolve())
    server = (args.mysqld or args.mariadbd).resolve()
    command = [str(server), '--no-defaults', '--datadir=' + directory,
               '--innodb-use-native-aio=0', '--innodb-buffer-pool-size=32M']
    if os.geteuid() == 0:
        command.append('--user=root')
    process = None

    def stop():
        nonlocal process
        if process is not None:
            process.terminate()
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise
            finally:
                process = None

    def restart():
        nonlocal process
        if not args.mysqld:
            return  # Each MariaDB bootstrap invocation already starts a new process.
        stop()
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline and process.poll() is None:
            probe = subprocess.run(client, input='SELECT 1;', text=True, capture_output=True, env=env, timeout=5)
            if probe.returncode == 0:
                return
            time.sleep(0.1)
        raise AssertionError('MySQL did not start:\n' + pathlib.Path(directory, 'mysql.log').read_text())

    def run(items, expect_success=True):
        payload = '\n'.join(item.rstrip().rstrip(';') + ';' for item in items) + '\n'
        result = subprocess.run(client, input=payload, text=True, capture_output=True, env=env, timeout=60)
        if (result.returncode == 0) != expect_success:
            raise AssertionError(result.stdout + result.stderr)
        return result

    try:
        if args.mysqld:
            command += ['--basedir=' + str(server.parent.parent), '--skip-networking', '--mysqlx=OFF',
                        '--secure-file-priv=NULL',
                        '--socket=' + directory + '/mysql.sock', '--pid-file=' + directory + '/mysql.pid',
                        '--log-error=' + directory + '/mysql.log']
            initialized = subprocess.run(command + ['--initialize-insecure'], text=True,
                                         capture_output=True, env=env, timeout=60)
            if initialized.returncode:
                raise AssertionError(initialized.stderr + pathlib.Path(directory, 'mysql.log').read_text())
            client = [str(args.mysql.resolve()), '--no-defaults', '--protocol=SOCKET',
                      '--socket=' + directory + '/mysql.sock', '--user=root', '--batch', '--skip-column-names']
            restart()
        else:
            client = command + ['--bootstrap']
        yield run, restart
    finally:
        stop()


module = pathlib.Path(__file__).resolve().parents[1]
source = (module / 'src/ItemScalingRegistry.cpp').read_text()

def strings(fragment):
    return ''.join(json.loads(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', fragment))

def function(name, next_name):
    return source[source.index(name):source.index(next_name, source.index(name))]

def query_after(marker):
    tail = source[source.index(marker) + len(marker):]
    return strings(re.match(r'\s*((?:"(?:\\.|[^"\\])*"\s*)+)', tail).group(1))

insert = strings(function('static std::string BuildItemTemplateInsertSQL', '\nnamespace'))
insert = insert[insert.index('INSERT INTO'):]
variant = strings(function('std::string VariantInsert', '// DirectCommitTransaction'))
request_insert = query_after('WorldDatabase.Execute(')
key_predicate = strings(function('std::string KeyPredicate', 'std::string DeleteRequest'))
completed_delete = strings(function('std::string DeleteCompletedRequest', 'std::string VariantInsert'))
base_columns = query_after('std::string const BaseColumns =')
base_sql = (module / 'sql/world/base/scaled_item_variant.sql').read_text()
install_files = sorted((module / 'data/sql/db-world/updates').glob('*.sql'))
assert len(install_files) == 1, 'One direct first-install schema is required'
install_sql = install_files[0].read_text()
assert install_sql == base_sql, 'Both installation routes must define exactly the same schema'
assert len(re.findall(r'^CREATE TABLE IF NOT EXISTS', install_sql, re.M)) == 2
assert not re.search(r'^\s*(ALTER|UPDATE|DELETE|DROP|INSERT)\b', install_sql, re.M | re.I)

# Source checks complement the SQL execution below; these do not execute worldserver control flow.
validate = function('bool ItemScalingRegistry::ValidateSchema()', 'bool ItemScalingRegistry::ResolveSyntheticEntryRange()')
initialize = function('void ItemScalingRegistry::Initialize()', 'uint32 ItemScalingRegistry::FindOrRequestVariant')
for body in [validate, initialize]:
    assert not any(write in body for write in ['DirectExecute', 'WorldDatabase.Execute', 'BeginTransaction', 'UPDATE '])
assert '_requestedKeys.insert(key)' in initialize
assert initialize.index('_requestedKeys.insert(key)') < initialize.index('if (!persisted)')
assert initialize.index('if (!persisted)') < initialize.index('_keyToEntry.emplace(key, entry)')
assert 'identity.Matches(*base)' in initialize and 'identity.Matches(*persisted)' in initialize
assert 'fields[15].IsNull()' not in initialize
materialize = function('bool ItemScalingRegistry::MaterializePendingRequests()',
                       'void ItemScalingRegistry::OnLoadCustomDatabaseTable()')
for guard in ['key.generatorRevision != ITEM_SCALING_GENERATOR_REVISION',
              'key.formulaVersion != sItemScalingConfig->FormulaVersion',
              'fields[14].Get<uint8>() != uint8(sItemScalingConfig->PreserveNonZeroStats)']:
    assert materialize.index(guard) < materialize.index('CreateScaledTemplate')
miss_path = source[source.index('uint32 ItemScalingRegistry::FindOrRequestVariant'):]
assert not any(blocking in miss_path for blocking in ['WorldDatabase.Query', 'DirectExecute', 'CreateScaledTemplate'])

stat_pairs = [value for i in range(10) for value in (3 + i, 20 + i)]
identity = [4, 4, -1, -1, 12345, 14, 1]
values = [60000, 4, 4, -1, 'name', 12345, 14, 150, 50, *stat_pairs, 25.0, 50.0, 0.0, 0.0,
          100, 11, 12, 13, 14, 15, 16, -1, 1, 0, 0, 77, 100]
assert insert.count('{}') == len(values)
clone = insert.format(*values)
def mapping(entry, required=50, formula=1, revision=1):
    return variant.format(entry, 100, 53, 150, formula, revision, required, 0, *identity, 1, entry)

def request(required=50, formula=1, revision=1, base=100):
    return request_insert.format(base, 53, 150, formula, revision, required, 0, *identity, 1)

def delete_request(required=50, formula=1, revision=1, entry=60000):
    return completed_delete.format(key_predicate.format(100, 53, 150, formula, revision, required, 0), entry)

key_insert = mapping(60000)

statements = ["CREATE DATABASE fixture", "USE fixture",
              "SET SESSION sql_mode='STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION'",
              'CREATE TABLE assertions (passed INT NOT NULL CHECK (passed=1)) ENGINE=InnoDB']

def sql(statement):
    statements.append(statement.rstrip().rstrip(';'))

def check(condition):
    sql('INSERT INTO assertions VALUES (IF((' + condition + '),1,0))')

for table in ['item_template']:
    text = (args.core / 'data/sql/base/db_world' / (table + '.sql')).read_text()
    ddl = re.search(r'CREATE TABLE.*?\) ENGINE=.*?;', text, re.S).group(0)
    sql(ddl)
# First module installation onto an existing ordinary world DB.
sql("INSERT INTO item_template (entry,class,subclass,name,Quality,InventoryType,ItemLevel,RequiredLevel,"
    "stat_type1,stat_value1,stat_type3,stat_value3,armor,block,holy_res,fire_res,nature_res,frost_res,shadow_res,arcane_res) "
    "VALUES (100,4,4,'Base shield',3,14,100,40,3,10,4,20,50,5,1,2,3,4,5,6)")
sql('CREATE TABLE base_before AS SELECT * FROM item_template')
sql(install_sql)
check('(SELECT COUNT(*) FROM scaled_item_variant)=0')
check('(SELECT COUNT(*) FROM scaled_item_variant_request)=0')
check('(SELECT COUNT(*) FROM item_template)=1')

# Fresh assembly followed by the module updater must also create final empty tables directly.
sql('CREATE DATABASE assembly')
sql('USE assembly')
sql(ddl)
sql(base_sql)
sql(install_sql)
sql('USE fixture')
check('(SELECT COUNT(*) FROM assembly.scaled_item_variant)=0')
check('(SELECT COUNT(*) FROM assembly.scaled_item_variant_request)=0')
for database in ['fixture', 'assembly']:
    check("(SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='" + database + "' "
          "AND TABLE_NAME IN ('item_template','scaled_item_variant','scaled_item_variant_request') "
          "AND ENGINE='InnoDB')=3")
    for table, index in [('scaled_item_variant', 'uk_variant_key'), ('scaled_item_variant_request', 'PRIMARY')]:
        check("(SELECT GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX) FROM information_schema.STATISTICS "
              f"WHERE TABLE_SCHEMA='{database}' AND TABLE_NAME='{table}' AND INDEX_NAME='{index}' AND NON_UNIQUE=0)="
              "'base_entry,target_effective_level,target_item_level,formula_version,generator_revision,required_level,random_property_id'")
        check("(SELECT COUNT(*) FROM information_schema.COLUMNS "
              f"WHERE TABLE_SCHEMA='{database}' AND TABLE_NAME='{table}' AND IS_NULLABLE='YES')=0")
        check("(SELECT COUNT(*) FROM information_schema.COLUMNS "
              f"WHERE TABLE_SCHEMA='{database}' AND TABLE_NAME='{table}' "
              "AND COLUMN_NAME IN ('formula_version','generator_revision') AND COLUMN_DEFAULT IS NULL)=2")

# The complete seven-field pending primary key collapses repeats across callers/processes.
for _ in range(5):
    sql(request())
check('(SELECT COUNT(*) FROM scaled_item_variant_request)=1')
sql(request(required=53))
sql(request(formula=2))
sql(request(revision=2))
check('(SELECT COUNT(*) FROM scaled_item_variant_request)=4')
check('(SELECT base_sound_override_subclass=-1 AND base_material=-1 AND base_displayid=12345 '
      'AND random_property_id=0 AND preserve_nonzero_stats=1 FROM scaled_item_variant_request WHERE formula_version=1 '
      'AND generator_revision=1 AND required_level=50)')

sql('START TRANSACTION')
sql(clone)
sql(key_insert)
sql(delete_request())
sql('COMMIT')
check('(SELECT COUNT(*) FROM item_template WHERE entry=60000 AND ItemLevel=150 AND RequiredLevel=50 '
      'AND armor=100 AND block=77 AND holy_res=11 AND fire_res=12 AND nature_res=13 '
      'AND frost_res=14 AND shadow_res=15 AND arcane_res=16 AND stat_value10=29)=1')
check("(SELECT name FROM item_template WHERE entry=60000)='Base shield'")
check('(SELECT class=4 AND subclass=4 AND SoundOverrideSubclass=-1 AND Material=-1 AND displayid=12345 '
      'AND InventoryType=14 AND sheath=1 FROM item_template WHERE entry=60000)')
check('(SELECT COUNT(*) FROM scaled_item_variant_request)=3')
check('(SELECT base_displayid=12345 AND base_material=-1 AND random_property_id=0 AND preserve_nonzero_stats=1 '
      'FROM scaled_item_variant WHERE variant_entry=60000)')
# Generating a variant must leave every column of the ordinary source item unchanged.
item_columns = re.findall(r'^\s*`([^`]+)`', ddl, re.M)
check('(SELECT COUNT(*) FROM base_before b JOIN item_template i ON b.entry=i.entry WHERE '
      + ' AND '.join(f'b.`{column}` <=> i.`{column}`' for column in item_columns) + ')=1')

second_values = values.copy()
second_values[0], second_values[8] = 60001, 53
sql('START TRANSACTION')
sql(insert.format(*second_values))
sql(mapping(60001, required=53))
sql(delete_request(required=53, entry=60001))
sql('COMMIT')
check('(SELECT COUNT(*) FROM scaled_item_variant WHERE base_entry=100 AND target_effective_level=53)=2')
# A missing base is deferred: no template or mapping is inserted and its demand is retained.
sql(request(base=999))
check('(SELECT COUNT(*) FROM scaled_item_variant_request WHERE base_entry=999)=1')
# INSERT ... SELECT returning zero rows cannot commit a mapping or consume the pending request.
missing_values = values.copy()
missing_values[0], missing_values[-1] = 60004, 999
sql('START TRANSACTION')
sql(insert.format(*missing_values))
sql(variant.format(60004, 999, 53, 150, 1, 1, 50, 0, *identity, 1, 60004))
sql(completed_delete.format(key_predicate.format(999, 53, 150, 1, 1, 50, 0), 60004))
sql('COMMIT')
check('(SELECT COUNT(*) FROM item_template WHERE entry=60004)=0')
check('(SELECT COUNT(*) FROM scaled_item_variant WHERE variant_entry=60004)=0')
check('(SELECT COUNT(*) FROM scaled_item_variant_request WHERE base_entry=999)=1')
# Complete snapshot widths/signs match the canonical item schema in both module tables.
for snapshot, original in [('base_class', 'class'), ('base_subclass', 'subclass'),
                           ('base_sound_override_subclass', 'SoundOverrideSubclass'),
                           ('base_material', 'Material'), ('base_displayid', 'displayid'),
                           ('base_inventory_type', 'InventoryType'), ('base_sheath', 'sheath')]:
    for table in ['scaled_item_variant', 'scaled_item_variant_request']:
        check("(SELECT s.COLUMN_TYPE=i.COLUMN_TYPE FROM information_schema.COLUMNS s "
              "JOIN information_schema.COLUMNS i ON i.TABLE_SCHEMA=s.TABLE_SCHEMA "
              f"WHERE s.TABLE_SCHEMA=DATABASE() AND s.TABLE_NAME='{table}' "
              f"AND s.COLUMN_NAME='{snapshot}' AND i.TABLE_NAME='item_template' AND i.COLUMN_NAME='{original}')")

# Exercise the shared partial-template projection, including a stat gap.
sql('CREATE TABLE base_projection AS SELECT ' + base_columns + ' FROM item_template b WHERE entry=100')
check('(SELECT stat_value3 FROM base_projection)=20')
# Execute every module SELECT from its actual C++ string, including formatted projections.
# Explicit bindings make newly introduced formatted queries require a fixture here.
key_columns = query_after('std::string const KeyColumns =')
identity_columns = query_after('std::string const IdentityColumns =')
query_bindings = {
    'SELECT COUNT(*) FROM scaled_item_variant s ': [('60000,60001',)],
    'SELECT COLUMN_NAME,DATA_TYPE': [('scaled_item_variant',), ('scaled_item_variant_request',)],
    'SELECT COLUMN_NAME,NON_UNIQUE': [('scaled_item_variant', 'uk_variant_key'),
                                   ('scaled_item_variant_request', 'PRIMARY')],
    'SELECT {},{},preserve_nonzero_stats': [(key_columns, identity_columns, key_columns)],
    'SELECT {} FROM scaled_item_variant': [(key_columns,)],
    'SELECT {} FROM item_template': [(base_columns, 100), (base_columns, 999)],
    'SELECT COUNT(*) FROM scaled_item_variant WHERE generator_revision': [(1, 1, 1)],
    'SELECT variant_entry,{},{},preserve_nonzero_stats': [(key_columns, identity_columns)],
}
query_count = 0
for cpp in sorted((module / 'src').glob('*.cpp')):
    contents = cpp.read_text()
    queries = re.findall(r'WorldDatabase\.Query\(\s*((?:"(?:\\.|[^"\\])*"\s*)+)', contents)
    assert len(queries) == contents.count('WorldDatabase.Query('), 'Unrecognized query in ' + str(cpp)
    for fragment in queries:
        query = strings(fragment)
        bindings = [()]
        if '{}' in query:
            matches = [values for prefix, values in query_bindings.items() if query.startswith(prefix)]
            assert len(matches) == 1, 'Add explicit fixture bindings for: ' + query
            bindings = matches[0]
        for parameters in bindings:
            sql(query.format(*parameters))
        query_count += 1

# The same exact-key deletion serves stale and redundant requests. Other demand must survive.
sql('START TRANSACTION')
sql(strings(function('std::string DeleteRequest', 'std::string DeleteCompletedRequest'))
    + key_predicate.format(100, 53, 150, 2, 1, 50, 0))
check('(SELECT COUNT(*) FROM scaled_item_variant_request)=2')
check('(SELECT COUNT(*) FROM scaled_item_variant_request WHERE generator_revision=2)=1')
sql('ROLLBACK')
check('(SELECT COUNT(*) FROM scaled_item_variant_request)=3')
# Persist complete generated rows for a normal database restart.
sql('CREATE TABLE committed_before AS SELECT * FROM scaled_item_variant')
sql('CREATE TABLE generated_before AS SELECT * FROM item_template WHERE entry=60000')

if args.mysql_initialize_only:
    # MySQL's supported init-file path executes SQL without starting any listener.
    # This subset deliberately makes no claim to cover restart or failure rollback.
    with tempfile.TemporaryDirectory(prefix='item-scaling-mysql-init-') as directory:
        root = pathlib.Path(directory)
        init_file = root / 'fixture.sql'
        payload = '\n'.join(statement.rstrip().rstrip(';') + ';' for statement in statements)
        payload = re.sub(r'^\s*--.*$', '', payload, flags=re.M)
        # Keep quoted semicolons (including schema COMMENT values) inside their statement.
        tokens = re.findall(r"'(?:''|\\.|[^'\\])*'|\"(?:\"\"|\\.|[^\"\\])*\"|`[^`]*`|;|[^;'\"`]+", payload)
        lines, pending = [], []
        for token in tokens:
            pending.append(token)
            if token == ';':
                lines.append(' '.join(''.join(pending).splitlines()).strip())
                pending.clear()
        assert not ''.join(pending).strip(), 'Unterminated SQL fixture statement'
        init_file.write_text('\n'.join(lines) + '\n')
        server = args.mysqld.resolve()
        env = os.environ.copy()
        if args.library_path:
            env['LD_LIBRARY_PATH'] = str(args.library_path.resolve())
        command = [str(server), '--no-defaults', '--initialize-insecure', '--datadir=' + str(root / 'data'),
                   '--basedir=' + str(server.parent.parent), '--secure-file-priv=NULL',
                   '--innodb-use-native-aio=0', '--innodb-buffer-pool-size=32M', '--init-file=' + str(init_file)]
        if os.geteuid() == 0:
            command.append('--user=root')
        result = subprocess.run(command, text=True, capture_output=True, env=env, timeout=60)
        assert result.returncode == 0 and '[ERROR]' not in result.stderr, result.stdout + result.stderr
    print(f'PASS: MySQL initialization: both install routes, schema, dedupe, materialization, all {query_count} SELECT sites')
    print('NOT RUN in initialization-only mode: database restart, NULL rejection and failed-transaction rollback')
    raise SystemExit(0)

with tempfile.TemporaryDirectory(prefix='item-scaling-sql-') as directory, sql_engine(directory) as (run, restart):
    run(statements)
    print('PASS: both first-install routes, final schema, dedupe, exact identity, materialization, complete snapshots', flush=True)
    print(f'PASS: all {query_count} module SELECT call sites and exact-key request deletion execute', flush=True)
    restart()
    run(['USE fixture',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM scaled_item_variant s '
         'JOIN committed_before b ON s.variant_entry=b.variant_entry WHERE '
         + ' AND '.join(f's.`{column}` <=> b.`{column}`' for column in [
             'base_entry', 'target_effective_level', 'target_item_level', 'formula_version',
             'generator_revision', 'required_level', 'random_property_id', 'base_class', 'base_subclass',
             'base_sound_override_subclass', 'base_material', 'base_displayid', 'base_inventory_type',
             'base_sheath', 'preserve_nonzero_stats', 'created_at']) + ')=2,1,0))',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM generated_before b '
         'JOIN item_template i ON b.entry=i.entry WHERE '
         + ' AND '.join(f'b.`{column}` <=> i.`{column}`' for column in item_columns) + ')=1,1,0))',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM scaled_item_variant_request)=3,1,0))'])
    print('PASS: a second DB start preserves exact committed IDs, values, metadata and pending demand', flush=True)
    # NULL metadata is invalid for either table; use strict inserts rather than INSERT IGNORE.
    for table, statement in [('scaled_item_variant_request', request(required=54).replace('INSERT IGNORE', 'INSERT')),
                             ('scaled_item_variant', mapping(60000, required=54))]:
        for position in range(8):
            tokens = statement.split('VALUES (' if table.endswith('_request') else 'SELECT ', 1)
            fields = tokens[1].split(',')
            index = (7 if table.endswith('_request') else 8) + position
            fields[index] = re.sub(r'^-?\d+', 'NULL', fields[index])
            invalid = tokens[0] + ('VALUES (' if table.endswith('_request') else 'SELECT ') + ','.join(fields)
            result = run(['USE fixture', "SET SESSION sql_mode='STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION'", invalid],
                         expect_success=False)
            assert 'cannot be null' in result.stderr.lower(), result.stderr
    print('PASS: all eight snapshot/provenance fields reject NULL in both tables', flush=True)
    # Force duplicate key failure in the second half of a transaction. The first half must roll back.
    failed_values = values.copy()
    failed_values[0] = 60003
    run(['USE fixture', request()])
    result = run(['USE fixture', 'START TRANSACTION', insert.format(*failed_values),
                  mapping(60003), delete_request(entry=60003), 'COMMIT'], expect_success=False)
    assert 'Duplicate entry' in result.stderr, result.stderr
    run(['USE fixture', 'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM item_template WHERE entry=60003)=0,1,0))',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM scaled_item_variant_request WHERE '
         + key_predicate.format(100, 53, 150, 1, 1, 50, 0) + ')=1,1,0))'])
    print('PASS: duplicate mapping causes transaction rollback; no orphan template and request retained', flush=True)
    # Missing committed templates are corruption: reserve the mapping, do not create a replacement.
    run(['USE fixture', 'DELETE FROM item_template WHERE entry=60001',
         'INSERT INTO assertions VALUES (IF((SELECT MAX(variant_entry) FROM scaled_item_variant)=60001,1,0))',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM scaled_item_variant '
         'WHERE variant_entry=60001 AND required_level=53)=1,1,0))'])
    print('PASS: missing-template mappings keep permanent allocator reservations (initialization guard checked statically)',
          flush=True)
