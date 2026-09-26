# Item level scaling for AzerothCore WotLK

Scale eligible dungeon and raid equipment using permanent, ordinary static item templates.
No AzerothCore core patch, Playerbots patch, client patch, custom client Item.dbc, or runtime
ItemTemplate publication is required.

## Demand-ledger lifecycle

1. Gameplay rolls eligible equipment and calculates an exact variant key: base entry, target effective
   level, target ItemLevel, FormulaVersion, generator revision, and RequiredLevel.
2. An existing safe variant is selected immediately. Both random-property ID and suffix factor are
   regenerated using the final synthetic entry.
3. For a first unseen key, the original item drops. The module queues one asynchronous `INSERT IGNORE`
   into `scaled_item_variant_request`, including the validated base identity and formula setting.
4. At the next worldserver startup, only persisted pending requests are materialized. Template insertion,
   mapping insertion, and removal of completed demand share one InnoDB transaction.
5. AzerothCore loads those rows through its normal item-template loader. The module validates/indexes
   the loaded templates; future identical drops use the generated entry.

A restart is required before a newly requested combination becomes available. Repeated drops before
that restart remain original. There is no dungeon-wide item/level pre-generation or loot-table discovery.
A combination never encountered in real gameplay never creates a new variant. Pending demand grows
with distinct actual gameplay requests, not an item × level × player-level matrix.

Startup retains the baseline calculation and module-owned `RandPropPoints`/`ScalingStatValues` curve
loading. These read formula inputs; they do not create variants. The baseline retains its existing
read-only `GetItemTemplateStore()` fallback; it never mutates the store. Recovery of already-committed identities
is independent of new demand and does not allocate replacement IDs.

## Database layout and migration

- `scaled_item_variant`: durable synthetic entry, six-field unique identity, nullable validated base
  identity snapshot, and nullable `preserve_nonzero_stats` generation provenance.
- `scaled_item_variant_request`: the same six fields as its composite primary key, seven non-null
  identity snapshot fields, `preserve_nonzero_stats`, and `requested_at`. Requests have no synthetic ID.
- `item_template`: ordinary persisted scaled stats, damage, armor, block, resistances, levels, and
  cloned spells, sockets, and other inherited metadata.

Fresh schema: `sql/world/base/scaled_item_variant.sql` creates both module tables. Existing installations
apply the released updates in order, including the new
`data/sql/db-world/updates/2026_09_26_00_item_scaling_demand_ledger.sql`. The older released migrations
are unchanged. The new migration adds metadata without renumbering, deleting, rekeying, or changing
scaled values of any existing item. It is repeatable and works after the fresh schema as well.

`include.sh` registers base SQL and module updates. Runtime C++ never creates/alters tables. It validates
column widths/signs/nullability, identity indexes, the committed entry primary key, and InnoDB across
all three tables. Missing or incompatible schema disables scaling for that run.

## Existing variants and recovery

Existing synthetic entries remain valid for inventory, equipped gear, mail, auctions, guild banks,
and trading. The module never compacts or recycles entries. Keep committed mappings even if their
item-template rows are missing: the allocator uses the maxima of both tables to reserve past IDs.
Do not manually delete both records of an issued identity.

The seven snapshot fields match the core's DBC-enforced identity: class, subclass, sound override
subclass, material, display ID, inventory type, and sheath. Signed and unsigned SQL types match
`item_template`. A new request captures these from the already-validated runtime base. Startup clones
base SQL and overrides these identity columns using that snapshot. The static scaling formula and
its existing raw-base projection remain unchanged; generator revision remains 1.

For legacy mappings, snapshot columns start NULL. Once the core has loaded the real base, initialization
backfills its validated identity. If a loaded synthetic template has different identity, initialization
updates only those seven DB columns for the same ID and withholds it from new drops until another
restart. It never fixes ObjectMgr in memory or rewrites issued scaled stats. Other inherited loot-metadata
mismatches are withheld and reported for source-data review.

Missing templates can be reconstructed under their original ID only for the current generator,
active FormulaVersion, known matching stat-preservation setting, and complete identity snapshot.
Historical generations are never rebuilt with current code. Legacy rows have unknown generation-setting
provenance: identity backfill alone cannot prove their original stats. A missing legacy template with
unknown provenance must be restored from backup, not guessed. Existing legacy templates remain usable
when their loaded values and identity validate.

## Configuration

```ini
ItemScaling.DemandLedger.Enable = 1
ItemScaling.MaxNewVariantsPerStartup = 25000
ItemScaling.BracketStep = 1
ItemScaling.SyntheticEntry.Start = "auto"
ItemScaling.SyntheticEntry.AutoOffset = 1000
ItemScaling.SyntheticEntry.Maximum = 2000000
ItemScaling.FormulaVersion = 1
```

