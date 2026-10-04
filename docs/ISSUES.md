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
| <a id="mils-009"></a> **[MILS-009]** | Default Dynamic Scaling Alignment: Ceiling 0 and Floor 5 Matching AutoBalance Parity | Low | **RESOLVED** | `conf/mod_item_level_scaling.conf.dist` |
| <a id="mils-010"></a> **[MILS-010]** | Native Reference Match Bypass & Dynamic ExcludedLevels Replacement | Medium | **RESOLVED** | `src/ItemScalingTarget.h`, `src/ItemScalingFormula.cpp`, `src/ItemScalingLootScript.cpp`, `src/ItemScalingLive.cpp`, `src/ItemScalingRegistry.cpp` |
| <a id="mils-011"></a> **[MILS-011]** | Dynamic Scaling Based on Current In-Instance Creature Level with Default Floor 3 | Medium | **RESOLVED** | `src/ItemScalingTarget.h`, `src/ItemScalingLootScript.cpp`, `src/ItemScalingConfig.h`, `src/ItemScalingConfig.cpp`, `conf/mod_item_level_scaling.conf.dist` |


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
  9. Resolved C++ compilation error by safely calling `ToInstanceMap()->GetMaxPlayers()` on `InstanceMap` rather than base `Map` across `ItemScalingConfig.cpp` and `ItemScalingLootScript.cpp`.
* **Regression Guard**:
  - `python tests/check_source.py --core "../azerothcore-wotlk"` passes all contract checks, tooltip field coverage, and non-duplicate config key assertions.
  - Expanded `tests/test_target_level.cpp` with unit tests for 10M, 15M, 20M, 25M, 40M, and per-instance ceiling/floor overrides.

---

<a id="mils-009"></a>

### [MILS-009] Default Dynamic Scaling Alignment: Ceiling 0 and Floor 5 Matching AutoBalance Parity

* **Severity**: Low
* **Component**: Configuration Defaults & Dynamic Target Scaling
* **Status**: RESOLVED
* **Affected Files**: [`conf/mod_item_level_scaling.conf.dist`](../conf/mod_item_level_scaling.conf.dist), [`Server/bin/configs/modules/mod_item_level_scaling.conf.dist`](../../Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf.dist), [`Server/bin/configs/modules/mod_item_level_scaling.conf`](../../Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf), [`src/ItemScalingConfig.h`](../src/ItemScalingConfig.h), [`src/ItemScalingConfig.cpp`](../src/ItemScalingConfig.cpp), [`tests/test_target_level.cpp`](../tests/test_target_level.cpp), [`tests/check_source.py`](../tests/check_source.py)
* **Symptoms & Evidence**:
  Default configuration templates and code initializers previously configured dynamic scaling ceilings to 5 (dungeons/heroics) and 3 (raids), and floors to 3 (dungeons) and 0 (raids/heroics). In comparison, AutoBalance uses a default DynamicLevel Floor of 5 across all instance categories (`Dungeons`, `HeroicDungeons`, `Raids`, `HeroicRaids`). User requirements specified configuring default `Dynamic.Ceiling` to 0 (capping maximum boss scaling level strictly to the player level) and aligning `Dynamic.Floor` to match AutoBalance per category (5).
* **Root Cause Analysis**:
  The module's shipped default configuration values and C++ fallback options in `ItemScalingConfig` did not align with the requested 0 Ceiling and AutoBalance category Floor of 5.
