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
