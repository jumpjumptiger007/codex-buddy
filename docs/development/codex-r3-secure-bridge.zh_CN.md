<p align="right">
  <strong>简体中文</strong> · <a href="codex-r3-secure-bridge.md">English</a>
</p>

# R3 安全桥接与会话层

基线：已审查 R2 `dd873ec53d543f1a41a9f47ddc932d2fbdaf6b49`，main，ahead8/behind0。
Iteration 1 Companion generation 架构与 iteration 2 自主实现已通过独立复审。
Iteration 3 安全架构与加密实现已通过独立复审。Iteration 4 接通 production CoreBluetooth event 与有界 central pump，已通过独立复审。Iteration 5 准备独立可运行验收端点。Iteration 6 fingerprint 与 write instrumentation 已通过复审。Iteration 7 deferred disconnect 已通过复审。Iteration 8 TX timeout/stale-owner 修正已通过复审。Iteration 9 published-owner termination lifecycle 已通过复审。Iteration 10 补齐 provisional physical connection ownership；补充复审撤回其初次 DONE。Iteration 11 修正已观测断连退役标记，等待独立复审。R3 尚未 DONE。
未开始 R4，没有 commit、push 或物理 BLE 操作。

## 边界与 donor 提取

`R2 Companion truth → R1 wire → authenticated session → bounded byte transport →
NimBLE/GATT/GAP → Passport`。Pre-R1 multiplexer 只管理身份与握手记录。
BLE 不解释 Codex 生命周期或 quota 语义。不会传输 prompt、transcript、tool/assistant
内容、commands、credentials、raw session identifiers，不增加审批或自动提交 turn。

Espressif `esp-desktop-buddy` S5 commit
`b6bac05db208717676e70180e5269d79f32b2d68`，Apache-2.0：改编
`components/esp_desktop_buddy_transport_ble/buddy_transport_ble.c`、
`buddy_transport_ble_gap.c`、`buddy_transport_ble_gatt.c`、`buddy_transport_ble_tx.c`，
以及 public transport header/private state header。`components/ambient_ble` 替换 API、
UUID、RX queue、TX ownership、安全观测与生命周期。保留 LICENSE 与 provenance NOTICE；
pinned donor 没有 NOTICE。没有 donor semantic core、content/permission model 或 build
依赖。Source boundary test 与实际固件编译验证提取边界。

## 方向化信任与授权

Passport 通过现有 NimBLE 谓词认证 Mac：connected、TX subscription、encryption、
authenticated MITM、persisted bond、LE Secure Connections、有效且本地接受的 central
identity，以及当前非零 link incarnation。新 peer enrollment 需要显式 local pairing
approval delegate；当前产品 main 未提供 delegate。Bond、donor tx_ready 或单独 encryption
均不足以授权。Accepted central identity 以7字节 project NVS record 持久化；queue/store
失败拒绝信任，不以擦除整个 NVS 恢复。

Mac 用显式 pin 的 P-256 public key 与 fresh challenge-response 认证 Passport。
新 host 谓词要求当前 connection、正确 service/characteristics、notify subscription、
非零 incarnation、pinned Passport identity、application auth complete，以及一致的
认证 incarnation。否则 `companion_secure_bridge_open` 不分配 generation、不发 HELLO。

CoreBluetooth 公开 API 不提供 per-link bond/encryption/MITM/SC 事实。
`CBPeripheral.identifier` 和 opaque peer token 只是 transport lookup key；OS Bluetooth
授权是隐私许可，均不证明 cryptographic identity。Mac 不再声称拥有这些缺失观测。
应用认证证明 Passport application identity，**不证明** macOS 使用了 SC/MITM。
Passport 独立执行 BLE 安全策略。公开 transcript hash confirmation 只确认顺序和绑定，
不是第二套 Mac identity proof。这遵循已批准的不对称信任，不猜测 BLE 事实。

## Passport 持久身份

`ambient_identity` 将有界生命周期策略与 ESP crypto/NVS backend 分离。
`ambient_identity_esp` 用 IDF 支持的 mbedTLS 生成 P-256，仅公开 canonical SEC1
uncompressed public65（`04 || X32 || Y32`）与 digest signing。Private scalar 不穿越
公开身份 API；没有 identity reset API。