* **Resolution**:
  1. Updated `conf/mod_item_level_scaling.conf.dist` setting `ItemScaling.Dynamic.Ceiling.* = 0` for all categories: `Dungeons`, `Raids`, `HeroicDungeons`, `HeroicDungeons.TBC`, `HeroicDungeons.Wrath`, `HeroicRaids`, `Raid10M`, `Raid10MHeroic`, `Raid15M`, `Raid20M`, `Raid25M`, `Raid25MHeroic`, and `Raid40M`.
  2. Updated `conf/mod_item_level_scaling.conf.dist` setting `ItemScaling.Dynamic.Floor.* = 5` across all dungeon, heroic, and raid categories matching AutoBalance's per-category floor of 5.
  3. Synchronized runtime server configurations at `Server/bin/configs/modules/mod_item_level_scaling.conf.dist` and `Server/bin/configs/modules/mod_item_level_scaling.conf`.
  4. Updated `src/ItemScalingConfig.h` default member initializers (`DynamicFloor* = 5`, `DynamicCeiling* = 0`) and `src/ItemScalingConfig.cpp` fallback option getters (`sConfigMgr->GetOption<uint32>(..., 0)` for ceilings, `5` for floors).
  5. Added unit test assertions in `tests/test_target_level.cpp` validating dynamic level resolution under ceiling 0 and floor 5.
  6. Added automated regression assertions in `tests/check_source.py` validating that `conf.dist` contains Ceiling 0 and Floor 5 for Dungeons, Raids, HeroicDungeons, and HeroicRaids.
* **Regression Guard**:
  - `python tests/check_source.py --core "../../Azerothcore server/azerothcore-wotlk"` passes, enforcing non-duplicate keys, valid syntax, and default ceiling (0) / floor (5) invariants.
  - `tests/test_target_level.cpp` verifies boss clamping to player level (+0) and trash clamping to floor (-5).
  - Maintained maintainer skill validation via `quick_validate.py`.

---

<a id="mils-010"></a>

### [MILS-010] Native Reference Match Bypass & Dynamic ExcludedLevels Replacement

* **Severity**: Medium (Optimization & Correctness)
* **Component**: Target Level Resolution, Native Reference Matching, Live Prewarm & Loot Engine
* **Status**: **RESOLVED** (2026-10-04)
* **Affected Files**: [`src/ItemScalingTarget.h`](../src/ItemScalingTarget.h), [`src/ItemScalingFormula.h`](../src/ItemScalingFormula.h), [`src/ItemScalingFormula.cpp`](../src/ItemScalingFormula.cpp), [`src/ItemScalingLootScript.h`](../src/ItemScalingLootScript.h), [`src/ItemScalingLootScript.cpp`](../src/ItemScalingLootScript.cpp), [`src/ItemScalingConfig.h`](../src/ItemScalingConfig.h), [`src/ItemScalingConfig.cpp`](../src/ItemScalingConfig.cpp), [`src/ItemScalingLive.cpp`](../src/ItemScalingLive.cpp), [`src/ItemScalingRegistry.cpp`](../src/ItemScalingRegistry.cpp), [`src/ItemScalingCommands.cpp`](../src/ItemScalingCommands.cpp), [`conf/mod_item_level_scaling.conf.dist`](../conf/mod_item_level_scaling.conf.dist), [`Server/bin/configs/modules/mod_item_level_scaling.conf.dist`](../../Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf.dist), [`Server/bin/configs/modules/mod_item_level_scaling.conf`](../../Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf), [`tests/test_target_level.cpp`](../tests/test_target_level.cpp), [`tests/check_source.py`](../tests/check_source.py)
* **Symptoms & Evidence**:
  Previously, when players ran instances at native tier levels (e.g. level 70 players in Black Temple, level 60 players in Molten Core, level 20 players in Deadmines), or when items rolled with native reference levels matching the scaling target or player level, the module could still construct synthetic items, allocate live reserved slots, and stage unnecessary variant records in the database. In live prewarm (`Live.GenerationMode = 1`), items matching native reference levels consumed CPU budget ticks and allocated preview loot structs even though no scaling would be applied. Furthermore, the module relied on a manual static blacklist (`ItemScaling.ExcludedLevels = ""`) which was inflexible, error-prone, and required server restarts to adjust.
