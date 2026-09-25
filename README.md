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
- The operator `FormulaVersion` and internal generator revision are separate identity fields. Existing
  rows are revision 1, so introducing generator tracking does not regenerate or renumber current items.
- Curves are loaded into module-owned DBC stores using `RandPropPoints.dbc`, `ScalingStatValues.dbc`,
  and their database-backed data. The core's global DBC stores are never reloaded by the module.
- Configuration is a startup snapshot. `.reload config` logs that a restart is required.
- SQL discovery follows `creature.id1/id2/id3`, difficulty templates, chest loot, and nested loot
  references with cycle detection.

## Upgrade from the original version

Back up the world database before deployment. Stop worldserver, update this module, rebuild, and restart.
There is no need to edit core source or delete existing synthetic items.

AzerothCore's module database updater applies this module's schema migration before the module startup
hook. The migration adds `required_level` when needed, preserves existing synthetic IDs and equip
requirements, and repairs the variant unique key. Runtime module code only validates that schema before
pre-staging; it no longer performs schema DDL itself. This does not change generated item values,
variant identity, scaling formulas, or runtime loot selection.

**Review your existing configuration:**

```ini
ItemScaling.PreStageDungeonLoot = 1
ItemScaling.BracketStep = 1
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

Existing template rows are not silently regenerated. `FormulaVersion = 1` remains the operator
baseline, while generator revision 1 records the current code implementation separately. The migration
maps every existing row to generator revision 1 and does not change synthetic IDs or `item_template`
values. Future code changes that alter generated template values can increment the internal revision
without requiring administrators to change `FormulaVersion`. Change `FormulaVersion` only when
changing scaling settings that should intentionally create a new variant family, including
`PreserveNonZeroStats`.

Previously issued items retain their IDs and stored values. A missing template from an older generator
revision is not reconstructed with newer generator logic; that prevents historical IDs from silently
changing meaning.

## Behavior and limits

- The highest eligible real player in the instance determines the target. Playerbot sessions are
  excluded when `RealPlayersOnly = 1`. If no eligible player exists, scaling is skipped.
- `fixed` uses that player's level. In `dynamic` mode, `RealPlayersOnly = 1` keeps the target
  derived from the real-player level and configured floor/ceiling; externally scaled creature levels
  are followed only when bots are allowed to affect the target.
- Native-level matches keep their original item. By default, generated item levels and
  `RequiredLevel` values follow the mob/boss target level exactly. Larger `BracketStep` values round
  other targets down to the configured bracket; `MaxLevel` is always included.
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
`include.sh` registers the base SQL with the database assembler. Existing installations are upgraded
by the module SQL update before ItemScaling startup validation and pre-staging.

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
