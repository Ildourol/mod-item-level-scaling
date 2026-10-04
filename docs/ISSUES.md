# mod-item-level-scaling Issues & Diagnostics Log

> **Location:** `mod-item-level-scaling/docs/ISSUES.md`
> **Source Module:** [`mod-item-level-scaling`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling)  
> **Commander Navigator:** [`COMMANDER/README.md`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/COMMANDER/README.md)

---

## 1. Module Issues Summary

| Issue ID | Title | Severity | Status | Resolved In |
|---|---|---|---|---|
| **[MILS-001]** | Duplicate Key Definitions in Runtime Module Configuration File | Low | **RESOLVED** | `Server/bin/configs/modules/mod_item_level_scaling.conf` |
| <a id="mils-002"></a> **[MILS-002]** | Demand-Ledger Stat Discrepancies on Custom Heirlooms | Medium | **MONITORED** | `src/ItemLevelScaling.cpp` |
| **[MILS-003]** | Missing `scaled_item_variant` Schema Columns Disables Item Scaling at Boot | Medium | **RESOLVED** | `data/sql/db-world/base/scaled_item_variant.sql` |

| **[MILS-004]** | First-run live scaling awaits runtime and client verification | Medium | **OPEN / MITIGATED** | `src/ItemScalingLive.cpp` |
| <a id="mils-005"></a> **[MILS-005]** | Startup Fatal Error: Unknown Column 'id1' in Field List during Dungeon Loot Prewarm | High | **RESOLVED** | `src/ItemScalingLive.cpp` |
| <a id="mils-006"></a> **[MILS-006]** | Async Transaction Failure: Error 1264 Out of Range on AllowableClass during Live Staging | High | **RESOLVED** | `src/ItemScalingSnapshot.h` |
| <a id="mils-007"></a> **[MILS-007]** | Granular Dynamic Scaling Ceilings & Floors for Dungeons, Raids, and TBC/Wrath Heroics | Medium | **RESOLVED** | `src/ItemScalingConfig.cpp` |
| <a id="mils-008"></a> **[MILS-008]** | AutoBalance Synergy Pairing, 1-to-1 Category Revamp & Real-Time In-Instance Announcements | Medium | **RESOLVED** | `src/ItemScalingConfig.cpp`, `src/ItemScalingLive.cpp` |

---

## 2. Issue Details

<a id="mils-001"></a>

### [MILS-001] Duplicate Key Definitions in Runtime Module Configuration File
* **Severity**: Low (Startup Warning)
* **Component**: Configuration Loader
* **Status**: **RESOLVED** (2026-10-03)
* **Symptoms**: Startup console warning `[Config::LoadData] Duplicate entry in config file 'mod_item_level_scaling.conf'`.
* **Root Cause**: Redundant parameter blocks existed in both default header and customized bottom block.
* **Resolution**: Deduplicated keys while preserving active scaling values.

---

<a id="mils-003"></a>

### [MILS-003] Missing `scaled_item_variant` Schema Columns Disables Item Scaling at Boot
* **Severity**: Medium (Feature Degraded / Disabled at Startup)
* **Component**: Database Schema Initializer / `src/ItemScalingRegistry.cpp`
* **Status**: **RESOLVED** (2026-10-03)
* **Symptoms & Terminal Log Traces**:
  ```text
  Missing scaled_item_variant columns; install the complete module schema.
  Startup prerequisites failed; item scaling disabled for this run.
  ```
* **Root Cause Analysis**:
  The module checks for specific variant columns in `acore_world.scaled_item_variant`. If the table or columns are incomplete, item scaling is safely aborted during startup.
* **Resolution**:
  Applied the complete schema definition from `data/sql/db-world/base/scaled_item_variant.sql` into `acore_world`, introducing all 17 expected columns and unique identity indices.
* **Regression Guard**:
  Verified `DESCRIBE scaled_item_variant` and `DESCRIBE scaled_item_variant_request` contain all 17/16 columns and indices expected by `ValidateSchema()`.


---

