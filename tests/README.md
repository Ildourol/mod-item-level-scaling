# Module regression checks

These checks do not configure/build AzerothCore, edit its source, or start worldserver.

## Module-only compiler and C++ regressions

```bash
python3 tests/check_compile.py --core /path/to/azerothcore-wotlk
```

Requires GCC with GNU C++20, binutils, Python 3, canonical core headers/vendored dependencies, Boost,
and MySQL/MariaDB development headers. Add `--headers /path/to/extracted/usr/include` when appropriate.
The script compiles module objects with `-Wall -Wextra -Werror`, combines them, checks module symbol
resolution, and runs `test_*.cpp`. This is not a linked worldserver build.

Tests cover configuration bounds, bracket widths, permanent ID allocation/overflow, fixed/dynamic
target policy, six-field variant identity, repeated request-key deduplication, identity snapshots
(including signed fields/range checks), and application of identity without changing scaled stats.

`test_startup_identity.cpp` links the core's unmodified DBC loader sources into a small executable with
a numeric database transport fixture. It exercises the production startup identity helper using an
actual WDBC file, database overlay precedence, database-only entries, empty/missing sources, failed
reads, enforcement on/off and changed identity across two loads. It also reports a 50,000-row fixture
load/verification time. This is not a real database transport test or a worldserver startup benchmark.

Run core C++ codestyle from this module root:

```bash
python3 /path/to/azerothcore-wotlk/apps/codestyle/codestyle-cpp.py
git diff --check
```

## Isolated SQL regressions

```bash
python3 tests/check_sql.py \
  --core /path/to/azerothcore-wotlk \
  --mariadbd /path/to/mariadbd \
  --library-path /path/to/extracted/usr/lib/x86_64-linux-gnu
```

For MySQL 8, run the same suite with the server and client binaries:

```bash
python3 tests/check_sql.py \
  --core /path/to/azerothcore-wotlk \
  --mysqld /path/to/mysql/bin/mysqld \
  --mysql /path/to/mysql/bin/mysql
```

The optional `--library-path` supports extracted dependencies with either engine. MariaDB uses
bootstrap mode; MySQL initializes a temporary datadir and uses a private Unix socket with both
network listeners disabled. No existing database, credentials, or application stack is used.
Both modes enforce strict SQL for metadata rejection and actually restart the database to check persistence.
This extends the permanent existing SQL regression, not a live-stack/e2e test.

When the environment prohibits sockets, replace `--mysql ...` with `--mysql-initialize-only`.
That limited MySQL mode executes both installation routes, schema assertions, request dedupe,
materialization and every module SELECT during temporary database initialization. It does **not**
cover restart, NULL rejection or failed-transaction rollback; the full suite requires one of the
normal modes above. The temporary database is always removed.

Coverage includes:

- One CREATE-only final schema, identical in the base and first-install updater files.
- First module installation onto ordinary world data and fresh assembly followed by updater SQL.
- Empty first-start tables, InnoDB engines and six-column unique identities.
- Different RequiredLevel, FormulaVersion and generator revision remain distinct; duplicate requests collapse.
- Complete metadata: SQL widths/signs match core, and all eight snapshot/provenance fields reject NULL.
- Actual C++ template/mapping inserts and completed-request deletion in one transaction.
- Every module SELECT call site, using its C++ SQL text and explicit fixture parameter bindings.
- Exact-key request deletion leaves unrelated pending requests untouched.
- A forced mapping failure rolls back the inserted template and retains the pending request.
- A missing source row cannot insert a mapping or consume pending demand.
- A second DB invocation preserves committed IDs, values, metadata and pending requests.
- A missing committed template keeps its mapping and allocator reservation.
- Changed-base request retirement across database starts deletes only the exact key, consumes no ID,
  preserves unrelated demand and committed snapshots, and permits a newly captured request.
  This scenario executes the module's SQL; it does not execute the worldserver C++ control flow.
- Source checks for validation-only initialization, request suppression of committed keys, immutable snapshot
  comparisons, identity resolution/checks before eligibility/allocation/generation, current-family checks
  before generation and a non-blocking loot miss path. These checks do
  not execute worldserver's initialization or stale-request control flow.

The core SQL checker assumes core directories. It rejects changes in any `base` directory by policy;
this module owns its explicitly requested final schema in `sql/world/base`. Report that directory-policy
result separately and run its SQL content checks on both installation files. Do not move module SQL
into core to satisfy the directory rule.

## Manual/live checklist (not executed by these checks)

Use an isolated realm with the canonical Playerbot core and mod-playerbots branches.

- [ ] First install onto an existing ordinary world DB: final schema validates; no demand creates no variants.
- [ ] Empty core DB with module enabled: core population plus module updater creates the final tables.
- [ ] Shell DB assembler path: final tables match the updater installation.
- [ ] Subsequent normal restarts preserve generated IDs, complete metadata and item values.
- [ ] First unseen drop: original item, exactly one pending key, no template or ID allocation.
- [ ] Repeated identical drops before restart: original item, still one pending row.
- [ ] Simultaneous identical map-worker/player/bot misses: one logical request.
- [ ] Restart: one template plus mapping committed; completed request disappears.
- [ ] Matching post-restart drop selects the exact generated entry.
- [ ] Same base at two target levels; distinct RequiredLevels stay separate.
- [ ] ScaleUp, ScaleDown, dynamic and fixed modes; BracketStep and MaxLevel boundaries.
- [ ] Quality/item/map/level exclusions and real-player versus bot-only groups.
- [ ] Chest loot and ordinary creature/boss loot.
- [ ] Quest-required loot, quest starters, MaxCount, unique-equipped and disabled items stay original.
- [ ] Category limits, conditional loot, free-for-all, follow-loot-rules and faction checks remain correct.
- [ ] Normal item, RandomProperty, RandomSuffix: both metadata fields match final entry.
- [ ] Group loot; Playerbot loot; Playerbot Need/Greed.
- [ ] Playerbot autogear, stat valuation and equip behavior.
- [ ] Trade, mail, auction, guild bank, reconnect and tooltip/icon cache.
- [ ] Missing committed template: log and withhold; keep its key and ID reserved without reconstruction.
- [ ] Mismatched committed identity/levels/loot metadata: withhold without changing database rows.
- [ ] Inactive generation family: issued template still loads; no regeneration or new selection.
- [ ] FormulaVersion/generator/PreserveNonZeroStats change retires stale pending requests.
- [ ] Unchanged effective identity materializes when raw SQL differs from an enforced DBC entry.
- [ ] Changed `Item.dbc`/`item_dbc` identity retires only the affected exact pending key before allocation.
- [ ] With DBC enforcement off, a changed raw SQL identity retires the affected request.
- [ ] With no matching DBC entry, the raw SQL identity determines eligibility and the comparison.
- [ ] Gameplay after retirement captures a fresh snapshot; the next restart materializes it normally.
- [ ] An unavailable identity source retains unprocessed demand and disables scaling for that run.
- [ ] Revision-1 committed items remain unchanged/loadable; new matching demand uses revision 2.
- [ ] Empty demand and disabled enforcement skip the private DBC load; inspect startup timing with demand.
- [ ] Known family conflict requires FormulaVersion bump without rewriting committed items.
- [ ] Missing base, startup cap and synthetic-ID exhaustion retain pending demand.
- [ ] DB outage during enqueue leaves loot original; retry limitation matches README.
- [ ] Interrupt startup transaction: no partial item/mapping/deletion; request remains on rollback.
