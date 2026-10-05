# Item Level Scaling for AzerothCore


Dynamic dungeon and raid equipment scaling for AzerothCore 3.3.5a.

The module creates permanent scaled variants of eligible equipment using the current instance scaling target, publishes first-run variants safely into AzerothCore's native item-template store, and persists the exact generated item so the same ID and stats survive future restarts.

No AzerothCore core patch, Playerbots patch, client patch, or DBC edit is required for the supported paths.

## What it does

When eligible loot is generated inside a supported instance, the module can:

- scale item level, required level, armor, weapon damage, stats, resistances, and other template fields;
- scale upward or downward;
- use either a fixed player-level target or dynamic scaling from the creature's current in-instance level;
- distinguish normal dungeons, heroic dungeons, and individual raid-size/difficulty categories;
- preserve native Blizzard loot automatically when scaling would be unnecessary;
- optionally roll configurable floor/ceiling variance for lucky higher-level drops;
- optionally mirror AutoBalance's level-scaling configuration;
- generate missing variants during the first dungeon visit or only when the item actually drops;
- persist generated variants asynchronously before they are awarded;
- preserve original loot if generation cannot complete safely;
- support AzerothCore's native item queries, bag/equipped items, chat links, group loot, ordinary chest loot, and Playerbots loot paths;
- optionally bake supported random-property/random-suffix bonuses into fixed template stats.

The module never modifies the original base item template. Generated variants receive their own permanent synthetic item entry.

## How the target level is chosen

The highest eligible player level in the instance is called **H**.

By default:

```ini
ItemScaling.RealPlayersOnly = 1
ItemScaling.IncludeGameMasters = 0
```

That means Playerbots do not determine **H**, and GMs with GM mode enabled do not affect it unless configured otherwise.

Two target methods are available:

| Method | Behavior |
|---|---|
| `fixed` | Target effective level is based directly on the highest eligible player level, **H**. |
| `dynamic` | Preserves the relative instance hierarchy around **H**, allowing trash and bosses to use configured floor/ceiling limits. |

Default:

```ini
ItemScaling.LevelScaling.Method = "dynamic"
```

### The target is not locked when the dungeon starts

The active scaling target is not permanently frozen when the first player enters.

In live generation mode 1, the module tracks the highest eligible player level for the active instance visit. If a higher eligible player enters later, preparation refreshes for the new target. The elapsed time does not create a 5-minute, 10-minute, or similar lock.

For example:

```text
00:00  Highest eligible player = 35
       Item scaling prepares level-35-centered variants.

10:00  A level-50 eligible player enters.
       H becomes 50.
       New preparation uses the level-50-centered target.

Later loot uses the current target and its exact variant key.
```

Already awarded items are immutable. They are not rewritten merely because **H** changes later.

If `ItemScaling.Announce = 1`, the module can announce the active instance category, highest eligible player, and dynamic floor/ceiling information when entering and when relevant in-instance level changes occur.

## Dynamic scaling model and defaults

Dynamic mode now derives loot targets from the creature's **current in-instance effective level** (`L_mob`) instead of comparing native creature-template levels to the instance maximum.

The effective calculation is:

```text
RawTarget = L_mob - Floor
Target    = clamp(
              RawTarget,
              ItemScaling.MinLevel,
              min(H + Ceiling, ItemScaling.MaxLevel)
            )
```

where **H** is the highest eligible player level.

For live creature loot, `L_mob` is the creature's current `GetLevel()`. This means external creature scaling such as AutoBalance is naturally reflected in the item target. Mode 1 prewarm predicts the same encounter hierarchy from creature rank and map type so the baseline preparation path remains aligned with live drops.

Current defaults are deliberately conservative:

| Instance category | Ceiling | Floor |
|---|---:|---:|
| Normal dungeon | 0 | 3 |
| TBC heroic dungeon | 0 | 3 |
| Wrath heroic dungeon | 0 | 3 |
| Generic/custom heroic dungeon | 0 | 3 |
| Raid fallback | 0 | 3 |
| 10-player raid | 0 | 3 |
| 10-player heroic raid | 0 | 3 |
| 15-player raid | 0 | 3 |
| 20-player raid | 0 | 3 |
| 25-player raid | 0 | 3 |
| 25-player heroic raid | 0 | 3 |
| 40-player raid | 0 | 3 |

Example defaults:

```ini
ItemScaling.Dynamic.Ceiling.Dungeons = 0
ItemScaling.Dynamic.Floor.Dungeons = 3

ItemScaling.Dynamic.Ceiling.HeroicDungeons.TBC = 0
ItemScaling.Dynamic.Floor.HeroicDungeons.TBC = 3

ItemScaling.Dynamic.Ceiling.HeroicDungeons.Wrath = 0
ItemScaling.Dynamic.Floor.HeroicDungeons.Wrath = 3

ItemScaling.Dynamic.Ceiling.Raid25M = 0
ItemScaling.Dynamic.Floor.Raid25M = 3
```

With `H = 80` and the default raid settings:

```text
Raid skull boss at level 83 -> 83 - 3 = target 80
Raid trash at level 80      -> 80 - 3 = target 77
```

With `H = 70` in Black Temple / Tempest Keep:

```text
Raid boss at level 73 -> 73 - 3 = target 70
```

That target can then trigger native-loot preservation when the original item is already a native level-70 drop.

Per-instance overrides remain supported:

```ini
ItemScaling.Dynamic.PerInstance = "229 3 0, 230 5 3"
```

The parser also accepts AutoBalance's five-token per-instance format:

```text
[MapID] [SkipHigher] [SkipLower] [Ceiling] [Floor]
```

## Native loot preservation

Native-equivalent loot is preserved by default:

```ini
ItemScaling.PreserveNativeLoot = 1
```

The module determines an item's native reference level using `RequiredLevel` when it is non-zero; otherwise it falls back to `ItemLevel`, clamped to the supported player-level range.

When preservation is enabled, the original Blizzard item is used directly if its native reference level matches any of:

- the requested target level;
- the bracketed target level;
- the highest eligible player level.

That bypass happens before synthetic generation wherever possible and is enforced again in the final loot/registry path.

Examples:

```text
Level 70 player in Black Temple
native item reference = 70
target = 70
=> original item drops
=> no synthetic slot, no staging row, no demand request

Level 80 player in Black Temple
native item reference = 70
target = 80
=> levels do not match
=> normal upward scaling can proceed
```

This replaces the old manual `ItemScaling.ExcludedLevels` blacklist. The new behavior is conditional: a native level-70 item can stay untouched for a level-70 player while still scaling for a level-80 player.

It works in Mode 1 prewarm, Mode 2 actual-drop generation, and persisted-only mode.

## Dynamic floor and ceiling variance

Dynamic scaling can roll controlled per-drop variation around the configured floor and ceiling.

Defaults:

```ini
ItemScaling.Dynamic.Floor.Variance.Enable = 1
ItemScaling.Dynamic.Ceiling.Variance.Enable = 0
ItemScaling.Dynamic.Variance.Scope = 1

ItemScaling.Dynamic.Floor.Variance.Dungeons = "-1:20.0, -2:10.0, -3:5.0"
ItemScaling.Dynamic.Floor.Variance.HeroicDungeons = "-1:20.0, -2:10.0, -3:5.0"
ItemScaling.Dynamic.Floor.Variance.Raids = ""
```

Because the target is `L_mob - Floor`, a **negative floor delta is an upgrade**:

| Roll | Effective floor from base 3 | Example: level-80 trash |
|---|---:|---:|
| no variance | 3 | target 77 |
| `-1` | 2 | target 78 |
| `-2` | 1 | target 79 |
| `-3` | 0 | target 80 |

The default weights mean:

```text
20% -> floor delta -1
10% -> floor delta -2
 5% -> floor delta -3
65% -> no floor change
```

Both named-pair and six-value positional formats are supported:

```ini
"-1:20.0, -2:10.0, -3:5.0"
"5.0, 10.0, 20.0, 0.0, 0.0, 0.0"
```

The positional order is `-3, -2, -1, +1, +2, +3`. Percentages do not need to total 100; the remainder means no change.

`ItemScaling.Dynamic.Variance.Scope` controls when the roll happens:

| Value | Scope |
|---|---|
| `0` | Per-loot/per-creature roll; items on that loot source share the rolled shift. |
| `1` | Per-item roll; each eligible item can resolve independently. |

Ceiling variance uses the same weight format but is disabled by default. A positive ceiling delta permits targets above `H`, still bounded by `ItemScaling.MaxLevel`.

Mode 1 prewarm intentionally uses **zero variance** to prepare deterministic baseline variants. Lucky rolled variants are generated on demand when real loot is processed.

## AutoBalance integration

AutoBalance integration is optional.

Default:

```ini
ItemScaling.UseAutoBalanceSettings = 0
```

With it disabled, Item Level Scaling uses only its own `ItemScaling.*` settings.

