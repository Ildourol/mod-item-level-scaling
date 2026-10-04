---
name: item-level-scaling-maintainer
description: Maintain, review, debug, plan, and extend mod-item-level-scaling (Item Level Scaling & Dynamic Stats) across AzerothCore C++ hooks, database schemas, and configuration settings.
---

# Item Level Scaling & Dynamic Stats Maintainer Skill (`item-level-scaling-maintainer`)

This skill provides operational workflows, engineering checklists, and diagnostic runbooks for maintaining and extending `mod-item-level-scaling`.

## Core Responsibilities
- **Component**: Item Level Scaling & Dynamic Stats
- **Description**: Dynamically scales item stats, armor, spell power, and weapon damage based on player level, live generation, and dungeon brackets.
- **Target Platform**: AzerothCore (WotLK 3.3.5a, Build 12340, C++17)

---

## Architectural Invariants
- Zero blocking DB calls on item tooltip generation or equip hooks.
- Item stat modifications must maintain idempotent live SQL persistence without permanent DBC corruption.
- Configuration files must never contain duplicate section keys.
- **Single Server Runtime Authority**: The single active server runtime is strictly located at `Server/bin/`. All binary builds and module configs must target `Server/bin/`.
- **Zero Blocking I/O**: The AzerothCore world loop is sacred. Never add blocking I/O, sleep loops, or external synchronous network requests to the world thread.
- **Pointer Safety**: Never hold raw entity pointers across game ticks. Re-resolve entities freshly using numeric GUIDs.
- **Idempotent SQL**: All database migrations must be 100% idempotent with `CREATE TABLE IF NOT EXISTS` and safe column checks.

---

## Maintenance Runbook

### 1. Preflight Inspection
1. Verify configuration files in `conf/` and `Server/bin/configs/modules/`.
2. Inspect registered script hooks (`PlayerScript`, `WorldScript`, `UnitScript`, etc.).
3. Verify that database tables match schema declarations in `data/sql/`.

### 2. Issue Investigation & Tracking
1. Consult [`docs/ISSUES.md`](file:///C:/Users/Admin/AntigravityProfiles/Projects Azerothcore/Azerothcore modules/mod-item-level-scaling/docs/ISSUES.md) for known issues and resolutions.
2. Update only the issue summary and direct module-record link in [COMMANDER/ISSUES.md](<../../../../COMMANDER/ISSUES.md>); keep technical records, fix instructions, and plans inside this module.
3. Follow the 9-point technical issue template strictly.

### 3. Verification & Safety Checks
- **MySQL Testing**: Perform all test compilation and general tests (unit tests, integration tests, SQL migrations, and runtime verification) against MySQL, the default database engine for AzerothCore.
- Verify thread safety of all member buffers and static registries using std::mutex or std::shared_mutex where required.
- Validate skill integrity:
  ```bash
  python "C:\Users\Admin\.codex\skills\.system\skill-creator\scripts\quick_validate.py" ".agents/skills/item-level-scaling-maintainer"
  ```
