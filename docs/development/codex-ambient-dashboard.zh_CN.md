<p align="right">
  <strong>简体中文</strong> · <a href="codex-ambient-dashboard.md">English</a>
</p>

# Codex Ambient Dashboard：vNext 架构与 R0–R7 路线图

本文是 Codex Ambient Dashboard 的权威架构和交付路线图，取代此前的 Phase 1、Phase 2、U0–U3、G2.1–G2.5 路线图。来源级证据和模块决策见配套的 [R0 复用审计](codex-buddy-reuse-audit.zh_CN.md)。

R0 只进行架构、来源审计、许可核验和文档整理，不增加生产 BLE、Codex Hooks、设备服务、最终 UI 或角色素材。R0 审查完成后停止；进入 R1 必须另由 Control Room 明确决定。

## 1. 范围与已核验基线

R0 审计时当前工作区基线为 codex-buddy 的 main 分支，提交 f12a6e0fbddaad0a3ba133084f37ff59a609be68；文档修改前工作树干净。附录固定了 donor 引用和完整提交 SHA。

目标产品是 FoloToy AI Passport：ESP32-C3、8 MB Flash、无 PSRAM、ESP-IDF 5.5.3。当前 BSP、引脚头文件和硬件指南仍是硬件事实的来源。

当前实现证据：

- companion/core 包含可进行主机测试的生命周期 reducer、配额归一化、消息验证与行分帧、传输回调及有界通知去重。
- companion/mac 包含持久化 rollout watcher、rollout 配额源、重置状态存储、Companion 编排，以及权限观察、STT 和 composer 插入接口。这些是主机侧模块和边界，不代表已经有生产 Codex Hook、App Server、Accessibility 或 STT adapter。
- 当前 rollout watcher 只接受既有探测已验证的生命周期形式。未知或格式错误的输入会 fail closed。配额只来自源提供的 300 分钟和 10080 分钟 rate-limit 记录；token 数量不作为配额来源。
- main/main.c 从与传输无关的 UI model 启动持久 Passport 外壳。main 产品目标不会启动硬件 demo 页面。demo_ble.c 只是不可连接的 demo 广播，不是产品设备链路。
- 主机测试覆盖纯逻辑和 fake；它们不能证明真实 macOS 集成、BLE 配对、射频行为或真机验收。

现有 Passport 合成模型仍包含 project 和 activity 文本字段，但它们不属于 vNext 线协议或产品契约，计划在产品集成门禁移除。U3 的确切卡片布局尚未冻结。紫色几何角色仍是占位图；本路线不启动旧 Character Engine 计划，也不定义最终吉祥物。

旧架构文档中的环境探测记录是历史观察。需要这些事实的后续门禁必须重新核验，不能将过往本机探测当成当前平台事实。

## 2. 决策词汇与 R0 结果

审计矩阵中的每个模块决策只能是以下四种之一：

- **KEEP OURS** —— 保留当前项目实现或契约，作为 vNext 基础。
- **REUSE DONOR** —— 完成许可、来源和集成检查后，基本按原样纳入 donor 实现。
- **ADAPT DONOR** —— 以 donor 实现为起点，按本产品契约、安全、硬件、边界或职责进行改造。
- **DROP** —— 将该 donor 实现或数据排除在 vNext 之外。

本次矩阵共有 21 项 KEEP OURS、12 项 ADAPT DONOR、8 项 DROP，没有 REUSE DONOR。没有 donor 模块同时满足直接、原样集成的条件：有用候选要么携带不兼容的内容或审批语义，要么依赖另一个语义核心，要么必须针对当前板卡和有界契约修改。

架构保留当前 Companion 真值核心、多会话 reducer、结果与新鲜度语义、配额源和重置持久化、通知与快照分离、与传输无关的 UI model、BSP 边界以及 fail-closed 主机接口。它将改造许可明确的 Codex Hook 和桌面 bridge 路径、选定的 ESP-IDF NimBLE 传输与 bond 管理模式、小型设置/持久化模式、支持工具和 simulator 测试接口。设备端审批、包含内容的 Buddy 字段、旧 project/activity 投影及来源不清的角色或媒体素材会被排除。审计附录逐项解释模块决策及证据。

## 3. vNext 组件职责与数据流

目标数据流：

Codex 生命周期 Hook 与已验证的配额源 → Mac Companion adapters → 每会话生命周期与配额真值 → 有界、无内容快照和一次性事件契约 → 安全 BLE 传输 → Passport 解析器/model/presenter/持久 UI。