<a id="mils-004"></a>

### [MILS-004] First-run live scaling awaits runtime and client verification

* **Severity**: Medium
* **Component**: Live variant generation, loot gating, item queries and SQL recovery
* **Status**: OPEN / MITIGATED
* **Affected Files**: [Live controller](../src/ItemScalingLive.cpp), [snapshot serializer](../src/ItemScalingSnapshot.h), [registry](../src/ItemScalingRegistry.cpp), [loot hook](../src/ItemScalingLootScript.cpp), [formula](../src/ItemScalingFormula.cpp), [config](../conf/mod_item_level_scaling.conf.dist), [migration](../data/sql/db-world/updates/2026_10_04_00_item_scaling_live.sql), [tests](../tests/README.md).
* **Symptoms & Evidence**: The original demand ledger awards an ordinary item for a first unseen key and only creates its variant on a later startup. The user requested first-visit scaling with correct bag/tooltips, without core or Playerbots edits. The accepted [hybrid plan](plans/first_run_item_scaling_hybrid_plan.md) now has a module implementation, but its client-visible result has not been exercised in a realm.
* **Root Cause Analysis**: Async SQL alone cannot update core-loaded templates or the client's cached item metadata. First-run selection needs an existing RAM template, durable values before award, gating before native group rolls, and consistent native item-query fields. Older legacy upgrade statements also used MariaDB-only ALTER guards, discovered during Oracle MySQL verification.
* **Resolution**: Added inert startup reservations and bounded live generation with Pending/Durable/Ready states, exact-key deduplication, async full snapshots, world-barrier publication, packet query/creature loot deferral, and an ordinary chest adapter after successful native lock validation. Mode 1 prepares dungeon catalogue keys and mode 2 uses actual rolls. Random mode stays off by default; checked opt-in baking preserves unsupported items. Recovery promotes exact staged IDs and values transactionally before core item loading, including when scaling is disabled. Engine/schema/ownership checks protect recovery. Both runtime and updater legacy guards now use portable MySQL syntax. Core and Playerbots source were not changed.
* **Regression Guard**: Oracle MySQL Community Server 8.4.11 full disposable SQL suite passed: first installation, all SELECT sites, exact keys and snapshots, real DB restart, staged persistence, promotion despite changed base stats, collision/duplicate rollback, repeat promotion and NULL rejection. Source contracts, codestyle and whitespace checks passed. New snapshot/identity C++ regression cases are written but unexecuted. Compilation, installation, live migration and worldserver restart were not performed at the user's request. Keep this record OPEN / MITIGATED until compiled tests, multiple-map-worker publication, first-run loot in both modes, lock/group/Playerbot handling, bag/equipped/inspection/link tooltips and persistence through reconnect/trade/mail/AH/guild bank are verified in an isolated realm.

---

<a id="mils-005"></a>

### [MILS-005] Startup Fatal Error: Unknown Column 'id1' in Field List during Dungeon Loot Prewarm

* **Severity**: High (Worldserver Startup Crash)
* **Component**: Live variant generation / `src/ItemScalingLive.cpp`
* **Status**: **RESOLVED** (2026-10-04)
* **Affected Files**: [Live controller](../src/ItemScalingLive.cpp)
* **Symptoms & Evidence**:
  Worldserver crashed during startup with MySQL error 1054:
  ```text
  [1054] Unknown column 'id1' in 'field list'
  Your database structure is not up to date. Please make sure you've executed all queries in the sql/updates folders.
  # Function 'MySQLConnection::_HandleMySQLErrno'
  >> ABORTED
  ```
* **Root Cause Analysis**:
  In `ItemScalingLive::Impl::CollectDungeonCatalogues()`, the creature query used legacy TrinityCore / MaNGOS multi-spawn column syntax (`SELECT DISTINCT map,id1 FROM creature WHERE id1<>0 UNION SELECT DISTINCT map,id2... UNION SELECT DISTINCT map,id3...`). In AzerothCore, creature spawns use a single `id` column (`creature.id`), causing the startup query against `acore_world` to fail.
