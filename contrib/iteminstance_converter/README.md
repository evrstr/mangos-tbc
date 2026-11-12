# Item Instance data converter / 物品实例数据转换器
Tool to convert the datablob in the item_instance table to extracted format.
https://github.com/henhouse/ItemInstanceConverter

用于将 `item_instance` 表中的二进制数据块（datablob）转换为拆分字段格式的工具。
https://github.com/henhouse/ItemInstanceConverter

## Instructions & Usage / 说明与用法

##### Warning: / 警告：
This script should be run **BEFORE** applying the character-db SQL update. This tool will extract
the current data inside the `item_instance` table and prepare it in the new format. It will output `INSERT`
statements for each item to be put back in after you apply the cmangos character-db update, which will `TRUNCATE`
the table.

BACKUP YOUR CHARACTER DATABASE!

该脚本必须在应用角色数据库（character-db）SQL 更新**之前**运行。本工具会提取 `item_instance` 表中的现有数据并转换为新格式，为每件物品生成 `INSERT` 语句，以便您在执行会 `TRUNCATE`（清空）该表的 CMaNGOS 角色库更新后将数据重新写回。

请务必提前备份您的角色数据库！

##### Usage: / 使用方法：
- Download and install the latest version of [Go from Google](https://golang.org/dl).
     - Win10 users with [Scoop](https://scoop.sh) can instead use: `scoop install go`
     - macOS users with [Homebrew](https://brew.sh) can instead use: `brew install go`
 - Run `run.sh` inside this folder a bash terminal (Linux/macOS) or Powershell (Windows) and follow the steps.

- 下载并安装最新版本的 [Go（Google 官方下载）](https://golang.org/dl)。
     - 使用 [Scoop](https://scoop.sh) 的 Windows 10 用户可直接运行：`scoop install go`
     - 使用 [Homebrew](https://brew.sh) 的 macOS 用户可直接运行：`brew install go`
- 在本目录下通过 bash 终端（Linux/macOS）或 PowerShell（Windows）运行 `run.sh`，并按提示步骤操作。

## Credit / 致谢
Inspired from: https://github.com/vmangos/ItemInstance

灵感源自：https://github.com/vmangos/ItemInstance
