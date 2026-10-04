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
    command = [str(server), '--no-defaults', '--datadir=' + directory, '--innodb-buffer-pool-size=32M']
    if os.name != 'nt':
        command.append('--innodb-use-native-aio=0')
    if hasattr(os, 'geteuid') and os.geteuid() == 0:
        command.append('--user=root')
    process = None
    error_log = pathlib.Path(directory, 'mysql.log' if args.mysqld else 'bootstrap.log')

    def stop():
        nonlocal process
        if process is not None:
            subprocess.run(client, input='SHUTDOWN;', text=True, capture_output=True, env=env, timeout=30)
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
            return
        stop()
        process = subprocess.Popen(command + (['--console'] if os.name == 'nt' else []),
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline and process.poll() is None:
            probe = subprocess.run(client, input='SELECT 1;', text=True, capture_output=True, env=env, timeout=5)
            if probe.returncode == 0:
                return
            time.sleep(0.1)
        raise AssertionError('MySQL did not start:\n' + pathlib.Path(directory, 'mysql.log').read_text())

    def run(items, expect_success=True):
        payload = '\n'.join(item.rstrip().rstrip(';') + ';' for item in items) + '\n'
        previous_log_size = error_log.stat().st_size if error_log.exists() else 0
        result = subprocess.run(client, input=payload, text=True, capture_output=True, env=env, timeout=60)
        if result.returncode and error_log.exists():
            result.stderr += error_log.read_bytes()[previous_log_size:].decode('utf-8', errors='replace')
        if (result.returncode == 0) != expect_success:
            raise AssertionError(result.stdout + result.stderr)
        return result

    try:
        if args.mysqld:
            socket = 'item-scaling-' + pathlib.Path(directory).name if os.name == 'nt' else directory + '/mysql.sock'
            command += ['--basedir=' + str(server.parent.parent), '--skip-networking', '--mysqlx=OFF',
                        '--secure-file-priv=NULL',
                        '--socket=' + socket, '--pid-file=' + directory + '/mysql.pid',
                        '--log-error=' + directory + '/mysql.log']
            if os.name == 'nt':
                command.append('--enable-named-pipe')
            initialized = subprocess.run(command + ['--initialize-insecure'], text=True,
                                         capture_output=True, env=env, timeout=60)
            if initialized.returncode:
                raise AssertionError(initialized.stderr + pathlib.Path(directory, 'mysql.log').read_text())
            client = [str(args.mysql.resolve()), '--no-defaults', '--protocol=' + ('PIPE' if os.name == 'nt' else 'SOCKET'),
                      '--socket=' + socket, '--user=root', '--batch', '--skip-column-names']
            restart()
        else:
            client = command + ['--bootstrap', '--log-error=' + str(error_log)]
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

base_sql = (module / 'data/sql/db-world/base/scaled_item_variant.sql').read_text()
base_sql_mirrored = (module / 'sql/world/base/scaled_item_variant.sql').read_text()
install_sql = (module / 'data/sql/db-world/updates/2026_09_27_00_item_scaling_initial_schema.sql').read_text()
live_sql = (module / 'data/sql/db-world/updates/2026_10_04_00_item_scaling_live.sql').read_text()
retire_sql = (module / 'data/sql/db-world/updates/2026_10_05_00_retire_demand_ledger.sql').read_text()

create_tables = lambda text: re.findall(r'CREATE TABLE.*?\) ENGINE=.*?;', text, re.S)
assert create_tables(base_sql) == create_tables(base_sql_mirrored), 'Base schemas must be mirrored identically'
assert len(re.findall(r'^CREATE TABLE IF NOT EXISTS `scaled_item_variant`', base_sql, re.M)) == 1
assert 'scaled_item_variant_request' not in base_sql, 'Base schema must not contain scaled_item_variant_request'
assert not re.search(r'^\s*(ALTER|UPDATE|DELETE|DROP|INSERT)\b', base_sql, re.M | re.I)

# Source checks complement the SQL execution below; these do not execute worldserver control flow.
validate = function('bool ItemScalingRegistry::ValidateSchema()', 'bool ItemScalingRegistry::ResolveSyntheticEntryRange()')
initialize = function('void ItemScalingRegistry::Initialize()', 'uint32 ItemScalingRegistry::FindExistingVariant')
for body in [validate, initialize]:
    assert not any(write in body for write in ['DirectExecute', 'WorldDatabase.Execute', 'BeginTransaction', 'UPDATE '])

assert '_committedKeys.insert(key)' in initialize
assert initialize.index('_committedKeys.insert(key)') < initialize.index('if (!persisted)')
assert initialize.index('if (!persisted)') < initialize.index('_keyToEntry.emplace(key, entry)')
assert 'identity.Matches(*base)' in initialize and 'identity.Matches(*persisted)' in initialize
assert 'fields[15].IsNull()' not in initialize

miss_path = source[source.index('uint32 ItemScalingRegistry::FindOrRequestVariant'):]
assert not any(blocking in miss_path for blocking in ['WorldDatabase.Query', 'DirectExecute', 'CreateScaledTemplate'])
assert '_committedKeys.count(key)' in miss_path
assert '_requestedKeys' not in source
assert '_requestMutex' not in source
assert 'MaterializePendingRequests' not in source
assert 'scaled_item_variant_request' not in source

# Key columns and identity columns for query formatting
key_columns = query_after('std::string const KeyColumns =')
identity_columns = query_after('std::string const IdentityColumns =')

statements = ["CREATE DATABASE fixture", "USE fixture",
              "SET SESSION sql_mode='STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION'",
              'CREATE TABLE assertions (passed INT NOT NULL CHECK (passed=1)) ENGINE=InnoDB']

def sql(statement):
    statements.append(statement.rstrip().rstrip(';'))

def check(condition):
    sql('INSERT INTO fixture.assertions VALUES (IF((' + condition + '),1,0))')

# Core item_template table
for table in ['item_template']:
    text = (args.core / 'data/sql/base/db_world' / (table + '.sql')).read_text()
    ddl = re.search(r'CREATE TABLE.*?\) ENGINE=.*?;', text, re.S).group(0)
    sql(ddl)

item_columns = re.findall(r'^\s*`([^`]+)`', ddl, re.M)

# Insert reference item
sql("INSERT INTO item_template (entry,class,subclass,name,Quality,InventoryType,ItemLevel,RequiredLevel,"
    "stat_type1,stat_value1,stat_type3,stat_value3,armor,block,holy_res,fire_res,nature_res,frost_res,shadow_res,arcane_res) "
    "VALUES (100,4,4,'Base shield',3,14,100,40,3,10,4,20,50,5,1,2,3,4,5,6)")
sql('CREATE TABLE base_before AS SELECT * FROM item_template')

# State A: Fresh install with base_sql + live_sql + retire_sql
sql('CREATE DATABASE fresh_install')
sql('USE fresh_install')
sql(ddl)
sql(base_sql)
sql(live_sql)
sql(retire_sql)
check('(SELECT COUNT(*) FROM fresh_install.scaled_item_variant)=0')
check("(SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='fresh_install' AND TABLE_NAME='scaled_item_variant_request')=0")
check("(SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='fresh_install' "
      "AND TABLE_NAME IN ('item_template','scaled_item_variant','mod_item_level_scaling_slot',"
      "'mod_item_level_scaling_staged_item','mod_item_level_scaling_staged_variant') AND ENGINE='InnoDB')=5")

# State B: Historical upgrade path (initial_sql -> live_sql -> retire_sql)
sql('USE fixture')
sql(install_sql)
sql(live_sql)
sql(retire_sql)
check('(SELECT COUNT(*) FROM fixture.scaled_item_variant)=0')
check("(SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='fixture' AND TABLE_NAME='scaled_item_variant_request')=0")
check("(SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='fixture' "
      "AND TABLE_NAME IN ('item_template','scaled_item_variant','mod_item_level_scaling_slot',"
      "'mod_item_level_scaling_staged_item','mod_item_level_scaling_staged_variant') AND ENGINE='InnoDB')=5")

# State C: Historical upgrade path with pending ledger rows
sql('CREATE DATABASE upgrade_pending')
sql('USE upgrade_pending')
sql(ddl)
sql(install_sql)
sql(live_sql)
# Add sample pending requests
sql("INSERT INTO scaled_item_variant_request (base_entry,target_effective_level,target_item_level,formula_version,generator_revision,required_level,random_property_id,base_class,base_subclass,base_sound_override_subclass,base_material,base_displayid,base_inventory_type,base_sheath,preserve_nonzero_stats) VALUES "
    "(100,53,150,1,1,50,0,4,4,-1,-1,12345,14,1,1),"
    "(100,60,180,1,1,58,0,4,4,-1,-1,12345,14,1,1),"
    "(100,70,200,1,1,68,0,4,4,-1,-1,12345,14,1,1)")
check('(SELECT COUNT(*) FROM upgrade_pending.scaled_item_variant_request)=3')
sql(retire_sql)
check("(SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='upgrade_pending' AND TABLE_NAME='scaled_item_variant_request')=0")
check("(SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='upgrade_pending' "
      "AND TABLE_NAME IN ('item_template','scaled_item_variant','mod_item_level_scaling_slot',"
      "'mod_item_level_scaling_staged_item','mod_item_level_scaling_staged_variant') AND ENGINE='InnoDB')=5")

sql('USE fixture')

# Index & column checks for scaled_item_variant
for database in ['fixture', 'fresh_install', 'upgrade_pending']:
    check("(SELECT GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX) FROM information_schema.STATISTICS "
          f"WHERE TABLE_SCHEMA='{database}' AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='uk_variant_key' AND NON_UNIQUE=0)="
          "'base_entry,target_effective_level,target_item_level,formula_version,generator_revision,required_level,random_property_id'")
    check("(SELECT COUNT(*) FROM information_schema.COLUMNS "
          f"WHERE TABLE_SCHEMA='{database}' AND TABLE_NAME='scaled_item_variant' AND IS_NULLABLE='YES')=0")

# Setup core world tables needed for queries in ItemScalingLive
for table in ['creature', 'gameobject', 'gameobject_template', 'creature_loot_template',
              'gameobject_loot_template', 'reference_loot_template', 'item_enchantment_template']:
    contents = (args.core / 'data/sql/base/db_world' / (table + '.sql')).read_text()
    ddl = re.search(r'CREATE TABLE.*?\) ENGINE=.*?;', contents, re.S).group(0)
    if table == 'creature' and '`id`' not in ddl and '`id1`' in ddl:
        ddl = ddl.replace('`id1` int unsigned', '`id` int unsigned NOT NULL DEFAULT \'0\',\n  `id1` int unsigned', 1)
    sql(ddl)

# Execute all module SELECT statements from C++ sources
query_bindings = {
    'SELECT COUNT(*) FROM scaled_item_variant WHERE generator_revision': [(1, 1, 1)],
    'SELECT variant_entry,{},{},preserve_nonzero_stats': [(key_columns, identity_columns)],
    'SELECT Entry,Item,Reference': [('creature_loot_template',), ('gameobject_loot_template',),
                                  ('reference_loot_template',)],
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

# Staged template recovery test
live_source = (module / 'src/ItemScalingLive.cpp').read_text()
recovery = live_source[live_source.index('bool ItemScalingLive::RecoverStagedTemplates()'):
                       live_source.index('bool ItemScalingLive::ReserveSlots()')]
promotion = [strings(fragment) for fragment in re.findall(
    r'transaction->Append\(\s*((?:"(?:\\.|[^"\\])*"\s*)+)\)', recovery)]
assert len(promotion) == 6, 'Recognize every live promotion statement'
recovery_query = strings(re.search(
    r'QueryResult counts = WorldDatabase.Query\(\s*((?:"(?:\\.|[^"\\])*"\s*)+)\)', recovery).group(1))

# Insert placeholder slot in item_template & mod_item_level_scaling_slot
sql("INSERT INTO item_template (entry,name,class,stackable) VALUES (65000,'ItemScaling reserved',15,1)")
sql('INSERT INTO mod_item_level_scaling_slot (entry,assigned) VALUES (65000,1)')

# Insert staged item snapshot and staged mapping
sql("INSERT INTO mod_item_level_scaling_staged_item (entry,class,subclass,name,Quality,InventoryType,ItemLevel,RequiredLevel,"
    "stat_type1,stat_value1,stat_type3,stat_value3,armor,block,holy_res,fire_res,nature_res,frost_res,shadow_res,arcane_res) "
    "VALUES (65000,4,4,'Scaled shield',3,14,150,50,3,25,4,40,100,77,11,12,13,14,15,16)")
sql("INSERT INTO mod_item_level_scaling_staged_variant (variant_entry,base_entry,target_effective_level,target_item_level,formula_version,"
    "generator_revision,required_level,random_property_id,base_class,base_subclass,base_sound_override_subclass,base_material,base_displayid,"
    "base_inventory_type,base_sheath,preserve_nonzero_stats) VALUES (65000,100,53,150,1,2,50,0,4,4,-1,-1,12345,14,1,1)")

sql('CREATE TABLE live_before AS SELECT * FROM mod_item_level_scaling_staged_item WHERE entry=65000')
check("(SELECT name FROM item_template WHERE entry=65000)='ItemScaling reserved'")
check('(SELECT COUNT(*) FROM scaled_item_variant WHERE variant_entry=65000)=0')
check('(SELECT COUNT(*) FROM mod_item_level_scaling_staged_variant WHERE variant_entry=65000)=1')
sql(recovery_query)

if args.mysql_initialize_only:
    with tempfile.TemporaryDirectory(prefix='item-scaling-mysql-init-') as directory:
        root = pathlib.Path(directory)
        init_file = root / 'fixture.sql'
        payload = '\n'.join(statement.rstrip().rstrip(';') + ';' for statement in statements)
        payload = re.sub(r'^\s*--.*$', '', payload, flags=re.M)
        tokens = re.findall(r"'(?:''|\\.|[^'\\])*'|\"(?:\"\"|\\.|[^\"\\])*\"|`[^`]*`|;|[^;'\"`]+", payload)
        lines, pending = [], []
        for token in tokens:
            pending.append(token)
            if token == ';':
                lines.append(' '.join(''.join(pending).splitlines()).strip())
                pending.clear()
        assert not ''.join(pending).strip(), 'Unterminated SQL fixture statement'
        init_file.write_text('\n'.join(lines) + '\n')
        (root / 'data').mkdir(parents=True, exist_ok=True)
        server = args.mysqld.resolve()
        env = os.environ.copy()
        if args.library_path:
            env['LD_LIBRARY_PATH'] = str(args.library_path.resolve())
        command = [str(server), '--no-defaults', '--initialize-insecure', '--datadir=' + str(root / 'data'),
                   '--basedir=' + str(server.parent.parent), '--secure-file-priv=NULL',
                   '--innodb-use-native-aio=0', '--innodb-buffer-pool-size=32M', '--init-file=' + str(init_file)]
        if hasattr(os, 'geteuid') and os.geteuid() == 0:
            command.append('--user=root')
        result = subprocess.run(command, text=True, capture_output=True, env=env, timeout=60)
        assert result.returncode == 0 and '[ERROR]' not in result.stderr, result.stdout + result.stderr
    print(f'PASS: MySQL initialization: 3-state upgrade matrix, schema, live staging recovery, all {query_count} SELECT sites')
    print('NOT RUN in initialization-only mode: database restart, NULL rejection and failed-transaction rollback')
    raise SystemExit(0)

with tempfile.TemporaryDirectory(prefix='item-scaling-sql-') as directory, sql_engine(directory) as (run, restart):
    run(statements)
    print('PASS: 3-state install/upgrade compatibility matrix, final schema, pure live staging', flush=True)
    print(f'PASS: all {query_count} module SELECT call sites execute successfully', flush=True)
    restart()
    run(['USE fixture',
         "INSERT INTO assertions VALUES (IF((SELECT name FROM item_template WHERE entry=65000)='ItemScaling reserved',1,0))",
         'INSERT INTO assertions VALUES (IF((SELECT assigned FROM mod_item_level_scaling_slot WHERE entry=65000)=1,1,0))',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM mod_item_level_scaling_staged_item s '
         'JOIN live_before b ON s.entry=b.entry WHERE '
         + ' AND '.join(f's.`{column}` <=> b.`{column}`' for column in item_columns) + ')=1,1,0))'])
    print('PASS: staged snapshot and reserved ID survive a DB restart before promotion', flush=True)

    # Promotion execution
    run(['USE fixture', 'START TRANSACTION', *promotion, 'COMMIT',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM live_before b '
         'JOIN item_template i ON b.entry=i.entry WHERE '
         + ' AND '.join(f'b.`{column}` <=> i.`{column}`' for column in item_columns) + ')=1,1,0))',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM scaled_item_variant WHERE variant_entry=65000)=1,1,0))',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM mod_item_level_scaling_staged_item)=0,1,0))',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM mod_item_level_scaling_staged_variant)=0,1,0))',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM mod_item_level_scaling_slot)=0,1,0))'])
    print('PASS: atomic promotion, exact snapshots after promotion, repeat promotion idempotency', flush=True)
