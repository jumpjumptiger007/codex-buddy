---
name: experience-pr
description: 固件发布之后，收集可复用的开发经验并作为文档 PR 提交到上游 FoloToy ai-passport 项目。
---

<p align="right">
  <strong>简体中文</strong> · <a href="SKILL.md">English</a>
</p>

# 收集开发经验并提交 PR

本 skill 把固件发布中可复用、可持续的经验，整理成提交到上游 `FoloToy/ai-passport` 项目的
文档 PR。它只在发布之后、且开发者明确要求收集经验时运行。

## 安全与同意门槛（必须先做）

在收集、起草或提交任何内容之前，必须先满足以下所有门槛。这些门槛是最高优先级约束。

1. **先确认同意。** 本工作涉及项目私有内容。开始收集前先向开发者确认是否同意进行发布后的
   收尾工作；开发者拒绝则立即停止。
2. **检查 GitHub 访问通道。** 提交前确认已有可用的 GitHub 通道。按顺序尝试已配置的通道，
   使用第一个可用的：GitHub MCP（已持久且已鉴权）、GitHub skill（如已安装的 `cindy-github`
   能力）、再退到 `gh` CLI（`gh auth status`）。若都不可用，则生成完整变更内容供开发者
   手动粘贴，然后停止；绝不替开发者提交。
3. **绝不在当前分支上修改或提交。** 发布经验通常来自当前工作分支，但该变更**不得**提交到
   开发者当前分支。要以最新上游 `main` 为干净基线，另起一个独立分支或 worktree。
   推送到 `FoloToy/ai-passport` 的单独 fork，并从该 fork 分支向上游开 PR。
   在 `codex-buddy` checkout 中，`origin` 属于 `jumpjumptiger007/codex-buddy`；请为
   `https://github.com/jumpjumptiger007/ai-passport.git` 配置并使用 `contribution` 等单独 remote。
   保持当前 checkout 不被改动。
4. **审查前不得提交。** 先把一切起草好、展示给开发者，等待明确批准。在开发者审查并授权
   之前，不 commit、不 push、不开 PR。开 PR 还需要单独的再次确认。
5. **不写入凭证或私有数据。** 永远不包含凭证、设备 QR 密钥、私密设备链接、个人数据或未脱敏
   日志。提交任何内容前先运行 `python3 tools/check_repo.py`。

## 上游 remote

本流程假定存在名为 `upstream` 的 remote，指向
`https://github.com/FoloToy/ai-passport.git`。若未配置，先添加：

```bash
git remote add upstream https://github.com/FoloToy/ai-passport.git
```

请 fetch `upstream` 以检查当前 FoloToy 基线。不要把本仓库的 `origin/main` 当作上游贡献 fork；它是
独立的 `codex-buddy` 分支。

## 收集可复用经验

聚焦 **codex-buddy 项目相对上游的 `docs/` 差异**——独立项目中创建或变更、因而与上游不同的
`docs/` 文档。这些是可复用的项目专属经验，值得记录。对比当前 codex-buddy `HEAD` 和项目文档与
`upstream/main`：

```bash
# 相对上游有差异（在 codex-buddy 中创建或变更）的项目文档
git diff --name-only upstream/main...HEAD -- docs/

# 只在 codex-buddy 存在、上游 main 没有的项目文档
comm -23 \
  <(git ls-tree -r --name-only HEAD -- docs/ | sort) \
  <(git ls-tree -r --name-only upstream/main -- docs/ | sort)
```

从这些差异文档中，只提取可持续、可复用的经验：

- codex-buddy 记录或变更、而上游没有的内容，以及原因。
- 项目文档记录的硬件事实、接口、时序、资源预算或失败行为。
- 项目做出的构建、验证或发布流程改进。
- 可推广到下一次发布的通用结论。

**不**保留临时调试笔记、半成品实验、或只解释这一次发布的零散信息。

## 经验分流

