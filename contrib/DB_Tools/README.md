# The DB_Tools subdirectory / DB_Tools 子目录说明
This directory is a place to store sql-scripts for database work and maintenance.

本目录用于存放数据库操作与维护相关的 SQL 脚本。

The core team will provide sql-files that will not change any content in any database, but will output data that is considered incorrect or obsolete.

核心团队提供的 SQL 文件不会修改任何数据库内容，而是用于检索并输出被视为错误或过时的数据。

We will use these subdirectories:
* WorldDB -- To output content we consider bad filled in the world-database (default: mangos)
* CharDB -- To output content we consider bad filled in the characters-database (default: characters)
* RealmDB -- To output content we consider bad filled in the realmd-database (default: realmd)

我们将使用以下子目录：
* `WorldDB` —— 用于输出世界数据库（默认：`mangos`）中存在异常填充的内容
* `CharDB` —— 用于输出角色数据库（默认：`characters`）中存在异常填充的内容
* `RealmDB` —— 用于输出账号/领域数据库（默认：`realmd`）中存在异常填充的内容

You can add additional Folders as you wish to store own sql-files for your own purposes.
These folders are also processable with the provided scripts.

您也可以根据需要自行添加其他文件夹来存放个人 SQL 脚本，所提供的辅助脚本同样可以处理这些文件夹。

You can use the sql-files just like any normal sql-files, however for easy use and batch-mode there are shipped two helper scripts:

您可以像使用普通 SQL 文件一样直接运行它们；不过为了便于操作和批处理，目录中还附带了两个辅助脚本：

1. UseDB_Tools.sh
2. UseDB_Tools_Interactive.sh

The only difference between these two scripts is, that the second will always run in "interactive" mode.

这两个脚本的唯一区别在于：第二个脚本始终以“交互式（interactive）”模式运行。

These scripts will help you executing the sql-files of the subdirectories.

这些脚本可以帮助您批量执行各个子目录中的 SQL 文件。

## Introduction to the UseDB_Tools[_Interactive].sh helper scripts / UseDB_Tools[_Interactive].sh 辅助脚本介绍
The scripts will execute sql-scripts of the subfolders and log the output in files named
`<subfolder>.log`.

脚本会执行子文件夹中的 SQL 脚本，并将输出结果记录到名为 `<subfolder>.log` 的日志文件中。

You can run them with the following parameters:
* -h display help
This will give a short information about the available options.
* -i for interactive mode
In the interactive mode you will be asked about which subfolders and which files to process.
* -f DIR to force processing only the subfolder DIR
This option allows you to use the script only on one of the given subfolders. The subfolder must be provided as plain name without additional path information.
* -q run in quiet mode
When this option is set, you will not be bothered with too much information.
* -d DB to force using the database DB instead of the one named in the configuration

您可以使用以下参数运行脚本：
* `-h`：显示帮助信息（简要列出可用选项）。
* `-i`：进入交互模式（脚本会询问您要处理哪些子文件夹和文件）。
* `-f DIR`：强制仅处理指定的 `DIR` 子文件夹（`DIR` 必须为纯目录名，不能包含额外路径）。
* `-q`：静默模式运行（启用后不会输出过多冗余提示信息）。
* `-d DB`：强制使用指定的数据库 `DB`，覆盖配置文件中的数据库名称。

## Configuration files in the subfolders / 子文件夹中的配置文件
When the script is run it will try to guess the used database for a subfolder based on its name.
With a normal setup, you will not have to worry about this at all!

脚本运行时会根据子文件夹名称自动推测对应的数据库。在常规配置下，您完全无需为此操心！

However in case that you use non default user or databasenames, or wish to use the tools on special databases, you can modify the files named

`db_info.conf`

located in each of a processed subfolder to your needs.

不过，如果您使用了非默认的用户名或数据库名，或者希望针对特定数据库运行这些工具，可以按需修改各待处理子文件夹中的 `db_info.conf` 文件。

These config files are pretty clear, they only store information about database, database-user and the user's password.

这些配置文件结构非常清晰，仅保存数据库名、数据库用户名及用户密码信息。

This information is used by the UseDB_Tools[_Interactive].sh helper scripts for the subfolder, the content of these files is only stored on your side and not safed with git or similar.

`UseDB_Tools[_Interactive].sh` 会读取这些信息来连接对应子文件夹的数据库；这些文件的内容仅保存在您的本地环境中，不会被 Git 等版本控制工具保存或提交。

Note however that storing sensible information is a security risk, so you should do so only on trusted systems (or when the information is non-critical)

请注意，明文保存敏感信息存在安全风险，因此仅应在受信任的系统上（或信息非关键时）这样配置。

## Suggested format of the sql-files / 建议的 SQL 文件格式
As the sql-files are processed automatically (with the mysql command), they must be proper sql syntax.

由于 SQL 文件由 `mysql` 命令自动执行，因此必须符合正确的 SQL 语法。

For automated use it is also suggested to follow these guidelines:
* First line should be a comment giving rough information about what the sql-commands in this file will do.
Note that the first line will always be displayed in the console as information about what is supposed to happen in the script.
* Additional comments that are emphasized with `-- --` will be output in non-quiet mode as additional information to the user.
* Further comments will be ignored
* The actual SQL statements should be placed at the end of the file.

为了配合自动化脚本执行，建议遵循以下格式规范：
* 第一行应为注释，简要说明该文件内 SQL 命令的作用（脚本运行期间第一行注释始终会显示在控制台上，用以提示当前脚本的用途）。
* 使用 `-- --` 强调的附加注释会在非静默模式下作为额外提示信息输出给用户。
* 其他普通注释将被忽略。
* 实际的 SQL 查询语句应放在文件末尾。

An example file might look like:

示例文件格式如下：

    -- This is an example. It will ensure that Hogger is spawned

    -- -- This comment is displayed without the -q (quiet) option.
    -- -- This SQL file ensures that the most important NPC is spawned in the world.
    -- -- Is Hogger spawned?

    -- This is a comment for the sql-file only, the script will not touch it.
    -- The following query ensures that Hogger is spawned!
    -- Do not forget the semicolon at the end
    SELECT guid, id, position_x, position_y, position_z, map FROM creature WHERE id=448;