语音数据流单独处理：用户明确按住 Passport 的 push-to-talk 按键后，设备进行有界音频采集 → 安全 BLE 将音频传到 Mac Companion → 本地 STT 返回文本 → Companion 只向一个已验证的 Codex composer 目标插入文本。用户检查并提交 Codex turn。没有任何组件会按 Enter 或以其他方式自动提交。

### Mac Companion

Companion 负责易变的 Codex/macOS 集成、官方 Hook 安装与生命周期映射、多会话聚合、源提供的配额获取与重置检测、桌面启动/恢复、通知生成、本地 STT、composer 验证/插入和 BLE 连接编排。

Companion 是唯一的生命周期聚合真值源。会话和 turn 标识留在 Mac，仅映射为有界的不透明状态。未知事件、过期数据源、不支持的权限信号和格式错误记录均 fail closed。失败与中止必须和成功完成区分开。

### Passport 与 BSP

Passport 负责显示、产品交互、按键、麦克风/扬声器硬件访问、本地设置、有界 BLE 解析与传输事件，以及少量确定性投影逻辑。它展示 Companion 已归一化的快照，不聚合 Codex 会话，也不推算配额。

BSP 继续负责板级引脚、显示器、按键、电池和音频接口。产品行为留在应用代码中。LVGL task 之外访问 LVGL 必须持有 bsp_lvgl_lock()。按键回调只投递有界工作，不能执行缓慢的存储、网络、音频或 UI 操作。销毁 UI 前先停止可能访问它的任务、回调和定时器。

### 无内容协议与安全

vNext 语义协议归本项目所有。允许字段只包括协议版本/能力、链路和新鲜度状态、枚举生命周期/结果值、有界聚合计数、源提供的配额值和重置标记，以及枚举的一次性通知代码。协议排除 PROJECT 和 ACTIVITY。线协议不得传输 prompt、transcript、command、diff、tool output、assistant 内容、tool 预览、审批请求、认证材料或 secret。

持久快照真值与临时通知分离。重连后先传完整的当前快照，再依赖后续事件。R1 在 [R1 契约](codex-r1-contracts.zh_CN.md)中冻结语义 sequence/revision/ACK、协议 version/capability、字段长度及主机 admission 上限：合法 state frame 最大95字节、line128字节/frame129字节、四条 notice/516字节 fake-transport queue、八 session/十六 dedup entry。R3 仍负责 BLE MTU/fragmentation、认证 generation 建立及物理 queue/resource 证明；实测可收紧，但不能静默超过 R1 硬上限。R5 负责音频 framing/codec 上限。

R0 已选择 Option B 作为目标架构，但这还不是已验证的实现：改造 Espressif Apache-2.0 NimBLE 传输，同时保留更小的项目自有语义协议。Espressif 已发布的传输实现通过必需的 esp_desktop_buddy core 指针及 GATT RX 分发与其 Buddy 语义核心耦合。R3 首先必须验证能否把 RX/TX、GAP/GATT、配对/bond 生命周期、重连、队列和 teardown 拆到项目自有 transport 接口之后，且不引入 Buddy message、entries、prompt、tool、hint、时间命令或权限回复语义。若拆分失败或仍需保留 Buddy core，R3 必须停止并返回架构审查，不得悄然改用 Option A。

R0 不冻结具体配对策略，只冻结一条不变量：应用数据只能在项目自有 secure-link predicate 验证通过后流动。该 predicate 必须强制后续选定的加密、bond、MITM/认证、Secure Connections、peer identity 和订阅要求；donor 的 tx_ready 或 helper 判定本身均不充分。Espressif transport 可配置 bonding、MITM 和 Secure Connections，但其 tx_ready 只检查已连接、已订阅和已加密。这只是传输就绪状态，不是产品授权。Codex Buddy helper 忽略 authenticated，且默认关闭 MITM，只能作为反例证据。donor 的安全默认值、UUID、MTU 假设、帧大小、重试间隔和射频声明均不是当前产品事实。

Passport 绝不是仓库、agent、shell/tool、部署或其他特权审批的控制器。权限相关观察最多产生 ATTENTION 通知，并提示用户回到 Mac 操作。设备不能批准或拒绝该操作。

## 4. 资源与失败规则

缓冲区、队列、保留的标识符和解码后的消息均须有明确上限。超长、格式错误、不支持、过期、重放或乱序数据，在进入产品状态前必须拒绝。不得根据 donor 的测量值提高资源上限。ESP32-C3 没有 PSRAM；R3 和 R4 必须在当前板卡测量内部 heap、最大连续内存块、任务 stack、音频 DMA、显示 DMA 及最坏帧压力。