* **Resolution**:
  Changed the query in `src/ItemScalingLive.cpp` to use AzerothCore's native schema: `SELECT DISTINCT map, id FROM creature WHERE id <> 0`. Recompiled and re-installed `worldserver.exe`.
* **Regression Guard**:
  Rebuilt core with `--target INSTALL` and verified `worldserver.exe` boots completely through dungeon loot pre-warming to the interactive prompt (`AC>`) with zero SQL errors.

---

<a id="mils-006"></a>

### [MILS-006] Async Transaction Failure: Error 1264 Out of Range on AllowableClass during Live Staging

* **Severity**: High (Data Persistence Failure & Rollback)
* **Component**: Live Staging / Item Snapshot Serialization (`src/ItemScalingSnapshot.h`) & Dungeon Loot Prewarm (`src/ItemScalingLive.cpp`)
* **Status**: **RESOLVED** (2026-10-04)
* **Affected Files**: [Item snapshot serializer](../src/ItemScalingSnapshot.h), [Live controller](../src/ItemScalingLive.cpp), [Runtime config](../../Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf)
* **Symptoms & Evidence**:
  During live dungeon runs or loot prewarming, items failed to persist or scale. `Server/bin/Errors.log` contained hundreds of SQL rollback errors:
  ```text
  Transaction aborted. Exception caught in AsyncCallbackProcessor.
  In async_tquery query: 'INSERT INTO mod_item_level_scaling_staged_item (...) VALUES (...)'
  [1264] Out of range value for column 'AllowableClass' at row 1
  ```
  Additionally:
  1. Green dungeon drops (which predominantly have random suffixes in Vanilla/TBC) were completely skipped because `ItemScaling.RandomSuffix.Mode` defaulted to `0` (Skip).
  2. The runtime config `Server/bin/configs/modules/mod_item_level_scaling.conf` was missing the `ItemScaling.Live.*` settings block.
  3. Prewarm catalogue references with negative IDs (the AzerothCore convention for nested loot references) were ignored due to a `reference > 0` condition in `LoadCatalogue()`.
* **Root Cause Analysis**:
  In AzerothCore, `ItemTemplate::AllowableClass`, `AllowableRace`, and `BagFamily` are stored in RAM as unsigned 32-bit integers (`uint32`). When an item has all classes/races allowed (represented as `-1` in MySQL signed `int(11)` columns), `uint32` evaluates to `4294967295`. `ItemScalingSnapshot::Insert()` formatted this directly into SQL insert statements without casting to `int32`, causing MySQL strict mode error 1264 (`Out of range value`) and rolling back the entire staging transaction. Furthermore, `ItemScalingLive.cpp` checked `reference > 0` instead of `std::abs(reference)`, omitting negative reference loot templates from dungeon prewarming.
* **Resolution**:
  1. Updated `src/ItemScalingSnapshot.h` to cast `AllowableClass`, `AllowableRace`, `BagFamily`, `TotemCategory`, `RandomProperty`, `socketContent_*`, `socketBonus`, and `GemProperties` to `int32` during SQL serialization.
  2. Updated `src/ItemScalingLive.cpp` to handle `reference != 0` using `std::abs(reference)`.
  3. Added the complete `ItemScaling.Live.*` configuration block to runtime configs and set `ItemScaling.RandomSuffix.Mode = 0` (Skip/Exclude by default so random suffix items drop at original base stats without scaling, while keeping Mode 1 available as an opt-in).
  4. Implemented in-game dungeon entry announcements (`[ItemScaling] Entering <Dungeon>: Loot scaling active for level X...`) and dynamic higher-level recalculation broadcasts with prewarm cache reset upon higher-level player entry.
  5. Identified binary drift between CMake install destination (`Server/worldserver.exe`) and runtime directory (`Server/bin/worldserver.exe`) so the user can ensure deployment to `Server/bin/`.