* **Root Cause Analysis**:
  1. The native reference match check existed only late in `scaleLootItem`, after preview objects were allocated and budget consumed in prewarm.
  2. The check was not accessible to Mode 1 prewarm or `FindOrRequestVariant`.
  3. Lower-level dungeons and instances with diverse boss levels (e.g. BRD: Gerstahn lvl 52 vs Thaurissan lvl 59) required checking against both target levels and player level to guarantee that original Blizzard drops drop cleanly when content is run at intended levels.
  4. `ItemScaling.ExcludedLevels` was a static string config rather than an intrinsic, automated bypass.
* **Resolution**:
  1. Removed `ItemScaling.ExcludedLevels` completely from configuration templates, C++ config structs, loot scripts, and commands, replacing it with `ItemScaling.PreserveNativeLoot = 1` (default: 1).
  2. Implemented `ItemScalingTarget::GetNativeReferenceLevel` and `ItemScalingTarget::IsNativeTargetMatch` (`nativeRef == requestedTarget || nativeRef == bracketedTarget || (playerLevel > 0 && nativeRef == playerLevel)`).
  3. Exposed authoritative helpers in `ItemScalingFormula` (`GetNativeReferenceLevel(ItemTemplate const*)` and `IsNativeTargetMatch(ItemTemplate const*, uint8, uint8, uint8)`).
  4. Extracted `ItemScalingLootScript::ResolveTargetLevels` to share authoritative target calculation across loot generation and prewarm.
  5. In Mode 1 Prewarm (`ItemScalingLive::Impl::Prewarm`), added early `IsNativeTargetMatch` check before allocating preview loot or consuming budget ticks, advancing cleanly to the next item.
  6. In Mode 2 Loot Roll (`ItemScalingLootScript::PrepareLoot`), preserved original Blizzard items on match without live tracking, packet deferral, or SQL staging.
  7. In `ItemScalingRegistry::FindOrRequestVariant` and `ItemScalingLive::FindOrRequest`, added defense-in-depth checks ensuring zero slot allocation and zero demand ledger queuing into `scaled_item_variant_request` when generation is disabled (`Live.Enable = 0`).
  8. Updated GM commands (`.itemscaling status` and `.itemscaling preview`) to report `PreserveNativeLoot` and native match bypass status.
  9. Preserved `ItemScaling.UseAutoBalanceSettings = 0` as the default standalone configuration.
* **Regression Guard**:
  - `tests/test_target_level.cpp` compiled and executed with 12 comprehensive acceptance tests covering:
    - High-level raids: H=70 BT native 70 (true), H=80 BT native 70 (false), H=60 MC native 60 (true), H=70 MC native 60 (false), H=70 native 80 downscale (false), BracketStep rounding (true).
    - Lower-level instances: H=20 Deadmines boss 20 (true), H=20 Deadmines mob 18 (true), H=40 SM Herod 40 (true), H=40 SM trash 40 (true).
    - Diverse boss levels: H=52 BRD Gerstahn native 47 (true), H=52 BRD Gerstahn native 52 (true), H=80 BRD native 47/52 (false, proper upscaling).
    - Reference level fallback when `RequiredLevel == 0`.
  - `tests/check_source.py` passes all assertions, verifying `PreserveNativeLoot == '1'` and complete removal of `ExcludedLevels`.
  - Maintainer skill validation via `quick_validate.py` passed with code 0.

---

<a id="mils-011"></a>

### [MILS-011] Dynamic Scaling Based on Current In-Instance Creature Level with Default Floor 3