`passport_id` namespace 保存 `created=1` 与134字节 versioned `PID1` record：
magic4、scalar32、public65、version1、SHA25632。先持久提交 creation marker，再创建 key。
Partial marker/record、错误长度/version/checksum/scalar、derived public 与持久 public
不匹配均 fail closed。Existing corrupt identity 不自动重新生成；creation/write/commit
失败也 fail closed。Ordinary local UNPAIR 仅访问 `ambient_r3` accepted-peer trust 与
NimBLE bond store，保留 application identity 和其他设置。

ESP singleton factory 必须在 **BSP、RF 或 ADC 初始化前**运行。只在 seed mbedTLS
CTR_DRBG 时启用受支持的 bootloader entropy source，随后禁用，避免与其他外设冲突。
Nonce 与 ECDSA blinding 使用 DRBG。达到配置的有限 request budget 后，automatic reseed
因 entropy source 不再归本模块所有而 fail closed；没有弱随机 fallback 或无限重试。
Re-entry 需要安全 early-startup entropy ownership（通常 reboot），crypto 调用需串行。
这是 application identity，不是 hardware attestation；没有建立物理 flash extraction
抵抗能力、secure boot 或 NVS encryption。

## Mac pin 持久化与显式 enrollment

`companion_pin_store` 使用调用方显式指定的路径，不安装默认用户配置，生命周期持有
nonblocking exclusive flock。要求 owner UID、regular file、最终路径不是 symlink、private
permissions。70字节 `PIN1` record 包含 presence byte 与完整 canonical public key。
只有 fresh lock marker 才能创建 empty store；existing missing/corrupt record fail closed。
持久替换依次 private temporary file、fsync/F_FULLFSYNC、atomic rename、parent fsync。
Durability 不确定时 poison owner，close/reopen 前不能读取 key。删除或 rollback store/lock
不在持久化保证内。

首次 pin 必须在持久化前取得调用方显式 approval callback；没有 silent TOFU。
Enrollment/reconnect 不能覆盖 existing pin；mismatch 需在正常 API 之外另行授权 trust reset/
re-enrollment。SHA256 派生的24位 hex（96-bit）fingerprint 仅用于独立人工比较，完整 public
key signature verification 才是授权依据。R4 提供最终 comparison UX；本轮只提供 callback seam。

## Pre-R1 协议与顺序

现有 GATT byte channel 在 R1 前携带有界 binary records：header
`50 41 01 TYPE LENGTH_BE16`（`PA`，version1），payload 固定长度如下。
Auth payload 不进入 `ambient_wire`，R1 message kind/field 均不变。

| Type | Payload | 总字节 |
| --- | --- | --- |
| 1 enrollment request | empty | 6 |
| 2 enrollment public key | canonical public65 | 71 |
| 3 challenge | Mac CSPRNG32 | 38 |
| 4 response | Passport nonce32 + ECDSA raw r32/s32 | 102 |
| 5 confirmation | transcript SHA25632 | 38 |

签名 hash 是 SHA256：精确 ASCII domain（不带 NUL）
`yliu.tech/Passport-auth/v1:Mac-challenge:Passport-proof:Mac-confirm`，随后 Mac challenge32、
Passport nonce32。Domain/version 与 role/direction 均绑定。
Existing pin → fresh challenge → fresh Passport nonce/signature → pinned-key verification
→ 同 transcript confirmation → application auth complete → fresh durable Companion generation/
HELLO → Passport capability negotiation/reply → full snapshot → ACK → notices。
Enrollment 在 challenge 前增加显式 request/key presentation 与 approval/persistence。
Passport 只有 BLE policy 与 explicit enrollment gate 通过后才允许 key presentation。

Host 仅在 confirmation 被有界有序 TX queue 接收后标记 auth complete；confirmation 在
同队列中先于 HELLO。Passport 仅在当前链路 valid confirmation 后打开 R1 responder。
Byte multiplexer 支持同一 ATT chunk 中 confirmation 与 HELLO 顺序 coalesced。
Failed/incomplete auth 不打开 R1、不改变 product truth。Duplicate/out-of-order auth、错误
signature/nonce/length、stale incarnation、security/subscription loss 与 disconnect 清理
认证/会话 ownership。R1 开始后拒绝 auth records，不在 active R1 session 内重新认证。
Reconnect 每次使用 fresh nonces 与 fresh generation。

## Generation 与 R1 重同步

Companion 拥有 `ambient_generation`：durable epoch32 + host CSPRNG salt32，即使 salt
重复也非零且严格递增。Read/write/RNG failure 或 epoch exhaustion 阻止 HELLO。
生产 `companion_generation_store` 使用 explicit path、exclusive owner lock、atomic durable
replacement 与 OS randomness。Existing missing/corrupt state fail closed；删除/backup
rollback 不支持；restart tests 保留 store。

