# Strategic Development Roadmap: `mod-item-level-scaling`

> **Module:** `mod-item-level-scaling` (Item Level Scaling & Dynamic Stats)  
> **Status:** Active / Maintained  

---

## Phased Milestones

### Phase 1: Core Architecture & Zero-Core Decoupling (Completed)
- [x] Standardize repository layout according to AzerothCore module guidelines.
- [x] Eliminate hard dependencies on core engine files (`azerothcore-wotlk`).
- [x] Establish idempotent database migration patterns.

### Phase 2: Runtime Stabilization & Invariant Hardening (Current)
- [x] Synchronize module issue tracker with central `COMMANDER/ISSUES.md` catalog.
- [x] Deploy standardized AI Maintainer skills and architectural documentation.
- [ ] Implement automated regression test cases and telemetry monitoring.

### Phase 3: Feature Expansion & Community Refinements (Upcoming)
- [ ] Expand customization options in `conf/mod-item-level-scaling.conf.dist`.
- [ ] Deepen telemetry and cross-module synergies with playerbots and AI bridges.
- [ ] Performance profiling and micro-optimizations under 500+ concurrent entities.


### First-run hybrid scaling (Implemented; release verification pending)

- [x] Module-only reserved RAM templates with asynchronous complete SQL snapshots.
- [x] Dungeon-entry mode 1 and actual-drop mode 2; random scaling off by default.
- [x] Query/loot gating, restart promotion, status diagnostics and Oracle MySQL SQL regressions.
- [ ] Compile and run C++ regressions when authorized.
- [ ] Verify bag/tooltips, first-run loot and publication concurrency in an isolated realm.

Track acceptance in [MILS-004](ISSUES.md#mils-004).
