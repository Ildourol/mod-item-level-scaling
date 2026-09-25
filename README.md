# Item level scaling for AzerothCore WotLK

Scale eligible dungeon and raid equipment with persistent synthetic item templates. This module uses
existing AzerothCore hooks and does not require core source changes.

## Safety model

Templates are generated **at startup, before the core loads `item_template`**. During gameplay the
module only looks up validated variants. It never resizes or inserts into the core's item-template
containers. A missing variant leaves the original item in the loot.

This replaces the earlier runtime insertion path, which could race with map threads and Playerbot
readers. The module's own mutex cannot make those core containers safe for concurrent mutation.

- Template and registry rows are written in the same transaction and checked after each batch.
- Existing item IDs are preserved. Allocation starts above all existing template and registry IDs.
- Required equip level is part of variant identity, so different player-level requirements cannot
  accidentally reuse the same variant.
- Curves are loaded into module-owned DBC stores using `RandPropPoints.dbc`, `ScalingStatValues.dbc`,
  and their database-backed data. The core's global DBC stores are never reloaded by the module.
- Configuration is a startup snapshot. `.reload config` logs that a restart is required.
- SQL discovery follows `creature.id1/id2/id3`, difficulty templates, chest loot, and nested loot
  references with cycle detection.

## Upgrade from the original version

Back up the world database before deployment. Stop worldserver, update this module, rebuild, and restart.
There is no need to edit core source or delete existing synthetic items.

The module upgrades its own `scaled_item_variant` table at startup by adding `required_level`, copying
existing equip requirements, and extending the unique key. The world database account therefore needs
`CREATE`, `ALTER`, `SELECT`, `INSERT`, and `UPDATE` privileges for this module's work. An incompatible
module table is reported and scaling is disabled for that run.

**Review your existing configuration:**

```ini
ItemScaling.PreStageDungeonLoot = 1
ItemScaling.BracketStep = 2
ItemScaling.MaxNewVariantsPerStartup = 25000
ItemScaling.SyntheticEntry.Maximum = 2000000
```

An existing `PreStageDungeonLoot = 0` setting is respected: only already-persisted variants will be
available. On-demand creation has been removed. Fresh installations use bounded pre-staging by default.

Generation is limited to 25,000 new variants per startup by default. Further restarts can generate
remaining variants; a limit warning explains when staging is incomplete. Tune the limit for your
hardware and database. First-time generation is real database work and can take significant time.
Total database and template memory usage increase as variants are generated; the ID cap does not bound
the total bytes used by templates.

Existing template rows are not silently regenerated. Previously issued items retain their IDs and
stored values. If an older release persisted incorrect stats, restoring a backup or deliberately
repairing those owned rows requires a separate reviewed data operation. Change `FormulaVersion` when
changing scaling settings that should generate a new set of variants, including `PreserveNonZeroStats`.
Old formula versions remain available for already-issued items. Recovery of a missing template uses
the current formula implementation and saved level/requirement; historical formulas are not archived.

## Behavior and limits

- The highest eligible real player in the instance determines the target. Playerbot sessions are
  excluded when `RealPlayersOnly = 1`. If no eligible player exists, scaling is skipped.
- `fixed` uses that player's level; `dynamic` uses the configured floor/ceiling or an already-scaled
  creature's level.
- Native-level matches keep their original item. Other targets round down to the configured bracket;
  `MaxLevel` is always included. Use `BracketStep = 1` for exact target levels.
- Pre-staging includes upward and downward levels and the configured dynamic window, including distinct
  equip requirements. Externally scaled targets outside that window can safely miss.
- Items are discovered from spawned instance creatures/gameobjects and their loot references.
  Script-only loot or creatures absent from the spawn tables may not be discovered. Such drops keep
  their original item unless a matching persisted variant already exists.
- Stats, armor, weapon damage, block, and resistances are scaled. Item spell effects, random-property
  definitions, socket bonuses, and other inherited effects are not individually rescaled.
- Block and resistances are persisted with their scaled values for newly generated templates.
- Generation stops at the configured synthetic-ID ceiling. Existing IDs are not compacted or reused.
- A world database shared by multiple concurrently starting worldservers is not supported for
  generation. Run startup generation with one writer; others can use pre-staging disabled afterward.

## Installation

Clone into the core's `modules` directory:

```bash
git clone https://github.com/Ildourol/mod-item-level-scaling.git modules/mod-item-level-scaling
```

Regenerate your existing core CMake configuration and rebuild worldserver. Copy the distributed module
configuration to the module configuration directory used by your installation, then review the options.
`include.sh` registers the base SQL with the database assembler; startup also creates/upgrades the
module table when needed.

## Compatibility and validation

The module targets C++20 AzerothCore WotLK. Bot detection uses compile-time API detection so the same
source can compile with stock master and the Playerbot fork.

Validation targets for this change:

- Playerbot core: `mod-playerbots/azerothcore-wotlk`, commit `7f12e89ee5f467a50e62eba1d525eac7dc953d03`.
- Stock core: `azerothcore/azerothcore-wotlk`, commit `b6c033cb009d4e137af70b60151d23bcbeb4b2b8`.

See `tests/README.md` for reproducible checks. Compiler checks and isolated SQL regression tests do not
replace testing your complete module combination in-game. Verify a dungeon drop, a raid drop, bot-only
instances, trade/mail, and the same item's stats after a server restart before production rollout.

## License

GNU General Public License v2 or later, consistent with AzerothCore.
