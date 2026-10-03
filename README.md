# Item Level Scaling for AzerothCore

![Item Level Scaling](docs/images/item_scaling_banner.png)

Scale eligible dungeon and raid equipment to the player or party level in AzerothCore 3.3.5a. The hybrid live path prepares new variants during the first visit, saves them asynchronously, and exposes the complete stats through native item queries. No core, Playerbots, client, or DBC changes are required.

## First-run behavior

`ItemScaling.Live.Enable = 1` enables live generation. Choose when preparation starts:

| GenerationMode | Behavior |
|---|---|
| `1` (default) | Prepare eligible equipment when players enter a dungeon. Preparation refreshes when the highest eligible player level or operational configuration changes. |
| `2` | Prepare only equipment requested by actual rolled loot. |

Both modes hold an unexpected first-time loot request briefly until its complete snapshot is committed and its reserved RAM template is ready. The same rolled item, count, ownership and slot are retained. Ready variants are used immediately. Successful generation requires no second dungeon run or intervening restart.

SQL failure, timeout, exhausted slots or queue saturation leave the original rolled item available. `.itemscaling status` reports the active mode, available slots, pending/durable/ready variants, and failure count. Unsupported equipment and excluded sources retain their original behavior.

Set `ItemScaling.Live.Enable = 0` to use the legacy demand ledger: unseen variants remain original until the next startup materializes their requests. Already issued variants remain valid in either mode.

## Configuration

See [the configuration template](conf/mod_item_level_scaling.conf.dist) for all options.

```ini
ItemScaling.Enable = 1
ItemScaling.LevelScaling.Method = "dynamic"

ItemScaling.Live.Enable = 1
# 1 = dungeon entry; 2 = actual rolled drop
ItemScaling.Live.GenerationMode = 1
ItemScaling.Live.ReservedSlots = 4096
ItemScaling.Live.MaxPendingVariants = 4096
ItemScaling.Live.MaxPublishPerTick = 64
ItemScaling.Live.LootWaitTimeoutMs = 10000

# 0 = retain original random-property/suffix gear (default)
# 1 = bake supported rolled bonuses into fixed template stats
ItemScaling.RandomSuffix.Mode = 0

# Used for new requests when Live.Enable=0
ItemScaling.DemandLedger.Enable = 1
ItemScaling.ScaleDungeons = 1
ItemScaling.ScaleRaids = 1
ItemScaling.ScaleHeroics = 1
ItemScaling.ScaleChests = 1
ItemScaling.BracketStep = 1
```

`Live.*`, formula version, random mode, and persistence invariants are startup settings. `.reload config` retains their active values and logs when a requested change needs a restart. Existing operational filters remain reloadable. AutoBalance integration can override the effective scaling method and dynamic limits.

## RAM templates and persistence

Before core item loading, the module recovers previously staged snapshots and reserves the configured number of unused templates in `item_template`. These rows are inert placeholders, excluded from statistical baselines and blocked from client item-query responses. Startup does not generate all dungeon variants.

Gameplay requests copy the base template into a synchronized module queue. A bounded world update generates the variant and submits one asynchronous transaction containing its complete snapshot, seven-field key, identity metadata and slot assignment. Only a successful commit acknowledgement permits publication.

At `WorldScript::OnUpdate`, after the core joins map workers, the module populates an existing reserved template object in place. It keeps both core lookup containers and their pointers stable. Published templates are immutable, use unique permanent IDs, and are never reassigned.

At the next startup, the saved snapshot replaces its owned SQL placeholder and gains a permanent mapping in one transaction. The original ID and values survive; the module does not recalculate issued items from a changed base or configuration. Recovery runs even when scaling is disabled. Incomplete staging, an ownership collision, nontransactional tables or an incompatible staging schema stop startup before player login.

The world database contains:

| Table | Purpose |
|---|---|
| `item_template` | Ordinary permanent variants and unused live placeholders |
| `scaled_item_variant` | Permanent variant keys and validated base identities |
| `scaled_item_variant_request` | Legacy requests awaiting startup materialization |
| `mod_item_level_scaling_slot` | Reserved IDs and durable assignment state |
| `mod_item_level_scaling_staged_item` | Complete saved RAM-template snapshots |
| `mod_item_level_scaling_staged_variant` | Keys and metadata awaiting promotion |

Keep these tables together in backups. Run one worldserver against a given world database; the live slot pool is owned by that worldserver.

## Bags, tooltips and random gear

Native `CMSG_ITEM_QUERY_SINGLE` responses read the same complete template used for gameplay: stats, damage, armor, resistances, block, item level, required level and inherited metadata. Pending placeholder responses are suppressed; deferred queries resume after publication. Zero-valued stat rows are compacted to match the core loader after restart.

Random-property/suffix scaling stays **off by default**. With `ItemScaling.RandomSuffix.Mode = 1`, the actual rolled property becomes part of the variant key. Supported stat and resistance bonuses and the rolled name are baked into fixed fields. Both template random fields and awarded instance random fields are cleared. Unsupported effects, missing DBC data, overflow or too many stats retain the original item rather than losing bonuses.

The packet and template paths are implemented for native bag, equipped and linked tooltips. Client rendering and actual equipped bonuses still require in-game verification; source and SQL checks cannot prove the UI result.

## Loot safety and Playerbots

Quest loot, quest starters, entry-based uniqueness, scripted equipment, native scaling distributions, disabled items and configured exclusions remain protected. Critters, totems, pets and temporary summons are excluded from actual creature loot scaling.

Creature loot packets wait before native group rolls start. The ordinary chest adapter runs after successful native open-lock validation, preserves the original contents and group-loot setup, and resumes after publication. Scripted chests and chests with a custom AI retain their native flow; their on-drop requests can prepare variants for later drops, but their first opening is outside the ordinary chest adapter's guarantee.

Playerbots' queued creature-loot packets use the same gate. Live valuation, Need/Greed and equip actions read ordinary templates. Playerbots' startup-built autogear catalogues gain new permanent variants on the next restart; the module does not rebuild or change Playerbots internals.

## Installation

1. Place this module in AzerothCore's `modules` directory.
2. Reconfigure and build worldserver when you are ready to deploy.
3. Copy `conf/mod_item_level_scaling.conf.dist` to the active module configuration location and choose the generation mode.
4. Let the module SQL updater apply [the initial schema](data/sql/db-world/updates/2026_09_27_00_item_scaling_initial_schema.sql) and [the live staging migration](data/sql/db-world/updates/2026_10_04_00_item_scaling_live.sql). Runtime startup also creates missing module tables idempotently.

Both schema installation routes define the same tables; legacy upgrades use `information_schema` guards compatible with Oracle MySQL. InnoDB is required for atomic staging and promotion.

The implementation task did not compile, install, restart worldserver or apply migrations to the live databases. See [verification instructions](tests/README.md), [architecture](docs/ARCHITECTURE.md), and [the implementation record](docs/ISSUES.md#mils-004).

## License

GNU General Public License v2 or later, consistent with AzerothCore.