Passport 没有 generation allocator 或 gen_epoch。应用认证后进入 WAIT_HELLO，绑定首条
合法 fresh Companion HELLO，协商 capability，回复同 generation HELLO。Companion 只接受
该 reply，随后调用不变的 R2 link_ready 并发布 full snapshot。Premature ACK/snapshot/notice、
mismatched/stale/second HELLO 拒绝。Reply failure 消耗 RAM replay marker，需要新 session。
Link incarnation 与 semantic generation 独立；snapshot ACK 门控 notices，latest snapshot
wins，断连期间的变化进入重连后的 full snapshot。

## Transport、生命周期与上限

| Attribute | 冻结 UUID / direction |
| --- | --- |
| Service | `0913bf62-0732-53ce-b7c7-c90db3db3881` |
| RX | `3b9da8eb-4a29-53bb-a219-d4ad4a6e1c11`，central writes，encrypted/authenticated GATT |
| TX | `90127ebc-fe65-5b74-8e8a-41f58a8c27af`，Passport notify，必须 subscription |

UUIDv5 namespace：`https://yliu.tech/ai-passport/ambient-ble/v1/`；没有新增 characteristic。
Preferred MTU23 → value20；95字节 snapshot 用5次 central writes，129字节 central frame 用7次 writes，
102字节 Passport auth response 用6次 notifications。Byte-stream fragmentation，不增加 per-fragment header。
R1 上限仍为 snapshot95、line128、frame129、notices4、sessions8。Passport RX ring516，
TX 单slot≤160；Mac RX/TX 各516。Auth input102/output102，public65/challenge32/nonce32/hash32
均为固定数组。这些是源码/host 上限，不是 NimBLE heap/stack high-water 实测。
TX task4096、NimBLE host default4096，不假设 PSRAM。

Central 管理串行 discovery/subscription、有界 write/backpressure、token/incarnation、auth
multiplexer 与 bridge；tick 有限工作，stale callbacks 不能重开 state。CoreBluetooth wrapper
使用已连接且授权的 peripheral 与显式 pin owner，公开 Security verify/SecRandomCopyBytes/
CommonCrypto SHA256。Gate 不创建 CBCentralManager、不 scan/connect；调用方管理串行 queue
和 manager connect/disconnect events。Queue 接收不代表 RF delivery ACK。

Iteration 4 在 auth 启动（包括已订阅后配置）、订阅就绪、接收 value 和公开
`peripheralIsReadyToSendWriteWithoutResponse:` callback 驱动同一 portable pump。
每次最多64 ticks，每 tick 至多一次 write（production chunk20）与129字节 RX；
该预算覆盖 production chunk 下有界队列与 auth/HELLO 输出。WOULD_BLOCK 保留 TX
并立即返回，write-ready 恢复。Stale peripheral/incarnation 不 dispatch；经 host 测试的 notification 接收入口对当前 link 的 empty/oversized/full RX
失败 invalidate；stale notification 不能关闭更新的 link。外部 bridge publication 产生 TX 后，调用方在同一串行 queue
调用 `pumpPendingBytes`。不安装 timer 或无限 retry。CLOCK_MONOTONIC 毫秒与 Unix
时间只用于 R2 freshness，不分配 generation/crypto identity。Portable helper 测试
覆盖 challenge write、102字节分片 response、confirmation-before-HELLO、backpressure/
resume、stale/teardown 与64 tick上限；wrapper 源码接线检查和实际 Objective-C 编译
是独立证据，不证明真实 CoreBluetooth delegate 执行或 RF delivery。

Production peripheral 使用小型 host fake platform backend 编译实际源码，覆盖 allocation/
mutex/queue/NVS/init/GATT/task failure、partial rollback、stop/deinit timeout/retry、trust
commit、unpair/bond removal failure、queue full、mbuf/notify failure、subscription/security
loss、stale worker items、50次 start/stop。Stop failure 保留 recoverable owner，成功 stop
消耗指针。Unpair atomic denial latch 先于可失败的 lock/store，失败后持续拒绝直到 retry。
Notification failure 先清理匹配 link ownership 再 terminate，无无限重试。

当前未提交变更共94 paths；execution_summary/audit 记录准确清单。

## 验证与证据

