<p align="right">
  <strong>简体中文</strong> · <a href="codex-r1-contracts.md">English</a>
</p>

# R1 主机契约与 R2 进入计划

本契约落实[权威架构](codex-ambient-dashboard.zh_CN.md)的 R1 gate，起点为 R0 checkpoint `4784877fd707477b86dec746f127db0de3c02281`。它定义合成、与传输无关的边界。生产 Codex Hook schema、权限、配额采集、认证传输、固件内存占用及设备行为仍未验证。进入 R2 需要 Control Room 单独授权。

## Hook 归一化输入

`companion/mac/codex_hook_contract.h` 接收已解码元数据。其枚举名是**归一化事实**，不声称是当前 Codex Hook 的事件名或字段。R2 必须证明来源 adapter 后才能选用 `VERIFIED`；不可用或未验证来源不产生事件。adapter 不得把没有错误、工具完成、`Stop` 或普通 session 结束视为已验证的 turn 成功。

| 事实 | turn 要求 | canonical 效果 |
| --- | --- | --- |
| SESSION_IDLE | 不存在 | 清除 active turn；IDLE |
| TURN_STARTED | 非零 | 设置/替换 active turn；WORKING |
| TURN_SUCCEEDED | 匹配 active turn | SUCCESS；DONE |
| TURN_FAILED | 匹配 active turn | FAILED；IDLE |
| TURN_ABORTED | 匹配 active turn | ABORTED；IDLE |
| ATTENTION_REQUIRED | 不存在 | ATTENTION；保留 active turn |
| ATTENTION_CLEARED | 不存在 | 有 active turn 时 WORKING，否则 IDLE |
| NON_LIFECYCLE | 忽略 | 无 reducer 输入 |
| 未知 | 拒绝 | 无 reducer 输入 |

每个 lifecycle 事实必须有 session identifier 和非零的 per-session sequence。session/turn ID 是借用的 1–127 字节 span，不允许内嵌 NUL。它们是主机 opaque 数据，不进入 wire，不要求 Unicode/文本解释。调用 mapper 前验证全部 span。注入 mapper 必须在 reducer 保留状态的生命周期内给出稳定、唯一、非零 numeric key；碰撞或容量失败必须拒绝。契约没有原始 payload 内容字段。映射拒绝时清零输出，不修改 reducer。

mapper 不制造顺序。R2 必须证明来源顺序，或通过每 session 唯一串行 ingestion 路径分配 ordinal。保留该 session 状态时 ordinal 不得重启，不得回绕。缺少已验证 turn ID/outcome 的 terminal 必须不可用。不支持的权限观察保持不可用；已验证权限观察只能转为无内容 ATTENTION，不能成为审批决定。

只读检查 S3 commit `90972308293076e9b321e90880ccc891a3d7c10b` 的 `tools/windows_buddy_controller/codex_hook.py`，发现 `UserPromptSubmit`、tool hooks、`Stop`、`SessionEnd` 映射，以及 project/model/tool/hint/message 和 approval 字段。这仅是模式证据：不导入 donor 代码或这些字段，`Stop → complete` 假设不能证明产品成功。现有 rollout watcher 保持独立，只发出此前已验证形态的 `turn_aborted` lifecycle。

## 现有 lifecycle 与容量

复用 `ambient_reducer`，不另建状态模型。不同 session 的 sequence 独立，同一 sequence 可同时用于两个 session。保留 session 拒绝 sequence ≤ 最后接受 sequence。terminal 指向其他/不存在 active turn 时拒绝，且不消耗 ordinal。新 start 替换 active turn，旧 turn 无法完成新 turn。failed/aborted 永远不能成为 DONE。

聚合优先级 ATTENTION > WORKING > DONE > IDLE；没有新鲜 session 时 OFFLINE。freshness 边界包含截止点，elapsed > freshness 才 stale。DONE 在 elapsed ≥ hold 时失效。attention 保留 active turn，清除后回到 WORKING。倒退单调时钟被钳制并在本地报告，防止状态重新变新或反转过期。

不驱逐 fresh slot。耗尽返回 NO_CAPACITY；最旧 stale slot 可被新 identity 复用并建立新 sequence baseline。顺序保护限于保留的 identity。R2 必须防止已退役 identity 的延迟 stream 被当作新 session 重引入；不存在无界 tombstone 历史。Companion 最大八 session、十六 notification dedup key；通用 reducer/dedup 仍由调用方限定容量。Companion 现在保留 OFFLINE，不再因自身仍存活而把缺失/过期来源改成 IDLE。主机进程可用性与 Codex 来源真值是不同事实。

