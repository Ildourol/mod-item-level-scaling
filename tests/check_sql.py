#!/usr/bin/env python3
"""Run module SQL against a disposable MariaDB bootstrap instance (no sockets or live server)."""
import argparse
import json
import os
import pathlib
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--core', required=True, type=pathlib.Path)
parser.add_argument('--mariadbd', required=True, type=pathlib.Path)
parser.add_argument('--library-path', type=pathlib.Path)
args = parser.parse_args()
module = pathlib.Path(__file__).resolve().parents[1]
source = (module / 'src/ItemScalingRegistry.cpp').read_text()

def strings(fragment):
    return ''.join(json.loads(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', fragment))

def function(name, next_name):
    return source[source.index(name):source.index(next_name, source.index(name))]

def query_after(marker):
    tail = source[source.index(marker) + len(marker):]
    return strings(re.match(r'\s*((?:"(?:\\.|[^"\\])*"\s*)+)', tail).group(1))

def query_in_function(name, next_name, marker):
    body = function(name, next_name)
    tail = body[body.index(marker) + len(marker):]
    return strings(re.match(r'\s*((?:"(?:\\.|[^"\\])*"\s*)+)', tail).group(1))

insert = strings(function('static std::string BuildItemTemplateInsertSQL', '\nnamespace'))
insert = insert[insert.index('INSERT INTO'):]
variant = strings(function('std::string VariantInsert', '// DirectCommitTransaction'))
request_insert = query_after('WorldDatabase.Execute(')
key_predicate = strings(function('std::string KeyPredicate', 'std::string DeleteRequest'))
completed_delete = strings(function('std::string DeleteCompletedRequest', 'std::string VariantInsert'))
base_columns = query_after('std::string const BaseColumns =')
sync = query_in_function(
    'bool ItemScalingRegistry::SynchronizeExistingVariants()',
    'bool ItemScalingRegistry::MaterializePendingRequests()',
    'QueryResult result = WorldDatabase.Query(',
)
schema_migration = (module / 'data/sql/db-world/updates/2026_09_25_00_item_scaling_registry_schema.sql').read_text()
generator_migration = (module / 'data/sql/db-world/updates/2026_09_25_01_item_scaling_generator_revision.sql').read_text()
ledger_migration = (module / 'data/sql/db-world/updates/2026_09_26_00_item_scaling_demand_ledger.sql').read_text()
base_sql = (module / 'sql/world/base/scaled_item_variant.sql').read_text()
if 'DirectExecute' in function('bool ItemScalingRegistry::ValidateSchema()', 'bool ItemScalingRegistry::ResolveSyntheticEntryRange()'):
    raise AssertionError('Runtime schema validation must remain read-only')

stat_pairs = [value for i in range(10) for value in (3 + i, 20 + i)]
identity = [4, 4, -1, -1, 12345, 14, 1]
values = [60000, 4, 4, -1, 12345, 14, 150, 50, *stat_pairs, 25.0, 50.0, 0.0, 0.0,
          100, 11, 12, 13, 14, 15, 16, -1, 1, 77, 100]
assert insert.count('{}') == len(values)
clone = insert.format(*values)
def mapping(entry, required=50, formula=1, revision=1):
    return variant.format(entry, 100, 53, 150, formula, revision, required, *identity, 1, entry)

def request(required=50, formula=1, revision=1, base=100):
    return request_insert.format(base, 53, 150, formula, revision, required, *identity, 1)

def delete_request(required=50, formula=1, revision=1, entry=60000):
    return completed_delete.format(key_predicate.format(100, 53, 150, formula, revision, required), entry)

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
# Start with the original registry schema: no required_level and no generator_revision.
legacy_create = re.search(
    r'CREATE TABLE IF NOT EXISTS.*?COMMENT=.*?;',
    schema_migration,
    re.S,
).group(0)
legacy = legacy_create.replace(
    "    `required_level` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT "
    "'Resolved equip requirement; part of variant identity',\n",
    '',
).replace(
    "        `formula_version`,\n        `required_level`\n",
    "        `formula_version`\n",
)
sql(legacy)
sql("INSERT INTO item_template (entry,class,subclass,name,Quality,InventoryType,ItemLevel,RequiredLevel,"
    "stat_type1,stat_value1,stat_type3,stat_value3,armor,block,holy_res,fire_res,nature_res,frost_res,shadow_res,arcane_res) "
    "VALUES (100,4,4,'Base shield',3,14,100,40,3,10,4,20,50,5,1,2,3,4,5,6),"
    "(59000,4,4,'Existing shield',3,14,140,47,3,15,4,25,75,6,1,2,3,4,5,6)")
sql('INSERT INTO scaled_item_variant (variant_entry,base_entry,target_effective_level,target_item_level,formula_version) '
    'VALUES (59000,100,50,140,1)')
sql(schema_migration)
sql(generator_migration)
sql('CREATE TABLE issued_item_before AS SELECT * FROM item_template WHERE entry=59000')
sql(ledger_migration)
check('(SELECT base_class IS NULL AND preserve_nonzero_stats IS NULL FROM scaled_item_variant WHERE variant_entry=59000)')
check('(SELECT required_level FROM scaled_item_variant WHERE variant_entry=59000)=47')
check('(SELECT generator_revision FROM scaled_item_variant WHERE variant_entry=59000)=1')
check('(SELECT COUNT(*) FROM item_template)=2')
check('(SELECT ItemLevel=140 AND RequiredLevel=47 AND armor=75 AND block=6 '
      'AND holy_res=1 AND fire_res=2 AND nature_res=3 AND frost_res=4 '
      'AND shadow_res=5 AND arcane_res=6 FROM item_template WHERE entry=59000)')
check('(SELECT base_entry=100 AND target_effective_level=50 AND target_item_level=140 '
      'AND formula_version=1 AND generator_revision=1 AND required_level=47 '
      'FROM scaled_item_variant WHERE variant_entry=59000)')
check('(SELECT COUNT(*) FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() '
      "AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='uk_variant_key' AND NON_UNIQUE=0)=6")
# A malformed non-unique index with the expected name must be repairable in place.
sql('ALTER TABLE scaled_item_variant DROP INDEX uk_variant_key, ADD KEY uk_variant_key '
    '(base_entry,target_effective_level,target_item_level,formula_version,generator_revision,required_level)')
sql(generator_migration)
check('(SELECT COUNT(*) FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() '
      "AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='uk_variant_key' AND NON_UNIQUE=0)=6")
# Reapplying both migrations must neither drop nor alter existing IDs/requirements.
sql(schema_migration)
sql(generator_migration)
check('(SELECT COUNT(*) FROM scaled_item_variant)=1')
check('(SELECT RequiredLevel FROM item_template WHERE entry=59000)=47')
check('(SELECT generator_revision FROM scaled_item_variant WHERE variant_entry=59000)=1')

# The complete six-field pending primary key collapses repeats across callers/processes.
for _ in range(5):
    sql(request())
check('(SELECT COUNT(*) FROM scaled_item_variant_request)=1')
sql(request(required=53))
sql(request(formula=2))
sql(request(revision=2))
check('(SELECT COUNT(*) FROM scaled_item_variant_request)=4')
check('(SELECT base_sound_override_subclass=-1 AND base_material=-1 AND base_displayid=12345 '
      'AND preserve_nonzero_stats=1 FROM scaled_item_variant_request WHERE formula_version=1 '
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
check('(SELECT base_displayid=12345 AND base_material=-1 AND preserve_nonzero_stats=1 '
      'FROM scaled_item_variant WHERE variant_entry=60000)')
# The issued item and its durable mapping remain byte-for-byte unchanged by the new migration.
sql(ledger_migration)
check('(SELECT COUNT(*) FROM item_template WHERE entry=59000 AND ItemLevel=140 AND armor=75)=1')
check('(SELECT variant_entry=59000 AND required_level=47 AND generator_revision=1 '
      'FROM scaled_item_variant WHERE variant_entry=59000)')
item_columns = re.findall(r'^\s*`([^`]+)`', ddl, re.M)
check('(SELECT COUNT(*) FROM issued_item_before old JOIN item_template current ON old.entry=current.entry WHERE '
      + ' AND '.join(f'old.`{column}` <=> current.`{column}`' for column in item_columns) + ')=1')


second_values = values.copy()
second_values[0], second_values[7] = 60001, 53
sql('START TRANSACTION')
sql(insert.format(*second_values))
sql(mapping(60001, required=53))
sql(delete_request(required=53, entry=60001))
sql('COMMIT')
third_values = values.copy()
third_values[0] = 60002
sql('START TRANSACTION')
sql(insert.format(*third_values))
sql(mapping(60002, revision=2))
sql(delete_request(revision=2, entry=60002))
sql('COMMIT')
check('(SELECT COUNT(*) FROM scaled_item_variant WHERE base_entry=100 AND target_effective_level=53)=3')
# A missing base is deferred: no template or mapping is inserted and its demand is retained.
sql(request(base=999))
check('(SELECT COUNT(*) FROM scaled_item_variant_request WHERE base_entry=999)=1')
# INSERT ... SELECT returning zero rows cannot commit a mapping or consume the pending request.
missing_values = values.copy()
missing_values[0], missing_values[-1] = 60004, 999
sql('START TRANSACTION')
sql(insert.format(*missing_values))
sql(variant.format(60004, 999, 53, 150, 1, 1, 50, *identity, 1, 60004))
sql(completed_delete.format(key_predicate.format(999, 53, 150, 1, 1, 50), 60004))
sql('COMMIT')
check('(SELECT COUNT(*) FROM item_template WHERE entry=60004)=0')
check('(SELECT COUNT(*) FROM scaled_item_variant WHERE variant_entry=60004)=0')
check('(SELECT COUNT(*) FROM scaled_item_variant_request WHERE base_entry=999)=1')
# Snapshot widths/signs match the canonical item schema, with nullable legacy recovery fields.
for snapshot, original in [('base_class', 'class'), ('base_subclass', 'subclass'),
                           ('base_sound_override_subclass', 'SoundOverrideSubclass'),
                           ('base_material', 'Material'), ('base_displayid', 'displayid'),
                           ('base_inventory_type', 'InventoryType'), ('base_sheath', 'sheath')]:
    check("(SELECT s.COLUMN_TYPE=i.COLUMN_TYPE FROM information_schema.COLUMNS s "
          "JOIN information_schema.COLUMNS i ON i.TABLE_SCHEMA=s.TABLE_SCHEMA "
          "WHERE s.TABLE_SCHEMA=DATABASE() AND s.TABLE_NAME='scaled_item_variant_request' "
          f"AND s.COLUMN_NAME='{snapshot}' AND i.TABLE_NAME='item_template' AND i.COLUMN_NAME='{original}')")

# Exercise the shared partial-template projection, including a stat gap.
sql('CREATE TABLE base_projection AS SELECT ' + base_columns + ' FROM item_template b WHERE entry=100')
check('(SELECT stat_value3 FROM base_projection)=20')
sql('DELETE FROM item_template WHERE entry=60001')
sql('CREATE TABLE recovery_candidates AS ' + sync.format(base_columns).replace('s.variant_entry,', 's.variant_entry AS recovered_entry,', 1)
    .replace('s.base_entry,', 's.base_entry AS recovered_base,', 1))
check('(SELECT COUNT(*) FROM recovery_candidates)=1')
check('(SELECT recovered_entry=60001 AND base_displayid=12345 FROM recovery_candidates)')
# Restore from the existing saved key without allocating or changing its ID.
sql(insert.format(*second_values))
check('(SELECT RequiredLevel FROM item_template WHERE entry=60001)=53')

# Historical missing rows are visible to recovery but must be rejected by the C++ family guard.
sql('DELETE FROM item_template WHERE entry=60002')
sql('CREATE TABLE historical_candidates AS ' + sync.format(base_columns)
    .replace('s.variant_entry,', 's.variant_entry AS recovered_entry,', 1)
    .replace('s.base_entry,', 's.base_entry AS recovered_base,', 1))
check('(SELECT recovered_entry=60002 AND generator_revision=2 FROM historical_candidates)')
check('(SELECT COUNT(*) FROM item_template WHERE entry=60002)=0')
recovery_body = function('bool ItemScalingRegistry::SynchronizeExistingVariants()',
                         'bool ItemScalingRegistry::MaterializePendingRequests()')
assert recovery_body.index('key.generatorRevision != ITEM_SCALING_GENERATOR_REVISION') < recovery_body.index('CreateScaledTemplate')

# Fresh base SQL also agrees with the migrated key layout.
sql('RENAME TABLE scaled_item_variant TO migrated_variants, scaled_item_variant_request TO migrated_requests')
sql(base_sql[base_sql.index('CREATE TABLE'):])
check('(SELECT COUNT(*) FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() '
      "AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='uk_variant_key')=6")
check('(SELECT COUNT(*) FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() '
      "AND TABLE_NAME='scaled_item_variant_request' AND INDEX_NAME='PRIMARY')=6")
sql('DROP TABLE scaled_item_variant, scaled_item_variant_request')
sql('RENAME TABLE migrated_variants TO scaled_item_variant, migrated_requests TO scaled_item_variant_request')

with tempfile.TemporaryDirectory(prefix='item-scaling-sql-') as directory:
    env = os.environ.copy()
    if args.library_path:
        env['LD_LIBRARY_PATH'] = str(args.library_path.resolve())
    command = [str(args.mariadbd.resolve()), '--no-defaults', '--bootstrap', '--datadir=' + directory,
               '--innodb-use-native-aio=0', '--innodb-buffer-pool-size=32M']
    if os.geteuid() == 0:
        command.append('--user=root')
    def run(items, expect_success=True):
        # Bootstrap reads SQL without a client, network listener, or application stack.
        payload = '\n'.join(item.rstrip().rstrip(';') + ';' for item in items) + '\n'
        result = subprocess.run(command, input=payload, text=True, capture_output=True, env=env)
        if (result.returncode == 0) != expect_success:
            raise AssertionError(result.stdout + result.stderr)
        return result
    run(statements)
    print('PASS: fresh schema, upgrades, pending dedupe/identity, materialization, snapshots, recovery SQL', flush=True)
    # Force duplicate key failure in the second half of a transaction. The first half must roll back.
    failed_values = values.copy()
    failed_values[0] = 60003
    run(['USE fixture', request()])
    result = run(['USE fixture', 'START TRANSACTION', insert.format(*failed_values),
                  mapping(60003), delete_request(entry=60003), 'COMMIT'], expect_success=False)
    assert 'Duplicate entry' in result.stderr, result.stderr
    run(['USE fixture', 'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM item_template WHERE entry=60003)=0,1,0))',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM scaled_item_variant_request WHERE '
         + key_predicate.format(100, 53, 150, 1, 1, 50) + ')=1,1,0))'])
    print('PASS: duplicate mapping causes transaction rollback; no orphan template and request retained', flush=True)