Companion 数据源不可用或过期时，Passport 显示 OFFLINE。失败或中止的 turn 不得变成 DONE。断开连接会清除依赖链路的就绪状态；安全重连成功后必须先完成快照同步，之后才能把事件视为当前状态。到达数据源 reset 边界后配额不可用，不能用 token 总数估算。

用户明确触发 unpair/reset 后，清理本地 bond 状态和有界链路状态。日志不得包含 prompt、transcript、command、payload 内容、credential 或原始授权数据。

## 5. R0–R7 交付门禁

每个门禁都要单独经过 Control Room 审查。一个门禁的退出证据不授权进入下一个门禁。

### R0 — 复用架构重基线

- **进入条件：** 已核验当前 workspace/Git 基线，所有必需来源均已 pin。
- **范围和 donor 输入：** 来源级许可/来源审计、模块矩阵、vNext 架构和 R0–R7 路线图。保留当前核心；仅改造明确许可的候选；排除不安全或许可不明的候选。
- **不在范围内：** 生产代码、已安装 hooks、依赖、固件更改、设备写入、commit 和 push。
- **退出证据：** 中英文文档对齐、来源证据已 pin、每个必要模块均有分类、文档/静态检查通过，且独立 Control Room 审查返回 DONE。
- **需要人工操作时停止：** 仅在来源访问、许可或重大产品决策无法在 R0 内安全解决时停止。审查结束后停在这里。

### R1 — 契约与协议收敛

- **进入条件：** R0 审查为 DONE，Control Room 授权 R1。
- **范围和 donor 输入：** 冻结 Hook allowlist 事件模型、每会话顺序/结果、多会话聚合输入、配额/重置语义、快照和通知规则、设置/unpair 契约、协议版本及可测试接口。只采用已许可 Codex Buddy 源中的安全 Hook 映射模式。
- **不在范围内：** 用户级 Hook 安装、生产 BLE、UI 改版、语音传输和最终角色设计。
- **必需证据：** 主机测试覆盖合法/未知 Hook 事件、并发会话、重放/顺序拒绝、失败/中止结果、配额窗口/重置、内容拒绝、分帧边界和版本/能力行为。
- **需要人工操作时停止：** 任何必须改写用户 Codex 配置、权限、信任或产品审批行为的操作。
- **退出证据：** 已审查的契约、有依据的字段/字节/队列上限，以及经批准的 R2 测试计划。

- **冻结主机契约：** [R1 契约与 R2 进入计划](codex-r1-contracts.zh_CN.md)定义归一化边界、wire v1、上限、恢复及来源证据要求。

### R2 — Companion 生产真值

- **进入条件：** R1 契约完成，且 Control Room 授权 R2。
- **范围和 donor 输入：** 实现经 R1 论证的生产 Companion Hook/lifecycle/quota/reset/startup/diagnostics adapters。改造已许可桌面 bridge 与安装模式，同时保留当前数据真值及 fail-closed 行为。
- **不在范围内：** Passport BLE/固件集成、turn 自动提交、设备审批和未支持的 App Server/Accessibility 行为。
- **必需证据：** 在获授权环境验证真实 Hook→Companion 流程、多会话集成、配额/重置持久化、过期/offline 恢复、脱敏诊断及主机验证。
- **需要人工操作时停止：** macOS 隐私/信任弹窗、用户级 Hook 配置同意、credential，或任何未支持的权限边界。
- **退出证据：** 可独立检查的源码、可重复的 Companion 测试、已记录限制，以及通过审查的 R3 安全链路输入契约。

### R3 — 安全桥接与会话层

- **进入条件：** R2 审查为 DONE；R1 协议、内容与安全要求已冻结到足以评估传输层。
- **首项可行性验证：** 把 Espressif NimBLE transport 拆到项目自有接口后，证明没有 esp_desktop_buddy 语义核心依赖或被禁止的内容/权限语义，并在产品集成前验证 ESP32-C3 / ESP-IDF 5.5.3 构建。
- **范围和 donor 输入：** 仅改造成功拆分后的 GATT/GAP、配对/bond 生命周期、重连、有界 TX/RX 队列和 teardown；实现项目自有 secure-link predicate；在安全重连后完整同步快照。
- **不在范围内：** Passport 产品导航、最终布局、角色引擎、PTT/STT 和自动审批。
- **必需证据：** ESP-IDF 5.5.3 C3 构建、central/peripheral 双向互通，以及独立于 donor tx_ready 的加密/bond/MITM-认证/Secure-Connections/peer policy 强制验证；明确拒绝满足 donor transport 就绪条件、却不满足产品安全策略的链路。还须验证重试/unpair、有界队列、断连后的完整快照恢复、teardown 和实测资源数据。
- **失败处置：** 若不能证明干净拆分、资源可接受、IDF 兼容或安全策略强制，R3 停止并返回架构审查。不得以采用 Espressif Buddy 语义核心作为回退。
- **需要人工操作时停止：** 配对信任弹窗、物理设备操作、重大协议/安全决策，或不可接受的资源/射频结果。
- **退出证据：** transport/core 解耦、安全策略强制、重连/同步、有界队列、teardown 和实测资源均通过独立审查。

