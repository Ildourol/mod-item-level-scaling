# Plan: Complete Removal of AutoBalance Cross-Module Coupling

## 1. Executive Summary & Objective

Currently, `mod-item-level-scaling` contains an optional coupling feature:
```ini
ItemScaling.UseAutoBalanceSettings = 1
```
When this setting is enabled (which was the default), the module silently queries `AutoBalance.conf` at startup and overwrites its own `ItemScaling.LevelScaling.Method`, `Dynamic.Floor.Dungeons`, `Dynamic.Ceiling.Dungeons`, `Dynamic.Floor.Raids`, and `Dynamic.Ceiling.Raids`.

### Problems Caused by this Auto-Link:
1. **Hidden Configuration Overrides**: An administrator adjusts `ItemScaling.Dynamic.Floor.Dungeons = 0` in `mod_item_level_scaling.conf`, but the server silently ignores it and continues using AutoBalance's `Floor = 5`, causing Level 80 players to receive Level 75 drops.
2. **Unnecessary Cross-Module Coupling**: `mod-item-level-scaling` should be completely autonomous and self-contained. It should not depend on or parse keys from `mod-autobalance`.
3. **Configuration Complexity**: Managing duplicate settings across two separate configuration files creates user confusion.

### Objective:
Completely remove `UseAutoBalanceSettings` from the C++ codebase and configuration templates, making `mod-item-level-scaling` 100% self-reliant on its own configuration.

---

## 2. Affected Code & Configuration Files

| Component | File Path | Planned Modification |
| :--- | :--- | :--- |
| **Config Header** | [`src/ItemScalingConfig.h`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling/src/ItemScalingConfig.h#L52) | Remove `bool UseAutoBalanceSettings{false};` member. |
| **Config Loader** | [`src/ItemScalingConfig.cpp`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling/src/ItemScalingConfig.cpp#L108-L129) | Remove `sConfigMgr->GetOption<bool>("ItemScaling.UseAutoBalanceSettings", ...)` and the `AutoBalance.*` option ingestion block. |
| **Commands / Diagnostics** | [`src/ItemScalingCommands.cpp`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling/src/ItemScalingCommands.cpp#L98) | Remove `AutoBalance Synergy` telemetry line from `.itemscaling status`. |
| **Config Template** | [`conf/mod_item_level_scaling.conf.dist`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling/conf/mod_item_level_scaling.conf.dist#L257) | Remove `# ItemScaling.UseAutoBalanceSettings` documentation and key definition. |
| **Active Server Config** | [`Server/bin/configs/modules/mod_item_level_scaling.conf`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf) | Remove `ItemScaling.UseAutoBalanceSettings` line. |

---

## 3. Step-by-Step Implementation Details

### Step 1: Remove `UseAutoBalanceSettings` from `src/ItemScalingConfig.h`
- Delete `bool UseAutoBalanceSettings{false};` from `struct ItemScalingConfig`.

### Step 2: Remove AutoBalance Ingestion in `src/ItemScalingConfig.cpp`
- Delete lines 108–129:
  ```cpp
  UseAutoBalanceSettings = sConfigMgr->GetOption<bool>("ItemScaling.UseAutoBalanceSettings", true);
  if (UseAutoBalanceSettings)
  {
      ...
  }
  ```
- **Result**: The module will strictly use its own parsed values:
  - `ItemScaling.LevelScaling.Method`
  - `ItemScaling.Dynamic.Floor.Dungeons`
  - `ItemScaling.Dynamic.Ceiling.Dungeons`
  - `ItemScaling.Dynamic.Floor.Raids`
  - `ItemScaling.Dynamic.Ceiling.Raids`

### Step 3: Clean `.itemscaling status` Output in `src/ItemScalingCommands.cpp`
- Remove:
  ```cpp
  handler->PSendSysMessage("AutoBalance Synergy: {}", ...);
  ```
- The command will report pure standalone scaling settings.

### Step 4: Clean Configuration Files
- Remove all mentions of `ItemScaling.UseAutoBalanceSettings` in:
  - `conf/mod_item_level_scaling.conf.dist`
  - `Server/bin/configs/modules/mod_item_level_scaling.conf` (and the junction `Server/configs/modules/mod_item_level_scaling.conf`)

### Step 5: Verification & Safety
- Run `tests/check_source.py` against core to ensure zero dangling symbols or contract breaks.
- Run `quick_validate.py` on `.agents/skills/item-level-scaling-maintainer`.
