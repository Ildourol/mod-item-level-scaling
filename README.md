# Item level scaling — version 1.0.0

Scale eligible dungeon and raid equipment using permanent, ordinary static item templates.
This is the initial demand-ledger release. No AzerothCore core patch, Playerbots patch, client patch,
custom client Item.dbc, or runtime ItemTemplate publication is required.

## Installation

Install the module under `modules/mod-item-level-scaling` in the canonical Playerbot core source tree,
include it in your worldserver build, and copy `conf/mod_item_level_scaling.conf.dist` into your server's
module configuration directory. The default formula version is 1; the current generator revision is 2.

Both module tables are created directly with their final schema:

- `sql/world/base/scaled_item_variant.sql` is the authoritative schema for manual installation and
  the shell DB assembler paths registered by `include.sh` / `conf/conf.sh.dist`.
- `data/sql/db-world/updates/2026_09_27_00_item_scaling_initial_schema.sql` contains the identical
  CREATE-only schema for the core module updater. Both files use `CREATE TABLE IF NOT EXISTS`.

For an empty AzerothCore world database, the core populates its own base tables first and then runs
module SQL. For a working AzerothCore world database receiving this module for the first time, the
module updater creates the two module tables directly without changing ordinary world items.
The module must be enabled in the worldserver build, its SQL files must be accessible under the core
source directory, and world database updates must be enabled. If automatic updates are disabled,
apply the final schema manually to the world database before starting worldserver.

Runtime C++ validates schema types, nullability, identity indexes and transactional engines.
Missing or incompatible tables disable scaling for that run. It never creates or alters tables.
The installation SQL does not transform an incompatible existing module schema.

## First start and gameplay

1. First startup validates the empty module tables, resolves the synthetic allocation range, loads
   formula inputs, and builds the baseline. With no pending requests, it creates no variants.
2. Gameplay rolls eligible equipment and calculates the exact key: base entry, target effective level,
   target ItemLevel, FormulaVersion, generator revision, and RequiredLevel.
3. On a first unseen key, the original item drops. One asynchronous `INSERT IGNORE` records the request,
   including the validated base identity and stat-preservation setting.
4. At the next startup, each pending request's saved identity is checked against the effective base
   identity that AzerothCore will load. Only matching requests are materialized. In one InnoDB transaction,
   the module inserts the static `item_template`, inserts the complete committed mapping, and deletes
   the completed request. AzerothCore then loads the item through `ObjectMgr::LoadItemTemplates()`.
5. The module validates and indexes the loaded templates. Future matching drops select the permanent
   synthetic entry and regenerate both random-property ID and suffix factor using that final entry.
6. Later restarts load the same committed templates and mappings under the same IDs.

A restart is required before a newly requested combination becomes available. Repeated drops before
that restart remain original. There is no dungeon-wide item/level pre-generation or loot discovery.
Demand grows with distinct combinations encountered in gameplay. Baseline reads and module-owned
`RandPropPoints` / `ScalingStatValues` curve loading supply formula inputs; they do not create variants.

## Complete persisted state

- `scaled_item_variant`: permanent synthetic entry, six-field unique key, seven non-null validated
  base-identity fields, non-null `preserve_nonzero_stats`, and `created_at`.
- `scaled_item_variant_request`: the same six fields as a composite primary key, seven non-null identity
  fields, non-null `preserve_nonzero_stats`, and `requested_at`. Requests have no allocated synthetic ID.
- `item_template`: real scaled stats, damage, armor, block, resistances, levels, and cloned spells,
  sockets, random-property definitions, and other inherited metadata.

The identity snapshot contains class, subclass, sound override subclass, material, display ID,
inventory type, and sheath. These match the core's DBC-enforced fields and their SQL widths/signs.
Gameplay captures them from the validated base template. Startup clones the ordinary base SQL row,
overrides those fields with the snapshot, and writes complete metadata into the committed mapping.

Before allocating an ID, startup resolves the base identity using the same file-first, database-second
order as the core: a private `Item.dbc` store with the `item_dbc` overlay. With `DBC.EnforceItemAttributes`
enabled, an available DBC entry supplies those seven fields. With enforcement disabled or no matching
DBC entry, the ordinary base SQL fields apply. Eligibility and formula generation use that resolved
identity too; raw SQL differing from an unchanged enforced DBC entry is not a changed identity.

A confirmed identity change retires only that exact pending request, without allocating an ID or
creating an item/mapping. Later gameplay can capture the new identity and request it again for another
startup. A missing base remains pending. If the identity sources cannot be verified, or an effective
identity cannot fit the snapshot's SQL types, materialization stops and scaling is disabled for that
run; unprocessed requests remain pending. The check does not snapshot or detect changes to rarity,
stats, spells or every other base field.

The private identity store is loaded only when there is pending demand and DBC enforcement is enabled,
and is released before the hook returns. Startup logs its load/verification time. This adds a temporary
DBC load and database overlay verification, with no additional gameplay work or global DBC mutation.