With:

```ini
ItemScaling.UseAutoBalanceSettings = 1
```

the module reads the relevant AutoBalance level-scaling configuration and adopts its effective:

- scaling method;
- dynamic floors;
- dynamic ceilings;
- per-instance dynamic-level overrides.

The instance category model is aligned with AutoBalance's normal/heroic dungeon and raid-size categories.

In dynamic mode, the current in-instance creature level is an authoritative input to the target calculation. AutoBalance can therefore affect loot through the creature level itself, while `ItemScaling.UseAutoBalanceSettings = 1` additionally adopts AutoBalance's configured method, floors, ceilings and per-instance overrides.

Item Level Scaling still creates its own exact variant key and permanent item template; it does not reuse or mutate an AutoBalance object.

## First-run live generation

```ini
ItemScaling.Live.Enable = 1
```

enables first-run live generation.

Two generation modes are available:

| GenerationMode | Behavior |
|---|---|
| `1` | **Dungeon-entry preparation.** Prepares eligible equipment for the current instance target and refreshes when the highest eligible player level changes. |
| `2` | **Actual-drop generation.** Requests a variant only when eligible loot really rolls. |

Default:

```ini
ItemScaling.Live.Enable = 1
ItemScaling.Live.GenerationMode = 2
```

Mode 1 prewarm prepares deterministic baseline targets and does not consume variance rolls. When real loot is processed, native-match preservation and any enabled variance are evaluated before the exact variant is selected.

If an unexpected or lucky-variance item is not ready when it drops, the module briefly defers that loot path while the exact variant is generated and durably staged.

The original rolled item identity, count, slot, ownership and random-property information are retained while waiting.

If generation succeeds, the ready scaled variant replaces the original entry.

If SQL persistence fails, the timeout expires, the queue is saturated, or no reserved slot is available, the original item is released instead. An uncommitted synthetic item is never intentionally awarded.

## Live template pool

The module does not resize AzerothCore's native item stores during gameplay.

At startup it creates or recovers a configurable pool of inert reserved `item_template` rows:

```ini
ItemScaling.Live.ReservedSlots = 4096
ItemScaling.Live.MaxPendingVariants = 4096
ItemScaling.Live.MaxPublishPerTick = 64
ItemScaling.Live.LootWaitTimeoutMs = 10000
```

A live request:

1. receives one unused reserved entry;
2. copies the base template into module-owned request state;
3. generates the scaled snapshot in bounded world-update work;
4. writes the complete staged item and exact variant mapping asynchronously;
5. waits for commit acknowledgement;
6. publishes the completed template into the already-existing reserved object;
7. marks the variant ready for normal AzerothCore use.

Published template objects keep stable native pointers and are immutable after issue.

## Persistence and restart recovery

Live variants are durable before they are published for award.

The world database uses:

| Table | Purpose |
|---|---|
| `item_template` | Base items, permanent scaled variants, and unused reserved placeholders |
| `scaled_item_variant` | Permanent exact variant keys, identity data and generated-entry mapping |
| `mod_item_level_scaling_slot` | Reserved live entry ownership |
| `mod_item_level_scaling_staged_item` | Complete staged item snapshots |
| `mod_item_level_scaling_staged_variant` | Staged exact-key mappings awaiting startup promotion |

At the next startup, complete staged records are promoted transactionally using the same synthetic entry and the exact saved values.

Previously issued items are therefore not recalculated from changed base stats or changed configuration.

Recovery also runs when item scaling itself is disabled, because already-issued synthetic item IDs may still exist on characters or in storage.

The live staging/promotion tables require InnoDB semantics.

## Persisted-only mode (`Live.Enable = 0`)

To disable dynamic live generation:

```ini
ItemScaling.Live.Enable = 0
```

In persisted-only mode, the module serves previously generated and committed variants directly from in-memory index (`_keyToEntry`) without database round-trips. Unseen item/level combinations drop as their original native item template and no new synthetic variants are generated or queued. All legacy demand-ledger mechanisms have been completely retired.

## Random properties and suffixes

Random gear has two modes.

### Mode 0 — skip

```ini
ItemScaling.RandomSuffix.Mode = 0
```

This is the default.

Items with random property/suffix data remain native and are not converted into synthetic scaled variants. This avoids the WoW 3.3.5a client showing incorrect zero-value random-suffix stats for synthetic item IDs.

### Mode 1 — stat baking

```ini
ItemScaling.RandomSuffix.Mode = 1
```

