# Plan: First-Run Scaling Resolution & In-Game Scaling Announcements

## 1. Executive Summary & Root Cause of the Continued Issue

During your recent dungeon rerun in Deadmines and Wailing Caverns, items still dropped unscaled. The investigation identified the exact causes:

### Root Cause 1: Binary Drift (`Server/bin/worldserver.exe` vs `Server/worldserver.exe`)
- The compilation completed at 12:09 PM, but CMake's `INSTALL` target deployed `worldserver.exe` to `C:/.../Azerothcore server/Server/worldserver.exe`.
- However, `open.bat` launches `C:/.../Azerothcore server/Server/bin/worldserver.exe`.
- The active running worldserver process was PID 12376, which was still running the **old binary** from 11:24 AM (`Server/bin/worldserver.exe`).
- Consequently, the running server did not have our `AllowableClass` signedness fix. When boss loot rolled in Wailing Caverns (items 10411, 6472, 6473, 6448, 6449), MySQL still threw:
  ```text
  [1264] Out of range value for column 'AllowableClass' at row 1
  Live persistence failed for base 10411; original loot retained.
  ```
  Because the transaction aborted, the engine safely fell back to the original unscaled item.

### Root Cause 2: Green Random Suffix Items Dropping as Original
- As requested, `ItemScaling.RandomSuffix.Mode = 0` (Skip / Exclude) was set in `mod_item_level_scaling.conf`.
- In Vanilla dungeons, trash mobs drop green items with random suffixes (e.g., *of the Bear*, *of the Eagle*). With Mode 0, these are skipped by design and drop as original unscaled items with native stats.

---

## 2. New Feature: Dungeon Entry Announcement & Higher-Level Recalculation

You requested:
> *"Is it possible to add like a line when I enter the dungeon that the items will scale at that level and when a higher level enters the dungeon the calculation makes again"*

### Proposed Feature Design:
1. **Dungeon Entry Message**:
   - When a player enters an eligible instance (dungeon/raid), send a system chat message to the player:
     ```text
     [ItemScaling] Entering Deadmines: Loot scaling active for level 80 (highest player: Testgr).
     ```
2. **Dynamic Recalculation on Higher-Level Entry**:
   - If a new player enters who has a higher level than the previous highest player in the instance (e.g. party was level 20, and a level 80 player joins):
     - Recalculate the instance scaling bracket immediately.
     - Reset the prewarming visitor so it begins prewarming items for the new higher level bracket.
     - Broadcast a system announcement to all players inside the instance:
       ```text
       [ItemScaling] Higher-level player Testgr (level 80) entered. Loot scaling recalculated to level 80!
       ```
3. **Player Leave / Level-Up Handling**:
   - If players level up inside the instance or the highest level changes, update the visit target dynamically.

---

## 3. Implementation Steps

### Step 1: Deploy Binary to the Correct Runtime Directory (`Server/bin/`)
- Shut down `worldserver` cleanly.
- Copy the newly built `Server/worldserver.exe` directly to `Server/bin/worldserver.exe`.
- Update `azerothcore-wotlk/src/server/apps/CMakeLists.txt` or build script so CMake `INSTALL` on Windows automatically deploys to both `Server/` and `Server/bin/`, preventing future binary drift.

### Step 2: Implement Entry Announcements & Recalculation Hook in `src/ItemScalingLive.cpp`
- In `ItemScalingLiveMapScript::OnPlayerEnterAll(Map* map, Player* player)`:
  - Pass `player` into `sItemScalingLive->EnterMap(*map, player)`.
- In `ItemScalingLive::EnterMap(Map const& map, Player* player)`:
  - Check if `map.IsDungeon()`.
  - Calculate `currentHighest = GetHighestEligibleRealPlayerLevel(&map)`.
  - Check previous recorded level for this `(mapId, instanceId)` visit:
    - **First entry**: Send sysmessage to `player`:
      `"[ItemScaling] %s: Loot will scale to level %u (party ceiling: %u)."`
    - **Higher-level player entered**: If `currentHighest > previousHighest`:
      - Broadcast sysmessage to all players in `map`:
        `"[ItemScaling] %s (level %u) joined. Dungeon loot scaling updated to level %u!"`
      - Reset `visit.sourceIndex = 0; visit.itemIndex = 0;` to immediately prewarm catalogue for the higher bracket.

### Step 3: Recompile and Deploy
- Incremental build of `modules.vcxproj` and `worldserver.vcxproj`.
- Ensure `Server/bin/worldserver.exe` receives the updated executable.

### Step 4: Verification in Server
- Launch server via `open.bat`.
- Enter dungeon on player.
- Verify the in-game chat message appears:
  `"[ItemScaling] Entering <Dungeon>: Loot will scale to level X..."`
- Kill a boss mob (e.g. Rhahk'Zor or Mr. Smite in Deadmines).
- Verify the boss item drops scaled to player level, with no Error 1264 in `Errors.log`.
- Bring in a higher level character; verify announcement triggers and recalculates.