* **Severity**: Medium (Scaling Accuracy & Gameplay Fairness)
* **Component**: Target Level Resolution, Creature In-Instance Level Tracking, Dynamic Floor & Ceiling Defaults
* **Status**: **RESOLVED** (2026-10-04)
* **Affected Files**: [`src/ItemScalingTarget.h`](../src/ItemScalingTarget.h), [`src/ItemScalingLootScript.cpp`](../src/ItemScalingLootScript.cpp), [`src/ItemScalingConfig.h`](../src/ItemScalingConfig.h), [`src/ItemScalingConfig.cpp`](../src/ItemScalingConfig.cpp), [`conf/mod_item_level_scaling.conf.dist`](../conf/mod_item_level_scaling.conf.dist), [`Server/bin/configs/modules/mod_item_level_scaling.conf`](../../Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf), [`tests/test_target_level.cpp`](../tests/test_target_level.cpp), [`tests/check_source.py`](../tests/check_source.py), [`docs/plans/current_mob_level_scaling_plan.md`](plans/current_mob_level_scaling_plan.md)
* **Symptoms & Evidence**:
  In dungeons and raids scaled by AutoBalance or engine scaling (e.g. Tempest Keep: The Eye, Black Temple, Molten Core), raid skull bosses (such as Void Reaver) were scaled to level 83 for level 80 players. However, loot dropped at level 75 instead of level 80 or 78.
* **Root Cause Analysis**:
  1. `ItemScalingLootScript::ResolveTargetLevels` computed `delta = instanceMaxLevel - creatureSourceLevel` by subtracting the native template level (73 in TBC `creature_template`) from `LFGDungeons.dbc::MaxLevel` (83 in Wrath DBC), creating a spurious cross-expansion $\Delta = 10$.
  2. With Ceiling 0 and Floor 5, the raw target became $80 + 0 - 10 = 70$, which was clamped to the floor $80 - 5 = 75$.
  3. `ItemScalingTarget::Resolve` checked `if (IsExternallyScaledCreature(input) && !input.realPlayersOnly)`. Because `realPlayersOnly` defaults to `true`, the live scaled level `observedCreatureLevel` was completely ignored.
* **Resolution**:
  1. Refactored `ItemScalingTarget::Resolve` to scale directly from the creature's in-instance effective level $L_{\text{mob}}$: $\text{RawTarget} = L_{\text{mob}} - \text{Floor}$, bounded by $\min(\text{PlayerLevel} + \text{Ceiling}, \text{MaxLevel})$.
  2. Implemented `ResolveEffectiveCreatureLevel` in `ItemScalingLootScript.cpp`:
     - Live combat (`creature != nullptr`): Authoritative live level `creature->GetLevel()` is used directly.
     - Mode 1 prewarm / catalogue (`creature == nullptr`): Predicts $L_{\text{mob}}$ using boss template rank and map type (+3 for raid skull boss, +2 for dungeon boss), guaranteeing 100% target level parity between prewarm generation and live loot drops.
     - Unscaled/standalone fallback: Evaluates unscaled creatures using the same boss rank offsets relative to player level.
  3. Changed global default `Floor` to `3` across all dungeons, heroic dungeons, and raids in `ItemScalingConfig.h`, `ItemScalingConfig.cpp`, and configuration templates.
     - For all raid bosses (level 83 at level 80): $83 - 3 = 80 \implies$ drops max level 80 items by default out-of-the-box.
     - For raid trash mobs (level 80 at level 80): $80 - 3 = 77 \implies$ drops level 77 items by default.
     - For 5-man dungeon bosses (level 82 at level 80): $82 - 3 = 79 \implies$ drops level 79 items by default (or level 80 with Floor 2 or Ceiling 3).
     - For intended tier runs (e.g. level 70 player in TK/BT): $73 - 3 = 70 \implies$ matches native level 70 drops and triggers Native Reference Match Bypass without synthetic generation.
