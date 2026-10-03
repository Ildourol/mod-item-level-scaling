# First-run item scaling with RAM and SQL persistence

## Summary

Implement a hybrid system entirely inside `mod-item-level-scaling`. New scaled variants become usable during the first dungeon visit, display their actual stats through native item queries, and survive restarts.

Reserve unused item templates at startup. During gameplay, generate a complete scaled template, save its snapshot asynchronously, then populate a reserved RAM template at a safe world-update boundary. At the next startup, promote saved snapshots into ordinary `item_template` records.

No core, Playerbots, client, or DBC modifications.

## Configuration and behavior

- Add `ItemScaling.Live.Enable = 1`. Setting it to `0` restores the existing demand-ledger behavior.
- Add `ItemScaling.Live.GenerationMode = 1`:
  - **1 — Dungeon entry:** Prepare eligible variants for the instance’s current player level and scaling policy. Refresh preparation when the relevant player level or party composition changes.
  - **2 — Actual drop:** Prepare only variants requested by rolled loot. Hold first-time loot briefly until persistence and RAM publication finish.
- Both modes use the same on-drop handling for an unexpected missing variant.
- Keep the existing `ItemScaling.RandomSuffix.Mode = 0` default. Document `1` as opt-in scaling with baked stats and suffix names.
- Add defaults: `ReservedSlots = 4096`, `MaxPendingVariants = 4096`, `MaxPublishPerTick = 64`, and `LootWaitTimeoutMs = 10000`.
- Treat slot capacity and generation mode as startup settings. Operational filters retain their existing reload behavior.

## Implementation

### Reserved templates and RAM publication

- Before the core loads items, reserve enough compact, collision-free IDs to provide the configured number of unused slots. Track ownership in a module table.
- Create inert placeholder rows with no equipment slot, loot source, vendor availability, or useful stats. Exclude these IDs from baseline calculations and variant discovery.
- Obtain the loaded template pointers through the existing `GetItemTemplateStoreFast()` API. Populate only module-owned, unused templates.
- Publish at `WorldScript::OnUpdate`, after map workers finish. Never resize core containers, replace template pointers, mutate base items, or overwrite issued variants.
- Use a synchronized module registry with states `Pending → Durable → Ready`. Deduplicate simultaneous requests by the existing seven-field variant key.
- Keep generated templates immutable after publication. Allocate a new ID for a different variant.

### Generation and loot handling

- Build a read-only dungeon loot catalogue during startup, including recursive reference loot, creature difficulty variants, and eligible chest loot. Detect reference cycles.
- Mode `1` uses this catalogue to prepare exact keys for current instance conditions. Use the existing target, bracket, item-level, and required-level calculations.
- Mode `2` records the actual rolled item and its random property. Preserve the original roll, count, conditions, ownership, and loot slot.
- Defer pending creature-loot requests through `ServerScript::CanPacketReceive` before native group rolls start. Playerbots’ queued loot packets use this same path.
- For ordinary eligible chests, add a module-owned preparation adapter at the existing gameobject hook. Preserve native access and lock checks, generate loot once, and defer group-roll initialization until variants are ready.
- Identify deferred sources by map, instance, GUID, and loot-generation token. Re-resolve objects before changing loot; cancel stale work after despawn, reset, logout, or map changes.
- Resume native loot processing after replacing pending entries with ready variants. Never reroll loot or convert items already awarded.
- On SQL failure, timeout, queue saturation, or slot exhaustion, release the original rolled item and report the reason. Do not award an undurable synthetic item.

### SQL persistence and restart recovery

- Add idempotent module-owned tables for reserved slots, staged variant metadata, and complete staged item-template snapshots.
- Persist the snapshot, assigned ID, variant key, identity metadata, and slot assignment in one asynchronous transaction.
- Publish and release loot only after successful commit acknowledgement. Perform no synchronous SQL or filesystem writes in gameplay hooks.
- At the next startup, before core item loading, atomically replace the owned placeholder with its saved template, insert the permanent variant mapping, and remove completed staging records.
- Recover committed staging records after a crash using their original IDs and values. Do not recalculate issued items from changed configuration or base templates.
- Retain existing permanent variants and pending demand. Include staged and reserved IDs in allocation and collision checks.

## Bags, tooltips, and Playerbots

- Use the complete scaled RAM template for both gameplay and native item-query responses: stats, armor, damage, DPS inputs, resistances, block, item level, and required level.
- Suppress placeholder query responses so clients cannot cache unfinished metadata. Answer deferred queries once the final template is ready.
- Never reuse an ID after publishing its metadata.
- With random scaling enabled, bake the rolled name and supported bonuses into fixed fields, clear both random-template fields and instance random fields, and validate baked variants correctly at startup.
- Reject unsupported or overflowing random bonuses rather than silently dropping stats.
- Verify Playerbots’ loot valuation, Need/Greed, and equipment handling against live templates. Existing startup-built autogear catalogues gain the permanent variants on the next restart; do not modify or rebuild Playerbots internals.

## Verification and acceptance

- Add meaningful tests for request deduplication, slot ownership, state transitions, queue limits, exact snapshot persistence, and crash recovery.
- Run isolated SQL tests for fresh installation, upgrade, repeated startup, failed transactions, and interrupted promotion.
- Exercise publication with multiple map workers and concurrent item queries; confirm stable pointers and no concurrent template mutation.
- In both modes, start with no generated variants and verify the first dungeon visit awards scaled gear without a restart or second run.
- Check loot-window, bag, equipped, inspection, and chat-link tooltips against actual server stats, using clean and existing client caches.
- Cover creature loot, chest locks, group loot, Need Before Greed, master loot, Playerbots, changing player levels, and instance resets.
- Test random-property scaling off and on, including suffix names and absence of `+0` bonuses.
- Verify identical IDs and stats after reconnect, restart, trade, mail, auction, and guild-bank storage.
- Extend `.itemscaling status` with slot availability, pending/durable/ready counts, failures, and generation mode. Update module documentation and replace the old “first drop stays unscaled” acceptance checks.

Acceptance requires live client verification of bag tooltips and a thread-safety check of the reserved-slot publication mechanism; compilation alone is insufficient.


## Implementation status (2026-10-04)

The hybrid source implementation, configuration modes, snapshot migration, restart recovery,
packet/chest adapters and diagnostics are present. Oracle MySQL 8.4.11 disposable SQL checks
passed. Compilation, installation, live migrations and worldserver restart were deliberately
not performed under the user's instruction. The acceptance checks requiring a real client and
concurrent map workers remain pending in [MILS-004](../ISSUES.md#mils-004).
