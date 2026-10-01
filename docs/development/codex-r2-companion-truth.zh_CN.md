<p align="right">
  <strong>简体中文</strong> · <a href="codex-r2-companion-truth.md">English</a>
</p>

# R2 Companion 生产真值

基线：已审查 R1 checkpoint `c9da1728857e17d1f7567db9d3acf7d8bac0524d`。本文记录 R2 自主实现及证据，等待 Control Room 独立审查。R3 未授权。[R1 契约](codex-r1-contracts.zh_CN.md) 保持不变。

## 2026-10-01 收集的证据

| 证据 | 状态与范围 |
| --- | --- |
| Desktop 身份 | 只读 VERIFIED：`/Applications/ChatGPT.app/Contents/Info.plist` 中 bundle 为 `com.openai.codex`，版本 `26.928.31416`，build `12553` |
| Desktop CLI | 内置 `Contents/Resources/codex-cli/codex-package.json` 与 `bin/codex --version` 确认 `0.159.2`、aarch64 Apple Darwin，VERIFIED |
| 独立 CLI | `codex --version` 确认为 `0.155.1`。它与 Desktop 不同，其 schema 不能作为 Desktop 版本证据 |
| Hook 文档 | [官方发布行为参考](https://learn.chatgpt.com/docs/hooks) 说明 session/turn 标识、`UserPromptSubmit`、`Interrupt`、`Stop` 与 permission Hook，但不确立每会话 ordinal。提交提示词可被阻止，因此不能单凭它证明 turn 已启动；Stop 不能确立产品成功 |
| App Server schema | 两个已安装 binary 均能离线生成 schema。Desktop `GetAccountRateLimitsResponse.json` SHA256 为 `cb9655e68130116f634ed9353e45b336492e6069a6628f424d1cd52e54eda0e0`；`TurnCompletedNotification.json` 为 `016870158603b0f84bd9f8f65f927161c9fd5128e5ec632087616462dc44e085`。这证明结构，不能证明实时投递或订阅覆盖 |
| App Server 传输 | [官方文档](https://learn.chatgpt.com/docs/app-server) 确立 stdio JSONL 与 initialize/initialized。未启动 server、发认证请求、使用凭据或账户端点；生产配额适配器保持 UNAVAILABLE |
| 当前 rollout 样本 | 只读最近五个文件尾部，每个至多 1 MiB：header 版本 `0.159.2`；300/10080 来源窗口各出现 116 次，usage/reset 为数值。每文件样本 ordinal 递增；没有已接受的 `codex.rate_limits` 形式。这不能确立 Hook 顺序或完整生产映射 |
| Donor 模式 | 重读 MIT S3 固定提交 `90972308293076e9b321e90880ccc891a3d7c10b` 的 instance guard、background bridge、file bridge 和 health 分类。其后台队列无界，文件桥接受通用字典。新代码仅采用生命周期/所有权概念，没有复制 donor 源码、内容模型、安装或审批机制 |

仅记录白名单聚合元数据及生成 schema 的 hash。没有保存原始来源 payload、session/turn ID、提示词、对话、工具内容、命令、凭据或授权 payload。样本出现 `task_started`/`task_complete` 标签，但标签存在不能证明结果。保留现有 rollout 解析与配额算法；当前样本不能输入既有配额 parser，因此该形式的生产 rollout 路径保持不可用。rollout 观测不是独立跨源验证。

## 来源、标识和顺序边界

`codex_source_adapter` 接受有限的已解码元数据结构，不能接受 JSON、字典或不透明数据。DISABLED 返回不可用；CURRENT_HOOK 未验证、不输出 fact。SYNTHETIC schema1 是明确的主机测试 profile，不是已验证生产 Hook schema。未知版本/fact、零 ordinal/epoch、无效 ID、缺少 turn、未支持来源状态均关闭并清空映射输出。在 schema、顺序、结果投递未验证期间，没有实时 Hook parser 或安装。

Runtime 的 `source_available` 表示所选 adapter 已准备好接收并接受观测；仅选择 profile 或 runtime 进程启动不代表来源可用。当前 R2 只有 SYNTHETIC 可接收主机测试 fixture。CURRENT_HOOK 可以启动 runtime，但诊断仍报告不可用/未验证；enqueue 会拒绝其输入，`source_recovered` 不能将其提升为可用。只有另行审查且有证据支持的生产 adapter/readiness 路径建立后，生产 profile 才能报告可用。

路径为来源观测 → 白名单 → `companion_ingestion` → 事务式 `identifier_registry` → 冻结 `codex_hook_contract` → CompanionCore。runtime 拥有唯一 dispatcher。runtime/ingestion/retirement 全部要求单个串行调用者；不是线程安全 worker API。busy 标记同时拒绝同步重入。来源 ordinal 原样保留，R2 不分配序号，不修复含糊生产顺序。每会话 reducer 顺序保持权威，包括 UINT64_MAX 耗尽、独立会话、错误 turn 终止拒绝和 attention 保留活动 turn。

registry 精确比较长度/字节，用单调非零数值分配 key，无 hash 混同风险。session/turn 命名空间分离。准入事务化：未知 fact、错误终止、重放、reducer 容量失败不消耗映射容量。reducer 替换槽位时自动退休被淘汰身份；显式退休要求会话已经 stale。退休身份在整个来源 epoch 内拒绝，旧流延迟到达不能悄悄重新准入。key 与退休历史不淘汰、不回绕。

重启丢弃全部易失真值、排队观测和映射。注入的所有权 lease 保留最高来源 epoch，启动要求更高 epoch，旧 callback 被 epoch 检查拒绝。实际进程 supervisor 必须建立新来源实例并停止旧 producer，才能选择新 epoch；合成 epoch 数字不能认证实时输入。没有持久化原始 ID tombstone 存储。

## 主机上限

| 上限 | 最大值与理由 |
| --- | --- |
| ID | 每 span 沿用 R1 127 字节，精确不透明字节、禁止内嵌 NUL |
| Registry | 总计 16 个 session 身份：八个 reducer 槽位与八个退休历史位置。总计 32 个 turn 身份，平均每个最大并发会话四个。保留全部历史，耗尽后拒绝直到整个来源重启，不能削弱顺序保护 |
| 输入队列 | 八个内联 record，每个至多336 主机字节，总计至多2688 字节。每个最大并发会话一个待处理观测的保守 burst 预算，不是 BLE 队列要求 |
| Runtime | 编译期至多16384 主机字节，包含内联 registry、reducer、队列、wire 和诊断。事务 registry 拷贝使用有界主机栈，均不迁往 ESP32 |
| 诊断 | 类型状态至多96 主机字节，数值序列化记录含本地 NUL 小于128 字节。计数器在 UINT32_MAX 饱和；无效状态或输出缓冲不足返回失败、长度零 |
| Notice | 发布前四个枚举 code，发布后进入 R1 四项 wire 队列。溢出丢弃新 notice 并计数，没有内容 body |
| 语义 wire | R1 不变：合法快照95 字节，line128/frame129，四 frame 假 FIFO516 字节，八会话、dedup16 |

registry 用有限重放保护换取有限 session/turn 吞吐。这些是主机准入值，不是已证明的 Desktop 服务容量或 ESP32 资源要求。后续增加须审查，R1 语义/frame 硬上限不变。没有无界分配、队列、重试循环或 tombstone 增长。

## Runtime、配额和诊断

`companion_runtime` 是项目本地服务状态机，不是 daemon 安装。同一 owner 的 start/stop 幂等，同一注入 lease 的第二实例被拒绝。来源启动失败时调用 stop 回滚部分启动并释放所有权。crash 测试显式模拟 supervisor 释放 lease，再用新 epoch 从 OFFLINE 重建。这不证明 OS 进程锁、持久启动或真实 macOS 后台运行。

来源丢失丢弃排队观测，通过既有来源路径清除配额可用性，并清空有界 R2 发布前 notice code 队列：这些 transient notice 属于已丢失的来源 epoch，不能重新绑定到后续 OFFLINE 快照。这不会改变持久快照真值，也不修改已由 R1 wire session 接管的 notice；其替代与链路行为仍由 R1 定义。生命周期自然老化到 OFFLINE，不刷新 last-seen。同 epoch 恢复保留 ordinal 与退休历史。stop 清除来源 ID、reducer 槽位、link/notice/ACK 状态。没有 detached task、login item、LaunchAgent 或用户配置。

rollout poll 包装保留既有 parser 与来源配额/reset 路径。仅 abort 生命周期流不能建立活动 turn，因此拒绝，不能伪造 start。文件缺失令来源不可用。合成解码配额经 CompanionCore 验证准确 300/10080 语义、15pp candidate/confirmation、到期、缺失与 reset 持久化。存储的 detector 状态不能独自令配额可用。

缺失、损坏、不可写 store 通过枚举诊断。原子写失败保留有效内存配额，标记 durability 降级，不制造 reset 确认。后续配额/发布调用即使来源值不变，也至多重试一次最新 detector 状态，没有后台重试循环。成功写清除降级。成功持久化的确认防止重启重复 reset notice；写失败无法保证重启 dedup。检测算法仍为既有算法。

诊断仅含饱和计数器、来源 profile/可用性、运行状态、队列/session/turn 数量、candidate/confirmed mask、持久化 load/write 结果与降级、批准的 reset marker。没有 string/blob/format-string 扩展。测试检查实际序列化记录并证明缓冲/诊断失败不改变真值。原始 ID 仅在有界本地 registry/queue 内存中，stop 时清除。

## 快照/notice 集成与 R3 输入

通知只变成 R1 枚举 code。runtime 保留自上次发布后至多四个 code，发布完整当前类型快照，然后把 code 绑定该快照排队。快照发送失败保留 pending code；当前快照 ACK 前不能发送 notice。新快照按 R1 替代旧 wire notice。丢失/重放 notice 不改变真值。

R3 必须在独立测试的产品安全谓词通过后提供获授权传输，再提供新的非零认证 generation。connected/subscribed/encrypted 或 donor `tx_ready` 不能单独授权。`companion_runtime_link_ready` 是注入主机 seam，不是认证；协商准确 R1 v1/caps7，无效 HELLO 保留原状态。link loss 清除协商、revision、notice 与 ACK。runtime 在其生命周期拒绝 generation 重用；R3 还须保证跨进程重启唯一性并拒绝旧 link callback/frame。

恢复后，新协商与完整当前快照先建立真值，再信任 notice。send OK 表示有界传输接收所有权，不是 peer 已收到；WOULD_BLOCK/disconnect 不推进 revision。R3 拥有有界调度/重试 deadline、teardown/framer reset 和 backpressure。它须保留 R1 frame 上限，测量无 PSRAM 的 ESP32-C3 内部 heap/stack、认证链路缓冲、MTU/分片、最大快照传输、RF/reconnect 与物理队列压力。UUID、MTU、bonding/MITM/SC 策略、配对 UX、物理资源验收仍属 R3。R2 没有实现 BLE 或设备代码。

## 验证与授权边界

独立套件覆盖 registry 容量/不混同/退休/重启；adapter 版本/身份拒绝；独立顺序、重放、延迟终止、活动 attention、成功/失败/abort、stale 替换；诊断表示/饱和/失败；启动回滚/lease/队列/来源丢失恢复；完整假传输投影/ACK/重放/backpressure/reconnect 以及 reset 重启、损坏、原子写失败恢复。Runtime 回归证明 CURRENT_HOOK 在启动、enqueue 和 recovery 后仍不可用，SYNTHETIC 则保持主机测试可用。假链路回归先生成待发布 ATTENTION 与 ERROR notice，再丢失来源、发布并 ACK 后续 OFFLINE 快照，证明两个旧 notice 均不可发送。全部进入 `tools/validate.sh --static`，并运行不变的 R1/reducer/quota/persistence 测试。

主机行为为合成，不能证明实时 Hook 投递。生产映射保持未验证/不可用，直到另行授权的观测建立当前 schema、身份、来源顺序、显式结果与 attention clearing。最小后续实时任务是可审查的纯元数据 Hook collector/配置 diff，经用户同意启用后，执行批准的 start/success/failure/abort/并发/attention 场景。不持久化或转发原始私人内容。App Server 凭据请求与持久启动需另行授权；不影响完成本次自主主机测试。本主机 gate 未运行固件 build 或设备测试。R3、commit、push 仍未授权。