* **Regression Guard**:
  - `tests/test_target_level.cpp` covers raid bosses ($L_{\text{mob}}=83$ with Floor 3 $\implies$ 80, Floor 5 $\implies$ 78, Floor 0 $\implies$ 80), trash ($L_{\text{mob}}=80$ with Floor 3 $\implies$ 77, Floor 5 $\implies$ 75, Floor 0 $\implies$ 80), dungeon bosses ($L_{\text{mob}}=82$ with Floor 3 $\implies$ 79, Floor 2 $\implies$ 80, Floor 0 $\implies$ 80), native level 70 runs ($L_{\text{mob}}=73$ with Floor 3 $\implies$ 70 native match), chests, and fixed mode.
  - `tests/check_source.py` passes with zero errors, validating all configuration defaults (`Floor = 3`, `Ceiling = 0`, `PreserveNativeLoot = 1`).
  - Maintainer skill validated via `quick_validate.py`.

---

<a id="mils-012"></a>

### [MILS-012] Configurable Weighted Linear Variance for Dynamic Floor and Ceiling (±1, ±2, ±3 Levels)

* **Severity**: Low (Feature & Progression Enhancement)
* **Component**: Target Level Resolution, Configuration Engine, Dynamic Floor & Ceiling Variance, Loot Generation
* **Status**: **RESOLVED** (2026-10-04)
* **Affected Files**: [`src/ItemScalingVariance.h`](../src/ItemScalingVariance.h), [`src/ItemScalingConfig.h`](../src/ItemScalingConfig.h), [`src/ItemScalingConfig.cpp`](../src/ItemScalingConfig.cpp), [`src/ItemScalingLootScript.h`](../src/ItemScalingLootScript.h), [`src/ItemScalingLootScript.cpp`](../src/ItemScalingLootScript.cpp), [`conf/mod_item_level_scaling.conf.dist`](../conf/mod_item_level_scaling.conf.dist), [`Server/bin/configs/modules/mod_item_level_scaling.conf`](../../Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf), [`tests/test_target_level.cpp`](../tests/test_target_level.cpp), [`docs/plans/dynamic_variance_floor_ceiling_plan.md`](plans/dynamic_variance_floor_ceiling_plan.md)
* **Symptoms & Evidence**:
  Previously, Floor and Ceiling values were static constants per instance category. In 5-man dungeons at player level 80 with Floor = 3, dungeon bosses scaled to level 82 always deterministically dropped level 79 loot ($82 - 3 = 79$). There was no mechanism for dungeon bosses to roll higher level items (e.g. level 80) or for trash mobs to drop jackpot upgrades without permanently altering global floor constants for the entire instance.
* **Root Cause Analysis**:
  `mod-item-level-scaling` lacked a probability variance model to dynamically shift floor and ceiling values on individual loot generation passes.
* **Resolution**:
  1. Created standalone header [`src/ItemScalingVariance.h`](../src/ItemScalingVariance.h) defining `struct VarianceWeights` and pure C++17 parsers `ParseWeights` and `RollDelta`.
  2. Implemented independent master configuration toggles:
     - `ItemScaling.Dynamic.Floor.Variance.Enable = 1` (default: 1 - Enabled)
     - `ItemScaling.Dynamic.Ceiling.Variance.Enable = 0` (default: 0 - Disabled)
     - `ItemScaling.Dynamic.Variance.Scope = 0` (0 = Per-Loot/Per-Boss, 1 = Per-Item)
  3. Added per-category variance configuration strings for all 13 dungeon and raid categories, supporting named pair syntax (`"-1:20.0, -2:10.0, -3:5.0"`) and 6-element positional syntax (`"5.0, 10.0, 20.0, 0.0, 0.0, 0.0"`).
  4. Configured default Floor variance across categories to `"-1:20.0, -2:10.0, -3:5.0"` (35% total shift chance; 65% base Floor 3):
     - -1 delta (20%): Floor becomes 2 $\implies$ level 82 dungeon bosses drop max level 80 items.
     - -2 delta (10%): Floor becomes 1 $\implies$ level 80 dungeon trash drops level 79 items.
     - -3 delta (5%): Floor becomes 0 $\implies$ jackpot drop where level 80 trash drops max level 80 items.
  5. Implemented `ItemScalingLootScript::ResolveTargetLevels` variance evaluation, passing effective clamped floor and ceiling into `ItemScalingTarget::Resolve`.
  6. Implemented Per-Item re-rolling in `PrepareLoot::scaleLootItem` when `VarianceScope = 1`.
  7. Mode 1 prewarm strictly uses delta 0 to ensure deterministic baseline template caching, with live on-demand generation seamlessly publishing lucky rolled variants.
  8. Synchronized configuration template and runtime server configuration.