## 配额与重置

只支持来源给出的 300 分钟和 10080 分钟窗口。缺失窗口独立不可用；`reset_marker <= now_unix_seconds` 时该窗口过期并清除 reset baseline。不支持窗口忽略。存在 used percentage 时必须为 [0,100] 有限值。无 token 估算，无 remaining-percent 字段。无关 bucket 被忽略，不覆盖已有 Codex 真值。无效/不可用 Codex 观察取消相关 baseline/candidate，恢复后重新建立 baseline。

`AMBIENT_QUOTA_PRODUCT_RESET_DROP_PERCENT` 固定为 15 个百分点，Companion 要求该产品设置；通用 detector 仍可配置。reset 需要下降 ≥15pp、marker 大于此前可比较观察的最高 marker，然后下一次可比较观察维持同一 candidate marker 和足够下降。candidate 本身不通知。marker 倒退/恢复、小幅下降、usage 上升、缺失数据或失去确认均不能制造 reset。两个窗口独立确认，确认后的重启持久化防止重复通知。

现有无内容 reset store 保留 baseline、highest marker、candidate，验证加载数据并原子写入。损坏/缺失/不可写持久化不会制造 quota 可用性或成功 reset。Companion 报告 load/write 错误；写失败不否定内存中合法的来源观察。写失败后重启不能承诺 durable dedup，R2 必须暴露有界诊断并测试恢复。保存 reset state 不等于缓存来源 quota 可用。

## 项目自有 wire v1

`ambient_wire.h` 是完整允许语义模型：HELLO=1、完整 SNAPSHOT=2、一次性 NOTICE=3、ACK=4。字段都是 numeric enum/bitset/counter，没有 string、blob、JSON object、extension map 或 generic/opaque payload。旧 `AMBIENT_MESSAGE_CONTROL` 保留但始终拒绝。Companion 任意 snapshot 编码回调已替换为 typed projection/publishing。

禁止 prompts、transcripts、assistant content、tool output、shell commands、diffs、认证材料、原始授权 payload 和 privileged approval decisions。也不含 project/activity/model string 或原始 session/turn ID。未知 kind/field、多余分隔符、追加数据、不支持编码和未知 enum 全部拒绝。权限观察只能化为 ATTENTION。ACK 只确认语义投递，不能触发 Codex 或 privileged operation。

编码为固定宽度的**大写 ASCII 十六进制**，字段以 `|` 分隔，帧以单个 LF 结束。decode 接收去掉 LF 的行。下表位宽必须严格匹配；小写、空白、CRLF、NUL、非 ASCII/UTF-8 内容、正负号及前缀均不支持。没有可扩展字段语法，不序列化 struct padding。

| kind | 按顺序列出字段（十六进制位数） | 含 LF 帧字节数 |
| --- | --- | --- |
| 公共前缀 | kind(1)、major version(2)、generation(16) | 仅前缀 |
| HELLO | 前缀、offered capabilities(8)、required capabilities(8) | 40 |
| SNAPSHOT | 前缀、revision(16)、status(1)、fresh(1)、working(1)、attention(1)、done(1)、quota presence(1)、short used(4)、long used(4)、short reset marker(16)、long reset marker(16) | 95 |
| NOTICE | 前缀、notice ID(16)、snapshot revision(16)、code(1) | 58 |
| ACK | 前缀、target kind(1)、ID/revision(16) | 41 |

版本是一个 major integer，目前为1；不支持版本 fail closed，不隐式降级。capability：snapshot=bit0、notices=bit1、ACK=bit2，v1 三者全部必需。peer 宣告 offered/required bitset，协商取已知 bit 交集，检查双方 required；未知 optional offered bit 忽略，未知 required bit 拒绝。目前无可选 application capability。协商前不接受 state/notice/ACK；已协商 session 不能原地重新协商。

非零 64-bit generation 标识一次安全传输 session。R3 必须在重连与进程重启时认证并建立新 generation，不假定 wall-clock 唯一。R1 不提供认证或随机数。初始化清除协商状态、收到的真值、revision、ACK、notice ID 和队列。其他 generation 的延迟记录拒绝。

snapshot revision 为每 generation 非零递增 64-bit counter，不回绕。只有 transport send 返回 OK 后才算 **emitted**：含义是有界 transport 接收所有权，不代表对端已收到。编码失败、背压、断线不推进 revision。新完整 snapshot 替代所有 pending 一次性 notice。receiver 只接受更新的完整 snapshot 并原子替换 retained truth；完整 snapshot 允许 revision 缺口。