The actual rolled random property becomes part of the exact variant identity.

Supported stat and resistance bonuses are baked into the synthetic template as fixed values, the rolled suffix name is preserved in the generated item name, and random fields are cleared only after successful conversion.

If the rolled enchantment cannot be represented safely, DBC data is missing, integer bounds would be exceeded, or the item has too many resulting stats, the original item is retained rather than silently losing bonuses.

## Required level

Scaled item required level is configurable:

```ini
ItemScaling.RequiredLevel.Policy = "target"
```

Supported policies:

| Policy | Result |
|---|---|
| `target` | Required level is the target effective level. |
| `player` | Required level is **H**. |
| `target-capped-player` | Uses `min(target, H)`, clamped to the supported player-level range. |

## Eligibility and safety filters

The module can independently enable or disable scaling for:

- normal dungeons;
- heroic dungeons;
- raids by size/difficulty;
- instanced chests;
- item qualities;
- upward scaling;
- downward scaling;
- native Blizzard scaling/heirloom-style items.

It also supports:

```ini
ItemScaling.MinLevel = 1
ItemScaling.MaxLevel = 80
ItemScaling.PreserveNativeLoot = 1
ItemScaling.ExcludedMapIds = ""
ItemScaling.ExcludedItemIds = ""
```

`ItemScaling.ExcludedLevels` has been replaced by `ItemScaling.PreserveNativeLoot`. Native matching is dynamic rather than a permanent blacklist of all items from particular progression levels.

Quest-related items, quest starters, scripted equipment, configured exclusions, disabled items and protected native behaviors are filtered from the ordinary scaling path.

Critters, totems, pets and temporary summons are excluded from normal creature-loot scaling.

## Loot, chests and native item queries

The module uses AzerothCore's normal item-template/query path after publication.

Pending reserved templates are not exposed to the client. Item queries for a still-pending generated entry are deferred until the template is ready.

Creature loot is gated before native group rolls start when an exact live variant is still pending.

The ordinary chest adapter runs only after successful native open-lock validation. It preserves the generated chest contents and native group-loot setup while waiting.

Scripted chests and gameobjects with custom AI keep their native script flow. Their observed drops can still prepare variants for future drops, but the first custom-script opening is outside the ordinary chest adapter's first-drop guarantee.

## Playerbots

Playerbots can coexist with this module without Playerbots source changes.

By default:

```ini
ItemScaling.RealPlayersOnly = 1
```

so bots do not raise the target player level.

Playerbots' queued creature-loot packets use the same live loot gate, and their normal valuation / Need-Greed / equip logic sees ordinary ready item templates.

Playerbots' startup-built autogear catalogues are not rebuilt dynamically. Newly permanent variants become visible to those startup catalogues after the next restart.

## GM commands

Two aliases are registered:

```text
.itemscaling
.mils
```

### Status

```text
.itemscaling status
```

Shows runtime diagnostics including:

- module state;
- fixed/dynamic method;
- AutoBalance synergy state;
- current dungeon/heroic/raid floors and ceilings;
- per-instance override count;
- ScaleUp / ScaleDown;
- RealPlayersOnly / IncludeGameMasters;
- required-level policy;
- random suffix mode;
- formula/generator revision;
- registry/database state;
- synthetic ID counts;
- live mode, available slots, pending, durable, ready and failed variant counts.

### Preview

```text
.itemscaling preview <itemLink|itemId> [targetLevel]
```

Previews the calculated scaled template without requiring the item to drop.

It reports eligibility, target/native level, target item level, required level, armor, weapon damage/DPS, stats, resistances and whether an already-indexed permanent variant exists.

## Important configuration

A representative live setup using the current defaults:

