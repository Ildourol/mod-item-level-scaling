# Debugging & Diagnostic Playbook: `mod-item-level-scaling`

> **Component:** `mod-item-level-scaling` (Item Level Scaling & Dynamic Stats)  
> **Target:** AzerothCore 3.3.5a (Build 12340)  
> **Module Issue Tracker:** [`mod-item-level-scaling/docs/ISSUES.md`](file:///C:/Users/Admin/AntigravityProfiles/Projects Azerothcore/Azerothcore modules/mod-item-level-scaling/docs/ISSUES.md)

---

## 1. Quick Diagnostic Checklist

When encountering errors, unexpected behavior, or performance drops related to `mod-item-level-scaling`:
1. **Worldserver Console**: Look for module-specific error tags or warnings during boot (`OnConfigLoad`, `OnStartup`).
2. **Errors Log**: Inspect `Server/bin/Errors.log` for SQL query syntax errors or failed table queries.
3. **Database Consistency**: Check that required tables in `world` exist and have expected columns.
4. **Configuration Check**: Ensure `conf/mod_item_level_scaling.conf.dist` and active `Server/bin/configs/modules/mod_item_level_scaling.conf` match.

---

## 2. Common Symptoms & Root Causes

### A. Server Crash or Access Violation During World Tick
* **Symptom**: `EXCEPTION_ACCESS_VIOLATION` in module script hooks.
* **Root Cause**: Holding raw entity pointers across ticks or accessing despawned units.
* **Resolution**: Re-fetch entity freshly via numeric GUID (`ObjectAccessor::FindPlayer(guid)` or creature accessor) and verify `!entity || !entity->IsInWorld()`.

### B. Module Inactive or Commands Missing
* **Symptom**: Module chat commands return unknown command or features do not trigger.
* **Root Cause**: Module disabled in configuration (`.Enable = 0`) or configuration key missing.
* **Resolution**: Check active `.conf` file and verify boolean toggles.

### C. Database Query Failures
* **Symptom**: `Table doesn't exist` or `Unknown column` in `Errors.log`.
* **Root Cause**: Unapplied or missing SQL migrations in `data/sql/`.
* **Resolution**: Apply idempotent migration from `data/sql/` manually or check database connection string.

---

## 3. Module Specific Invariants
- **Invariant**: Zero blocking DB calls on item tooltip generation or equip hooks.
- **Invariant**: Item stat modifications must maintain idempotent demand ledgers without permanent DBC corruption.
- **Invariant**: Configuration files must never contain duplicate section keys.

---

## 4. Useful Diagnostic SQL Queries

```sql
-- Check character data associated with this module
SELECT * FROM information_schema.tables 
WHERE table_schema = DATABASE() 
  AND table_name LIKE '%item_level_scaling%';
```


## Live variant diagnostics

Use `.itemscaling status` to check the active generation mode, slot capacity and pending/durable/ready counts. A rising failure count can indicate queue/slot limits, a timeout, unsupported metadata or a failed async transaction; inspect `module.ItemScaling` messages.

Read-only world database checks:

```sql
SELECT assigned, COUNT(*) FROM mod_item_level_scaling_slot GROUP BY assigned;
SELECT COUNT(*) FROM mod_item_level_scaling_staged_item;
SELECT COUNT(*) FROM mod_item_level_scaling_staged_variant;
```

During gameplay, committed live items remain in staging while SQL `item_template` holds their inert placeholders. This is expected; native RAM templates supply their real stats. Next startup promotes them atomically. Keep staged snapshots and mappings: issued character items can depend on them. Recovery failure preserves staging and stops startup; restore a consistent backup or resolve the recorded ownership/schema conflict.

Actual client rendering and concurrent publication checks remain pending in [MILS-004](ISSUES.md#mils-004). No build or live migration was performed in the implementation task.