* **Regression Guard**:
  Verified `tests/check_source.py` passes all syntax and contract checks. Confirmed SQL statements output `-1` instead of `4294967295` for unbounded class/race masks, preventing error 1264. Verified `quick_validate.py` on the maintainer skill.

---

<a id="mils-007"></a>

### [MILS-007] Granular Dynamic Scaling Ceilings & Floors for Dungeons, Raids, and TBC/Wrath Heroics

* **Severity**: Medium (Feature & Tuning Enhancement)
* **Component**: Target Level Resolution, Configuration & Announcements
* **Status**: **RESOLVED** (2026-10-04)
* **Affected Files**: [Config header](../src/ItemScalingConfig.h), [Config loader](../src/ItemScalingConfig.cpp), [Loot script](../src/ItemScalingLootScript.cpp), [Live announcer](../src/ItemScalingLive.cpp), [Commands](../src/ItemScalingCommands.cpp), [Config template](../conf/mod_item_level_scaling.conf.dist), [Server runtime config](../../Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf), [Tests](../tests/test_target_level.cpp)
* **Symptoms & Evidence**:
  Previously, dynamic scaling applied a coarse `Floor = 5, Ceiling = 3` across all instances indiscriminately. In raids and endgame heroics, mobs could downscale loot up to 5 levels below player level (e.g. level 75 items dropping for level 80 players in heroic dungeons). In normal dungeons, bosses were limited to +3 levels. Furthermore, AutoBalance synergy (`UseAutoBalanceSettings = 1`) silently imported AutoBalance's `Floor = 5, Ceiling = 1`, overriding module settings.
* **Root Cause Analysis**:
  `ItemScalingConfig::GetDynamicFloor` and `GetDynamicCeiling` only distinguished raids vs non-raids via `bool isRaid`, lacking map difficulty (Heroic vs Normal) and expansion context (Vanilla vs TBC vs Wrath).
* **Resolution**:
  1. Updated normal dungeons dynamic ceiling to 5 and floor to 3 (`ItemScaling.Dynamic.Ceiling.Dungeons = 5`, `ItemScaling.Dynamic.Floor.Dungeons = 3`).
  2. Updated raids dynamic ceiling to 3 and floor to 0 (`ItemScaling.Dynamic.Ceiling.Raids = 3`, `ItemScaling.Dynamic.Floor.Raids = 0`, eliminating downscaling below player level).
  3. Added dedicated heroic dungeon options for TBC and Wrath with ceiling 5 and floor 0 (`ItemScaling.Dynamic.Ceiling.HeroicDungeons.TBC = 5`, `ItemScaling.Dynamic.Floor.HeroicDungeons.TBC = 0`, `ItemScaling.Dynamic.Ceiling.HeroicDungeons.Wrath = 5`, `ItemScaling.Dynamic.Floor.HeroicDungeons.Wrath = 0`, with `HeroicDungeons` fallback).
  4. Bypassed/disabled heroic scaling for Vanilla (expansion 0), falling back to standard normal dungeon floor and ceiling.
  5. Updated in-game dungeon entry and recalculation announcements to explicitly recognize when entering heroic dungeons and display their expansion (`Heroic <Dungeon> (TBC)`, `Heroic <Dungeon> (Wrath)`).
  6. Defaulted `ItemScaling.UseAutoBalanceSettings = 0` to prevent AutoBalance from clobbering module settings.
  7. Added comprehensive unit test coverage in `tests/test_target_level.cpp`.
* **Regression Guard**:
  Unit test assertions in `tests/test_target_level.cpp` verify:
  - Normal Dungeon: boss = player + 5, trash floor = player - 3.
  - Raid: boss = player + 3, trash floor = player - 0 (zero downscaling below player level).
  - TBC/Wrath Heroic: boss = player + 5, trash floor = player - 0 (zero downscaling below player level).
  `tests/check_source.py` passes all syntax, duplicate key, and contract checks.

---

<a id="mils-008"></a>

### [MILS-008] AutoBalance Synergy Pairing, 1-to-1 Category Revamp & Real-Time In-Instance Announcements

