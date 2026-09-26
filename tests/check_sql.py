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
roots = query_after('QueryResult roots = WorldDatabase.Query(')
base_columns = query_after('std::string const BaseColumns =')
sync = query_in_function(
    'bool ItemScalingRegistry::SynchronizeExistingVariants()',
    'bool ItemScalingRegistry::PreStageDungeonLoot()',
    'QueryResult result = WorldDatabase.Query(',
)
base_sql = (module / 'sql/world/base/scaled_item_variant.sql').read_text()
if 'DirectExecute' in function('bool ItemScalingRegistry::ValidateSchema()', 'bool ItemScalingRegistry::ResolveSyntheticEntryRange()'):
    raise AssertionError('Runtime schema validation must remain read-only')

stat_pairs = [value for i in range(10) for value in (3 + i, 20 + i)]
values = [60000, 150, 50, *stat_pairs, 25.0, 50.0, 0.0, 0.0, 100, 11, 12, 13, 14, 15, 16, 77, 100]
assert insert.count('{}') == len(values)
clone = insert.format(*values)
key_insert = variant.format(60000, 100, 53, 150, 1, 50)

statements = ["CREATE DATABASE fixture", "USE fixture",
              "SET SESSION sql_mode='STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION'",
              'CREATE TABLE assertions (passed INT NOT NULL CHECK (passed=1)) ENGINE=InnoDB']

def sql(statement):
    statements.append(statement.rstrip().rstrip(';'))

def check(condition):
    sql('INSERT INTO assertions VALUES (IF((' + condition + '),1,0))')

for table in ['item_template', 'creature', 'creature_multispawn', 'instance_template', 'creature_template',
              'creature_loot_template', 'reference_loot_template', 'gameobject_loot_template',
              'gameobject', 'gameobject_template']:
    text = (args.core / 'data/sql/base/db_world' / (table + '.sql')).read_text()
    ddl = re.search(r'CREATE TABLE.*?\) ENGINE=.*?;', text, re.S).group(0)
    sql(ddl)

sql(base_sql[base_sql.index('CREATE TABLE'):])
check("(SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() "
      "AND TABLE_NAME='scaled_item_variant')=7")
check("(SELECT COUNT(*) FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() "
      "AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='uk_variant_key' AND NON_UNIQUE=0)=5")

sql("INSERT INTO item_template (entry,class,subclass,name,Quality,InventoryType,ItemLevel,RequiredLevel,"
    "stat_type1,stat_value1,stat_type3,stat_value3,armor,block,holy_res,fire_res,nature_res,frost_res,shadow_res,arcane_res) "
    "VALUES (100,4,4,'Base shield',3,14,100,40,3,10,4,20,50,5,1,2,3,4,5,6)")

sql('START TRANSACTION')
sql(clone)
sql(key_insert)
sql('COMMIT')
check('(SELECT COUNT(*) FROM item_template WHERE entry=60000 AND ItemLevel=150 AND RequiredLevel=50 '
      'AND armor=100 AND block=77 AND holy_res=11 AND fire_res=12 AND nature_res=13 '
      'AND frost_res=14 AND shadow_res=15 AND arcane_res=16 AND stat_value10=29)=1')
check("(SELECT name FROM item_template WHERE entry=60000)='Base shield'")
check('(SELECT base_entry=100 AND target_effective_level=53 AND target_item_level=150 '
      'AND formula_version=1 AND required_level=50 '
      'FROM scaled_item_variant WHERE variant_entry=60000)')

# Same base/target/formula but another equip requirement must coexist.
second_values = values.copy()
second_values[0], second_values[2] = 60001, 53
sql('START TRANSACTION')
sql(insert.format(*second_values))
sql(variant.format(60001, 100, 53, 150, 1, 53))
sql('COMMIT')

# Same base/target/requirement under another formula version must remain distinct.
third_values = values.copy()
third_values[0] = 60002
sql('START TRANSACTION')
sql(insert.format(*third_values))
sql(variant.format(60002, 100, 53, 150, 2, 50))
sql('COMMIT')
check('(SELECT COUNT(*) FROM scaled_item_variant WHERE base_entry=100 AND target_effective_level=53)=3')

# All spawn alternatives, a difficulty template, and chest roots resolve on the real core schema.
sql("INSERT INTO instance_template (map,parent,script,allowMount) VALUES (33,0,'',0)")
sql('INSERT INTO creature_template (entry,difficulty_entry_1,lootid) VALUES (10,11,100),(11,0,101),(20,0,102),(30,0,103)')
sql('INSERT INTO creature (guid,id,map) VALUES (1,10,33)')
sql('INSERT INTO creature_multispawn (spawnId,entry) VALUES (1,20),(1,30)')
for entry, item in [(100,201),(101,202),(102,203),(103,204)]:
    sql(f'INSERT INTO creature_loot_template (Entry,Item,Reference,Chance) VALUES ({entry},{item},0,100)')
sql('INSERT INTO gameobject_template (entry,type,Data1) VALUES (40,3,104)')
sql('INSERT INTO gameobject (guid,id,map) VALUES (2,40,33)')
sql('INSERT INTO gameobject_loot_template (Entry,Item,Reference,Chance) VALUES (104,205,-500,100)')
sql('INSERT INTO reference_loot_template (Entry,Item,Reference,Chance) VALUES (500,206,-501,100),(501,207,0,100)')
sql('CREATE TABLE root_results AS ' + roots)
check('(SELECT COUNT(*) FROM root_results)=5')
check('(SELECT COUNT(*) FROM root_results WHERE Reference=-500)=1')

# Exercise the shared partial-template projection, including a stat gap.
sql('CREATE TABLE base_projection AS SELECT ' + base_columns + ' FROM item_template b WHERE entry=100')
check('(SELECT stat_value3 FROM base_projection)=20')

# Recovery still restores a missing current-schema template under its persisted ID.
sql('DELETE FROM item_template WHERE entry=60001')
sql('CREATE TABLE recovery_candidates AS ' + sync.format(base_columns).replace(
    's.variant_entry,', 's.variant_entry AS recovered_entry,', 1).replace(
    's.base_entry,', 's.base_entry AS recovered_base,', 1))
check('(SELECT COUNT(*) FROM recovery_candidates)=1')
sql(insert.format(*second_values))
check('(SELECT RequiredLevel FROM item_template WHERE entry=60001)=53')

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
    print('PASS: fresh schema, scaled persistence, variant identity, discovery, recovery', flush=True)

    # Force duplicate key failure in the second half of a transaction. The first half must roll back.
    failed_values = values.copy()
    failed_values[0] = 60003
    result = run(['USE fixture', 'START TRANSACTION', insert.format(*failed_values),
                  variant.format(60003, 100, 53, 150, 1, 50), 'COMMIT'], expect_success=False)
    assert 'Duplicate entry' in result.stderr, result.stderr
    run(['USE fixture',
         'INSERT INTO assertions VALUES (IF((SELECT COUNT(*) FROM item_template WHERE entry=60003)=0,1,0))'])
    print('PASS: duplicate mapping causes transaction rollback; no orphan template', flush=True)