| 类别 | 状态与限制 |
| --- | --- |
| Portable host | Auth/parser/lifecycle/identity policy、central/bridge/session/generation/store、ring/boundary、全部 R1/R2/existing suites；deterministic crypto doubles 明确为 synthetic |
| Production Mac crypto | 公开 Apple Security P-256 verify、SHA256、secure RNG；RFC6979 A.2.5 known-good/bad vector、wrong key、真实 pin file/lock/restart 与 write/sync/replace fault；warnings-as-errors compile，未执行 Bluetooth |
| Production ESP crypto on host | 实际 `ambient_identity_esp.c` + pinned IDF mbedTLS，真实 P-256 generation/signing + Apple verification、跨 backend auth/fingerprint；NVS/entropy APIs stubbed，不是 ESP 硬件 entropy/NVS 证据 |
| Firmware | 实际 ESP-IDF5.5.3/ESP32-C3 build/archive verify，no-start seam 链接 identity/auth/session entry；最终测量记录在 execution_output |
| Device | UNVERIFIED；没有 scan/connect/permission/pair/flash/erase/unpair/device operation |
| Runtime resources | UNVERIFIED：ATT/security/RF interoperability、internal heap/largest block、mbuf footprint、task high-water、重复物理 reconnect leakage、early entropy/BSP coexistence |

Iteration 4 最终固件 **PASS**：ESP-IDF5.5.3/C3，app967504字节、merged1033040字节。
已验证归档：`build/firmware/a558e561674db44b23200189c45d1f13a8054901f420802fc8688db76f8da992`。
Link seam 不调用 identity creation、BLE 或设备操作。

