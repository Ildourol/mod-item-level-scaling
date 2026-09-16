# Mod Item Level Scaling for AzerothCore (WotLK 3.3.5a)

<p align="center">
  <img src="assets/banner.png" alt="Mod Item Level Scaling Banner" width="850">
</p>

<p align="center">
  <a href="https://github.com/azerothcore/azerothcore-wotlk"><img src="https://img.shields.io/badge/AzerothCore-WotLK%203.3.5a-blue.svg" alt="AzerothCore"></a>
  <a href="https://github.com/Ildourol/mod-item-level-scaling/blob/master/conf/mod_item_level_scaling.conf.dist"><img src="https://img.shields.io/badge/Configuration-Fully%20Configurable-brightgreen.svg" alt="Configurable"></a>
  <a href="https://github.com/Ildourol/mod-item-level-scaling/blob/master/acore-module.json"><img src="https://img.shields.io/badge/Compatibility-v1.0.0-orange.svg" alt="Module Version"></a>
</p>

---

## 📖 Description

**mod-item-level-scaling** is a high-performance AzerothCore C++ module that dynamically scales equippable combat loot dropped inside instanced dungeons and raids to match real player progression.

Designed for level-scaling servers, solo-play, and dungeon-leveling experiences (including setups running `mod-autobalance` and `mod-playerbots`), it ensures that dungeon runs always reward level-appropriate equipment while strictly preserving Blizzard itemization balance, stat budgets, weapon speed curves, and boss loot prestige.

---

## ✨ Features

- 🛡️ **Real Player Authority**: Target scaling levels ($H$) are determined exclusively by the highest-level **real** player inside the instance. Playerbots never unintentionally inflate or skew item scaling targets.
- ⚖️ **Dynamic & Fixed Scaling Modes**:
  - **Dynamic Mode**: Retains authentic dungeon hierarchy ($Trash < Elite < Boss$) relative to $H$ using customizable level floors and ceilings.
  - **Fixed Mode**: Pinpoints the highest real player level $H$ directly.
- 📊 **Blizzard Data-Driven ItemLevel Model**: Calculates target `ItemLevel` from stock Blizzard equipment medians $M(\text{Level}, \text{Quality}, \text{SlotFamily})$, ensuring boss and raid tier gear remains superior without carrying endgame stat inflation into low-level brackets.
- 📈 **Native DBC Growth Curves**:
  - Leverages `RandomPropertiesPoints.dbc` point budget tables across all qualities (Uncommon, Rare, Epic) and all 5 inventory slot families to accurately rescale primary stats, ratings, armor, block value, and resistances.
  - Employs `ScalingStatValues.dbc` DPS curves for weapon damage while keeping weapon delay (speed) and damage spread intact.
- 💎 **Exhaustive Stat Scaling**:
  - Primary Stats: Strength, Agility, Stamina, Intellect, Spirit
  - Melee/Ranged Ratings: Attack Power, Ranged AP, Crit, Hit, Haste, Expertise, Armor Penetration
  - Spell Ratings: Spell Power, Spell Hit, Spell Crit, Spell Haste, Spell Penetration, MP5
  - Defensive Ratings: Defense, Dodge, Parry, Shield Block Rating, Shield Block Value, Health Regen
- 🔄 **Persistent Synthetic Templates**:
  - Generates custom item templates and stores them in the `scaled_item_variant` table.
  - Items survive server restarts, logouts, mail, guild bank, auction house, and player trades.
  - Tooltip stats match equipped stats with 100% fidelity without client-side patch requirements.
- 🛡️ **Zero Global Mutation**:
  - Base `ItemTemplate` entries are never mutated. Vendor gear, quest rewards, world drops, and items owned by other players remain completely unaffected.
- 🤖 **Broad Compatibility**:
  - Fully tested and compatible with `mod-playerbots` (rolling and auto-equip gear evaluation) and `mod-autobalance`.

---

## 📂 Repository Structure

