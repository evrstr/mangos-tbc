# AGENTS.md

Guidelines and hard boundaries for developing `mangos-tbc` (CMaNGOS TBC 2.4.3 + `PlayerBots` module) in this workspace.
（本工作区 `mangos-tbc`（CMaNGOS TBC 2.4.3 + `PlayerBots` 模块）的开发规范与硬性边界说明。）

## 1. Repository & Workspace Layout / 仓库与工作区布局

- **Core repo / 核心仓库**: `D:\Games\wowsf\mangos-tbc` (fork of `cmangos/mangos-tbc`, branch `master` / 上游 `cmangos/mangos-tbc` 的 `master` 分支).
- **PlayerBots module / PlayerBots 模块**: `src/modules/PlayerBots/` (cloned by CMake `FetchContent` from `cmangos/playerbots` / 由 CMake `FetchContent` 自动拉取).
  - `src/modules/` is listed in root `.gitignore`; `src/modules/PlayerBots/` is a **separate git repository**. Track and commit core changes vs. `PlayerBots` changes in their respective repositories.
  - （根目录 `.gitignore` 已忽略 `src/modules/`；`src/modules/PlayerBots/` 为独立 Git 仓库，核心改动与 PlayerBots 改动须分别提交。）
- **External build artifacts / 外部构建产物** (outside repo / 均位于仓库外):
  - Boost 1.87.0 (MinGW static / 静态库): `D:\Games\wowsf\deps\boost`
  - CMake build directory / 构建目录: `D:\Games\wowsf\build`
  - Output binaries & runtime DLLs / 可执行文件与运行时 DLL: `D:\Games\wowsf\build\bin\x64_Release\` (`mangosd.exe`, `realmd.exe`, `*.conf.dist`)

## 2. Build & Verification (MinGW GCC + Ninja, No MSVC Required) / 编译与验证

### Incremental rebuild (after editing C++/CMake) / 增量重编
```bash
PATH="/d/Software/mingw64/bin:$PATH" cmake --build "D:/Games/wowsf/build" --parallel
```

### Clean CMake configure (if adding files or changing options) / 重新配置 CMake
```bash
HTTP_PROXY="http://127.0.0.1:10808" HTTPS_PROXY="http://127.0.0.1:10808" \
BOOST_ROOT="D:/Games/wowsf/deps/boost" \
PATH="/d/Software/mingw64/bin:$PATH" \
cmake -G Ninja -S "D:/Games/wowsf/mangos-tbc" -B "D:/Games/wowsf/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="D:/Games/wowsf/install" \
  -DBUILD_PLAYERBOTS=ON
```

### Build rules / 构建约束
- **Always rebuild cleanly with zero new warnings** before concluding a code change.（每次代码修改收尾前必须重新编译通过，且不引入新的编译警告。）
- **Keep `GenerateBotTests` disabled in `src/modules/PlayerBots/playerbot/PlayerbotAIAware.h`** unless explicitly building test-generation tooling across **all** targets (`game` + `playerbots`). Enabling it only inside `PlayerbotAIAware.h` causes an ODR layout mismatch on `PlayerbotHolder` / `RandomPlayerbotMgr::instance()` between `libgame.a` and `libplayerbots.a`.
  （除非全目标统一启用测试生成工具，否则必须保持注释掉 `PlayerbotAIAware.h` 中的 `#define GenerateBotTests`，避免 `libgame.a` 与 `libplayerbots.a` 出现 ODR 内存布局不一致。）
- Whenever pulling upstream `cmangos/playerbots`, immediately verify `PlayerbotAIAware.h` does not re-enable `#define GenerateBotTests`.（拉取上游 `playerbots` 更新后，立即检查该宏是否被重新打开。）

## 3. Architecture & Thread-Safety Boundaries / 架构与线程安全边界