* **Regression Guard**:
  - `tests/test_target_level.cpp` includes unit tests for named pair parsing, positional parsing, deterministic interval rolls, boundary checks, and target level resolution with rolled floor and ceiling variance.
  - `python tests/check_source.py --core "../../Azerothcore server/azerothcore-wotlk"` passes with zero duplicate config keys and all checks green.
  - Maintainer skill validated with code 0 via `quick_validate.py`.

---

<a id="mils-013"></a>

### [MILS-013] Decommission Legacy Demand Ledger in Favor of Pure Live Generation

* **Severity**: Medium (Architectural Simplification & Zero Blocking DB I/O)
* **Component**: ItemScalingRegistry, ItemScalingConfig, Database Migrations, Live Generation Engine
* **Status**: **RESOLVED** (2026-10-04)
* **Affected Files**: [`src/ItemScalingConfig.h`](../src/ItemScalingConfig.h), [`src/ItemScalingConfig.cpp`](../src/ItemScalingConfig.cpp), [`src/ItemScalingRegistry.h`](../src/ItemScalingRegistry.h), [`src/ItemScalingRegistry.cpp`](../src/ItemScalingRegistry.cpp), [`src/ItemScalingLive.h`](../src/ItemScalingLive.h), [`src/ItemScalingLootScript.cpp`](../src/ItemScalingLootScript.cpp), [`src/ItemScalingCommands.cpp`](../src/ItemScalingCommands.cpp), [`conf/mod_item_level_scaling.conf.dist`](../conf/mod_item_level_scaling.conf.dist), [`data/sql/db-world/base/scaled_item_variant.sql`](../data/sql/db-world/base/scaled_item_variant.sql), [`sql/world/base/scaled_item_variant.sql`](../sql/world/base/scaled_item_variant.sql), [`data/sql/db-world/updates/2026_10_05_00_retire_demand_ledger.sql`](../data/sql/db-world/updates/2026_10_05_00_retire_demand_ledger.sql), [`data/sql/db-world/mod_item_level_scaling_readme.sql`](../data/sql/db-world/mod_item_level_scaling_readme.sql), [`tests/check_source.py`](../tests/check_source.py), [`tests/check_sql.py`](../tests/check_sql.py), [`README.md`](../README.md), [`AGENTS.md`](../AGENTS.md), [`docs/ARCHITECTURE.md`](ARCHITECTURE.md), [`docs/plans/no_ledger_live_scaling_plan_final.md`](plans/no_ledger_live_scaling_plan_final.md)
* **Symptoms & Evidence**:
  The module previously operated a split architecture between legacy demand ledger materialization at server startup (`scaled_item_variant_request`) and first-run live scaling (`ItemScalingLive`). When players encountered ungenerated scalable items with `ItemScaling.Live.Enable = 0`, requests were queued to a database table to be materialized on the next server reboot. This created architectural dualism, maintenance overhead, dead materializer code paths (`BuildItemTemplateInsertSQL`, `CommitBatch`, `MaterializePendingRequests`), and complex synchronous startup bottlenecks.
* **Root Cause Analysis**:
  The demand ledger was originally introduced as a temporary bridge prior to the full implementation of the pure live scaling engine (MILS-004). With the completion of bounded world-thread prewarm (Mode 1), on-demand live generation with async durable staging (Mode 2), publication barriers into pre-allocated `item_template` slots, and startup staging recovery (`RecoverStagedTemplates`), the demand ledger became completely redundant. Furthermore, `_requestedKeys` was previously misunderstood as ledger-only, whereas in reality it served a critical role protecting against duplicate synthetic ID allocation collisions for committed variants.