只有最新实际 emitted snapshot revision 可 ACK；stale/duplicate/zero/future ACK 不改变状态。snapshot emitted 后可入队 notice，但当前 revision ACK 前不得发送。notice ID 非零递增且不回绕，每条引用当前 snapshot revision。receiver 要求恰好当前 revision 和更大 notice ID，允许缺口，丢弃重复/replay。notice 不改变 retained snapshot。只允许 ACK 实际发送过的队头 notice；重试使用相同 ID，由调用方限定调度。队列满时丢弃新 notice，真值不变。断线后必须重新初始化 session、reset framer，重新协商并先接收 full snapshot 才可信任 notice。R3 必须落实安全 disconnect/reset 与有限 retry timeout；R1 没有 worker、timer 或 reconnect loop。

snapshot status/count 必须符合 reducer 优先级、counts ≤8、分类 counts 总和 ≤fresh。quota presence bit 对应固定两窗口；缺失窗口的 wire usage/marker 为零。来源 used percentage 仅在 wire 展示时舍入到 0.01 个百分点（0–10000 basis points）；reset 仍使用原 double。不发送 quota 估算或推导 remaining。notice code 仅 ATTENTION、COMPLETED、ERROR、SHORT_RESET、LONG_RESET，无 body/permission details。

## 上限与恢复

| 项目 | R1 主机硬上限 | 依据/后续测量 |
| --- | --- | --- |
| 原始 Hook session/turn ID | 各127字节 | 本地 opaque identity admission 预算；长 ID fail closed，变更须有来源证据及审查 |
| numeric field | 16 ASCII 字节 | 覆盖 uint64，无可变位宽 parser/integer overflow |
| 允许 state payload/frame | 94/95字节 | 固定 snapshot 语法的精确最坏情况，不依赖数值 |
| line/frame | 128/129字节 | 比语法多34字节空间；空余不授权新字段 |
| framer storage | 129字节 | 128字节行加本地 NUL；encoder 不需要 NUL |
| retained typed snapshot | ≤64 host 字节 | 编译期 struct 预算，wire 始终95字节 |
| typed semantic message | ≤64 host 字节 | 编译期预算，无动态 payload |
| protocol session | ≤512 host 字节 | 编译期预算，包含四条 inline notice 和 retained truth |
| sessions/lifecycle dedup | 8/16条 | 有界并发 dashboard，平均每最大 session 两个 dedup key；dedup 驱逐后仍由 reducer 拒绝旧 lifecycle |
| pending notice/transport queue 证据 | 4条/516字节 | host fake transport 四个129字节 slot；允许有界 transient loss，避免无界 backlog |
| rollout host file line | 现有1 MiB | 单独的桌面文件 parser 预算，不是 semantic frame/device allocation |

八 session、十六 dedup、四 notice 是保守 R1 admission 上限，不是实测 ESP32 需求或 donor MTU。R3 必须测量 C3 internal RAM 高水位、parser/queue stack/heap、认证链路与 BLE buffers、所选 MTU/fragmentation 开销、95字节最大 snapshot 传输和无 PSRAM 下背压/重连负载。可在记录兼容性/capability 影响后收紧上限，不能静默超过任何 R1 硬上限。产品 preference key 必须单独审查 schema 变更。

现有 caller-owned framer 消耗至首个 LF，支持任意 split/coalesced 输入。空输入 NEED_MORE。恰好上限的行有界；超限丢弃至 LF 并报告一次，随后恢复下一帧。limit mismatch、不可能 storage size 或损坏 retained length 返回 invalid 且不访问 buffer。truncated record 始终有界，在 session/framer reset 时丢弃，不接受 EOF 为完整记录。empty/malformed/unsupported record 经严格 decoder 拒绝且不改变真值。重复错误不增加 parser state。

## 本地 settings 与 unpair

`ambient_settings.h` 冻结 schema1，**无获准 preference key**，避免臆造 UX 设置。只原子接受精确版本和空 keyset。未知/损坏版本或 key 保留现有设置并 fail closed，不猜测 migration。未来 migration 必须明确有界 schema 测试。

验证请求前清零调用方提供的 local-effect 输出；任何拒绝都使全部 effect flag 为 false，即使前一次是成功 unpair/factory reset。unpair 必须由明确的设备本地请求触发，无远程 unpair/factory-reset wire opcode。policy effect 清除 bond 与 link/session，保留 product settings 和全部 host quota/history。factory reset 是独立明确本地操作，可额外清除 product settings。二者不能授予审批、提交 Codex、清除 host quota persistence 或执行 command。R1 只返回有界 success/failed/unavailable acknowledgement code；未来 executor 必须完成 effects 才确认成功，否则 failed/unavailable。没有实现 bond deletion、NVS write 或 settings UI。