### Process & database split / 进程与数据库划分
- `src/realmd/`: Login/SRP6 auth server (`LoginDatabase`) / 登录与 SRP6 认证服。
- `src/mangosd/`: World server host (`WorldRunnable`, CLI/RA/SOAP threads, Boost.Asio network threads) / 世界服主程序与线程宿主。
- `src/game/`: Core simulation (`World`, `Maps`, `Grids`, `Entities`, `Spells`, `Combat`, `AI`, `MotionGenerators`, `Server`) / 核心游戏逻辑。
- `src/shared/` & `src/framework/`: Threading, DB pools, auth crypto, network, utilities / 线程、连接池、加密与基础框架。

### Hard threading rules in `mangosd` / `mangosd` 线程铁律
1. **Network IO threads / 网络 IO 线程** (`WorldSocket` via Boost.Asio): Only decode/enqueue packets into `WorldSession::m_recvQueue` (or handle `PROCESS_IMMEDIATE` opcodes). Never mutate `Map`, `Grid`, or `Unit` state directly.（仅解包入队或处理 `PROCESS_IMMEDIATE` 包，严禁直接修改 `Map`、`Grid` 或 `Unit` 状态。）
2. **World main thread / 世界主线程** (`WorldRunnable` -> `World::Update`):
   - Runs global timers, `sRandomPlayerbotMgr`, `World::UpdateSessions` (`PROCESS_THREADUNSAFE` opcodes: login/logout, groups, guilds, chat, trade), `sMapMgr.Update`, `UpdateResultQueue` (async SQL callbacks), and `RemoveAllObjectsInRemoveList`.（负责全局定时器、随机 Bot 调度、线程不安全封包、驱动地图更新、异步 SQL 回调与延迟删除。）
3. **Map worker threads / 地图工作线程** (`MapUpdater` -> `Map::Update`):
   - Each `Map` instance updates its own `WorldSession::UpdateMap` (`PROCESS_MAP_THREAD` / `PROCESS_THREADSAFE`), `Player::Update`, `Player::UpdateAI` (`PlayerbotAI`), and grid `Creature`/`GameObject`/`Pet` updates.
   - **Never** mutate another `Map` or global cross-map state directly from inside `Map::Update`. Use `GetMessager().AddMessage(...)` on `World`, `Map`, or `WorldSession` to marshal cross-thread actions safely.（严禁在 `Map::Update` 中直接修改跨地图全局状态；跨线程操作统一通过 `GetMessager().AddMessage(...)` 投递。）
4. **Async DB threads / 异步数据库线程** (`SqlDelayThread`):
   - Do not hold raw entity pointers across async SQL callbacks; pass `ObjectGuid` and re-resolve inside the callback (which executes on the world thread via `UpdateResultQueue`).（异步 SQL 回调禁止捕获实体裸指针，只传 `ObjectGuid` 并在主线程回调内重新解析。）

### PlayerBots integration boundaries / PlayerBots 集成边界
- Core hooks must remain guarded by `#ifdef ENABLE_PLAYERBOTS` so `-DBUILD_PLAYERBOTS=OFF` still compiles cleanly.（核心代码中的 Bot 钩子必须保持 `#ifdef ENABLE_PLAYERBOTS` 隔离，保证关闭开关仍可编译。）
- Bot AI follows `Trigger -> Strategy -> Action / Multiplier / Value` under `src/modules/PlayerBots/playerbot/strategy/` with separate `NON_COMBAT`, `COMBAT`, `DEAD` engines plus `ReactionEngine`.（Bot AI 遵循 `Trigger -> Strategy -> Action / Multiplier / Value` 架构，分 `NON_COMBAT`、`COMBAT`、`DEAD` 与 `ReactionEngine`。）
- Remember `Map.cpp` (`disableBotOptimizations = false` by default) throttles bot & nearby-grid updates in zones with no real players. Check `aiplayerbot.conf` when debugging bot behavior in empty zones.（注意 `Map.cpp` 默认会在无真人区域对 Bot 与附近网格降频；调试空区 Bot 时需检查 `aiplayerbot.conf` 中的 `disableBotOptimizations`。）

