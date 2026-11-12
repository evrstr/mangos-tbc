# CMaNGOS -- History of visible changes / 重要可见变更历史

This file is part of the CMaNGOS Project. See [Copyright](COPYRIGHT.md) file for Copyright information

本文件是 CMaNGOS 项目的一部分。版权信息请参阅 [Copyright](COPYRIGHT.md) 文件。

Most of our code is released under the GPL v2, a copy is shipped with the file [LICENSE](LICENSE).

我们的绝大多数代码基于 GPL v2 许可证发布，完整副本见 [LICENSE](LICENSE) 文件。

Visit our project website for forums, links and more:
http://cmangos.net

访问项目网站获取论坛、相关链接及更多信息：
http://cmangos.net

Please submit bug reports at our bugtracker at:
https://github.com/cmangos/issues

请在我们的缺陷追踪系统提交 Bug 报告：
https://github.com/cmangos/issues

See the file ChangeLog.md for details!

详细信息请参阅 `ChangeLog.md` 文件！

## Version 0.12
 * Migrate from SVN to GIT.
 * Implemented arena system.
 * Start using Adaptive Communication Environment (ACE) framework v.5.6.6 in server network code.
 * Update Visual Leak Detector lib to Version 1.9g (beta).
 * Upgrade to client version 2.4.3 (build 8606).

### 版本 0.12（中文摘要）
 * 代码库从 SVN 迁移至 Git。
 * 实现竞技场系统。
 * 服务端网络代码开始采用 Adaptive Communication Environment（ACE）框架 v5.6.6。
 * Visual Leak Detector 库更新至 1.9g (beta)。
 * 升级支持客户端版本 2.4.3（build 8606）。

## Version 0.11
 * Lots of improvements in the spell system.
 * Now use Mersenne Twister random number generator.
 * MySQL client libs updated to 5.0.56.
 * Upgrade to client version 2.4.2 (build 8278).

### 版本 0.11（中文摘要）
 * 法术系统迎来大量改进。
 * 改用梅森旋转（Mersenne Twister）随机数生成器。
 * MySQL 客户端库更新至 5.0.56。
 * 升级支持客户端版本 2.4.2（build 8278）。

## Version 0.10
 * master looter lot mode,
 * FFA PvP zones and FFA PvP server type support,
 * Guild Banks,
 * unique equipped items support, including gems,
 * talent inspecting,
 * PostgreSql support,
 * sockets library updated to v.2.2.9 version,
 * 64-bit binaries building at Windows,
 * Upgrade to client version 2.3.0 (build 7561).

### 版本 0.10（中文摘要）
 * 队长分配（Master Looter）拾取模式；
 * 自由混战（FFA）PvP 区域及服务器类型支持；
 * 公会银行；
 * 唯一装备（Unique-equipped）物品支持（含宝石）；
 * 天赋查看功能；
 * PostgreSQL 数据库支持；
 * sockets 网络库更新至 v2.2.9；
 * 支持在 Windows 下编译 64 位二进制程序；
 * 升级支持客户端版本 2.3.0（build 7561）。

## Version 0.9
 * recipes discovery system support,
 * more auras, effects, spells, and talents working,
 * players queue at login support,
 * mangosd/realmd as Windows services support,
 * autogeneration mangosd/realmd crash reports (Windows only),
 * Visual Studio 2008 Express and Pro support,
 * division Mangos DataBase to MangosDB(WorldDB) and CharactersDB.
 * Upgrade to client version 2.2.3 (build 7359).

### 版本 0.9（中文摘要）
 * 支持配方领悟系统；
 * 更多光环、效果、法术和天赋生效；
 * 支持登录排队机制；
 * 支持将 `mangosd`/`realmd` 作为 Windows 服务运行；
 * 自动生成 `mangosd`/`realmd` 崩溃报告（仅限 Windows）；
 * 支持 Visual Studio 2008 Express 和 Pro；
 * 将 MaNGOS 数据库拆分为 MangosDB（WorldDB）和 CharactersDB；
 * 升级支持客户端版本 2.2.3（build 7359）。

## Version 0.8
 * battleground support,
 * gender specific creatures,
 * game objects triggering more scripts,
 * more auras, effects, spells, and talents working,
 * dungeon specific data,
 * localization support for data,
 * vmaps for line of sight handling,
 * build support for OpenBSD and FreeBSD,
 * Upgrade to client version 2.1.3 (build 6898).

