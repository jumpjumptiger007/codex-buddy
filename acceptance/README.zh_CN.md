<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

# R3 实机验收端点

Iteration 5 准备端点，**不授权运行**。正常 product `main/app_main()` 仍进入产品 UI，
只保留 R3 symbol references。独立 `r3_device` IDF app 有自己的 `app_main`，通过
`EXTRA_COMPONENT_DIRS` 复用项目 components，不使用 product compile macro。
`r3_mac` 是 test-only executable，CI/build gate 不运行它，也不 flash。

## 只构建

激活仓库支持的临时 ESP-IDF5.5.3。运行正常 `./tools/validate.sh --static` 与
`./tools/validate.sh --firmware`。然后为 `./tools/check_r3_acceptance_device.sh`
提供**全新**显式临时目录 `R3_ACCEPTANCE_DEVICE_BUILD`。构建 ESP32-C3/8MB/no-PSRAM，
merge 并验证共享默认分区，在 `r3-acceptance-manifest.json` 记录 image/ELF/MAP/
sdkconfig/component image 的 SHA256 和大小，不 flash。Main task stack8192字节；
LVGL 固定9.5.0，与正常 target 一致。分别报告正常 product 与 acceptance artifacts。

`./tools/check_r3_acceptance_mac.sh` 使用 Apple frameworks、warnings-as-errors
编译并**链接**完整测试 executable，但不运行。显式设置 `R3_ACCEPTANCE_MAC_OUTPUT`
可保存 executable。不新增全局安装、login item 或持久 service。

## 获得明确实机授权后的行为

设备在任何 BSP/RF/ADC/audio 前打开 identity，然后初始化自己的 fingerprint/passkey
label 与 BSP buttons，再启动 NimBLE。不复用 product navigation 或 demo screens。
OK click 预先 arm 一次 pairing，10秒有效；NimBLE callback 非阻塞、原子消费已有授权，
把 display event 入 depth4 queue（zero wait），queue failure 拒绝 pairing。
Display/storage/session/crypto 在 worker 执行，BLE/button callback 不做重活。
UP long 仅在操作员明确获准后执行 local UNPAIR，保留 application identity。

Worker 每次 RX 前刷新真实 link facts，必须满足全部 Passport BLE policy；app auth
不伪造 BLE facts。每50ms wake 至多处理4 UI events、4个129字节 RX 和一次 output/ACK
flush。Auth/full snapshot10秒超时 fail closed，直到新 link，不无限 retry。
Disconnected baseline 也持续测量资源。Identity 保持打开，验证 BSP/BLE coexistence；
ADC/RF 初始化后不重新打开 early entropy。DRBG 耗尽 fail closed，需要单独授权 reboot。

Mac CLI 形式（授权前**禁止运行**）：

```text
r3-acceptance PERIPHERAL_UUID PIN_PATH GENERATION_PATH CYCLES [EXPECTED_24_HEX_FINGERPRINT]
```

UUID 明确选择单一 CoreBluetooth peripheral；不知道 UUID 时，先进行获准的 discovery。
Path 显式指定、parent 已存在且 private；跨运行保留 pin/generation 及 lock files，
不要删除/reset 来重试。首次 enrollment 独立比对屏幕/serial public fingerprint，
在启动前提供预先确认值。Delegate approval 只比较，不交互等待，不 silent pin。
既有 pin 不能被新 CLI fingerprint 替换。

Runner 拥有 serial CBCentralManager queue、固定 service-filter scan、selected
peripheral connect/disconnect、unbound core begin 后的真实 transport bind、wrapper
lifetime 和 timer/signal shutdown。Cycles1–20，attempts 至多 cycles+2，每阶段30秒
deadline。Snapshot ACK 后记录递增 generation，disconnect 并 secure reconnect。
R2 使用 `CODEX_SOURCE_DISABLED`，status/quota 为真实 unknown，不伪造 live Codex。
Timer pump 有界；不发送 prompt/transcript/tool/raw session/turn ID。SIGINT 关闭 owner/store。