```
mod-item-level-scaling/
├── conf/
│   ├── conf.sh.dist                     # Module build and SQL registration script
│   └── mod_item_level_scaling.conf.dist # Complete module configuration file
├── sql/
│   └── world/
│       └── base/
│           └── scaled_item_variant.sql  # Database table for persistent item variants
├── src/
│   ├── ItemScalingBaseline.cpp          # Stock median ItemLevel baseline lookup
│   ├── ItemScalingBaseline.h
│   ├── ItemScalingCommon.h              # Common definitions and data models
│   ├── ItemScalingConfig.cpp            # Configuration parser and cache
│   ├── ItemScalingConfig.h
│   ├── ItemScalingFormula.cpp           # DBC-driven stat and weapon scaling logic
│   ├── ItemScalingFormula.h
│   ├── ItemScalingLootScript.cpp        # Loot hook (rolls & drops item variants)
│   ├── ItemScalingLootScript.h
│   ├── ItemScalingRegistry.cpp          # Synthetic ItemTemplate manager & DB persistence
│   ├── ItemScalingRegistry.h
│   ├── ItemScalingWorldScript.cpp       # World lifecycle hooks and startup loader
│   ├── ItemScalingWorldScript.h
│   └── ItemScaling_loader.cpp           # Script registry entry point
├── assets/                              # Documentation media
├── acore-module.json                    # Module metadata
└── include.sh                           # Bash build integration
```

---

## 🚀 Installation

1. Navigate to your AzerothCore modules folder and clone this repository:
   ```bash
   cd azerothcore-wotlk/modules
   git clone https://github.com/Ildourol/mod-item-level-scaling.git
   ```

2. Re-generate CMake and compile the project:
   ```bash
   cd azerothcore-wotlk/build
   cmake ../ -DCMAKE_INSTALL_PREFIX=/path/to/server
   make -j $(nproc)
   make install
   ```

3. Configure the module:
   ```bash
   cp ../modules/mod-item-level-scaling/conf/mod_item_level_scaling.conf.dist /path/to/server/etc/mod_item_level_scaling.conf
   ```

4. Database Setup:
   - If using `db_assembler.sh`, the SQL file in `sql/world/base/` will be imported automatically.
   - Alternatively, import manually into your `acore_world` database:
     ```bash
     mysql -u acore -p acore_world < ../modules/mod-item-level-scaling/sql/world/base/scaled_item_variant.sql
     ```
   *(Note: The module also checks and executes `CREATE TABLE IF NOT EXISTS` automatically upon worldserver startup).*

---

## ⚙️ Configuration

Detailed configuration options are documented in `conf/mod_item_level_scaling.conf.dist`. Key settings include:

| Setting | Default | Description |
| :--- | :---: | :--- |
| `ItemScaling.Enable` | `1` | Enable or disable the module entirely |
| `ItemScaling.ScaleDungeons` | `1` | Enable scaling in normal 5-player instances |
| `ItemScaling.ScaleRaids` | `1` | Enable scaling in 10/25-player raids |
| `ItemScaling.ScaleHeroics` | `1` | Enable scaling in heroic dungeons/raids |
| `ItemScaling.ScaleChests` | `1` | Enable scaling for instanced chests and gameobjects |
| `ItemScaling.LevelScaling.Method` | `"dynamic"` | Scaling mode: `"dynamic"` or `"fixed"` |
| `ItemScaling.ExcludedLevels` | `"60, 70, 80"` | Milestone levels excluded from scaling to preserve stock endgame loot |
| `ItemScaling.PersistVariants` | `1` | Persist generated synthetic items to database across restarts |

---

## 🤝 Compatibility & Requirements

- **AzerothCore WotLK (branch `master`)** (commit supported: latest)
- **Client**: World of Warcraft: Wrath of the Lich King (3.3.5a - Build 12340)
- Compatible with:
  - [mod-autobalance](https://github.com/azerothcore/mod-autobalance)
  - [mod-playerbots](https://github.com/liyunfan1223/mod-playerbots)

---

## 📜 License

This module is released under the GNU General Public License v2 (or at your option any later version) in accordance with AzerothCore licensing.