公开签名向量来自 [RFC6979 Appendix A.2.5](https://www.rfc-editor.org/rfc/rfc6979#appendix-A.2.5)。
Crypto host gate：在临时 CMake 目录构建 pinned IDF `components/mbedtls/mbedtls`，禁用 tests/
programs，再为 `tools/check_r3_crypto.sh` 设置 `R3_MBEDTLS_SOURCE`、`R3_MBEDTLS_HOST_BUILD`。
`tools/check_mac_ble_central.sh` 仅编译；`./tools/validate.sh --static` 包含 portable auth 与
production peripheral fault suites。实际固件使用仓库 `--firmware` gate；不需全局安装或
持久机器配置。

## R4 输入契约与剩余验收

R4 消费 R3 authenticated transport/R1 session，只提供最终 local pairing approval、
independent fingerprint comparison/enrollment UX、navigation 和 rendering，不创建 Mac BLE
transport 或弱化 auth。产品 main 仍为 no-start link seam，没有最终设备集成；R4 未开始。

独立架构/源码复审后，真机验收仍需显式授权：C3 flash、OS Bluetooth permission、secure
pairing、reconnect/wrong-peer/unpair、reboot/unpair 后 identity persistence、pre-R1 auth 与
fragmentation/backpressure，以及 heap/mbuf/stack/reconnect resources。本轮没有这些授权或操作。

## Iteration 5 独立验收准备

[验收端点与实机矩阵](../../acceptance/README.zh_CN.md) 描述独立 IDF app 和需明确调用的
Mac CLI。正常 product startup 仍是 no-start symbol seam。
`companion_ble_central_begin(NULL platform)` 创建 unbound incarnation，再由 wrapper
在 discovery/auth 前一次性 bind 真实 CoreBluetooth writer，不使用 dummy writer。
Portable tests 覆盖 stale/double bind、bind 前不可 I/O。

设备启动在 BSP display/ADC buttons/BLE 前打开 opaque ESP identity。非阻塞 prior
pairing approval 使用 atomic one-shot latch 和有界 display-event queue。实时安全事实
控制有界 worker/authenticated session；测试 display 独立于 product UI。
Mac runner 拥有 manager/filter/selection/connect/disconnect/serialized lifetime；
预先确认 fingerprint 的 callback 只比较，不在 delegate queue 交互。
Cycles1–20、至多 cycles+2 attempts、30秒 deadline，无无限 retry。
R2 source 明确 disabled，transport 验证保留真实 unknown。

只读 BLE metrics 暴露 lifetime RX byte/TX slot high-water 和公开 FreeRTOS task
minimum-free stack 字节，与 stop 串行。Worker 记录 internal heap/minimum-ever/largest
和 disconnected baseline；mbuf occupancy 为 UNAVAILABLE。设备记录 confirmation
先于 HELLO generation binding；Mac 跨 accepted cycles 比较固定 SHA256 challenge/
nonce digest，重复则拒绝，只输出枚举结果。这些 diagnostics 尚非 runtime measurements。

Iteration5 outputs 分别记录 host/static isolation、真实 facts mapping、metrics/fault
suites、Mac executable warnings-as-errors link、实际正常 product 和独立 IDF5.5.3/C3
build。Compile gates 不运行 executable。Device/security/RF/resources 仍 UNVERIFIED；
没有 Bluetooth permission/scan/connect/pair/flash/unpair/erase、R4、commit 或 push。

Iteration5 最终 build PASS：正常 product app967568 / merged1033104字节，归档
`build/firmware/b47c4269cf0a5fbf6588efa5cfe6e4a4592d5c6eb1ef313eb49c937e2a55e054`。
独立 acceptance app962176 / merged1027712字节，保留于
`/private/tmp/codex-r3-i5-acceptance-final`；full SHA256
`700cbfa2e0ece38026d75d4a6febd29b7fa194bb4ec3e52fbc99ebf276acbaad`，
ELF SHA256 `531a038fd3a614e0da13632cef96932116987fa6fd80929ee1fdeab1567094f5`。
生成 manifest 绑定 artifacts/configuration。Mac linked executable 为
`/private/tmp/codex-buddy-r3-i5-mac-acceptance`（NOT RUN）。首次 acceptance build 因
继承的相对 partition filename 失败，现 acceptance SDK defaults 显式指向共享 root
layout，最终 fresh gate 已 PASS；未修改 root partition 或 dependency lock。

## Iteration 6 修正与证据

Enrollment 仅接受恰好24个 ASCII hex 字符，大小写均可，统一为大写后与真实
Passport fingerprint 精确比较。Host tests 覆盖实际 fingerprint、大写/小写、
不匹配、非法字符和超长输入；实际 Apple/ESP crypto gate 也验证此规范化。

Forced disconnect 在清除 link facts 前保存准确 handle 与 connection incarnation。
匹配的 disconnect event 仅在 unpair 完成、没有更新 incarnation、未开始 shutdown
时拥有一次恢复 advertising 的权利。ENOTCONN 使用同一路径；trust-store 清理失败
保持 fail closed，retry 可消费已观察到的 disconnect。使用 production 源码的 fake
backend 测试覆盖 unpair、notification failure、重复/过期 event、shutdown、ENOTCONN
及 retry。恢复 advertising 不恢复授权，也不复用旧 authenticated session。

Central byte boundary 记录独立完整帧长度与成功 platform write 分片数；WOULD_BLOCK
保留数据且不计数。合并排队帧不会误报为一个完整测量帧。Runner 要求首次获得 ACK
的 snapshot 在20-byte payload 下为95 bytes / 五次 central writes。计数表示
CoreBluetooth 接受写入；随后 Passport R1 ACK 是单独的协议证据。AUTH_OPEN 输出真实
ATT MTU。RF 到达及设备观测仍为 UNVERIFIED，须另行明确授权实机验收。

Iteration 6：完整 static/R1/R2/R3 host suites、CoreBluetooth object compile、Mac
acceptance executable warnings-as-errors link（未运行）及实际 Apple/ESP crypto gate
均 PASS。实际 ESP-IDF 5.5.3 / ESP32-C3 正常产品构建 PASS：app967968 bytes，merged
1033504 bytes，archive
`build/firmware/8c777691518457a24d938dde164894f85c7101f84fec64b8d7eb90f5e2a5af08`。
全新独立 acceptance build PASS：`/private/tmp/codex-r3-i6-acceptance`，app962624 bytes，
merged1028160 bytes，merged SHA256
`9b2cd12015f095d3d8846614f8e88f75b18c8ec8ab93338c494aa98bb3b92eb6`。
上文 iteration 5 产物仅保留为历史证据。

Device tests / runtime resource measurements：UNVERIFIED；NimBLE mbuf：UNAVAILABLE。
未 scan/connect、请求 permission、pairing、flash、erase、真实 unpair、R4、commit/push。
R3 为 EXECUTED 等待独立复审，尚未 DONE。

## Iteration 7 deferred disconnect 所有权

状态锁超时不再消费未记录的 disconnect。单个固定 pending slot 在短 C3 FreeRTOS
critical section 中保留 handle 与已发布的64-bit link incarnation；仅处理数值发布，
不分配、不调用 NimBLE。GAP callback 安全发布事件后，即使立即 cleanup 失败也可返回。
现有 TX worker 每轮仅尝试一次，沿用100ms idle wait 与最多20ms状态锁获取时间；
不新增 queue 容量、task、忙重试循环，不解释 R1 内容。

Pending 期间 link facts 否认 peer acceptance，send/read/GATT 拒绝产品数据，peer approval
持久化与 worker commit 也被拒绝。获得状态锁后，匹配 cleanup 重置 RX，最多清空既有
单 TX slot。Forced ownership 跨 unpair/ENOTCONN 保留；unpair 首次获取锁失败、尚未建立
marker 时收到的 disconnect，也由后续 retry 恢复。更新连接的 disconnect 可替换旧
pending slot；旧 incarnation 不得清除更新 link。Shutdown 禁止 restart。Advertising
restart 与 ownership 消费在同一次成功状态锁事务内完成，避免第二次获取锁失败导致
事件停滞；处理期间 critical section 发布的新事件保持 latch，留给下一轮 worker。

实际 production 源码 + fake API 测试覆盖 ordinary/unpair/notify disconnect 首次锁失败、
forced resume 锁失败、ENOTCONN 立即与 resume 两次锁失败、一次性恢复/重复事件、旧pending
与新link/新disconnect覆盖、shutdown，以及 worker cleanup 前 send/read 拒绝。
Host fake critical section 是串行 no-op；真实 C3 调度/critical section 仍为 Device
UNVERIFIED。不代表任何新增实机授权。

Iteration 7 Host：完整 static/R1/R2/R3 production BLE lifecycle/fault suites、
Apple/ESP crypto gate、CoreBluetooth object compile 与 Mac acceptance warnings-as-errors
LINK-only gate 均 PASS。Device endpoint 未运行。全新 IDF5.5.3/C3 acceptance app963088
bytes / merged1028624 bytes，保留 `/private/tmp/codex-r3-i7-acceptance-final`；merged SHA256
`b590f1818a58437a5ec6cea9c610598f3882ffe5d9641db2c39fd2b58b7a37fc`。
实机互通/安全及运行资源仍为 UNVERIFIED。

Iteration 7 最终正常产品 IDF5.5.3/C3 构建 PASS：app968368 bytes，merged1033904 bytes；archive `build/firmware/7823d0ae0ace85a093873dae6693435f4dec6c02434e1e0dc6ca0558756de137`。正常产品仍不启动 R3。

## Iteration 8 TX 不确定性与恢复

Production TX snapshot 保留 ESP_ERR_TIMEOUT，不再将它误分类为已证明过期的
ESP_ERR_INVALID_STATE。已入队帧无法确认所有权时，不会在仍授权的 link 上静默丢弃：
其 handle/incarnation 经既有固定 slot 请求 fail-closed lifecycle recovery。短 critical
section 在设置拒绝状态前比较准确 item incarnation 与当前 published owner；即使状态
锁不可用，旧 item 也不能请求断开新连接。

同一 slot 区分“请求 forced termination”与“已观察 disconnect”。Worker 获取状态锁后
建立 forced ownership、清除 authorization/RX/TX，并仅请求一次 termination。成功请求
等待匹配 disconnect；ENOTCONN 在同一事务中记录完成并仅恢复一次 advertising。实际
收到 disconnect 会取代已排队的 termination 请求。Generic TX recovery 不设置或清除
显式 local-unpair latch。其它 terminate error 保留 fail-closed ownership，等待真实
断开或显式本地 lifecycle retry；不形成无限 termination retry loop。

实际 production 源码测试覆盖 notify failure 后 recovery-lock 失败、成功 termination
与随后 callback、ENOTCONN、当前 queued frame 的 snapshot-lock 超时、旧 queued timeout
不能影响更新 incarnation、pending 期间 send/read/GATT/trust-store 拒绝、重复 request/
callback，以及所有 iteration 7 回归。实现期间修复两次中间 host assertion（更新 owner
的 pending 所有权及 fake worker 下一轮重试）；最终完整 static gate PASS。
不增加 queue/task/allocation，不改变 security/R1 policy。Device/runtime 仍 UNVERIFIED；
Mac acceptance 仅 compile/link。未操作 hardware/permission/R4/commit/push。

Iteration 8 最终验证：完整 static/R1/R2/R3/BLE faults 与 Apple/ESP crypto PASS；
CoreBluetooth compile、Mac acceptance warnings-as-errors LINK PASS（未运行）。
正常产品 IDF5.5.3/C3 build PASS：app968544 / merged1034080 bytes；archive
`build/firmware/31bdac1ec8fe159fdb220183c352db2fba22b3bb1a95d87eedc6d0e6fb6a6c95`。
全新独立 acceptance build PASS：app963280 / merged1028816 bytes，保留
`/private/tmp/codex-r3-i8-acceptance`；merged SHA256
`065cb48a0fb95434603319791f52c573e1a632154d872f36ec50c579c09a1806`。
Device/runtime UNVERIFIED；mbuf UNAVAILABLE。R3 等待独立复审，尚未 DONE。

## Iteration 9 有界 termination lifecycle（当前实现）

所有已发布 current connection 的项目主动 rejection/recovery 共用固定 owner slot：
TX uncertainty/notify failure、security initiation failure、失败/不足的 ENC_CHANGE
（包括 observation timeout）、缺少本地 peer approval、拒绝 passkey/injection failure、
repeat pairing denial、trust queue-full。显式 local unpair 与 shutdown 仍独立。
Incarnation 耗尽时的旧 direct terminate 仅拒绝尚未发布的连接；不得授权 frame 或分配
回绕 incarnation。

每个 owner 的预算为**总共3次尝试**，尝试间隔**至少100ms**，使用可跨 rollover 的
FreeRTOS tick 差值。仅现有 TX worker 执行 retry；初始 dispatch 可以执行首次尝试。
重复请求不得重置预算或加速 retry。返回0等待匹配 disconnect；ENOTCONN 立即经既有
一次性 cleanup 完成。其它错误保留拒绝状态与有限 worker retry ownership。匹配
 disconnect 即使在预算耗尽后也可取消 retry 并正常完成；新 incarnation 使旧 retry
失效；shutdown 禁止 retry 与 advertising。

第三次错误后停止 retry，继续拒绝产品流量。组件输出 content-free enum
`terminal lifecycle fault=TERMINATE`，只读 metrics API 暴露当前 `termination_attempts`
与 `termination_fault`，acceptance worker 输出这些数值。不得静默恢复 authorization、
轮换 identity、擦除 bond 或 reboot。Terminal recovery 保留显式 component stop/re-entry
边界；实机操作需要另行授权。这替代上文 iteration 8 的单次尝试错误处理。

实际 production 源码故障测试覆盖 error→success/callback、error→ENOTCONN、恰好三次
耗尽、重复请求不增加尝试/不重置预算、尝试间真实 disconnect、旧 owner/新 connection、
shutdown、tick rollover、所有 security/pairing/trust-queue 拒绝路径与全部 iteration7/8
回归。旧测试 fixture 已修正：新 fail-closed pairing rejection 后先重连，再测后续操作；
没有弱化 security predicate。Device 行为/运行资源仍 UNVERIFIED；未 scan/connect/
permission/flash/pair/unpair/R4/commit/push。

Iteration 9 最终 gates PASS：完整 static/R1/R2/R3/BLE lifecycle faults、实际 Apple/ESP
crypto host gate、CoreBluetooth compile 与 Mac acceptance warnings-as-errors LINK only、
check_repo/diff-check。正常 IDF5.5.3/C3 product app969056 / merged1034592 bytes；archive
`build/firmware/cb4f2e27bf318eca1a21649c57618ebb6e42be221ccdb43fe44276ebf42a29bc`。
全新独立 acceptance app963840 / merged1029376 bytes，保留
`/private/tmp/codex-r3-i9-acceptance`；merged SHA256
`1574fcd0d03ce5456df083337cdbacedc0bc817386fcb03433c2d2bf48a32e91`。
Device/runtime UNVERIFIED，mbuf UNAVAILABLE。等待独立复审，R3 尚未 DONE。

## Iteration 10 provisional physical ownership（当前实现）

成功 NimBLE CONNECT 在获取可能失败的状态锁之前，先在短 critical section 中记录单个
固定 provisional handle 与单调的 lifetime physical-admission serial。这只拥有 cleanup
责任，不提供 authenticated link incarnation、app-auth、R1 generation 或 accepted bytes；
不持久化、不传输。Admission serial 与正常 link-incarnation allocator 均不回绕；耗尽
只拒绝 admission，不创建 session。

Link publication 返回显式结果。只有 publication 成功才发起 security。状态锁超时或
UINT64_MAX incarnation 耗尽，均通过相同的3次预算 / >=100ms worker termination policy
拒绝 provisional owner。匹配 provisional disconnect 或 ENOTCONN 清理并仅恢复一次
advertising；其它错误保留有限 retry ownership，耗尽暴露既有 TERMINATE fault。
更新的 published owner 使旧 provisional work 失效。单个固定 retired 数值 marker
拒绝延迟的旧 provisional callback（含 controller handle reuse），不保留帧或权限。
Shutdown 禁止 retry/restart。真实 NimBLE GAP callback 顺序/硬件调度仍 Device UNVERIFIED。

每次 published CONNECT 都分配新的 link incarnation，即使旧 project facts 尚未清理而
controller handle 已被复用。Publication 状态锁内清空 RX、单 TX slot 与 pending pairing
facts。Publication 失败不得发起 security/auth/R1。相邻 subscription/ENC/MTU/identity
观察更新不能确认所有权时，也拒绝 current owner，避免保留旧 authorization observations。
GAP 只保留统一有界 termination 调用；显式 stop/unpair 仍独立。

实际 production 源码测试覆盖 CONNECT 首次锁失败、成功 termination/随后 callback、
error/retry、ENOTCONN、三次错误 terminal fault、尝试间 disconnect、正常 incarnation
耗尽不回绕、physical-admission serial 耗尽、旧 provisional callback/deferred work
不能影响新 published link、handle reuse、publication 失败不发起 security、shutdown、
观察锁失败及全部 iteration7/8/9 回归。一次中间 fixture assertion 已修正：仅 pending
recovery 期间验证 internal trust-store 拒绝；ENOTCONN 完成后已断开，不得接纳 product
bytes。Security predicate 未弱化。Device/runtime UNVERIFIED；mbuf UNAVAILABLE。
未 hardware/R4/commit/push。

Iteration 10 最终 gates PASS：完整 static/R1/R2/R3/BLE production fault suites、
Apple/ESP actual crypto host gate、CoreBluetooth compile、Mac acceptance warnings-as-errors
LINK only、repository/diff audit。Product IDF5.5.3/C3 app970016 / merged1035552 bytes；
archive `build/firmware/2397969791ddb2b84da481f98cf531cd18234f65ca3757d0b0721ec8f338731e`。
全新独立 acceptance app964800 / merged1030336 bytes，保留
`/private/tmp/codex-r3-i10-acceptance`；merged SHA256
`399c9b73c94aca3d04345c0ab002dc515cb2826df7e05db9bf9587b876fe0537`。
Device/runtime UNVERIFIED；mbuf UNAVAILABLE。等待独立复审，R3 尚未 DONE。

## Iteration 11 — 已观测 provisional 断连的退役清理

补充独立复审确认遗漏的回调顺序，撤回 iteration 10 DONE：provisional DISCONNECT
已入槽但状态锁处理失败，新同 handle CONNECT 可先于 worker 清理完成发布。现在
消费已观测 provisional 槽时，仅在退役 handle 与 serial 精确匹配的情况下，在已有
临界区内清除退役标记。待处理的 terminate 请求不清除标记，因为旧物理断连回调
尚未到达。没有新分配、队列/任务增长、安全策略或 wire 变更。

新增 production-source 回归修复前失败、修复后通过：旧断连已观测 → 锁失败 →
同 handle 新发布 → 消费旧观测槽 → 清除匹配标记 → 新连接真实断连清除状态，
且广播仅恢复一次。互补的未观测 terminate 场景保留标记，忽略第一个迟到的旧
回调，再正常处理当前连接回调。重复回调不重复恢复广播；既有 shutdown、stale
owner、有界 terminate 与 iteration 7–10 测试均保留。

Iteration 11 独立复审待完成。实机互通与运行资源测量仍 UNVERIFIED；NimBLE mbuf
稳定 accessor 仍 UNAVAILABLE。没有实机操作、Bluetooth runner 执行、R4、commit/push。

Iteration 11 最终验证：static/R1/R2/R3/production BLE faults、真实 Apple/ESP crypto、
CoreBluetooth 编译、Mac warnings-as-errors 仅 LINK、仓库/diff 审计均 PASS。
实际产品 IDF5.5.3/C3 app970080 / merged1035616 bytes，归档
`build/firmware/903ce368779b3e27417dcde12a43edee57189d96905a67c1109b71e913d23f6c`。
独立 acceptance app964848 / merged1030384 bytes，保存在
`/private/tmp/codex-r3-i11-acceptance`，merged SHA256
`d77b148d779af9b3d5a4341bca56b26f4c6e7347ee7880ba54bc6b56b29d0e72`。
独立复审待完成；实机验收仍 UNVERIFIED。
