# AI Agent Operating Guidelines: `mod-item-level-scaling`

Welcome to `mod-item-level-scaling`. This document defines the strict architectural boundaries, C++ threading invariants, database conventions, and code patterns required when modifying or extending this repository.

---

## 1. Architectural Invariants (Non-Negotiable)

### A. The World Thread Is Sacred (Zero Blocking I/O)
* **Never perform HTTP network calls, file system blocking I/O, or long-running computations inside C++ world thread execution.**
* The AzerothCore world loop runs at 50–100ms per tick. Any blocking operation freezes the entire game world for all connected players.

### B. Pointer Safety & Engine Lifecycle
* In C++, never store raw pointers (`Player*`, `Creature*`, `Unit*`, `GameObject*`) across game ticks or in asynchronous worker threads.
* Objects in AzerothCore can be deleted, despawned, or logged out between ticks.
* Always track entities by numeric GUID counters (`uint32` or `ObjectGuid`) and resolve them freshly each tick via `ObjectAccessor::FindPlayer(guid)` or equivalent accessor methods.
* Verify entity validity and `IsInWorld()` at the start of every controller or action execution method.

### C. Database Idempotency & Schema Safety
* All SQL migrations in `data/sql/` must be completely **idempotent**.
* Always guard table creations with `CREATE TABLE IF NOT EXISTS`.
* Always guard column additions with `INFORMATION_SCHEMA` conditional logic or `ALTER TABLE ... ADD COLUMN IF NOT EXISTS`.
* Upgrades must never wipe existing character data, progression records, or playerbot database tables.

### D. Module Specific Invariants
* **Zero blocking DB calls on item tooltip generation or equip hooks.**: Zero blocking DB calls on item tooltip generation or equip hooks.
* **Item stat modifications must maintain idempotent live SQL persistence without permanent DBC corruption.**: Item stat modifications must maintain idempotent live SQL persistence without permanent DBC corruption.
* **Configuration files must never contain duplicate section keys.**: Configuration files must never contain duplicate section keys.

### E. Single Server Runtime Authority & Binary Path Invariant
* **Authoritative Runtime Environment**: The single active server runtime is strictly located at `Azerothcore server/Server/bin/`.
* **Zero Binary Drift**: All compiled server executables (`worldserver.exe`, `authserver.exe`) and runtime module configurations must reside in `Server/bin/` and `Server/bin/configs/modules/`.
* Never inspect, edit, or create executables or configs in the parent `Server/` root. `Server/configs` is a junction pointing to `Server/bin/configs`.

---

## 2. Repository Layout & Component Ownership

```text
mod-item-level-scaling/
├── .agents/skills/             # Specialized project AI skills & runbooks
│   └── item-level-scaling-maintainer/ # Full-lifecycle maintainer skill
├── conf/                       # Configuration templates
├── data/                       # Database migrations (characters & world)
├── docs/                       # Comprehensive documentation suite
│   ├── ARCHITECTURE.md         # Subsystem architecture & interaction flow
│   ├── ISSUES.md               # Authoritative local issue tracker & resolution log
│   ├── ROADMAP.md              # Phased milestones & strategic vision
│   └── plans/                  # Dedicated implementation plans
├── src/                        # C++ module source code (AzerothCore hooks)
└── README.md                   # User guide & installation instructions
```

---

## 3. Coding Standards & Conventions

### C++ Conventions
- Standard: **C++17** (AzerothCore baseline).
- Include guards: `#pragma once` or `#ifndef MOD_*_H`.
- String formatting: Use AzerothCore logging macros (`LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_DEBUG`).
- Memory allocations: Prefer stack allocation and reusable member buffers. Avoid dynamic heap allocations inside fast update loops.
- Error Routing: Internal command failures or validation warnings should use dedicated player chat or addon messages rather than dumping unformatted errors to public console.

### Database Conventions
- Schema: Keep character-specific data in `characters` DB and gameplay/mechanics data in `world` DB.
- Naming: Prefix custom tables with module-identifying tags (e.g. `mod_item_level_scaling_*`).

---

## 4. Verification & Testing Workflow

Before committing any changes or closing an issue:
1. **Maintainer Skill Runbook**:
   Consult and follow [`.agents/skills/item-level-scaling-maintainer/SKILL.md`](file:///C:/Users/Admin/AntigravityProfiles/Projects Azerothcore/Azerothcore modules/mod-item-level-scaling/.agents/skills/item-level-scaling-maintainer/SKILL.md) for cross-layer checklists. Validate the skill with:
   ```bash
   python "C:\Users\Admin\.codex\skills\.system\skill-creator\scripts\quick_validate.py" ".agents/skills/item-level-scaling-maintainer"
   ```
2. **Database Migration Safety & MySQL Testing**:
   Verify that any new `.sql` file runs cleanly and idempotently on an existing populated database using MySQL (AzerothCore's default). All test compilation and general tests must be executed with MySQL.
3. **World-thread Safety Review**:
   Ensure zero blocking operations, zero thread sleep calls, and safe pointer validation across all script hooks.

---

## 5. Module Issue Ownership & Catalog Reporting

Before investigating or fixing an issue, read this module's AGENTS.md, any more specific instructions, and its applicable local maintainer skill and documentation. Follow this module's architecture, safety rules, and verification requirements.

* **Module authority**: Keep the complete issue record, evidence, root cause, fix instructions, implementation changes, and verification results in [docs/ISSUES.md](<docs/ISSUES.md>).
* **Local plans and instructions**: Keep debugging procedures, methodology, and resolution plans inside this module. Use its existing plan location and documentation conventions.
* **Central catalog**: Update only the issue ID, owning module, title, severity, status, and direct link in [COMMANDER/ISSUES.md](<../COMMANDER/ISSUES.md>). Central reports may summarize progress and blockers. Full technical records, plans, agent instructions, and skills belong in this module.
* **Issue identity and completion**: Preserve existing IDs and allocate the next unused sequential ID in this module's namespace. Mark an issue resolved only after the module's required verification succeeds; record pending checks and mitigations accurately in the local tracker and catalog.

* **Engineering Methodology, Docs & Plans Authority**:
  - The module repository (`mod-item-level-scaling/`) is the primary authority housing the engineering methodology (`docs/ARCHITECTURE.md`, `docs/DEBUGGING.md`), AI maintainer skills (`.agents/skills/`), and resolution plans (`docs/plans/`) detailing exactly *how* issues are fixed and verified.
* **Standard Issue Sheet Structure**: Every issue detail entry must strictly contain:
  1. `### [<TAG>-XXX] <Clear Descriptive Title>`
  2. `* **Severity**: Low | Medium | High | Critical`
  3. `* **Component**: <Subsystem name>`
  4. `* **Status**: RESOLVED | MONITORED | OPEN / MITIGATED | IDENTIFIED / PENDING FIX`
  5. `* **Affected Files**: <Clickable file links>`
  6. `* **Symptoms & Evidence**: <Logs, reproduction steps, or crash stack traces>`
  7. `* **Root Cause Analysis**: <Architectural or technical explanation>`
  8. `* **Resolution**: <Concrete code changes implemented or planned>`
  9. `* **Regression Guard**: <Automated test, query, or runtime validation procedure>`
