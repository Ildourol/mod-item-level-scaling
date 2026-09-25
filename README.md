# Item level scaling for AzerothCore WotLK

Scale eligible dungeon and raid equipment with persistent synthetic item templates. The module uses
AzerothCore module hooks only: no AzerothCore core patch, Playerbot patch, MPQ, or custom client DBC is
required by the module design.

## Safety model

Variant rows are generated during startup, before AzerothCore loads `item_template`. After the core has
loaded the normal templates, `WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED` re-publishes every valid persisted
scaled variant from two sources:

- the already-loaded and DBC-validated base `ItemTemplate`, which supplies the item's normal identity,
  display, class/subclass, inventory type, material, sheath, spells, sockets, random-property metadata,
  prices, and other inherited fields;
- the persisted scaled template, which supplies the synthetic entry ID plus the fields intentionally
  changed by this module: ItemLevel, RequiredLevel, stats, weapon damage, armor, block, and resistances.

This restores the useful behavior of the original runtime-registration implementation without returning
to live runtime mutation. Publication happens before the world becomes connectable. During gameplay the
module only looks up indexed variants and never creates, inserts, or resizes item-template containers.
A missing variant leaves the original loot item unchanged.

Historical generator revisions are also re-published so already-issued items retain the corrected
runtime metadata. Only variants using the current internal generator revision are indexed for new loot.

Additional safety properties:

- Template and registry rows are written in the same transaction and checked after each batch.
- Existing synthetic item IDs are preserved and never compacted or reused.
- Required equip level is part of variant identity, so distinct equip requirements cannot collide.
- Operator `FormulaVersion` and the internal generator revision are separate identity fields.
- Missing historical templates are not regenerated with newer generator logic.
- Curves are loaded into module-owned DBC stores using `RandPropPoints.dbc`,
  `ScalingStatValues.dbc`, and their database-backed data. Core global DBC stores are not reloaded.
- Configuration is a startup snapshot. `.reload config` warns that a restart is required.
- Startup logs report recovery, pre-staging, publication/indexing, DBC identity corrections, variant
  counts, the synthetic-ID range, and elapsed time.
- SQL loot discovery follows `creature.id1/id2/id3`, difficulty templates, chest loot, and nested loot
  references with cycle detection.

## Synthetic item icons and client cache

Synthetic IDs do not exist in the stock client `Item.dbc`. AzerothCore therefore cannot perform the
normal DBC validation on those IDs while loading their raw `item_template` rows.

The module handles this without adding an `item_dbc` row or patching the client. At
`WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED`, each synthetic runtime template is rebuilt on top of the
already validated base item. This makes the scaled variant inherit the base item's corrected
`DisplayInfoID` and related identity fields while retaining the scaled values listed above.

The module also implements `WORLDHOOK_ON_BEFORE_FINALIZE_PLAYER_WORLD_SESSION` and salts AzerothCore's
client-cache version while ItemScaling is enabled. This makes clients discard stale cached item-query
data from older module versions instead of continuing to reuse an old broken synthetic-item response.

Existing synthetic IDs do not need to be deleted or renumbered for this publication fix. Rebuild and
restart worldserver after updating the module. Live-client testing is still recommended before a
production rollout.

## Database layout and upgrades

The module intentionally keeps one module-owned persistent table:

`scaled_item_variant`
: maps each synthetic item entry to its base item, target level, target ItemLevel, FormulaVersion,
  generator revision, and RequiredLevel. It is required for deterministic restart recovery, existing
  item stability, variant identity, and current-versus-historical generator handling.

The SQL files have separate roles and are not duplicate or obsolete databases:

- `sql/world/base/scaled_item_variant.sql` defines the current table for fresh database assembly.
- `data/sql/db-world/updates/2026_09_25_00_item_scaling_registry_schema.sql` upgrades older installs
  with RequiredLevel identity and repairs the variant unique key.
- `data/sql/db-world/updates/2026_09_25_01_item_scaling_generator_revision.sql` adds the internal
  generator revision while preserving existing IDs and generated item values.