* **Resolution**:
  1. **Purged Legacy Demand Ledger**:
     - Removed `ItemScaling.DemandLedger.Enable` and `ItemScaling.MaxNewVariantsPerStartup` from `conf/mod_item_level_scaling.conf.dist`, `ItemScalingConfig.h`, and `ItemScalingConfig.cpp`. Documented `ItemScaling.Live.Enable = 0` as pure persisted-only mode (serves existing variants from in-memory index; unseen items drop native Blizzard template).
     - Removed `MaterializePendingRequests`, `QueueVariantRequest`, `BuildItemTemplateInsertSQL`, `CommitBatch`, `VariantInsert`, `DeleteRequest`, `DeleteCompletedRequest`, and dead helper types from `ItemScalingRegistry.h` and `ItemScalingRegistry.cpp`.
     - Removed `_requestMutex` and `<mutex>` from `ItemScalingRegistry`.
  2. **Retained & Renamed Collision Protection**:
     - Renamed `_requestedKeys` to `_committedKeys`. Populated during `ItemScalingRegistry::Initialize()` from `scaled_item_variant` records (including invalid/unusable variants).
     - Made `_committedKeys` strictly immutable after initialization, providing lock-free O(1) protection against duplicate synthetic ID allocation collisions in `FindOrRequestVariant`.
  3. **Strict Two-InnoDB Schema Invariants**:
     - Modernized `ItemScalingRegistry::EnsureSchema()` and `ValidateSchema()` to strictly require exactly two InnoDB tables (`item_template` and `scaled_item_variant`).
     - Removed `scaled_item_variant_request` table definitions from `base/scaled_item_variant.sql` in both `data/sql/db-world/base/` and `sql/world/base/`.
     - Added idempotent retirement migration `data/sql/db-world/updates/2026_10_05_00_retire_demand_ledger.sql` (`DROP TABLE IF EXISTS scaled_item_variant_request;`). Preserved historical initial migration `2026_09_27_00_item_scaling_initial_schema.sql` intact for schema history.
  4. **Enforced Startup Synchronization Order**:
     - `ItemScalingRegistry::_dbSynchronized` is set to `true` strictly after `sItemScalingLive->ReserveSlots()` completes successfully.
  5. **Cleaned Command & Script References**:
     - Updated `.itemscaling preview` in `ItemScalingCommands.cpp` to accurately distinguish between indexed in-memory variants, live on-demand generation, and persisted-only mode.
     - Updated comments in `ItemScalingLive.h` and `ItemScalingLootScript.cpp`.
* **Regression Guard**:
  - `tests/check_source.py --core "../../Azerothcore server/azerothcore-wotlk"` passes with code 0, verifying 0 active ledger symbols, 0 duplicate config keys, packet API hooks, publication barriers, and `_committedKeys` collision guards.
  - `tests/check_sql.py --core "../../Azerothcore server/azerothcore-wotlk" --mariadbd "../../Azerothcore server/mini sql/bin/mysqld.exe"` passes 100% with code 0, validating:
    - 3-state install/upgrade compatibility matrix: State A (fresh install), State B (existing empty request table), State C (existing install with pending requests dropped cleanly).
    - Non-interference assertions: `scaled_item_variant`, `item_template`, and all three live staging tables (`mod_item_level_scaling_slot`, `mod_item_level_scaling_staged_item`, `mod_item_level_scaling_staged_variant`) remain strictly untouched.
    - All 25 module `SELECT` call sites execute cleanly.
    - Atomic promotion and restart persistence across database restart.
  - Maintainer skill validated via `quick_validate.py` with code 0.