## 已准备的实机矩阵（NOT RUN）

| 操作/观测 | 必需证据 |
| --- | --- |
| 授权准确 C3 image、兼容 flash/NVS policy 与设备 | Image/ELF/MAP hashes；flash 成功不等于验收；保留 identity/bond，不暗示 erase |
| 独立 app 启动 | Identity/fingerprint 先于 BSP/BLE；无 entropy/RF ownership 冲突 |
| OK 预授权首次 pairing | Passkey label；SC/MITM/encryption/bond/accepted-peer facts；未 arm/过期拒绝 |
| 显式 fingerprint enrollment | 比对后接受；错误拒绝；无 silent TOFU |
| 应用认证顺序 | 每次连接新 Mac challenge 与 Passport nonce；confirmation 先于 R1 HELLO；时间不产生 uniqueness |
| 完整 handshake/snapshot ACK | Auth→HELLO reply→full current snapshot→ACK→notice |
| Reconnect/restart/bond persistence | 新 app-auth nonces、递增 Companion durable generation、full snapshot resync；不复用旧 session；Passport identity/pin 稳定 |
| Security/subscription/failure | 不修改 product truth；wrong peer/key/replay/interrupted pairing/subscription loss/malformed/incomplete 拒绝 |
| Disconnected 状态变化 | 重连 full latest snapshot；notice 不先于 snapshot/ACK。Host faults 是 synthetic；live producer 需独立授权集成 |
| 授权 local unpair/retry | 移除 trust/bond，保留 application identity；非 factory reset/Codex action |
| 重复 cycles/teardown | Internal free/minimum/largest、worker/TX/host stack minimum-free bytes、lifetime RX bytes/TX slots high-water、无增长泄漏趋势 |
| ATT/RF | 协商 MTU、95字节分片、notify/backpressure/loss、真实 RF delivery/ordering |

Mac runner 仅保留上次已接受 challenge/nonce 的固定 SHA256 digest；下一次
accepted reconnect 若任何 digest 重复就拒绝，只输出枚举 fresh-nonce result，
不记录 raw bytes/digest。设备逐字节 auth dispatch 在 R1 generation 仍为零时
记录 confirmation receipt，再记录后续 HELLO generation；执行后可观测所需顺序。
这些已编译 diagnostics 目前不等于 physical evidence。
BLE metrics 只用公开 FreeRTOS API，不暴露 NimBLE private structures，调用与 stop
串行。ESP-IDF stack 单位是字节；mbuf occupancy **UNAVAILABLE**，不是零。
Runtime heap/stack/queue/RF、identity NVS persistence、pairing/interoperability 全部
**UNVERIFIED**，直到授权并执行矩阵。Diagnostics 仅 phase/public fingerprint/
generation/cycle/resource 数值与枚举 failure。

R4 消费已完成 secure transport；测试端点不新增 R4 UX。
Iteration5 compile gate 不操作硬件、不申请 Bluetooth permission、不 scan/connect/
pair/unpair/erase，不 commit/push。

明确授权后的 acceptance-only controls：DOWN click 在 current app auth 后发送一次
有界129字节 malformed frame（仅零和 terminator），观察最大 transport pressure/
rejection；不生成 semantic truth。DOWN long 关闭 auth/停止 BLE，记录 teardown 后
heap/minimum/largest/worker stack，保留 worker 以维持 BSP callback lifetime。Stop
失败保留 owner，再次 DOWN long 仅重试一次；成功后需 reboot 才 re-enter，不自动 retry
或重开 entropy。Static validation 在 Darwin 也链接 Mac executable，但不运行。

CLI 在 Mach-O `__info_plist` section 嵌入 Bluetooth purpose string（见
[Apple usage-description 文档](https://developer.apple.com/documentation/bundleresources/information-property-list/nsbluetoothalwaysusagedescription)）。
Link/section 检查不请求权限；真实 OS consent 与 CLI permission attribution 仍待实机验收。

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
