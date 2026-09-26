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

The optional library path supports extracted dependencies. MariaDB bootstrap uses a disposable
local datadir, strict SQL mode, no network listener, no credentials, and no application stack.
This extends the permanent existing SQL regression, not a live-stack/e2e test.

Coverage includes:

- Fresh pending/committed schema and legacy upgrades without changing old IDs, keys, or issued stats.
- Repeatable migrations and six-column unique identities, including different RequiredLevel,
  FormulaVersion, and generator revision; repeated pending requests collapse to one row.
- Identity snapshot persistence and SQL types/signs matching the canonical item schema.
- The actual C++ template/mapping SQL and completed-request deletion in one transaction.
- Mapping failure rolls back the inserted template and retains the pending request.
- A missing source row cannot insert a mapping or consume pending demand.
- Current missing-template projection and restoration under the original ID.
- Historical missing-template projection and a source guard preventing current-code regeneration.
  This last check is static/SQL coverage, not execution of worldserver's recovery control flow.

The core SQL checker is designed for core directories. Its content checks apply to the new migration;
it rejects changes in any `base` directory by policy. This module's explicitly requested fresh schema
belongs in its own `sql/world/base`, so that directory-policy rejection is expected and must be
reported separately from SQL execution results. Never move the schema into core to satisfy this rule.

## Manual/live checklist (not executed by these checks)

Use an isolated realm with the canonical Playerbot core and mod-playerbots branches.

- [ ] Clean install: both schemas validate; no demand means no new variants.
- [ ] Upgrade: existing inventory/equipment IDs and scaled values remain unchanged.
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
- [ ] Legacy identity repair: withheld on repair startup, selected after clean restart; cache refreshes.
- [ ] Missing current template with known snapshot/config recovers under the same ID.
- [ ] Legacy unknown provenance defers recovery; historical generator is never regenerated.
- [ ] FormulaVersion/generator/PreserveNonZeroStats change retires stale pending requests.
- [ ] Known family conflict requires FormulaVersion bump without rewriting committed items.
- [ ] Missing base, startup cap and synthetic-ID exhaustion retain pending demand.
- [ ] DB outage during enqueue leaves loot original; retry limitation matches README.
- [ ] Interrupt startup transaction: no partial item/mapping/deletion; request remains on rollback.
