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

Publication happens before the world becomes connectable. During gameplay the module only looks up
indexed variants and never creates, inserts, or resizes item-template containers. A missing variant
leaves the original loot item unchanged.

Additional safety properties:

- Template and registry rows are written in the same transaction and checked after each batch.
- Existing synthetic item IDs are preserved and never compacted or reused.
- Required equip level is part of variant identity, so distinct equip requirements cannot collide.
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
client-cache version while ItemScaling is enabled. This ties client item-query cache data for synthetic
entries to the active module metadata without adding an `item_dbc` row or patching the client.

Live-client testing is still recommended before a production rollout.

## Database layout

The module intentionally keeps one module-owned persistent world table:

`scaled_item_variant`
: maps each synthetic item entry to its base item, target level, target ItemLevel, FormulaVersion,
  and RequiredLevel. It provides deterministic restart recovery, variant identity, and lookup-only
  gameplay.

`sql/world/base/scaled_item_variant.sql` defines the complete schema used by a fresh installation.
Runtime C++ does not create or alter the schema; it validates the expected structure and disables
scaling for that run if the required structure is missing or incompatible.

Scaled variants are persisted as `item_template` rows. If a current-schema template row is missing,
startup can reconstruct it under the same synthetic ID before normal template loading.

`FormulaVersion` remains part of variant identity. Change it only when intentionally creating a
separate scaling family for formula-affecting settings.

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

`include.sh` registers the module base SQL with AzerothCore's database assembler.

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