### R4 — Passport 产品集成

- **进入条件：** R3 安全会话通过审查，协议稳定。
- **范围和 donor 输入：** 将快照和一次性事件接到持久产品 UI；通过当前 BSP 实现有界按键/导航、设置、电池、必要时的时间显示、声音、持久化和显式 unpair。只改造经验证的小型 service 模式。
- **不在范围内：** 在设备聚合 Codex、project/activity 字段、设备审批、最终吉祥物和语音采集。
- **必需证据：** host mapping 测试，以及当前板卡上的显示、按键回调、LVGL locking、存储损坏/恢复、链路丢失和安全重连同步验收。
- **需要人工操作时停止：** 物理硬件操作、导致范围扩大的产品导航决策，或对真实用户数据执行破坏性 reset。
- **退出证据：** 持久 UI 与已审查的无内容契约一致，并通过真机验收。

### R5 — 语音路径

- **进入条件：** R4 产品集成通过审查，音频传输与隐私限制已批准。
- **范围和 donor 输入：** 实现 push-to-talk 采集、有界音频分帧/传输、Mac 本地 STT、单一目标 composer 验证和插入，以及清晰的失败/恢复行为。使用当前 BSP 音频硬件；只有实测证明有用时才评估小型、许可明确的音频模式。
- **不在范围内：** 常开录音、唤醒词服务、云端转写、在 Passport 显示/保留 transcript，以及自动提交 Codex。
- **必需证据：** 采集和队列上限、codec 或 PCM 取舍、STT 可用性/延迟、中断和断连处理、目标验证，以及证明不存在提交操作的测试。
- **需要人工操作时停止：** 麦克风/Accessibility 权限、音频数据同意或重大 codec/隐私选择。
- **退出证据：** Mac 与设备之间可重复完成语音插入，且不会自动提交。

### R6 — 加固与可支持性

- **进入条件：** R4 和 R5 审查均为 DONE。
- **范围和 donor 输入：** 加固 sleep/wake、RAM/task/audio 资源预算、启动/autostart、安装/卸载、诊断、simulator 覆盖、持久化迁移和恢复。改造许可明确的 setup/support 模式；simulator 只用于其确实建模的行为。
- **不在范围内：** 新产品功能、新角色素材、无界 telemetry 和发布。
- **必需证据：** 干净安装/升级、卸载/unpair、重启和故障注入、simulator 覆盖边界、脱敏支持材料及 C3 资源结果。
- **需要人工操作时停止：** 全局/系统配置、签名 credential、破坏性迁移或生产分发。
- **退出证据：** 支持/恢复流程成文，所有本地自动检查通过。

### R7 — 端到端验收与发布准备

- **进入条件：** R6 审查为 DONE，且验收硬件和获授权参与者均已到位。
- **范围和 donor 输入：** 在真机验证 BLE 安全/重连、生命周期和多会话真值、配额/重置、UI/按键、语音、sleep/资源、安装/启动、诊断、许可及 simulator/硬件差异。
- **不在范围内：** 未获明确授权的发布、GitHub push 或部署。
- **必需证据：** 已签字的测试矩阵、无敏感内容的日志、源码/许可清单、可复现构建/打包记录、硬件结果和剩余限制。
- **需要人工操作时停止：** 物理设备操作、账号或签名访问、发布操作，或任何未解决的安全/隐私决策。
- **退出证据：** Control Room 审查确认已满足目标验收条件；发布操作仍需单独授权。

## 6. 留待后续门禁解决的事实

R1 后以下项目仍未解决：Hook payload/版本的正式语义、生产配额源传输、能否安全读取 App Server 配额、BLE UUID/GATT schema 和 central 兼容性、准确的 bonding/MITM/Secure Connections 策略、R1 硬上限内的 BLE MTU/fragmentation 和物理 queue/resource 验收、认证 generation 建立、R5 音频 framing/codec 上限、当前板卡安全与 RF 验收、设置迁移格式、产品是否需要设备时钟、STT 模型/runtime 性能，以及 macOS composer 目标的实际验证方式。

各门禁只收集自身范围所需的证据。Donor README、donor 构建成功或 simulator 运行结果，均不能证明当前 Passport 的行为。没有任何任意 prompt、tool 或 assistant 内容获准发送到设备。