Released update files should remain in place so existing installations can upgrade safely. Runtime C++
does not create or alter the schema; it validates the expected schema and disables scaling for that run
if the required structure is missing or incompatible.

Scaled variants themselves remain persisted as `item_template` rows. If a current-revision template
row is missing, startup can reconstruct it under the same synthetic ID. A missing template from a
historical generator revision is not reconstructed with newer generator logic.

## Upgrade from the original version

Back up the world database before deployment. Stop worldserver, update the module, rebuild, and restart.
There is no need to edit core source, patch Playerbots, patch the client, delete
`scaled_item_variant`, or renumber existing synthetic items.

Review the existing configuration:

```ini
ItemScaling.PreStageDungeonLoot = 1
ItemScaling.BracketStep = 1
ItemScaling.MaxNewVariantsPerStartup = 25000
ItemScaling.SyntheticEntry.Maximum = 2000000
```

An existing `PreStageDungeonLoot = 0` setting is respected: only already-persisted variants are
available. On-demand gameplay creation has been removed. Fresh installations use bounded pre-staging
by default.

Generation is limited to 25,000 new variants per startup by default. Further restarts can generate
remaining variants. Tune the limit for your hardware and database. First-time generation performs real
database work, and total database/template memory usage grows as variants are generated.

`FormulaVersion = 1` remains the operator baseline. Generator revision 1 records the implementation
family separately. Change `FormulaVersion` only when changing scaling settings that should
intentionally create a new variant family, including `PreserveNonZeroStats`.

## Behavior and limits

- The highest eligible real player in the instance determines the target. Playerbot sessions are
  excluded when `RealPlayersOnly = 1`. If no eligible player exists, scaling is skipped.
- `fixed` uses that player's level. In `dynamic` mode with `RealPlayersOnly = 1`, the target remains
  derived from the real-player level and configured floor/ceiling. Externally scaled creature levels
  are followed only when bots are allowed to affect the target.
- Native-level matches keep their original item. By default, generated ItemLevel and RequiredLevel
  follow the resolved mob/boss target level. Larger `BracketStep` values round other targets down to
  the configured bracket; `MaxLevel` is always included.
- Pre-staging includes upward/downward levels and the configured dynamic window, including distinct
  equip requirements. Unusual externally scaled targets outside that staged window can safely miss.
- Items are discovered from spawned instance creatures/gameobjects and their loot references.
  Script-only loot or creatures absent from the spawn tables may not be discovered. Such drops keep
  their original item unless a matching persisted variant already exists.
- Stats, armor, weapon damage, block, and resistances are scaled. Inherited item spell effects,
  random-property definitions, socket bonuses, and other inherited effects are not independently
  rescaled.
- Generation stops at the configured synthetic-ID ceiling.
- A world database shared by multiple concurrently starting worldservers is not supported for
  generation. Run startup generation with one writer; other servers can use pre-staging disabled.

## Installation

Clone into the core's `modules` directory:

```bash
git clone https://github.com/Ildourol/mod-item-level-scaling.git modules/mod-item-level-scaling
```

Regenerate the existing core CMake configuration and rebuild worldserver. Copy the distributed module
configuration to the module configuration directory used by the installation, then review the options.

`include.sh` registers the module base SQL with AzerothCore's database assembler and the module update
directory with the database updater.

## Compatibility and validation

The canonical runtime for this project is:

- `mod-playerbots/azerothcore-wotlk`, branch `Playerbot`;
- `mod-playerbots/mod-playerbots`, branch `master`.

The module also keeps compile-time bot detection isolated so the source can remain compatible with
stock AzerothCore where the required hooks/APIs are present.

See `tests/README.md` for the repository's compiler and isolated SQL regression checks. Those checks
do not replace live-stack validation. Before production rollout, verify at minimum a scaled dungeon
drop, a scaled raid drop, the icon/tooltip after reconnecting, equip/use behavior, bot-only behavior,
trade/mail, and the same issued item's values after a restart.

## License

GNU General Public License v2 or later, consistent with AzerothCore.
