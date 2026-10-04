# Implementation Plan: Dynamic Scaling Floor & Ceiling for Dungeons, Raids, and TBC/Wrath Heroics

## Goal Description
Configure and extend `mod-item-level-scaling` to refine dynamic level scaling boundaries across all instance types:
1. **Preserve Dynamic Scaling as Default**: Retain `ItemScaling.LevelScaling.Method = "dynamic"`.
2. **Normal Dungeons**: Set Dynamic Ceiling to **5** (bosses scale up to player level + 5) and Dynamic Floor to **3** (trash scales down to at most player level - 3).
3. **Raids**: Set Dynamic Ceiling to **3** (bosses scale up to player level + 3) and Dynamic Floor to **0** (no downscaling below player level).
4. **Heroic Dungeons (TBC & Wrath)**: Add dedicated configuration options and runtime resolution for Heroic 5-player dungeons:
   - **TBC Heroic Dungeons**: Ceiling **5**, Floor **0** (no downscaling below player level).
   - **Wrath Heroic Dungeons**: Ceiling **5**, Floor **0** (no downscaling below player level).
   - General **Heroic Dungeons fallback**: Ceiling **5**, Floor **0**.

---

## User Review Required

> [!IMPORTANT]
> **AutoBalance Synergy Hazard (`ItemScaling.UseAutoBalanceSettings`)**:
> In `mod_item_level_scaling.conf.dist` and active server config `Server/bin/configs/modules/mod_item_level_scaling.conf`, `ItemScaling.UseAutoBalanceSettings` was previously enabled (`1`). If left enabled, it imports AutoBalance's defaults (`Ceiling = 1, Floor = 5` for dungeons), which would silently overwrite your configured values!
> **Action**: This plan explicitly defaults `ItemScaling.UseAutoBalanceSettings = 0` in both configuration files so that your custom settings are strictly authoritative.

> [!NOTE]
> **Level Clamping Invariant (1..80)**:
> In dynamic scaling, targets are computed as:
> $$\text{raw} = \text{playerLevel} + \text{ceiling} - \text{delta}$$
> clamped within $[\text{playerLevel} - \text{floor}, \text{playerLevel} + \text{ceiling}]$ and bounded by $[MinLevel, MaxLevel]$ (1..80).
> - For a Level 80 player in a Wrath Heroic or Level 80 Raid: $\text{playerLevel} = 80, \text{floor} = 0 \implies \text{floorLevel} = 80$. Trash and bosses will drop at Level 80 ($80 + 5$ clamped to $80$). Zero items drop below Level 80.
> - For a Level 70 player in a TBC Heroic: $\text{floor} = 0, \text{ceiling} = 5 \implies$ drops range between Level 70 and 75.
> - For a Level 20 player in Deadmines: $\text{floor} = 3, \text{ceiling} = 5 \implies$ drops range between Level 17 and 25.

---

## Architecture Overview

```mermaid
flowchart TD
    A["Loot Generated in Instance"] --> B["ItemScalingLootScript::PrepareLoot()"]
    B --> C["Resolve Map Details: map->IsRaid(), map->IsHeroic(), map->GetEntry()->Expansion()"]
    C --> D["ItemScalingConfig::GetDynamicFloor(map) & GetDynamicCeiling(map)"]

    D -->|IsRaid == true| E["Raid Bracket: Ceiling 3, Floor 0"]
    D -->|IsHeroic == true| F{"Expansion ID"}
    F -->|Expansion 1 (TBC)| G["TBC Heroic: Ceiling 5, Floor 0"]
    F -->|Expansion 2 (Wrath)| H["Wrath Heroic: Ceiling 5, Floor 0"]
    F -->|Other / Generic| I["Heroic Dungeons Fallback: Ceiling 5, Floor 0"]
    D -->|Normal Dungeon| J["Normal Dungeon: Ceiling 5, Floor 3"]

    E --> K["ItemScalingTarget::Input (floor, ceiling)"]
    G --> K
    H --> K
    I --> K
    J --> K

    K --> L["ItemScalingTarget::Resolve() -> Target Level L_target"]
    L --> M["Live / Prewarm Scaled Item Assignment"]
```

---

## Proposed Changes

