<p align="right">
  <strong>简体中文</strong> · <a href="codex-ambient-dashboard.md">English</a>
</p>

# Codex Ambient Dashboard：架构与第一阶段门禁

本文记录 Codex Ambient Dashboard 已冻结的架构和交付门禁。第一阶段包括架构、真实环境验证、可进行主机测试的核心逻辑，以及 Mac Companion 真值层；本阶段在实现 Passport 产品固件或 UI 之前结束。

## 产品边界与平台

目标硬件为 ESP32-C3，配备 8 MB Flash、无 PSRAM，使用 ESP-IDF 5.5.3。

- **Passport** 负责显示和产品 UI、角色动画、按键、麦克风采集、扬声器提示、BLE 传输，以及少量确定性的状态/reducer 逻辑。
- **Mac Companion** 负责 Codex 集成、配额获取与重置检测、生命周期解释、通知、本地语音转文字（STT）、向已验证的 Codex composer 安全插入文本，以及 BLE 重连/编排。
- 复用上游 BSP。不得为了应用方便修改 BSP，也不得把基线硬件测试菜单或视觉外壳直接用作产品 UI。
- Passport 是产品设备，绝不能作为仓库、agent、部署或其他特权操作的审批控制器。

可脱离硬件测试的状态、reducer、协议和计时逻辑应与 ESP-IDF、LVGL 解耦。

## 状态与真值

Codex 状态为 `OFFLINE`、`IDLE`、`WORKING`、`ATTENTION` 或 `DONE`。语音覆盖层状态为 `NONE`、`LISTENING`、`TRANSCRIBING`、`READY` 或 `FAILED`。

- `OFFLINE` 表示没有新鲜的 Companion 快照。
- `DONE` 是时长约三秒的短暂成功提示。失败或中止的 turn 不得显示为 `DONE`。
- Companion 快照是当前状态的真值来源。通知是一次性事件，不能替代快照状态。

按 `window_minutes`（短窗口 300 分钟、长窗口 10080 分钟）归一化持久化 rollout `rate_limits.primary` 和 `rate_limits.secondary`，只使用数据源提供的用量和重置数据。在 `reset_at` 边界及之后，该窗口不可用；绝不得根据 token 数量估算配额。定义 `AppServerQuotaSource` 接口，但在安全的 transport 和 request framing 得到验证之前，真实 adapter 保持不可用。只通过原子替换持久化有界的 reset detector 状态；状态文件缺失或损坏时必须安全地重新建立基线。

## BLE、语音与数据边界

使用项目自有的 128 位 BLE service。`CONTROL` 从 Mac Companion 流向 Passport；`EVENT` 和 `AUDIO` 从 Passport 流向 Mac Companion。控制消息采用有界、按行分隔的 JSON；音频采用有界二进制格式。确切的 UUID 值、schema、frame 上限和其他线协议细节，必须先依据实测需求确定再实现。

用户按住 OK 说话。Passport 负责采集麦克风音频，但不运行 STT。Mac Companion 在本地执行 STT，并将识别出的文本插入已验证的 Codex composer。不得自动按 Enter 或以其他方式自动提交 turn。Composer 的选择和文本插入方式必须以 Gate 0 的 Accessibility 探测结果为依据。

不得向 Passport 发送 prompt、transcript、command、diff、tool output、assistant 内容、认证材料或 secret。解析前校验长度和格式，并为所有 payload、buffer、queue 和保留数据设定上限。只发送当前功能所需的字段。

公开角色引擎/代码和公开示例素材必须保持可分发。Spider-Man 及所有其他私有或不可分发的角色源素材不得进入公开 Git 历史或公开发行包。引入私有或本地角色源素材包前，必须先在 `.gitignore` 中保护其源路径；本文档不决定该路径。公开生成素材不得在未明确说明的情况下派生自私有或不可分发来源。

`demo/claude-buddy-port` 仅作为代码移植与架构参考。除非针对本产品和当前硬件重新验证，否则其中的 BLE/协议实现及测量结果不属于本产品事实。

## Gate 0 已审查的环境发现

以下是为本阶段检查的本地环境观察，不构成新的产品不变量。

- **Codex Desktop 身份：**bundle display name 为 `ChatGPT`，bundle ID 为 `com.openai.codex`，观测到的版本为 `26.924.22138`（build `11645`），可执行文件架构为 `arm64`。通过检查的 bundle metadata 无法确定 framework/runtime 版本。
- **Rollout 生命周期与并发：**未观察到确切事件名 `SessionConfigured`、`TurnStarted` 和 `TurnComplete`。后续 metadata-only 检查在 32 个 rollout 文件中验证了 46 条持久化的 `event_msg` 记录，其 `payload.type = turn_aborted`。外层字段为 `type`、`ordinal`、`timestamp` 和 `payload`；payload 字段为 `type`、`turn_id`、`reason`、`started_at`、`completed_at` 和 `duration_ms`。每个检查过的文件都包含一条 `session_meta.payload.session_id`，且各文件内 ordinal 递增。G2.1 只会从此已验证的 abort 形式发出生命周期输入；`thread_settings_applied`、`task_started` 和 `task_complete` 不视为等价事件，并会 fail closed。现有 metadata 显示不同 session 之间存在重叠的 task 活动。
- **配额：**现有 rollout `rate_limits` 记录包含按时长识别的短、长窗口（300 和 10080 分钟），以及数据源提供的用量和重置字段。由于未能确定可安全调用的 transport 或 request framing，App Server `account/rateLimits/read` 对比仍不可用；没有发送新请求。Rollout 数据不构成跨数据源验证，也没有使用基于 token 的估算。
- **权限与信任：**对现有 rollout event/type label 的 metadata-only 扫描未发现匹配 permission、approval、trust、policy、sandbox、`allowed` 或 `denied` 的信号。该结论仅适用于已扫描的语料，不能证明应用没有其他权限行为。
- **Composer Accessibility：**`NOT_INSPECTED`。可用的 CUA Accessibility hierarchy 调用没有 metadata-only 过滤功能，并可能返回文本值，因此没有查询 composer。没有触发 TCC 对话框或更改设置。Composer selector、可编辑性和文本插入行为仍未经验证。
- **STT 基准测试：**在所探测的环境中不可用，因为检查的现有位置没有受支持的 `whisper.cpp` runtime 或预先存在的 `base`/`small` 模型对。没有进行推理、下载或安装，也没有使用用户音频；模型性能仍未知。