## 验证证据

`tools/validate.sh --static` 以 C11、`-Wall -Wextra -Werror` 编译全部新增 pure-C suite 和受影响现有 suite。覆盖不可用/未验证 Hook、ID/mapper 拒绝、全部归一化事实、独立相同 sequence、多 turn 替换、容量/stale 恢复、attention overlay、outcome 真值、freshness/clock、quota 非有限/范围/threshold/reset/rebaseline、原子持久化失败与重启、全部 wire kind、严格位宽、malformed/truncated/extra/forbidden 字段、不兼容 version/capability、整数极值/禁止回绕、split/coalesced framing、重复 oversized 恢复、背压/断线、未发送/future/duplicate ACK、notice overflow/replay/drop、重连 full-snapshot 顺序。拒绝后有合法恢复测试。静态 firmware-layout 工具使用合成 artifact，不能证明 firmware build/device 行为。

## 可执行 R2 计划 — Companion Production Truth

R2 只能在 R1 独立审查通过并获 Control Room 明确授权后开始，以当前仓库为准，不使用旧 U4/U5/U6 chat plan。

### A. 无需改用户配置的自主工作

1. 围绕已验证归一化契约实现 host adapter，注入 identifier registry、每 session 串行 sequence 与 outcome mapper。合成 start/success/failure/abort/attention、并发、来源消失、退役 stream 延迟 fixture；unsupported 保持 unavailable。
2. 检查获准安装的应用/CLI 文档和只读 source/version metadata，建立候选来源接口。记录 version/path/schema evidence，不修改 Hook 配置，不将 private payload 复制到诊断。
3. 扩展 Companion 从 source observation 到 reducer/quota persistence 再到 typed snapshot/notice 的 integration tests。覆盖 restart、expiry、full-resync 和 bounded diagnostics；保持 fake transport，R2 不引入 device link。
4. 测试原子 reset store、损坏/不可写 path、candidate/confirmation 前后重启、写失败诊断和 rebaseline，不臆造 quota truth。
5. 准备可逆 project-local startup/background 代码，测试幂等 start/stop、crash、source unavailable、stale recovery、bounded queue。持久 login item/daemon 仍需单独同意。

### B. 明确同意/可能仅人工解决的阻塞

| 证据 | 精确采集与验收 |
| --- | --- |
| Hook version/schema | 记录当前 Codex build/CLI version、支持 Hook 名和 schema/version 来源；安装/修改用户 Hook 前取得同意。只收集 allowlisted metadata 是否存在及类型、合成/脱敏实例；证明 session/turn identity 和串行 ordering |
| outcome mapping | 获授权真实 turn start、success、failure、user abort，证明 terminal outcome 与 turn 关联；模糊 `Stop`/termination 不可判 SUCCESS |
| concurrent sessions | 两个重叠真实 session，各自 sequence baseline；延迟/重复记录、session retirement/restart；证明 outcome 不跨 session 污染 |
| permission/attention | 获准观察 attention-required/clearing，保留 active turn，只采集 bounded semantic metadata。macOS trust/accessibility 弹窗或无法观察 clearing 是人工 blocker；不自动审批 |
| quota source | 识别当前权威生产来源、version/bucket、来源提供的300/10080窗口、used percentage 和 reset timestamp；缺失、过期、不可用行为。无 token/remaining 估算 |
| reset persistence | 真实/可重现来源 marker advance +≥15pp drop +confirmation，确认前后重启、写失败与恢复；不强制/伪造 quota reset |
| startup/background | 持久 login/daemon 改动前获用户同意；观察 start/stop/restart、source outage/crash recovery，证明 stale 回到 OFFLINE |
| diagnostics/privacy | 明确观察范围；验证 log 只有 bounded code/counter/window marker，无 prompt/transcript/tool/command/diff/auth/approval payload；检查实际产生的 sanitized output |

### C. 未支持路径保持不可用

未知 Hook name/schema、缺少 identity/outcome/ordering、未验证 attention clearing、不可访问 quota endpoint、模糊 session end、不支持 App Server/Accessibility 来源必须 unavailable/unsupported。不推断成功、不重建 private content、不猜 quota、不绕过同意、不装 fallback。人工 blocker 停止受影响路径，报告精确缺失证据；已授权 R2 的独立合成工作可继续。R1 未开始任何上述 live integration。