```ini
ItemScaling.Enable = 1

ItemScaling.ScaleDungeons = 1
ItemScaling.ScaleRaids = 1
ItemScaling.ScaleHeroics = 1
ItemScaling.ScaleChests = 1

ItemScaling.LevelScaling.Method = "dynamic"
ItemScaling.RealPlayersOnly = 1
ItemScaling.IncludeGameMasters = 0
ItemScaling.PreserveNativeLoot = 1

ItemScaling.Dynamic.Ceiling.Dungeons = 0
ItemScaling.Dynamic.Floor.Dungeons = 3
ItemScaling.Dynamic.Ceiling.Raids = 0
ItemScaling.Dynamic.Floor.Raids = 3

ItemScaling.Dynamic.Floor.Variance.Enable = 1
ItemScaling.Dynamic.Ceiling.Variance.Enable = 0
ItemScaling.Dynamic.Variance.Scope = 1
ItemScaling.Dynamic.Floor.Variance.Dungeons = "-1:20.0, -2:10.0, -3:5.0"
ItemScaling.Dynamic.Floor.Variance.HeroicDungeons = "-1:20.0, -2:10.0, -3:5.0"
ItemScaling.Dynamic.Floor.Variance.Raids = ""

ItemScaling.Live.Enable = 1
ItemScaling.Live.GenerationMode = 2
ItemScaling.Live.ReservedSlots = 4096
ItemScaling.Live.MaxPendingVariants = 4096
ItemScaling.Live.MaxPublishPerTick = 64
ItemScaling.Live.LootWaitTimeoutMs = 10000

ItemScaling.RandomSuffix.Mode = 0
ItemScaling.RequiredLevel.Policy = "target"

ItemScaling.UseAutoBalanceSettings = 0
ItemScaling.Announce = 1
ItemScaling.BracketStep = 1
```

See the complete documented template:

[conf/mod_item_level_scaling.conf.dist](conf/mod_item_level_scaling.conf.dist)

All current `ItemScaling.*` settings are treated as restart settings. `.reload config` does not mutate the active item-scaling snapshot.

## Synthetic item IDs

By default:

```ini
ItemScaling.SyntheticEntry.Start = "auto"
ItemScaling.SyntheticEntry.AutoOffset = 1000
ItemScaling.SyntheticEntry.Maximum = 2000000
```

`auto` chooses a safe range above existing item entries. Existing issued IDs are never renumbered.

The core fast lookup vector grows according to the highest allocated item ID, so unnecessarily huge fixed starting IDs should be avoided.

## Installation

1. Clone or place this repository under AzerothCore's `modules/` directory.
2. Reconfigure and rebuild AzerothCore/worldserver when you are ready to deploy the module.
3. Copy or install `conf/mod_item_level_scaling.conf.dist` into the active module configuration location and review the settings.
4. Ensure the module world-database updates are applied.
5. Start worldserver and verify `.itemscaling status`.

World SQL currently includes:

- [initial schema](data/sql/db-world/updates/2026_09_27_00_item_scaling_initial_schema.sql)
- [live staging migration](data/sql/db-world/updates/2026_10_04_00_item_scaling_live.sql)
- [retire demand ledger migration](data/sql/db-world/updates/2026_10_05_00_retire_demand_ledger.sql)

The module also contains runtime schema safeguards for its required tables and legacy column upgrades. Database migrations remain the authoritative deployment path.

## Updating an existing installation

When pulling a newer version:

1. stop worldserver cleanly;
2. update the module source;
3. review changes in `conf/mod_item_level_scaling.conf.dist`;
4. rebuild/reinstall worldserver if the C++ module changed;
5. allow the module/AzerothCore SQL update path to apply new migrations;
6. restart worldserver;
7. run `.itemscaling status`;
8. test one normal dungeon, heroic dungeon and raid appropriate to your configuration before treating the update as validated.

Do not delete `scaled_item_variant`, the staging tables, or reserved-slot state merely to "reset" the module if players already own generated items. Those tables are part of the persistence contract for permanent synthetic IDs.


## Architecture

The high-level live path is:

```text
Dungeon entry / rolled loot
        |
        v
Exact variant key
        |
        +--> ready? -------- yes --> use immutable scaled template
        |
        no
        v
Reserve existing inert template slot
        |
        v
Bounded generation
        |
        v
Asynchronous complete snapshot transaction
        |
        +--> failure/timeout --> retain original loot
        |
        v
Commit acknowledged
        |
        v
Publish into reserved native template object
        |
        v
Ready for native AzerothCore loot/query/equip paths
        |
        v
Next startup: promote staged snapshot permanently with same ID
```

## Troubleshooting

Start with:

```text
.itemscaling status
```

Useful checks:

- confirm the module is enabled;
- confirm live mode and generation mode;
- check available reserved slots;
- check pending/durable/ready/failure counts;
- confirm `RealPlayersOnly` matches the intended Playerbot behavior;
- confirm AutoBalance synergy is enabled only when desired;
- confirm the active floor/ceiling category;
- confirm `PreserveNativeLoot` if native progression drops should remain untouched;
- confirm floor/ceiling variance settings and scope;
- use `.itemscaling preview` on a known item;

Verbose calculation logging can be enabled with:

```ini
ItemScaling.Debug = 1
```

## License

GNU General Public License v2 or later, consistent with AzerothCore.