## 第一阶段主机实现状态（G2.1–G2.5）

当前 Gate 2 实现仅面向主机，并复用 Gate 1 核心：

- **G2.1 — RolloutWatcher：**增量读取有界的持久化 rollout 记录，并且只发出已验证的 `turn_aborted` 生命周期形式。未知或未验证的生命周期标签 fail closed；不完整、格式错误和超长输入均会受到长度限制并被安全拒绝。
- **G2.2 — 配额数据源与持久化：**`RolloutQuotaSource` 读取持久化的 `rate_limits`，并按时长匹配 300 分钟和 10080 分钟窗口，只使用数据源提供的用量与重置数据。窗口过期时会移除对应窗口，并且只清除该窗口的 reset detector 状态。Reset detector 持久化数据有界且通过原子替换写入；状态缺失或损坏时会安全地重新建立基线。`AppServerQuotaSource` 仍只是不可用的接口边界。
- **G2.3 — CompanionCore：**通过现有 reducer 聚合由主机注入的各 session 生命周期事件，优先级为 `ATTENTION > WORKING > DONE > IDLE`。新生成的 Companion 快照在没有新鲜 Codex session 时为 `IDLE`。生命周期和已确认的配额重置通知均为一次性事件，并使用有界去重。快照发布只通过 fake transport 测试。
- **G2.4 — 外部边界：**`PermissionObserver` 报告注入的数据源观察状态，不安装 hook，也不合成事件。STT 提供可注入的 backend 边界以及不可用/失败行为，不包含 runtime 或模型。Composer 文本插入必须通过单个已验证的不透明 target，接口只提供文本插入；没有实现真实 Accessibility selector、自动改选其他 target 或提交操作。
- **G2.5 — 集成验证：**主机测试覆盖 rollout 到 Companion 的配额/状态/快照/fake-transport 流程、重启后的 reset 持久化、过期与重新出现，以及合成的 fake STT 结果流入已验证的 fake composer。测试还验证权限观察器或 STT 不可用时不会创建 attention 状态或可用 transcript。这些测试验证的是主机抽象和 fake，不是真实 macOS 集成。

真实 App Server transport 和 request framing 仍不可用。尚未建立真实的 permission-request 或 hook 路径。STT runtime 和模型仍不可用。真实 Codex Accessibility composer 尚未验证，也没有生产用文本插入 adapter。主机接口和 fake 不能证明这些集成可用。尚未开始 Passport 产品固件、产品 UI 或物理 BLE 实现。

## 门禁顺序

按顺序完成各门禁。记录证据和限制，不得持久化 secret。如果门禁需要用户处理 macOS 权限对话框、hook 信任、系统/全局安装或配置更改、GitHub origin 设置、公开发布或物理设备写入，应暂停并交由用户处理。未经 C2C Control Room 明确授权，不得 commit 或 push。

| 门禁 | 范围 | 退出证据 |
| --- | --- | --- |
| 架构 | 使用配对的英文和简体中文文档记录本架构与门禁计划。 | 两份文档对已冻结决策、未决细节和门禁顺序的描述一致；仓库文档验证通过。 |
| 0 — 环境真值 | 探测 Codex Desktop 身份/runtime/bundle；rollout `SessionConfigured`、`TurnStarted`、`TurnComplete`、`TurnAborted`；并发 session 行为；App Server 配额与 rollout 后备数据；`PermissionRequest` 和信任行为；实际 Accessibility composer 特征；以及在可行时对比 whisper.cpp `base` 与 `small` 性能。 | 对每项探测记录观察结果、证据来源、方法和限制。若无法完成探测或基准测试，明确记录原因。不得持久化 secret。 |
| 1 — 可主机测试的核心逻辑 | 实现有界协议模型和错误输入测试、多 session 状态 reducer、配额标准化/重置检测、新鲜度逻辑、通知去重，以及 fake/mock BLE transport。 | 主机测试覆盖合法和错误消息、并发 session 隔离、过期/新鲜快照、成功与失败/中止 turn、配额窗口/重置、通知去重和有界传输行为；通过独立 C2C 审查。 |
| 2.1–2.5 — Mac Companion 真值层 | 实现上述主机专用 rollout watcher、配额数据源与有界 reset 持久化、CompanionCore、fail-closed 外部边界和集成测试。复用 Gate 1 reducer、protocol、deduplication 与 transport。 | 相关主机测试套件和静态验证通过；明确标记不可用的真实集成；通过独立 C2C 审查。 |
| 后续阶段 — Passport 产品固件/UI | 仅当本阶段通过审查且另一个 goal 明确授权后，才实现设备端产品体验。 | 本阶段不开始 Passport 产品固件、UI、刷写或设备写入。 |

## 暂不确定的细节

本计划不臆定 BLE UUID 值、characteristic schema、数值型 payload 上限、音频编码、确切的新鲜度阈值、macOS 权限处理方式、Accessibility 元素选择器或 STT 模型/调优参数。实现前应依据 Gate 0 证据和有界产品需求确定这些细节，同时保持上述所有权边界。