### 版本 0.8（中文摘要）
 * 战场支持；
 * 区分性别的生物模型；
 * 游戏对象可触发更多脚本；
 * 更多光环、效果、法术和天赋生效；
 * 地下城专属数据；
 * 数据本地化支持；
 * 引入 vmaps 用于视线（LOS）判定；
 * 支持 OpenBSD 和 FreeBSD 编译；
 * 升级支持客户端版本 2.1.3（build 6898）。

## Version 0.7, Codename "Eye of the Storm"
 * Full rewrite of the GNU autotools based build system, in order to
   smooth and clean up the build process.
 * Full rewrite of the Windows build system, allowing MaNGOS to be built
   from Visual C++ 2003 Toolkit, Visual C++ 2003, Visual C++ 2005.
 * Upgrade to client version 2.0.12 (build 6546).

### 版本 0.7，代号“风暴之眼（Eye of the Storm）”（中文摘要）
 * 全面重写基于 GNU autotools 的构建系统，使编译流程更加顺畅整洁。
 * 全面重写 Windows 构建系统，支持通过 Visual C++ 2003 Toolkit、Visual C++ 2003 及 Visual C++ 2005 编译 MaNGOS。
 * 升级支持客户端版本 2.0.12（build 6546）。

## Version 0.6, Codename "Black Dragonflight"
 * MaNGOS development moved to http://sourceforge.net/projects/mangos/
 * A lots of speed improvements to data handling and data transfer have been
   completed.
 * Mail, auction house, profession, creature, and game object systems have seen
   a lot of improvements and can be considered feature complete. Some minor bugs
   still are left.
 * Rest system has been finished.
 * Initial pet stable support has been added.
 * PvP and duel system have seen lots of improvements
 * Guilds are not just a myth anymore. Coming closer to full guild support.
 * Taxi and transport system is close to a fully working system.
 * A lot more has been added.

### 版本 0.6，代号“黑龙军团（Black Dragonflight）”（中文摘要）
 * MaNGOS 开发迁移至 http://sourceforge.net/projects/mangos/。
 * 数据处理与传输速度获得大幅提升。
 * 邮件、拍卖行、专业技能、生物与游戏对象系统得到大量改进，功能已基本完备（仍存少量小问题）。
 * 休息（双倍经验）系统完工。
 * 新增初步的宠物兽栏支持。
 * PvP 与决斗系统迎来大量改进。
 * 公会功能不再只是设想，正逐步接近完整支持。
 * 飞行路线（Taxi）与交通工具（船/飞艇）系统接近完全可用。
 * 另有大量其他新增功能。

## Version 0.5, Codename "Stable Master"
 * Core stability improvements, lots of threading and memory usage related
   bugs have been resolved.
 * MySQL Database backend has been cleaned up.
 * Proper support for game clients of version 1.10.2 has been added.
 * Support for auras, pets, spells, talents, totems, etc. has been added
   or improved.
 * Cross-platform issues have been resolved, MaNGOS should now build on
   more platforms, including FreeBSD.

### 版本 0.5，代号“兽栏管理员（Stable Master）”（中文摘要）
 * 核心稳定性提升，修复了大量线程与内存使用相关的 Bug。
 * 清理优化了 MySQL 数据库后端。
 * 正式支持 1.10.2 版本游戏客户端。
 * 新增或改进了光环、宠物、法术、天赋、图腾等功能支持。
 * 解决了跨平台兼容性问题，MaNGOS 现可在包括 FreeBSD 在内的更多平台上编译。

## Version 0.1, Codename "Lightbringer"
 * AI system
 * Grid system
 * Lots a game features working.
 * Lovely optimizations for the beloved server.

### 版本 0.1，代号“光明使者（Lightbringer）”（中文摘要）
 * AI 系统；
 * 网格（Grid）系统；
 * 大量游戏特性开始正常工作；
 * 针对服务器进行了多项优化。

## Version 0.0.3, Codename "Mango Carpet"
 * Interim release, not gone public.

### 版本 0.0.3，代号“Mango Carpet”（中文摘要）
 * 内部过渡版本，未公开发布。

## Version 0.0.2, Codename "Library"
 * Most data conversions working.
 * Database cleanup.
 * More features working.
 * Many bugs fixed.

### 版本 0.0.2，代号“图书馆（Library）”（中文摘要）
 * 大部分数据转换已可正常工作；
 * 数据库结构清理；
 * 更多功能可用；
 * 修复了大量 Bug。

## Version 0.0.1, Codename "Endeavour"
 * Data converter added.
 * Build system improvements.
 * Bug fixing and code cleaning.

### 版本 0.0.1，代号“奋进号（Endeavour）”（中文摘要）
 * 新增数据转换工具；
 * 构建系统改进；
 * 修复 Bug 与代码清理。
