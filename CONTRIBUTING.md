# How to contribute to the CMaNGOS project / 如何为 CMaNGOS 项目做贡献

There are many different ways to contribute to CMaNGOS to continue improving it!

有很多不同的方式可以为 CMaNGOS 做出贡献，帮助它不断完善！

These guidelines shall help you by providing suggested ways of how you can help.

本指南将为您介绍参与贡献的建议途径。

As CMaNGOS and its friendly projects are a huge and widespread way of working,
you can dig in at any place you want to!

由于 CMaNGOS 及其关联项目涵盖范围广泛，您可以从任何感兴趣的地方入手！

*Work for any part is appreciated!*

*任何部分的贡献我们都由衷欢迎！*

This guide however is only for contributing to the CMaNGOS core part -
which is hosted at https://github.com/cmangos

不过，本指南仅适用于对托管在 https://github.com/cmangos 的 CMaNGOS 核心代码部分进行贡献。

## Getting Started / 快速开始

* Make sure you have a [GitHub account](https://github.com/signup/free)
* Watch the CMaNGOS repository on GitHub (This way you will get informed about changes by GitHub)
* Optionally: Fork the CMaNGOS repository on GitHub in order to be able to publish own changes and create pull requests with them (see below for details)

* 确保您拥有一个 [GitHub 账号](https://github.com/signup/free)
* 在 GitHub 上 Watch（关注）CMaNGOS 仓库（这样您就能及时收到 GitHub 的变更通知）
* 可选：在 GitHub 上 Fork CMaNGOS 仓库，以便发布自己的修改并创建 Pull Request（详情见下文）

You can help us developing the core by (at least!)
* Giving Feedback on Bugs
* Testing features in development
* Writing own patches

您至少可以通过以下方式帮助我们开发核心代码：
* 反馈 Bug（缺陷）
* 测试正在开发中的功能
* 编写并提交自己的补丁

# Bug-Reports / 缺陷报告（Bug 反馈）

If you spot a bug, please report it by opening an issue on our centralized GitHub issue tracking repository: [cmangos/issues](https://github.com/cmangos/issues/issues).
Before you do so, please consider the following points:

如果您发现了 Bug，请在我们的集中式 GitHub 问题追踪仓库 [cmangos/issues](https://github.com/cmangos/issues/issues) 提交一个 Issue。
在提交之前，请先确认以下几点：

* The bug happens on a clean core
* You can reproduce the bug
* The bug is not already reported. If it is, you are however very welcome to add a "for me too" comment to the existing issue or give additional feedback.

* 该 Bug 出现在纯净（未做自定义修改）的核心版本上
* 您能够稳定复现该 Bug
* 该 Bug 尚未被他人报告。如果已有相关 Issue，欢迎在原 Issue 下评论“+1 / 同样遇到”或补充更多线索

When you decide to create a new bug report, please make sure that this report includes:

当您决定创建新的 Bug 报告时，请确保包含以下信息：

* Revision on which you encountered this bug (if known, also the revision or timeframe where it first appeared) - you can also post the link to the commit on the upstream repository
* Revisions of additional parts of the CMaNGOS familiy (database name and version, and such)
* A detailed step-by-step report how this bug can be reproduced
* A detailed explanation of what DOES happen
* A detailed explanation of what SHOULD happen - if possible also add a source why you think this should happen
* When reporting spell-related bugs, please use spell-IDs in your description and try to use .cast commands to reproduce your bug. This makes reproducing much easier!
* When reporting crashes, please include a crash-log (as detailed as possible - we like GDB logs from debug builds!)
* Only report one bug per issue/thread

* 遇到该 Bug 的核心提交版本（Revision）（如果知道最初出现该 Bug 的版本或时间段也请注明，也可直接贴出上游仓库的 commit 链接）
* CMaNGOS 生态其他组件的版本信息（如数据库名称及版本等）
* 详细、分步骤的 Bug 复现流程
* 实际发生了什么（当前异常表现的详细说明）
* 预期应该发生什么（正常行为的详细说明；如有可能，请附上证明该预期行为的资料来源）
* 报告法术（Spell）相关 Bug 时，请在描述中使用法术 ID（spell-ID），并尽量使用 `.cast` 命令进行复现，这样会大大降低复现难度！
* 报告崩溃（Crash）问题时，请附上崩溃日志（越详细越好，我们尤其欢迎来自 Debug 构建的 GDB 日志！）
* 每个 Issue / 帖子只报告一个 Bug

# Testing features in development / 测试开发中的功能
## Importing pull-requests / 导入 Pull Request
Merging pull-requests into your branch for testing can be done by the GUIs provided by GitHub.
For people not wanting to use them, we have a "MergeHelper.sh" script shipped in contrib/

可以通过 GitHub 提供的图形界面将 Pull Request 合并到您的分支中进行测试。
对于不想使用图形界面的用户，我们在 `contrib/` 目录下提供了 `MergeHelper.sh` 脚本。

You can - also on Windows within a Git Bash - start the script by using
```
contrib/MergeHelper.sh -h
```

您可以通过以下命令启动该脚本（在 Windows 下的 Git Bash 中同样适用）：
```
contrib/MergeHelper.sh -h
```

This script operates in two modes:
* Pass the pull-request number you want to merge as parameter, i.e.
```
contrib/MergeHelper.sh 37
```
which will simply merge pull-request 37 from your origin repository

* You can use the script with a (created) config file to automatically fetch+merge repositories,
  branches and pull-requests

  This should be especially reasonable, if you yourself use topic branches like described in the next section!

该脚本支持两种运行模式：
* 将要合并的 Pull Request 编号作为参数传入，例如：
```
contrib/MergeHelper.sh 37
```
  这会直接从您的 `origin` 仓库合并第 37 号 Pull Request。
* 使用您创建的配置文件，自动拉取并合并指定的仓库、分支和 Pull Request。如果您像下一节所述那样使用特性分支（topic branch），这种方式会非常顺手！

Please be sure to feedback your test-results through proper channels -- this means commenting on the pull-requests, commenting on issues and commits on GitHub.

请务必通过正当渠道反馈您的测试结果——即在 GitHub 上对应的 Pull Request、Issue 或 Commit 下方发表评论。

## Importing features from third-party repos / 从第三方仓库导入功能

Sometimes you might want to import commits from a third-party repository.

有时您可能需要从第三方仓库导入提交（commit）。

For this you need to setup a remote pointing to this repository
```
git remote add <Name> git://github.com/USER/REPO.git
```
and then git fetch it and merge the branches you are interested in.

为此，您需要先添加一个指向该仓库的远程仓库（remote）：
```
git remote add <Name> git://github.com/USER/REPO.git
```
然后执行 `git fetch` 拉取代码并合并您感兴趣的分支。

Also note that you can easily use the MergeHelper script mentioned above to automate fetch+merge after initial setup!

此外，在完成初始配置后，您也可以直接使用上文提到的 `MergeHelper` 脚本来自动执行拉取和合并操作！

# Contributing patches / 提交代码补丁

## Topic branches / 特性分支（Topic branches）
We suggest to use so-called topic-branches for developing some contribution.
The idea is to bundle all required commits for a project into one topic branch.
This topic branch is expected to be published and shared very fast, so that people can comment on your work as soon as possible

我们建议在开发某项贡献时使用所谓的“特性分支（topic-branch）”。其核心思路是将实现某一功能/修复所需的全部提交打包到同一个独立分支中，并尽早发布共享，以便社区成员尽快对您的工作提出意见。

To easily create a topic branch we have added a small helper script, that will guide you interactively through
the process of setting up a topic branch (even if this is basic git-use).

为了方便创建特性分支，我们提供了一个辅助脚本，能够交互式地引导您完成分支创建流程（即使只熟悉基础的 Git 操作也能轻松上手）。

You can start the script by using
```
contrib/CreateTopicBranch.sh
```

您可以使用以下命令启动该脚本：
```
contrib/CreateTopicBranch.sh
```

While developing you should try to keep these points in mind:
* *Commit often, commit early*
* *Write good commit messages to tell people what you are trying to do in your commit*
  This must include some test instructions, link to bug-report and similar

在开发过程中，请尽量牢记以下几点：
* *勤提交、早提交（Commit often, commit early）*
* *撰写清晰的提交信息（Commit Message），向他人说明本次提交的意图*
  提交信息中应包含测试说明、相关 Bug 报告链接等内容。

Also we expect you to keep our [Coding Standards](https://github.com/cmangos/issues/wiki/Coding-Standards)
This code style is automatically enforced by the helper scripts in contrib/cleanupTools (which require AStyle to be installed)

同时，我们希望您遵守项目的 [编码规范（Coding Standards）](https://github.com/cmangos/issues/wiki/Coding-Standards)。该代码风格可通过 `contrib/cleanupTools` 中的辅助脚本自动格式化（需要安装 AStyle）。

## Deployment / 发布与提交

You can publish and share your contribution projects via forum or by pull-requests

您可以通过论坛或 Pull Request 来发布和分享您的贡献。

When you have created your topic-branch and pushed it to your local repository, you can easily create a pull-request to our repo from this!
Please always create the pull request on the repository containing the CMaNGOS code you were working on.

当您创建好特性分支并将其推送到您的远程 GitHub 仓库后，就可以基于该分支向我们的仓库发起 Pull Request！
请务必向您所修改的 CMaNGOS 代码对应的那个仓库提交 Pull Request。

Here is the [GitHub Information on Pull-Requests](https://help.github.com/articles/using-pull-requests)

请参阅 [GitHub 关于 Pull Request 的官方说明](https://help.github.com/articles/using-pull-requests)。

This has these advantages:
* Your code will be read and commented on by many people
* Your code will be fetched by people willing to help testing
* Once you have opened a pull-request for a branch, every change of the branch (like a new commit) will automatically be added to the pull-request.
  So you won't have any additional overhead in maintaining the pull-request :)
* Travis CI will automatically test the build of your pull request and report the status just as it does for our commits

这样做具有以下优势：
* 您的代码会被更多人阅读和评审
* 愿意协助测试的人可以直接拉取您的代码进行测试
* 一旦基于某个分支创建了 Pull Request，之后向该分支推送的每次更新（如新提交）都会自动同步到该 Pull Request 中，无需额外的维护成本 :)
* 持续集成（CI）会自动测试您 Pull Request 的编译构建并反馈状态，与主线提交一致

If it is only a minor change and you do not want to create a topic-branch and a pull request (though you should, it's fast and easy to do!),
you can also open a normal issue and paste the diff in there (but please use the Markdown tags to mark it as a code!). Rule of thumb is that
more than 20 lines should most definitely be posted as a pull request instead of an issue with a simple diff.

如果只是极小的改动，且您不想创建特性分支和 Pull Request（尽管我们还是建议这样做，其实又快又简单！），您也可以提交一个普通 Issue 并粘贴 diff 内容（请务必使用 Markdown 代码块标签进行标注！）。经验法则是：超过 20 行的改动绝对应该以 Pull Request 的形式提交，而不是在 Issue 中贴简单的 diff。

# Additional Resources / 更多资源

* [General GitHub documentation](http://help.github.com/)
* [GitHub pull request documentation](http://help.github.com/send-pull-requests/)
* [CMaNGOS Discord](https://discord.gg/Dgzerzb)

* [GitHub 通用文档](http://help.github.com/)
* [GitHub Pull Request 文档](http://help.github.com/send-pull-requests/)
* [CMaNGOS Discord 社区](https://discord.gg/Dgzerzb)