### Configuration Definitions & Module Settings
Order of dependencies: Config header -> Config implementation -> Module scripts -> Config templates.

---

#### [MODIFY] [`src/ItemScalingConfig.h`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling/src/ItemScalingConfig.h)

- Add new configuration members for Heroic Dungeons, TBC Heroic Dungeons, and Wrath Heroic Dungeons.
- Update default values for Dungeons and Raids.
- Overload `GetDynamicFloor` and `GetDynamicCeiling` to accept `Map const* map` as well as explicit `(bool isRaid, bool isHeroic, uint32 expansion)`.

```diff
--- a/src/ItemScalingConfig.h
+++ b/src/ItemScalingConfig.h
@@ -47,8 +47,15 @@ public:
-    uint8 DynamicFloorDungeons{5};
-    uint8 DynamicCeilingDungeons{3};
-    uint8 DynamicFloorRaids{5};
+    uint8 DynamicFloorDungeons{3};
+    uint8 DynamicCeilingDungeons{5};
+    uint8 DynamicFloorRaids{0};
     uint8 DynamicCeilingRaids{3};
+
+    uint8 DynamicFloorHeroicDungeons{0};
+    uint8 DynamicCeilingHeroicDungeons{5};
+    uint8 DynamicFloorHeroicDungeonsTBC{0};
+    uint8 DynamicCeilingHeroicDungeonsTBC{5};
+    uint8 DynamicFloorHeroicDungeonsWrath{0};
+    uint8 DynamicCeilingHeroicDungeonsWrath{5};
 
-    bool UseAutoBalanceSettings{false};
+    bool UseAutoBalanceSettings{false};
@@ -82,4 +89,6 @@ public:
-    [[nodiscard]] uint8 GetDynamicFloor(bool isRaid) const;
-    [[nodiscard]] uint8 GetDynamicCeiling(bool isRaid) const;
+    [[nodiscard]] uint8 GetDynamicFloor(bool isRaid, bool isHeroic = false, uint32 expansion = 0) const;
+    [[nodiscard]] uint8 GetDynamicCeiling(bool isRaid, bool isHeroic = false, uint32 expansion = 0) const;
+    [[nodiscard]] uint8 GetDynamicFloor(Map const* map) const;
+    [[nodiscard]] uint8 GetDynamicCeiling(Map const* map) const;
```

---

#### [MODIFY] [`src/ItemScalingConfig.cpp`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling/src/ItemScalingConfig.cpp)

- Parse new config keys in `ItemScalingConfig::Load()`:
  - `ItemScaling.Dynamic.Ceiling.Dungeons` (default 5)
  - `ItemScaling.Dynamic.Floor.Dungeons` (default 3)
  - `ItemScaling.Dynamic.Ceiling.Raids` (default 3)
  - `ItemScaling.Dynamic.Floor.Raids` (default 0)
  - `ItemScaling.Dynamic.Ceiling.HeroicDungeons` (default 5)
  - `ItemScaling.Dynamic.Floor.HeroicDungeons` (default 0)
  - `ItemScaling.Dynamic.Ceiling.HeroicDungeons.TBC` (default 5)
  - `ItemScaling.Dynamic.Floor.HeroicDungeons.TBC` (default 0)
  - `ItemScaling.Dynamic.Ceiling.HeroicDungeons.Wrath` (default 5)
  - `ItemScaling.Dynamic.Floor.HeroicDungeons.Wrath` (default 0)
- Implement hierarchical resolution in `GetDynamicFloor` and `GetDynamicCeiling`.