## 4. Database & SQL Rules / 数据库与 SQL 规范

- **Test MySQL Environment / 测试 MySQL 环境**:
  - Host: `192.168.100.115`
  - Port: `3306`
  - User: `root`
  - Password: `123456`
  - Target DBs: `mangos` (world), `characters`, `realmd`, `logs`
- **SQL Fixes & Knowledge Base (`dev/`) / 数据库修复脚本与知识库**:
  - `dev/`: 包含 83 个通用 Bug 修复脚本（任务、刷怪组、中文补全、门控、法术等），可通过 `dev/run_dev_sql.bat` / `dev/run_dev_sql.ps1` 批量幂等执行。
  - `dev/custom_tuning/`: 包含 43 个私服定制/魔改脚本（AHBot 做市商、掉率倍率提升、集合石等级放宽等），已隔离存放，默认不参与通用修复批量执行。
  - `dev/KNOWN_ISSUES.md`: 核心问题总账与根因文档，记录所有修复证据与测试口径。
- **Supported DB / 支持的数据库**: MySQL 8 / MariaDB only. Do **not** target PostgreSQL (`DatabasePostgre` is unmaintained/incomplete and core SQL uses MySQL syntax such as `REPLACE INTO`, `AUTO_INCREMENT`, and backticks).（仅支持 MySQL 8 / MariaDB；禁止基于已废弃的 `DatabasePostgre` 开发。）
- Any schema or data change must be placed in numbered/structured SQL files under `sql/updates/` (core) or `src/modules/PlayerBots/sql/` (PlayerBots) — never silently assume manual DB edits.（所有表结构与数据改动必须写入对应 `sql/updates/` 或 `src/modules/PlayerBots/sql/` 迁移文件。）
- Never hardcode database credentials or personal machine paths in committed source/config templates (`.conf.dist.in`).（严禁在提交的源码或配置模板中硬编码数据库密码或个人本机路径。）

## 5. Code Style & Editing Rules / 代码风格与编辑规范

Follow `.clang-format` and surrounding CMaNGOS style（遵循 `.clang-format` 与周边代码风格）:
- **Standard / 语言标准**: C++20 (`CMAKE_CXX_STANDARD 20`).
- **Formatting / 排版**: 4 spaces, no tabs, LF line endings (`UseCRLF: false`), Allman braces (opening `{` on new line for namespaces, classes, functions, and control statements), left-aligned pointers/references (`Player* player`, `Unit const& target`).（4 空格无 Tab、LF 换行、Allman 大括号、指针/引用左对齐。）
- **Includes / 头文件**: Do **not** reorder existing `#include` blocks (`SortIncludes: Never`); missing standard headers under GCC 15 (e.g. `<cstdint>`) should be added minimally where needed.（禁止重排现有 `#include` 顺序。）
- **Minimal diffs / 最小改动**: Match the idioms of the subsystem you touch (`src/game` vs `src/modules/PlayerBots`, which uses `#pragma once` and slightly different naming). Avoid unrelated refactors in large core files (`Player.cpp`, `SpellEffects.cpp`, `PlayerbotAI.cpp`).（贴合所属子系统习惯，避免在超大核心文件中做无关重构。）
- **Comments / 注释规范**: Keep original English technical/Doxygen comments intact and maintain bilingual Chinese annotations for newly added or modified modules/logic.（保留原有英文技术注释，新增或修改逻辑同步补充中文注释。）
- **Git hygiene / Git 规范**: Never commit `bin/`, `build/`, `*.conf` (only `.conf.dist.in` templates), or extracted client assets (`dbc/`, `maps/`, `vmaps/`, `mmaps/`).（严禁提交构建目录、实际 `.conf` 或客户端提取数据。）
