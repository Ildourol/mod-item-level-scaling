#!/usr/bin/env python3
"""Audit live scaling's source contracts; does not compile C++ or start worldserver."""
import argparse
import pathlib
import re

parser = argparse.ArgumentParser()
parser.add_argument('--core', required=True, type=pathlib.Path)
args = parser.parse_args()
module = pathlib.Path(__file__).resolve().parents[1]
core = args.core.resolve()
live = (module / 'src/ItemScalingLive.cpp').read_text()
snapshot = (module / 'src/ItemScalingSnapshot.h').read_text()

# Native item queries must read the same fields that we persist, including arrays.
handler = (core / 'src/server/game/Handlers/ItemHandler.cpp').read_text()
handler = handler[handler.index('void WorldSession::HandleItemQuerySingleOpcode'):
                  handler.index('void WorldSession::HandleReadItem')]
native_fields = set(re.findall(r'pProto->(\w+)', re.sub(r'//[^\n]*|/\*.*?\*/', '', handler, flags=re.S)))
serialized_fields = set(re.findall(r'item\.(\w+)', snapshot))
assert native_fields <= serialized_fields, 'Unpersisted tooltip fields: ' + str(native_fields - serialized_fields)
assert 'GetItemTemplate(item)' in handler, 'Recheck the native item-query lookup'

manager = (core / 'src/server/game/Globals/ObjectMgr.h').read_text()
assert 'std::vector<ItemTemplate*> const* GetItemTemplateStoreFast()' in manager
assert '*(*store)[entry] = request.scaled;' in live
assert 'const_cast' not in live and 'AddCustomItemTemplate' not in live

# This implementation relies on joined map workers and deferred session dispatch.
world = (core / 'src/server/game/World/World.cpp').read_text()
assert world.index('sMapMgr->Update(diff)') < world.index('sScriptMgr->OnWorldUpdate(diff)')
map_manager = (core / 'src/server/game/Maps/MapMgr.cpp').read_text()
update = map_manager[map_manager.index('void MapMgr::Update('):map_manager.index('void MapMgr::', map_manager.index('void MapMgr::Update(') + 1)]
assert '_updater.wait()' in update, 'Recheck the publication barrier on this core'
opcodes = (core / 'src/server/game/Server/Protocol/Opcodes.cpp').read_text()
query_opcode = next(line for line in opcodes.splitlines() if 'DEFINE_HANDLER(CMSG_ITEM_QUERY_SINGLE,' in line)
assert 'PROCESS_THREADSAFE' in query_opcode
dispatch = (core / 'src/server/game/Scripting/ScriptDefines/ServerScript.cpp').read_text()
assert 'WorldPacket const& packet' in dispatch
assert 'CanPacketReceive(WorldSession* session, WorldPacket const& packet) override' in live

# No synchronous database/file operations in gameplay entry points or Update.
gameplay = live[live.index('uint32 ItemScalingLive::FindOrRequest('):live.index('bool ItemScalingLive::Impl::LoadCatalogue()')]
prewarm_and_update = live[live.index('void ItemScalingLive::Impl::Prewarm('):]
for body in [gameplay, prewarm_and_update]:
    assert not any(operation in body for operation in ['WorldDatabase.Query(', 'DirectExecute(',
                                                      'DirectCommitTransaction(', 'EscapeString(', 'sleep(', 'fstream'])
assert 'AsyncCommitTransaction(transaction)' in prewarm_and_update
assert live.index('request.state = Impl::State::Ready') > live.index('_impl->durable.pop_front()')

# Reserved IDs cannot reach the client before their complete template is published.
assert 'SERVERHOOK_CAN_PACKET_RECEIVE' in live and 'SERVERHOOK_CAN_PACKET_SEND' in live
assert 'CMSG_ITEM_QUERY_SINGLE' in live and 'SMSG_ITEM_QUERY_SINGLE_RESPONSE' in live
assert '_impl->reserved.erase(entry)' in live
assert 'loot->loot_type == LOOT_NONE' in live
assert 'loot->items[item.index].randomSuffix == item.suffix' in live

config = (module / 'conf/mod_item_level_scaling.conf.dist').read_text()
options = re.findall(r'^(ItemScaling\.[\w.]+)\s*=\s*(.*)$', config, re.M)
assert len(options) == len(dict(options)), 'Duplicate config keys'
assert dict(options)['ItemScaling.Live.GenerationMode'].strip() == '1'
assert dict(options)['ItemScaling.RandomSuffix.Mode'].strip() == '0'
assert dict(options)['ItemScaling.Dynamic.Ceiling.Dungeons'].strip() == '0'
assert dict(options)['ItemScaling.Dynamic.Floor.Dungeons'].strip() == '3'
assert dict(options)['ItemScaling.Dynamic.Ceiling.Raids'].strip() == '0'
assert dict(options)['ItemScaling.Dynamic.Floor.Raids'].strip() == '3'
assert dict(options)['ItemScaling.Dynamic.Ceiling.HeroicDungeons'].strip() == '0'
assert dict(options)['ItemScaling.Dynamic.Floor.HeroicDungeons'].strip() == '3'
assert dict(options)['ItemScaling.Dynamic.Ceiling.HeroicRaids'].strip() == '0'
assert dict(options)['ItemScaling.Dynamic.Floor.HeroicRaids'].strip() == '3'
assert 'ItemScaling.ExcludedLevels' not in dict(options), 'ItemScaling.ExcludedLevels should be removed'
assert dict(options)['ItemScaling.PreserveNativeLoot'].strip() == '1'
assert 'ItemScaling.DemandLedger.Enable' not in dict(options), 'DemandLedger.Enable should be removed from config'
assert 'ItemScaling.MaxNewVariantsPerStartup' not in dict(options), 'MaxNewVariantsPerStartup should be removed from config'

for sql in (module / 'data/sql').rglob('*.sql'):
    assert not re.search(r'ALTER TABLE[^;]*\b(?:ADD COLUMN IF NOT EXISTS|DROP KEY IF EXISTS)\b', sql.read_text(), re.I)
registry = (module / 'src/ItemScalingRegistry.cpp').read_text()
assert 'ADD COLUMN IF NOT EXISTS `' not in registry and 'DROP KEY IF EXISTS `' not in registry

# Zero active demand ledger references in active source, config, and base SQL schemas
active_files = list((module / 'src').glob('*.*')) + [
    module / 'conf/mod_item_level_scaling.conf.dist',
    module / 'data/sql/db-world/base/scaled_item_variant.sql',
    module / 'sql/world/base/scaled_item_variant.sql',
]
for path in active_files:
    content = path.read_text()
    assert 'scaled_item_variant_request' not in content, f'Active file {path.name} contains scaled_item_variant_request'
    assert 'MaterializePendingRequests' not in content, f'Active file {path.name} contains MaterializePendingRequests'
    assert 'DemandLedger' not in content, f'Active file {path.name} contains DemandLedger'

# Collision guard and synchronization ordering checks in registry
assert '_committedKeys.insert(key)' in registry
assert 'if (_committedKeys.count(key))' in registry
assert '_requestedKeys' not in registry
assert '_requestMutex' not in registry
assert 'sItemScalingLive->ReserveSlots()' in registry

print('PASS: native tooltip field coverage, publication barrier, packet API, asynchronous gameplay paths, pure live config, zero active ledger symbols, and MySQL syntax guards')
print('NOT VERIFIED: C++ compilation, concurrent runtime behavior, first-run loot and client rendering')