```cpp
uint8 ItemScalingConfig::GetDynamicFloor(bool isRaid, bool isHeroic, uint32 expansion) const
{
    if (isRaid)
    {
        return DynamicFloorRaids;
    }

    if (isHeroic)
    {
        if (expansion == 1) // TBC
            return DynamicFloorHeroicDungeonsTBC;
        if (expansion == 2) // Wrath
            return DynamicFloorHeroicDungeonsWrath;
        return DynamicFloorHeroicDungeons;
    }

    return DynamicFloorDungeons;
}

uint8 ItemScalingConfig::GetDynamicCeiling(bool isRaid, bool isHeroic, uint32 expansion) const
{
    if (isRaid)
    {
        return DynamicCeilingRaids;
    }

    if (isHeroic)
    {
        if (expansion == 1) // TBC
            return DynamicCeilingHeroicDungeonsTBC;
        if (expansion == 2) // Wrath
            return DynamicCeilingHeroicDungeonsWrath;
        return DynamicCeilingHeroicDungeons;
    }

    return DynamicCeilingDungeons;
}

uint8 ItemScalingConfig::GetDynamicFloor(Map const* map) const
{
    if (!map)
        return DynamicFloorDungeons;

    bool isRaid = map->IsRaid();
    bool isHeroic = map->IsHeroic();
    uint32 expansion = map->GetEntry() ? map->GetEntry()->Expansion() : 0;
    return GetDynamicFloor(isRaid, isHeroic, expansion);
}

uint8 ItemScalingConfig::GetDynamicCeiling(Map const* map) const
{
    if (!map)
        return DynamicCeilingDungeons;

    bool isRaid = map->IsRaid();
    bool isHeroic = map->IsHeroic();
    uint32 expansion = map->GetEntry() ? map->GetEntry()->Expansion() : 0;
    return GetDynamicCeiling(isRaid, isHeroic, expansion);
}
```

---

#### [MODIFY] [`src/ItemScalingLootScript.cpp`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling/src/ItemScalingLootScript.cpp)

- Update line 279-280 to pass `map` to `GetDynamicFloor` and `GetDynamicCeiling`:
```diff
--- a/src/ItemScalingLootScript.cpp
+++ b/src/ItemScalingLootScript.cpp
@@ -276,8 +276,8 @@ void ItemScalingLootScript::PrepareLoot(...)
     targetInput.creatureSourceLevel = cSrc;
     targetInput.instanceMaxLevel = cMax;
     targetInput.observedCreatureLevel = creature ? creature->GetLevel() : cSrc;
-    targetInput.floor = sItemScalingConfig->GetDynamicFloor(map->IsRaid());
-    targetInput.ceiling = sItemScalingConfig->GetDynamicCeiling(map->IsRaid());
+    targetInput.floor = sItemScalingConfig->GetDynamicFloor(map);
+    targetInput.ceiling = sItemScalingConfig->GetDynamicCeiling(map);
     targetInput.minLevel = sItemScalingConfig->MinLevel;
```

---

#### [MODIFY] [`src/ItemScalingCommands.cpp`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling/src/ItemScalingCommands.cpp)

- Update `.itemscaling status` to report the new TBC and Wrath heroic dungeon scaling boundaries:
```diff
--- a/src/ItemScalingCommands.cpp
+++ b/src/ItemScalingCommands.cpp
@@ -99,4 +99,6 @@ public:
         handler->PSendSysMessage("Dynamic Dungeons: Floor -{}, Ceiling +{}", sItemScalingConfig->DynamicFloorDungeons, sItemScalingConfig->DynamicCeilingDungeons);
         handler->PSendSysMessage("Dynamic Raids: Floor -{}, Ceiling +{}", sItemScalingConfig->DynamicFloorRaids, sItemScalingConfig->DynamicCeilingRaids);
+        handler->PSendSysMessage("Dynamic Heroic Dungeons (TBC): Floor -{}, Ceiling +{}", sItemScalingConfig->DynamicFloorHeroicDungeonsTBC, sItemScalingConfig->DynamicCeilingHeroicDungeonsTBC);
+        handler->PSendSysMessage("Dynamic Heroic Dungeons (Wrath): Floor -{}, Ceiling +{}", sItemScalingConfig->DynamicFloorHeroicDungeonsWrath, sItemScalingConfig->DynamicCeilingHeroicDungeonsWrath);
```

---

#### [MODIFY] [`conf/mod_item_level_scaling.conf.dist`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling/conf/mod_item_level_scaling.conf.dist)