Initialization only reads, validates and indexes. A missing template or a mismatch in identity,
levels, or loot-sensitive inherited metadata withholds the variant from new drops and logs the issue.
It never rewrites a committed template, changes its key, or reconstructs its scaled values from current
formula inputs. Restore corrupt state from a consistent database backup. A withheld mapping still
reserves its key and synthetic ID; new drops remain original.

IDs remain permanent for equipment, inventory, mail, auctions, guild banks and trading. The allocator
accounts for maxima in both `item_template` and `scaled_item_variant`. Do not delete both records of
an issued identity: without either record the allocator cannot know that its entry was used.

## Configuration

```ini
ItemScaling.Enable = 1
ItemScaling.DemandLedger.Enable = 1
ItemScaling.MaxNewVariantsPerStartup = 25000
ItemScaling.BracketStep = 1
ItemScaling.SyntheticEntry.Start = "auto"
ItemScaling.SyntheticEntry.AutoOffset = 1000
ItemScaling.SyntheticEntry.Maximum = 2000000
ItemScaling.FormulaVersion = 1
ItemScaling.PreserveNonZeroStats = 1
```

`DemandLedger.Enable = 0` stops new request insertion and pending materialization; already-loaded,
validated variants remain selectable. Pending requests remain in the database.

`MaxNewVariantsPerStartup` limits pending requests materialized during one startup (1–250000).
Excess requests and requests with missing/ineligible base items remain pending. Reaching the synthetic
entry maximum also retains pending demand and warns. The ceiling limits new allocations; it does not
invalidate already-committed IDs above a subsequently lowered ceiling.

`BracketStep` remains a runtime rule: targets round down to their bracket, with MaxLevel included.
Dynamic/fixed mode, ScaleUp/ScaleDown, quality filters, excluded items/maps/levels, and real-player
selection remain in effect. Chests use the same request lifecycle when enabled.

All settings require restart. FormulaVersion and generator revision identify permanent generation
families; they do not convert existing items. If changing `PreserveNonZeroStats` or formula input data
(base stats/curves), use a distinct FormulaVersion before generating further items. A known committed
stat-preservation conflict blocks new generation in that family. A pending request incompatible with
the active formula version, generator revision or setting is retired with a warning; later gameplay
can request the active family. Committed rows are never regenerated or rekeyed. The module cannot
detect arbitrary edits to formula inputs, so keep those inputs stable within a family.

Generator revision 2 accounts for using the resolved class, subclass and inventory type in eligibility
and stat generation. Existing revision-1 templates, IDs and stats are unchanged and still load normally,
but the active lookup selects revision-2 keys. Revision-1 pending requests are retired by the same
family-safety check; gameplay can request the current family. There is no schema migration or automatic
conversion of committed items. The client cache salt is unchanged because issued entries are not rewritten.

## Loot safety and Playerbots

Quest-required loot retains its original entry for quest checks. MaxCount, unique-equipped items,
quest starters, scripted equipment and disabled entries are skipped. Shared category limits,
conditions on the original LootItem, free-for-all/multi-drop, follow-loot-rules, faction restrictions,
and recipe visibility remain subject to cloned and validated metadata. Ordinary recipes are outside
scalable equipment classes. Loot counts, slots, allowed looters and vectors are preserved.

Playerbots reads ordinary static ItemTemplate stats, levels, damage, armor and inherited metadata.
StatsCollector, StatsWeightCalculator, ItemUsageValue, LootRollAction, EquipAction and factory/autogear
paths use these templates without a special case. There is no heirloom-based main scaling model.
This is a source compatibility assessment; in-game behavior belongs in the manual checklist.

## Concurrency, failures and cache

The lookup map and synthetic-entry set are immutable after release/acquire publication. Miss dedupe
uses a short mutex scope, and the DB enqueue occurs after unlocking. Map workers perform no synchronous
DB operation, template construction, ID allocation, loot scan or ObjectMgr mutation.

DB uniqueness collapses identical requests across callers. Synthetic allocation supports one starting
worldserver per shared world DB; there is no distributed allocator lock. A database outage or crash
before an asynchronous insert completes can lose that request while the original loot remains valid.
Process dedupe permits one enqueue per key, so failed inserts are retried only when a new process
encounters that combination. Once committed, a request survives restart. Materialization failure rolls
back its transaction and retains demand; batch verification checks template/mapping completeness and
request removal before enabling scaling. Do not edit `item_template`, `item_dbc` or `Item.dbc` during
startup: the identity check and the core's subsequent item load require stable source data.

The normal player-session cache hook uses a stable module salt. Synthetic IDs are permanent and
committed metadata is immutable; no special restart state or client patch is required.

## Runtime and checks

Canonical runtime:

- `mod-playerbots/azerothcore-wotlk`, branch `Playerbot`.
- `mod-playerbots/mod-playerbots`, branch `master`.

See [tests/README.md](tests/README.md) for module-only checks and the manual validation checklist.

## License

GNU General Public License v2 or later, consistent with AzerothCore.
