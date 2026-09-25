# Module regression checks

These checks do not edit or build the core checkout and do not start worldserver.

## Compiler and C++ checks

Requirements: GCC with GNU C++20, binutils, Python 3, a core checkout containing `src` and vendored
`deps`, and the core's development headers (including Boost and MySQL).

```bash
python3 tests/check_compile.py --core /path/to/azerothcore-wotlk
```

For development headers extracted outside system paths, add `--headers /path/to/extracted/usr/include`.
The script compiles every module source with `-Wall -Wextra -Werror`, combines the objects to catch
module symbol problems, checks for unresolved module symbols, and runs the C++ regressions. Core
symbols remain unresolved intentionally: this is not a linked worldserver build.

The executable regressions cover invalid and extreme level configuration, all supported bracket
widths and levels, maximum-level preservation, ID allocation beyond existing templates and mappings,
32-bit overflow avoidance, distinct required-level/formula-version/generator-revision identities,
runtime publication preserving persisted scaled fields while restoring validated base metadata, and
target-resolution semantics for fixed/dynamic mode, dungeon hierarchy, external creature scaling,
RealPlayersOnly, and level bounds.

## Isolated SQL checks

Requirements: Python 3, MariaDB 10.11 `mariadbd`, and a core checkout containing the world base schema.
No client library, listener, existing database, or credentials are needed.

```bash
python3 tests/check_sql.py \
  --core /path/to/azerothcore-wotlk \
  --mariadbd /usr/sbin/mariadbd
```

For extracted runtime libraries, add `--library-path /path/to/extracted/usr/lib/x86_64-linux-gnu`.
The script creates a disposable datadir, executes SQL through MariaDB's bootstrap input, then removes
that datadir. It executes the module-owned migration SQL and uses the core's table definitions.
Assertions cover:

- Module-owned V1-to-V2 schema migration plus generator-revision migration without renumbering or changing issued items.
- Malformed named-index repair, repeat application, and the six-column variant identity key.
- Fresh-install schema matching the migrated key layout.
- Persisted block, all resistances, item level, equip requirement, and cloned metadata.
- Multiple equip requirements for the same base/target/formula.
- Creature spawn alternatives, difficulty templates, and chest loot root discovery.
- The recovery projection and restoration of a missing variant under its original ID.
- Transaction rollback after a duplicate mapping, with no orphan template left behind.

## Validation scope

The change was checked against Playerbot commit `7f12e89ee5f467a50e62eba1d525eac7dc953d03` and stock
master commit `b6c033cb009d4e137af70b60151d23bcbeb4b2b8`. MariaDB tests use strict SQL mode.

The upstream C++ codestyle checker applies to this module. The upstream SQL checker rejects edits to
any `base` directory by design; this module owns its installation schema in `sql/world/base`, so that
core-directory policy is not an indication of invalid module SQL. SQL behavior is tested above.

These checks do not exercise live worldserver startup, client tooltips, Playerbot equipment selection,
or other installed modules. Test dungeon/raid loot, a bot-only instance, trade/mail, and persistence
across restart in a staging server before deploying to production.