- Document the new configuration options with descriptions and default values:
```ini
#
#    Dynamic Scaling Floor and Ceiling
#        Description: In dynamic mode, controls how many levels below and above the highest real player (H)
#                     a creature's effective level is allowed to scale.
#                     Floor:   levels below H (e.g. 3 means trash scales down to at most H - 3; 0 means no downscaling below H)
#                     Ceiling: levels above H (e.g. 5 means boss scales up to at most H + 5)
#

ItemScaling.Dynamic.Ceiling.Dungeons = 5
ItemScaling.Dynamic.Floor.Dungeons = 3
ItemScaling.Dynamic.Ceiling.Raids = 3
ItemScaling.Dynamic.Floor.Raids = 0

#
#    Dynamic Scaling for Heroic Dungeons (TBC & Wrath)
#        Description: Fine-grained ceiling and floor configuration for 5-player Heroic dungeons.
#                     TBC: The Burning Crusade heroic dungeons (expansion 1)
#                     Wrath: Wrath of the Lich King heroic dungeons (expansion 2)
#                     HeroicDungeons serves as the general fallback for custom/other heroic dungeons.
#

ItemScaling.Dynamic.Ceiling.HeroicDungeons = 5
ItemScaling.Dynamic.Floor.HeroicDungeons = 0
ItemScaling.Dynamic.Ceiling.HeroicDungeons.TBC = 5
ItemScaling.Dynamic.Floor.HeroicDungeons.TBC = 0
ItemScaling.Dynamic.Ceiling.HeroicDungeons.Wrath = 5
ItemScaling.Dynamic.Floor.HeroicDungeons.Wrath = 0

#
#    ItemScaling.UseAutoBalanceSettings
#        Description: If mod-autobalance is loaded, automatically adopt its configured settings.
#                     Set to 0 to prevent mod-autobalance from overriding your ItemScaling floor/ceiling settings.
#        Default:     0 - Disabled (strictly use ItemScaling.* values)
#                     1 - Enabled (adopt AutoBalance values)
#

ItemScaling.UseAutoBalanceSettings = 0
```

---

#### [MODIFY] [`Server/bin/configs/modules/mod_item_level_scaling.conf`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20server/Server/bin/configs/modules/mod_item_level_scaling.conf)

- Mirror the new configuration keys and defaults into the runtime server configuration.
- Set `ItemScaling.UseAutoBalanceSettings = 0` so that AutoBalance does not override the new settings.

---

#### [MODIFY] [`tests/test_target_level.cpp`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-item-level-scaling/tests/test_target_level.cpp)

- Add regression assertions testing:
  - Normal Dungeon input (Ceiling 5, Floor 3).
  - Raid input (Ceiling 3, Floor 0: trash never scales below player level).
  - Heroic input (Ceiling 5, Floor 0: trash never scales below player level, boss scales +5).

---

## Verification Plan

### Automated Tests
1. **Source & Contract Verification**:
   ```powershell
   python tests/check_source.py --core "../azerothcore-wotlk"
   ```
   Validates no duplicate keys, correct regex parsing, and contract adherence.

2. **Skill Runbook Check**:
   ```powershell
   python "C:\Users\Admin\.codex\skills\.system\skill-creator\scripts\quick_validate.py" ".agents/skills/item-level-scaling-maintainer"
   ```

3. **C++ Compilation**:
   ```powershell
   & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe" "C:\Users\Admin\AntigravityProfiles\Projects Azerothcore\Azerothcore server\azerothcore-wotlk\build\modules\mod-item-level-scaling.vcxproj" /p:Configuration=RelWithDebInfo
   ```

### Manual Verification
1. **Startup Check**:
   - Launch server or check `.itemscaling status` in-game.
   - Confirm status outputs:
     - `Dynamic Dungeons: Floor -3, Ceiling +5`
     - `Dynamic Raids: Floor -0, Ceiling +3`
     - `Dynamic Heroic Dungeons (TBC): Floor -0, Ceiling +5`
     - `Dynamic Heroic Dungeons (Wrath): Floor -0, Ceiling +5`
2. **Normal Dungeon Test**:
   - Enter Deadmines on Level 20 character.
   - Verify trash loot does not scale below Level 17 (20 - 3), and final boss scales up to Level 25 (20 + 5).
3. **Heroic Dungeon Test (TBC & Wrath)**:
   - Enter Heroic Blood Furnace or Heroic Utgarde Keep on Level 70 / 80 character.
   - Verify trash loot never drops below player level (Floor = 0).
   - Verify boss loot scales with Ceiling = 5 (clamped to 80 on Level 80).
