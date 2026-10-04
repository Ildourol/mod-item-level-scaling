# Unified Single-Server Runtime Architecture & Build Pipeline Alignment Plan

## 1. Executive Summary & Problem Definition

The AzerothCore server repository currently suffers from **directory duplication and binary drift**:
1. **Split Directory Structure**:
   - The runtime files (30+ Windows DLLs, maps/vmaps/mmaps junctions, active configs, and Eluna scripts) are located in `Server/bin/`.
   - The launch script [`open.bat`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20server/open.bat) starts executables exclusively from `Server/bin/worldserver.exe` and `Server/bin/authserver.exe`.
   - However, CMake's `CMAKE_INSTALL_PREFIX` was set to `Server/` (the parent folder).
2. **Consequences of the Split**:
   - Every time CMake builds and runs `INSTALL`, it deploys newly compiled executables (`worldserver.exe`, `authserver.exe`) to `Server/`, leaving `Server/bin/` untouched with stale binaries.
   - This caused silent **binary drift** (Issue `[CORE-005]`), where code fixes compiled successfully but the running game server continued executing the old buggy binary.
   - It also generated duplicate configuration directories (`Server/configs` vs `Server/bin/configs`), duplicate `.conf.dist` files, and stray log files in the root of `Server/`.

---

## 2. Target Unified Architecture

We will unify the server so that **`Server/bin/` is the single, authoritative server environment**, and CMake directly deploys all compiled binaries and configs into `Server/bin/`.

```text
Azerothcore server/
├── azerothcore-wotlk/               # Source repository & CMake build directory
│   └── build/                       # CMake build tree (MSBuild / Ninja)
├── mini sql/                        # Portable MariaDB database engine
├── Server/                          # Top-level asset & runtime parent
│   ├── AIO_Client/                  # Client addon distribution
│   ├── data/                        # Core DBC, maps, mmaps, vmaps assets
│   ├── sql/                         # Base world and character SQL updates
│   └── bin/                         # <<< THE ONLY RUNTIME SERVER DIRECTORY >>>
│       ├── worldserver.exe          # Live worldserver (automatically overwritten on build)
│       ├── authserver.exe           # Live authserver (automatically overwritten on build)
│       ├── *.dll                    # All 30+ runtime dependencies
│       ├── configs/                 # Authoritative configs (worldserver.conf, authserver.conf)
│       │   └── modules/             # Authoritative module configs (mod_*.conf)
│       ├── lua_scripts/             # Live Eluna Lua scripts
│       └── *.log                    # Live server logs (Server.log, Errors.log, etc.)
├── close.bat                        # Clean multi-process shutdown script
├── open.bat                         # Unified one-click startup script
└── start_all_bridges.bat            # Dual-bridge AI orchestrator launcher
```

---

## 3. Phased Implementation Steps

### Phase 1: Pre-Migration Health Check & Process Termination
1. Execute `close.bat` to ensure `worldserver.exe`, `authserver.exe`, `mariadbd.exe`, and any background bridges are fully stopped.
2. Confirm with PowerShell that no processes hold file locks on executables in `Server/` or `Server/bin/`.

### Phase 2: Clean Up Duplicated and Obsolete Files in `Server/` Root
1. **Remove Duplicate Binaries in `Server/` Root**:
   - Delete `Server/worldserver.exe`, `Server/worldserver.pdb`.
   - Delete `Server/authserver.exe`.
   - Delete `Server/lua52.lib`, `Server/lua52_compiler.exe`, `Server/lua52_compiler.pdb`, `Server/lua52_interpreter.exe`, `Server/lua52_interpreter.pdb`.
2. **Remove Stray Log Files in `Server/` Root**:
   - Delete `Server/Server.log`, `Server/Auth.log`, `Server/Errors.log`, `Server/Playerbots.log` (all active logs are generated in `Server/bin/`).
3. **Remove Stray Root Configuration**:
   - Delete `Server/mod_low_level_arena.conf` (the active config is in `Server/bin/configs/modules/mod_low_level_arena.conf`).
4. **Clean Duplicate Config Folder**:
   - Remove `Server/configs` directory (which was only created by CMake install), leaving `Server/bin/configs` as the sole authoritative configuration directory.

### Phase 3: Update CMake Install Prefix to `Server/bin`
1. Update `CMAKE_INSTALL_PREFIX` in `azerothcore-wotlk/build/CMakeCache.txt`:
   ```cmake
   CMAKE_INSTALL_PREFIX:PATH=C:/Users/Admin/AntigravityProfiles/Projects Azerothcore/Azerothcore server/Server/bin
   ```
2. Re-run CMake configuration generation:
   ```cmd
   cmake -B "C:\Users\Admin\AntigravityProfiles\Projects Azerothcore\Azerothcore server\azerothcore-wotlk\build" -S "C:\Users\Admin\AntigravityProfiles\Projects Azerothcore\Azerothcore server\azerothcore-wotlk"
   ```
3. **Result**: When you or any script runs `--target INSTALL`, CMake will automatically copy `worldserver.exe` and `authserver.exe` directly into `Server/bin/`, instantly overwriting the active binaries. No manual copy command will ever be needed again.

### Phase 4: Validate Scripts & Launch Pipeline
1. Inspect [`open.bat`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20server/open.bat):
   - Confirm it starts MySQL, Auth Server in `%~dp0Server\bin`, and World Server in `%~dp0Server\bin`.
2. Inspect [`close.bat`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20server/close.bat):
   - Confirm clean taskkill and database shutdown.
3. Inspect [`start_all_bridges.bat`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20server/start_all_bridges.bat):
   - Confirm proper execution with no unquoted syntax issues.

### Phase 5: Update Agent Operating Guidelines (`AGENTS.md`)
1. Add an explicit invariant to `AGENTS.md` across all modules:
   - **Single Server Authority**: All runtime configuration edits, log inspections, and executable deployments must strictly target `Azerothcore server/Server/bin/`. Never inspect or edit root `Server/` for configs or binaries.
2. Update `.agents/skills/item-level-scaling-maintainer` and related skills to reflect the single server path.

---

## 4. Verification & Validation Checklist

- [ ] `Server/` root is completely clean of duplicate `.exe`, `.pdb`, `.lib`, `.log`, and `.conf` files.
- [ ] `build/CMakeCache.txt` has `CMAKE_INSTALL_PREFIX` pointing to `Server/bin`.
- [ ] Running `--target INSTALL` places the new `worldserver.exe` directly into `Server/bin/worldserver.exe` with a matching timestamp.
- [ ] Server launches cleanly via `open.bat` and all 30+ DLLs, configs, and junctions resolve immediately.
- [ ] Clean shutdown verified via `close.bat`.