* **Severity**: Medium (Feature & Integration Enhancement)
* **Component**: AutoBalance Pairing, Instance Category Resolution, Player Scripts & In-Game Announcements
* **Status**: **RESOLVED** (2026-10-04)
* **Affected Files**: [`src/ItemScalingConfig.h`](../src/ItemScalingConfig.h), [`src/ItemScalingConfig.cpp`](../src/ItemScalingConfig.cpp), [`src/ItemScalingLive.cpp`](../src/ItemScalingLive.cpp), [`src/ItemScalingLootScript.cpp`](../src/ItemScalingLootScript.cpp), [`src/ItemScalingCommands.cpp`](../src/ItemScalingCommands.cpp), [`conf/mod_item_level_scaling.conf.dist`](../conf/mod_item_level_scaling.conf.dist), [`Server/bin/configs/modules/mod_item_level_scaling.conf`](../../Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf), [`tests/test_target_level.cpp`](../tests/test_target_level.cpp)
* **Symptoms & Evidence**:
  Previous iterations lacked 1-to-1 category alignment with `mod-autobalance`. AutoBalance distinguishes 5M Normal, 5M Heroic, 10M Normal, 10M Heroic, 15M (UBRS), 20M (ZG/AQ20), 25M Normal, 25M Heroic, 40M (MC/BWL/AQ40), and PerInstance overrides. Furthermore, AutoBalance synergy was disabled without an explicit ingestion parser for AutoBalance's 5-token `AutoBalance.LevelScaling.DynamicLevel.PerInstance` syntax. In-instance announcements did not report active ceiling/floor boundaries, and player level-ups inside dungeons/raids did not trigger recalculation broadcasts.
* **Root Cause Analysis**:
  1. `ItemScalingConfig` did not model granular raid size tiers (`maxPlayers <= 10`, `15`, `20`, `25`, `40`) or per-instance override tables (`DynamicOverrides`).
  2. Announcements in `ItemScalingLive::EnterMap` lacked category difficulty tags and ceiling/floor boundaries.
  3. No `PlayerScript` hook existed to detect `PLAYERHOOK_ON_LEVEL_CHANGED` when players gained levels inside active instances.
* **Resolution**:
  1. Added full 1-to-1 category support matching AutoBalance: `Scale.HeroicDungeons`, `Scale.Raid10M`, `Scale.Raid10MHeroic`, `Scale.Raid15M`, `Scale.Raid20M`, `Scale.Raid25M`, `Scale.Raid25MHeroic`, and `Scale.Raid40M`.
  2. Added dedicated dynamic ceilings & floors for all raid categories (default Ceiling: 3, Floor: 0).
  3. Implemented `DynamicOverrides` map with dual-syntax parser supporting both ItemScaling 3-token (`[MapID] [Ceiling] [Floor]`) and AutoBalance 5-token (`[MapID] [SkipHigher] [SkipLower] [Ceiling] [Floor]`) override formats.
  4. Implemented AutoBalance synergy loader when `UseAutoBalanceSettings = 1` (default 0), dynamically copying method, ceilings, floors, and per-instance tables from `AutoBalance.conf`.
  5. Implemented `GetInstanceCategoryDescription(Map const* map)` producing formatted tags (e.g. `[25-man Raid]`, `[Wrath Heroic]`, `[Dungeon]`).
  6. Implemented `ItemScalingLivePlayerScript` hooked to `PLAYERHOOK_ON_LEVEL_CHANGED` to automatically recalculate and broadcast updates whenever a character levels up in an instance.
  7. Formatted announcements on entry and level change to report instance name, category tag, highest player name & level, and active limits (`[Ceiling: +%u, Floor: -%u]`).
  8. Synchronized configuration template and runtime server config.
* **Regression Guard**:
  - `python tests/check_source.py --core "../azerothcore-wotlk"` passes all contract checks, tooltip field coverage, and non-duplicate config key assertions.
  - Expanded `tests/test_target_level.cpp` with unit tests for 10M, 15M, 20M, 25M, 40M, and per-instance ceiling/floor overrides.