并非每条项目专属差异都属于上游。提交前先确定每条经验归属：

- **返回上游：通用、上游也受益的经验** —— 对任何 AI Passport 用户都有价值、应属于上游基线的
  经验（例如通用的构建/验证改进、可公开的上游硬件事实、可复用的接口或发布流程改进）。这些
  作为 PR 提交到上游 `FoloToy/ai-passport`。
- **留在 codex-buddy：项目专属定制** —— 产品定制内容、项目私有的业务规则或项目专属资源，
  不应提交回上游。这些**不要**提交上游；作为独立项目的本地文档变更记录即可。
  [`docs/fork-guide.md`](../../docs/fork-guide.zh_CN.md) 仅适用于遵循该指南的实际下游 fork。

按这个分流为每条经验定归属；不要把项目专属定制放进上游 PR。codex-buddy 的 `origin` 是独立项目
remote；上游贡献工作使用单独的贡献 fork。

## 写入经验条目

每次经验采集可产出**一条或多条**经验，每条是一个独立的 `.md` 文件并配其 `.zh_CN.md`，放在
`docs/reference/<username>/` 下，按条目内容概要命名（小写连字符，例如
`audio-compression-trade-offs.md`），其中 `<username>` 是贡献开发者的 GitHub 用户名
（英文小写连字符），把该开发者的条目聚在一起。
本仓库要求默认 `.md` 路径用英文、配套 `.zh_CN.md`，两者互指语言链接，且英文文件中不得混入中文。
从 `docs/reference/README.md` 索引链接它。

开发者不限于一条经验。档案保存**每位开发者一条或多条经验**，每条都是该开发者文件夹下的独立
`.md` 文件（含其配对 `.zh_CN.md`）。每一次可复用的、发布后沉淀的经验都应**新增一条**，而不是
并入已有条目，以保证每条都是一个独立、自包含的主题。

让这个变更落在**独立分支或 worktree** 上。**不要**提交到开发者当前分支。

## 审查与提交

1. 把 diff 和草案交给开发者，确认分流归属（上游 vs 留在 `codex-buddy` 独立项目），等待明确授权。
2. 批准后在独立分支上 commit（英文祈使句 Conventional Commit 标题，例如
   `docs(development): add post-release experience notes`），并推到单独的上游贡献 fork remote，而不是
   `codex-buddy` 的 `origin`。
3. 用英文完整填写上游 `.github/PULL_REQUEST_TEMPLATE.md`，并分别上报 Build、Host tests、
   Device tests、Unverified。
4. 开 PR 前单独征求确认，然后通过第一个可用的 GitHub 通道（GitHub MCP、GitHub skill、
   或 `gh pr create --repo FoloToy/ai-passport --base main --head <fork>:<branch>`）
   从 fork 分支向上游 `FoloToy/ai-passport` 开 PR，并回读确认。

## 交付上报

按 `Build`、`Host tests`、`Device tests`、`Unverified` 分别上报。纯文档变更没有固件或
host-test 工作，因此这些上报为 NOT RUN 并说明原因。经验 PR 不修改中英文变更日志；用户可见或
发布流程影响写入 PR 正文，后续由发布负责人统一汇总。

## 本 skill 不做的事

- 不发布固件、不运行 publisher 流程。
- 不把经验内容提交到额外的公开位置。
- 不在开发者当前分支上提交或修改。
- 未经开发者审查与同意，不自动提交任何内容。

## 相关文档

- 项目开发完成流程总览：`docs/development/release/project-completion.md`
- 经验索引：`docs/reference/README.md`
- 经验条目：`docs/reference/`
- 固件发布：`docs/development/release/publish-to-community.md`
- PR 模板：`.github/PULL_REQUEST_TEMPLATE.md`
- 贡献与提交规则：`docs/contribution/commit-and-pr.md`
- fork 分支与 PR 工作流：`docs/fork-guide.md`