`DemandLedger.Enable = 0` disables both queuing and pending materialization; existing safe variants and
committed recovery remain available. If the new option is absent, deprecated `ItemScaling.PreStageDungeonLoot`
is used as the fallback (default true). An existing explicit legacy value of 0 therefore remains disabled.
An explicitly supplied new option takes precedence, including when copying the new distributed config.
There is no internal pre-staging flag or pre-staging operation.

`MaxNewVariantsPerStartup` limits pending requests materialized during one startup (1–250000).
Excess demand stays pending. Missing/ineligible bases also remain pending and are retried on later
starts. Reaching `SyntheticEntry.Maximum` retains requests and warns; the allocator cannot wrap.
The maximum controls new allocations, not restoration of an already-issued ID above a lowered ceiling.

`BracketStep` remains a runtime rule: targets round down to their bracket, with MaxLevel included.
Dynamic/fixed mode, ScaleUp/ScaleDown, quality filters, excluded items/maps/levels, and real-player
selection remain in effect. Chests use the same request lifecycle when enabled.

All options require restart. Increase `FormulaVersion` when changing `PreserveNonZeroStats` or formula
input data (base stats/curves). Pending obsolete FormulaVersion/generator/setting requests are retired
with a warning; future gameplay can request the current key. Known committed setting conflicts prevent
new generation until FormulaVersion changes. Historical committed families are retained. The module
cannot infer old generation settings or detect arbitrary edits to base rows/DBC data; preserve those
inputs for recovery and treat intentional changes as a new formula family.

## Loot safety and Playerbots

Quest-required loot is never substituted: both the quest vector and any `needs_quest` ordinary entry
retain their IDs for `HasQuestForItem`. MaxCount, unique-equipped items, quest starters, and scripted
equipment are skipped because synthetic entry identities could bypass limits or entry-specific scripts.
Disabled base/final entries are also skipped.

Category limits (`ItemLimitCategory`) are retained: the core counts shared categories across IDs.
Conditions remain on the original `LootItem`; their player checks are preserved. Free-for-all/multi-drop,
follow-loot-rules, faction restrictions, and recipe-visibility metadata are cloned and validated before
indexing. Ordinary recipes are outside scalable equipment classes. Loot counts, slots, allowed looters,
and flags are not rebuilt or moved between vectors. Normal, random-property, and random-suffix items
all refresh both entry-dependent random fields after a successful replacement.

Playerbots sees ordinary static `ItemTemplate` fields. Source inspection of StatsCollector,
StatsWeightCalculator, ItemUsageValue, LootRollAction, EquipAction, and factory/autogear paths confirms
that this fits their existing template-based reads. No Playerbots code changes or heirloom-based
main scaling model are introduced. This source compatibility assessment is not an in-game validation.

## Concurrency and failure behavior

The lookup map and synthetic-entry set are immutable after release/acquire initialization publication.
Miss deduplication uses a short mutex scope; the DB enqueue happens after unlocking. No map-thread
SELECT, direct DB write, template construction, loot scan, ID allocation, or ObjectMgr mutation occurs.
DB uniqueness collapses identical requests from different processes; allocation still supports only
one starting worldserver per shared world DB. There is no distributed allocation lock.

Asynchronous queuing does not wait for database acknowledgment. A database outage or crash before the
queued insert completes can lose that request, while the current original loot remains valid. Dedupe
allows at most one enqueue per key/process; failed writes are not retried until a new process encounters
the combination. After the insert commits, demand survives crashes. Materialization failure rolls back
the batch and leaves demand pending. Batch verification checks mappings, levels, identity, and completed
request removal before enabling scaling. Do not edit base data concurrently with startup generation.

## Client cache and deployment

The `OnBeforeFinalizePlayerWorldSession` hook and stable `ITEM_SCALING_CLIENT_CACHE_SALT` are preserved.
The stable salt is not bumped for the lifecycle change. A startup that repairs old identity uses a
separate temporary cache-version marker: the next clean restart invalidates any incorrect responses
cached while those old templates were still loaded. New synthetic entries always have permanent IDs.

Stop worldserver, back up the world DB, update the module, apply the module migrations, rebuild your
worldserver with the module, and restart. Review startup validation and repair logs. If identity repair
is reported, restart again before resuming normal play. Newly requested combinations require their
own subsequent restart. No live schema migration or worldserver build is performed by the module.

Canonical coupled runtime inspected for this implementation:

- `mod-playerbots/azerothcore-wotlk`, `Playerbot`, `7f12e89ee5f467a50e62eba1d525eac7dc953d03`.
- `mod-playerbots/mod-playerbots`, `master`, `7bae1b5c58c76a0aa20381155edc08096d1485b2`.
- Module starting `master`: `cbfa3be7f855c836400137582c42d188424ad576`.

See [tests/README.md](tests/README.md) for repeatable lightweight checks and the manual deployment checklist.

## License

GNU General Public License v2 or later, consistent with AzerothCore.
