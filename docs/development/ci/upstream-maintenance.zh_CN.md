<p align="right">
  <strong>简体中文</strong> · <a href="upstream-maintenance.md">English</a>
</p>

# 独立仓库上游维护

`codex-buddy` 是独立仓库。它的 `main` 分支跟踪自己的 `origin/main`；FoloToy 上游变更只用于审查参考，绝不会自动同步或合并到本项目的 `main`。

## Remote 角色

- `origin`：`https://github.com/jumpjumptiger007/codex-buddy.git`，本项目的独立仓库。
- `upstream`：`https://github.com/FoloToy/ai-passport.git`，AI Passport 源仓库与比较基准。
- 本地 `main` 跟踪 `origin/main`。`upstream/main` 不会自动替换或合并到 `main`。

允许 fetch `upstream` 以只读比较。Fetch 会更新 remote-tracking 引用，不会把提交集成到本地分支，也不会改动工作区。提出集成前必须审查差异。上游集成必须是范围明确、单独审查的任务；常规维护不改写历史。

## 审查上游变更

Fetch 后检查提交和文件差异。优先审查 BSP、硬件定义、ESP-IDF 与构建工具、固件分区布局、验证流程及普遍适用于 AI Passport 的改进。

项目专属的 Companion、协议、安全/会话、产品 UI 和 Codex 行为由 `codex-buddy` 维护，除非另行选定上游贡献。不得自动用上游版本替换这些项目变更。

使用以下只读命令比较分支：

```bash
git fetch upstream
git status --short --branch
git log --oneline --left-right --graph main...upstream/main
git diff --stat main...upstream/main
git diff --name-status main...upstream/main
git log --oneline main..upstream/main
```

这些命令检查已 fetch 的引用；不会 merge、rebase、cherry-pick 或 push。审查相关提交和差异后，再单独提出集成任务。上游审查过程中不要修改 remote 或分支跟踪配置。

## 向上游贡献变更

向上游提交 PR 时，可以使用单独的 GitHub fork：`jumpjumptiger007/ai-passport`。该仓库不是本项目的 `origin`。需要时将它配置为单独的 contribution remote；`origin` 始终指向 `jumpjumptiger007/codex-buddy`。

只有在实际 fork 式项目中工作时，才参考[下游 fork 指南](../../fork-guide.zh_CN.md)。该指南中的分支模型不适用于本仓库。
