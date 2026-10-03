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
target policy, seven-field variant identity, repeated request-key deduplication, identity snapshots
(including signed fields/range checks), and application of identity without changing scaled stats.

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
bootstrap mode; MySQL initializes a temporary datadir and uses a private Unix socket (or a unique Windows named pipe) with both
network listeners disabled. No existing database, credentials, or application stack is used.
Both modes enforce strict SQL for metadata rejection and actually restart the database to check persistence.
This extends the permanent existing SQL regression, not a live-stack/e2e test.

When the environment prohibits sockets, replace `--mysql ...` with `--mysql-initialize-only`.
That limited MySQL mode executes both installation routes, schema assertions, request dedupe,
materialization and every module SELECT during temporary database initialization. It does **not**
cover restart, NULL rejection or failed-transaction rollback; the full suite requires one of the
normal modes above. The temporary database is always removed.

Coverage includes:

- Identical CREATE definitions in the base and first-install updater; guarded legacy column upgrades use MySQL syntax.
- First module installation onto ordinary world data and fresh assembly followed by updater SQL.
- Empty first-start tables, InnoDB engines and seven-column unique identities.
- Different RequiredLevel, FormulaVersion and generator revision remain distinct; duplicate requests collapse.
- Complete metadata: SQL widths/signs match core, and all eight snapshot/provenance fields reject NULL.
- Actual C++ template/mapping inserts and completed-request deletion in one transaction.
- Every module SELECT call site, using its C++ SQL text and explicit fixture parameter bindings.
- Exact-key request deletion leaves unrelated pending requests untouched.
- A forced mapping failure rolls back the inserted template and retains the pending request.
- A missing source row cannot insert a mapping or consume pending demand.
- A second DB invocation preserves committed IDs, values, metadata and pending requests.
- A missing committed template keeps its mapping and allocator reservation.
- Source checks for validation-only initialization, request suppression of committed keys, immutable snapshot
  comparisons, current-family checks before generation and a non-blocking loot miss path. These checks do
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
- [ ] Live mode 1: first dungeon entry prepares eligible exact keys; the first unseen drop awards a scaled entry without another run or restart.
- [ ] Live mode 2: the first rolled unseen drop is held, then awards a durable scaled entry.
- [ ] Repeated identical drops and concurrent map workers share one request and ID.
- [ ] Live disabled: first unseen drop remains original and creates one legacy demand key.
- [ ] Simultaneous identical map-worker/player/bot misses: one logical request.
- [ ] Restart promotes complete staged snapshots with the same IDs and values; legacy completed demand disappears.
- [ ] Matching post-restart drop selects the exact generated entry.
- [ ] Same base at two target levels; distinct RequiredLevels stay separate.
- [ ] ScaleUp, ScaleDown, dynamic and fixed modes; BracketStep and MaxLevel boundaries.
- [ ] Quality/item/map/level exclusions and real-player versus bot-only groups.
- [ ] Chest loot and ordinary creature/boss loot.
- [ ] Quest-required loot, quest starters, MaxCount, unique-equipped and disabled items stay original.
- [ ] Category limits, conditional loot, free-for-all, follow-loot-rules and faction checks remain correct.
- [ ] Random mode 0 leaves random gear original; mode 1 bakes rolled names and supported bonuses and clears template/instance random fields. Unsupported effects keep all original bonuses.
- [ ] Group loot; Playerbot loot; Playerbot Need/Greed.
- [ ] Playerbot autogear, stat valuation and equip behavior.
- [ ] Trade, mail, auction, guild bank, reconnect and tooltip/icon cache.
- [ ] Missing committed template: log and withhold; keep its key and ID reserved without reconstruction.
- [ ] Mismatched committed identity/levels/loot metadata: withhold without changing database rows.
- [ ] Inactive generation family: issued template still loads; no regeneration or new selection.
- [ ] FormulaVersion/generator/PreserveNonZeroStats change retires stale pending requests.
- [ ] Known family conflict requires FormulaVersion bump without rewriting committed items.
- [ ] Missing base, startup cap and synthetic-ID exhaustion retain pending demand.
- [ ] SQL failure, timeout, queue saturation and slot exhaustion release original loot with diagnostics.
- [ ] Deferred sources cancel on despawn/reset and cannot rewrite active rolls or a new loot generation.
- [ ] Chest locks, keys and skills remain enforced; pending contents are generated once.
- [ ] SQL promotion collision retains staged snapshots; recovery fails before player login.
- [ ] Disable scaling after awarding live gear; next startup still recovers it.
- [ ] Clean and existing client caches: loot, bag, equipped, inspection and chat-link stats match server bonuses.
- [ ] Multiple map workers and concurrent item queries observe complete templates with stable pointers.
- [ ] Interrupt startup transaction: no partial item/mapping/deletion; request remains on rollback.

## Source contracts without compilation

```powershell
python tests/check_source.py --core '..\..\Azerothcore server\azerothcore-wotlk'
```

Audits native tooltip field coverage, the publication barrier, the const packet hook,
asynchronous gameplay paths, configuration defaults and duplicate keys, and MySQL syntax guards.
These are source checks, not compiled or runtime tests.

`test_snapshot.cpp` covers serialization of signed values, populated and unused stat slots,
spell/sockets metadata, damage precision, names/descriptions with quotes and newlines, and UTF-8
hex literals. The identity regression also covers baked random-field restart validation.
Both are picked up by `check_compile.py` when compilation is explicitly requested.

## Verification for the first-run implementation (2026-10-04)

- Oracle MySQL Community Server 8.4.11: full disposable SQL suite passed using a temporary
  portable distribution and a private named pipe; no service was installed. Includes a real
  database restart, all module SELECT sites, staging persistence, promotion after base changes,
  duplicate-key/collision rollback, repeat promotion and strict NULL rejection.
- Core C++ codestyle, whitespace and source-contract checks: passed.
- C++ compilation and C++ regression execution: **not run**, at the user's request.
- Worldserver installation, restart and live database migration: **not performed**.
- Client rendering, first-run loot, chest/group/Playerbot behavior and runtime concurrency:
  **pending**. See [MILS-004](../docs/ISSUES.md#mils-004); do not close it on SQL evidence alone.

Earlier MariaDB 10.11.8 disposable results are separate from the Oracle MySQL verification.
The active realm's engine/version was not queried. No compatibility with other MySQL versions
is claimed by the 8.4.11 run.

Maintainer skill validation: the original file has a pre-existing UTF-8 BOM rejected by the validator. A temporary BOM-free copy passed; the original skill was left unchanged.

Final cleanup: the test server was stopped, but automatic tool approval rejected removal of
the portable test distribution and a prior disposable test leftover with "blocked by policy".
They remain under the temporary directory; nothing was installed as a service or deployed.
