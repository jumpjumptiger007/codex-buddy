<p align="right">
  <strong>简体中文</strong> · <a href="codex-buddy-reuse-audit.md">English</a>
</p>

# Codex Buddy R0 复用审计

本文是 [vNext 架构与 R0–R7 路线图](codex-ambient-dashboard.zh_CN.md)的证据附录。架构文档负责权威目标决策和门禁范围；本附录记录已 pin 的来源、许可/来源证据、模块比较和证据限制。R0 没有把 donor 生产源码、二进制或素材复制进 codex-buddy。

## 审计方法

本工作区在文档编辑前核验为 main 分支、提交 f12a6e0fbddaad0a3ba133084f37ff59a609be68，且当时工作区干净。当前源码和测试优先于 donor 声明。公开 donor 仓库以只读方式放入工作区外的临时目录，并固定到完整 commit SHA。没有向项目添加 donor remote、submodule、依赖、源码、生成文件或素材。

证据优先级：

1. 当前项目源码、头文件、测试、BSP 契约和构建目标。
2. 每个 pinned donor commit 中的源码和许可证原文。
3. 可用时，以文件 blob 身份和 commit 历史判断来源关系。
4. Donor README 只用于定位和了解能力声明，不能证明正确性、安全性、硬件兼容性或许可。

不推定仓库根许可证涵盖另行取得的图片、字体、模型、音频、固件包、编译后的 WASM 或 vendor 依赖。REUSE DONOR 和 ADAPT DONOR 都要求相关代码有明确的正面许可证据。决策列只使用 KEEP OURS、REUSE DONOR、ADAPT DONOR、DROP 四个值；来源和不确定性单独记录。

## 已 pin 的来源与许可证据

| ID | 检查的 ref 和来源 | 许可与来源证据 |
| --- | --- | --- |
| S0 | 当前 codex-buddy，main：f12a6e0fbddaad0a3ba133084f37ff59a609be68。根目录 [LICENSE](../../LICENSE)。 | 根许可证为 MIT，版权归 FoloToy 2026。项目规则另行要求保护私人或不可再分发的角色素材，并明确记录每个素材的来源。 |
| S1 | FoloToy/ai-passport，upstream/main：0b9e4c81ee4421c0bac39ca3561d65a8285acd4a。[固定源码树](https://github.com/FoloToy/ai-passport/tree/0b9e4c81ee4421c0bac39ca3561d65a8285acd4a)。 | 此 ref 的根 LICENSE 为 MIT，版权归 FoloToy。它是硬件与上游基线参考。 |
| S2 | FoloToy/ai-passport，upstream/demo/claude-buddy-port：9f5e2a43b7f8da9dda119174f0286ca8f2485795。[固定源码树](https://github.com/FoloToy/ai-passport/tree/9f5e2a43b7f8da9dda119174f0286ca8f2485795)。 | 固定源码树有 NOTICE，但没有根 LICENSE。基础仓库 main 是 MIT；本审计不假设该许可证明确覆盖只在这个分支新增的内容。NOTICE 描述了对 Anthropic 公开协议的实现，但不能代替整个分支的根许可证。 |
| S3 | zhangsan2000w-art/ai-passport-codex-buddy，origin/main：90972308293076e9b321e90880ccc891a3d7c10b。[固定源码树](https://github.com/zhangsan2000w-art/ai-passport-codex-buddy/tree/90972308293076e9b321e90880ccc891a3d7c10b)。 | 根 LICENSE 为 MIT，并列出 FoloToy 和 Codex Buddy 贡献者。NOTICE 将协议兼容性归于 Anthropic，并说明 ESP-IDF runtime 和 pixel sprite 是另行实现；固件未包含 Anthropic GIF 或 renderer。未来复用代码时必须保留 MIT 声明。 |
| S4 | anthropics/claude-desktop-buddy，origin/main：a280c6421931431ba6905aee9d2b50b2bfd8c103。[固定源码树](https://github.com/anthropics/claude-desktop-buddy/tree/a280c6421931431ba6905aee9d2b50b2bfd8c103)。 | 根 LICENSE 为 MIT，版权归 Anthropic PBC 2026。characters/bufo/README.md 明确说明社区 Bufo GIF 集不在该 MIT 许可证范围内。 |
| S5 | espressif/esp-desktop-buddy，origin/main：b6bac05db208717676e70180e5269d79f32b2d68。[固定源码树](https://github.com/espressif/esp-desktop-buddy/tree/b6bac05db208717676e70180e5269d79f32b2d68)。 | 根 LICENSE 为 Apache-2.0。ESP-IDF component manifests 标明组件依赖。未来改造代码时须保留 Apache 许可证、版权和修改声明，以及适用的 NOTICE 内容。 |
| S6 | openelab-commits/codex-buddy，origin/main：420232607e3b6f1641ea02d576abf140bd09b53b。[固定源码树](https://github.com/openelab-commits/codex-buddy/tree/420232607e3b6f1641ea02d576abf140bd09b53b)。 | 仓库根目录没有 LICENSE 或 NOTICE。嵌套的 Apache-2.0 LICENSE.txt 只适用于 plugins/codex-usage-stick/skills/codex-usage-pet，不覆盖整个仓库或同级插件脚本。 |
| S7 | FoloToy/folo-ai-passport-xiaozhi，origin/main：72da544d1b51678f5d88967adc299b3aec956946。[固定源码树](https://github.com/FoloToy/folo-ai-passport-xiaozhi/tree/72da544d1b51678f5d88967adc299b3aec956946)。 | 根 LICENSE 为 MIT。NOTICE 指出其来源为 78/xiaozhi-esp32 commit bb9122ab08c3083eeb4f67b3974b7afe771723b8，并列出 AI Passport 板级更改。嵌套 GIF decoder 许可证只适用于该 decoder。大型应用依赖清单和分开的内置素材不自动成为受根许可证覆盖的项目资产。 |
| S8 | VOID001/FoloToy-Passport-Simulator，origin/main：91b0914e5400df4d2bbb102774970c509ef39151。[固定源码树](https://github.com/VOID001/FoloToy-Passport-Simulator/tree/91b0914e5400df4d2bbb102774970c509ef39151)。 | 根 LICENSE 为 MIT。public/wasm/pkg/LICENSE 是内置 ESP-EMU 包单独的 Apache-2.0 声明。随附 demo/社区固件和素材仍须各自核验来源与许可。 |
| S9 | CosmicCoderDev/codex-pet-lab，origin/main：c10f0c52558ab34d2ca2debaf81b9e7e119b640f。[固定源码树](https://github.com/CosmicCoderDev/codex-pet-lab/tree/c10f0c52558ab34d2ca2debaf81b9e7e119b640f)。 | 仓库级角色为 PROVENANCE-ONLY。根 LICENSE 为 MIT；ASSET_POLICY.md 规定只有在贡献者或项目确有权利时才能分发素材，并要求逐项记录来源。R0 不从该来源选用生产模块或角色素材。 |

## 来源关系与沿袭

- S2 固定源码树没有根 LICENSE。其父仓库 S1 的 MIT 许可证本身不能确定只在 S2 新增代码的许可。本次未复制或批准复用 S2 独有实现。
- S2 与 S3 有 36 个相同路径、字节完全一致的文件 blob，包括 main/buddy_ble_lifecycle.c/.h、main/buddy_ble_store.c/.h、main/buddy_character.c/.h、main/buddy_line.c/.h、main/buddy_i4.c/.h，以及若干相同测试和 controller 文件。这证明内容重合。S3 浅克隆快照不能证明复制方向或共有 Git 历史，因此本审计不推断哪个仓库是这些文件的作者。后续候选若来自 S3，以 S3 的 MIT 许可证和 NOTICE 作为许可证据；不以 S2 快照作为许可依据。
- S4 与 S6 有 11 个相同路径、字节完全一致的文件 blob，包括 src/ble_bridge.cpp、src/ble_bridge.h、src/buddy_common.h、REFERENCE.md，以及共用的串口/角色工具。S6 没有根许可证。需要引用协议时，以 S4 作为可识别的已许可来源；不能把 S6 的其他实现归于 S4，也不能在缺少独立许可依据时复用 S6。
- S3 的源码级 Hook 和桌面 Python 模块与 S2 同路径文件并不完全相同。其候选许可依据是 S3 根 MIT；未来复用时还应保留 S3 的 NOTICE 义务。
- S7 的 NOTICE 明确记录了上游祖先 commit。本审计检查了 S7 的 AI Passport 板卡、设置、按键、电池和音频 service 源码，但没有复制其应用框架、生成素材、云端行为或第三方模型/音频素材。
- S8 是有用的浏览器端 ESP32-C3 固件和输入模拟器，但它不实现或验证 Passport 的真实 BLE 安全、射频行为或主机配对。
- S9 的 MIT 许可证不会自动赋予其内置角色素材可安全用于产品的权利。它只作为来源核验参考；没有采用 pet 包或图片。

## 模块级目标矩阵

表格将当前源码、donor 源码与单一目标决策分开。后续验证事项不是第二个决策。S0–S9 对应上面的固定 ref 和许可证据。

### A. 桌面真值与 Codex 集成

| 模块 | 当前 codex-buddy 证据 | Donor 来源及许可/技术证据 | 目标决策 | 理由与后续验证 |
| --- | --- | --- | --- | --- |
| Codex 生命周期 Hook 来源与事件 adapter | companion/mac/rollout_watcher.c 只接受目前核验过的持久化 rollout 形式；companion/mac/permission_observer.c 是注入式观察接口。 | S3 tools/windows_buddy_controller/codex_hook.py、file_bridge.py、hook_health.py 和 install_codex_hooks.py；S3 MIT。它映射 Hook 事件并处理 PermissionRequest，同时也输出 project/model/tool 预览和审批 payload。 | ADAPT DONOR | 保留 allowlist Hook 传输与安装思路，移除内容预览和权限响应行为。后续通过主机测试核验官方事件语义、payload 版本、abort/failure 结果、本地配置归属和无内容保证。 |
| 生命周期归一化与结果 | companion/core/ambient_model.h 和 ambient_reducer.c 区分 OFFLINE、IDLE、WORKING、ATTENTION、DONE，以及成功、失败和中止，并处理新鲜度、顺序、重放和过期 turn。 | S3 tools/windows_buddy_controller/codex_state.py；S4 src/buddy.cpp 和 src/main.cpp；S6 复制了 S4 的一部分。Donor 状态较粗；S3 的 token/completion 行为不能作为 Codex 结果来源。 | KEEP OURS | 保留当前归一化真值和失败语义。后续为每种获准 Hook 映射测试 success、failure、abort、stop、stale 和 duplicate。 |
| 多会话行为 | companion/core/ambient_reducer.c 维护有界的逐会话槽位，并聚合新鲜会话。 | S3 main/buddy_state.h 存储聚合 heartbeat 和一个 prompt，而不是当前逐会话 reducer 契约；S4 设备状态也是单一 Buddy 视图。S3 和 S4 均为 MIT。 | KEEP OURS | 当前 reducer 是审计中唯一满足所需会话隔离和事件顺序的实现。后续压力测试容量、独立时钟、过期会话和并发。 |
| 配额获取与归一化 | companion/mac/rollout_quota_source.c 读取持久化来源的 rate limit，并识别 300 分钟和 10080 分钟窗口；companion/core/ambient_quota.c 归一化来源数据。 | S3 main/buddy_protocol.c 和 controller protocol 传递 token 与 tokens_today，而非来源权威的配额窗口。S6 插件报告使用情况，但没有已核验的 Codex rate-limit 来源；S6 根仓库未许可。 | KEEP OURS | 仅使用来源提供的配额，绝不从 token 总数推算。后续核验生产来源的新鲜度、窗口缺失行为和重置边界。 |
| 配额重置检测与持久化 | companion/core/ambient_quota.c 和 companion/mac/quota_reset_state_store.c 实现重置检测与有界原子持久化。 | Donor 设备设置和 NVS store 不是配额重置来源。S3、S7 为 MIT，但其设置 schema 与当前用途无关。 | KEEP OURS | 现有有界主机持久化符合 Companion 职责。后续用生产来源数据测试原子替换、损坏、过期、重启和重置再次出现。 |
| 桌面 bridge 与 daemon 生命周期 | 当前模块提供 Companion callback 和通用 transport；没有生产托盘应用或后台 BLE daemon。 | S3 tools/windows_buddy_controller/background_bridge.py、file_bridge.py、instance_guard.py、platform_paths.py 和平台打包代码；S3 MIT。bridge 有本地队列、loopback/file IPC、heartbeat 和重连调度，但其状态模型是单一 Buddy 投影。 | ADAPT DONOR | 以进程/IPC 生命周期为起点改造，Codex 真值仍由当前 Companion core 持有。后续测试单实例、队列上限、崩溃恢复、过期 heartbeat 和 macOS 职责。 |
| 权限与特权审批 | companion/mac/permission_observer.c 只报告注入的观察结果，不能决定或授予权限。 | S3 codex_hook.py 根据 PermissionRequest 返回 allow/deny；S3 main/buddy_protocol.c 暴露权限命令；S4 REFERENCE.md 和 S5 Buddy core 也建模权限请求/回复。许可为上文所列 MIT/Apache-2.0。 | DROP | 设备和 Buddy 审批语义违反产品信任边界。后续确认权限观察最多产生无内容 ATTENTION 通知，不能影响操作结果。 |

### B. Bridge、协议与传输

| 模块 | 当前 codex-buddy 证据 | Donor 来源及许可/技术证据 | 目标决策 | 理由与后续验证 |
| --- | --- | --- | --- | --- |
| 语义 wire schema | companion/core/ambient_protocol.h 定义与 transport 无关的 CONTROL/EVENT 校验；生产 BLE schema 尚未冻结。 | S3 main/buddy_protocol.c 和 tools/windows_buddy_controller/protocol.py、S4 REFERENCE.md、S5 esp_desktop_buddy/src/buddy_protocol.c 均建模 message、entries、token、tool hint、时间和权限命令。S3/S4 为 MIT，S5 为 Apache-2.0。 | KEEP OURS | 保留更小的项目自有无内容 schema，不采用 Anthropic NUS 或 Buddy message 字段。项目自有版本与 capability negotiation 属于 KEEP OURS，并在 R1 冻结契约；R1 还定义 allowlist 字段、标识符、sequence/revision、ack 和错误输入行为。 |
| 分帧与解析器 | companion/core/ambient_protocol.c 实现有界行分帧和消息校验。 | S2/S3 main/buddy_line.c、S3 protocol parser、S4 newline JSON protocol、S5 buddy_codec.c/buddy_linebuf.c。S2 无根许可证；S3 MIT、S4 MIT、S5 Apache-2.0。 | KEEP OURS | 当前纯逻辑且有主机测试的分帧边界适合作为基础。payload 与 frame 上限由项目持有（KEEP OURS）；donor 的 MTU、frame 和 queue 上限不是产品值。确定编码前，后续 fuzz 分片/合并、格式错误、超长、UTF-8 和恢复输入。 |
| BLE GATT transport | 产品启动没有生产 GATT service；main/demo_ble.c 是不可连接的硬件 demo。 | S5 components/esp_desktop_buddy_transport_ble/src/buddy_transport_ble.c、*_gap.c、*_gatt.c 和 *_tx.c；Apache-2.0。公开配置要求 esp_desktop_buddy 语义核心指针，GATT 会把 RX 字节直接转给该核心。S2 的分支代码适用何种根许可仍未确定。 | ADAPT DONOR | 将 NimBLE transport 边界拆出或改造，不带入 Buddy 语义核心。R3 必须证明 C3/ESP-IDF 5.5.3 构建、单 peer 行为、task teardown、callback 约束和主机互通。 |
| Espressif Buddy 语义核心与命令模型 | 当前 Companion schema 无内容，也不依赖 Buddy core。 | S5 components/esp_desktop_buddy/src/buddy_core.c 和 buddy_protocol.c 持有 message、entries、prompt、tool/hint、时间命令和权限回复语义；S5 Apache-2.0。 | DROP | 不保留或裁剪该语义核心作为受信任产品边界。R3 目标是把 transport 拆到项目自有语义接口之后。 |
| BLE 安全、配对与 bond 机制 | 当前 main 没有产品配对或 bond 策略。 | S5 transport 提供 bonding、MITM、Secure Connections、IO capability 配置和 bond 操作；Apache-2.0。S3 main/buddy_ble.c 开启 bonding 和 Secure Connections，但将 MITM 设为 false；S3 为 MIT。 | ADAPT DONOR | 只改造有明确许可的配对/bond 机制。由项目自有 secure-link predicate 强制所选策略；R3 测试加密、bond、所选 MITM/SC、错误 peer 拒绝、重连和 unpair。Donor 默认值不是策略。 |
| Espressif tx_ready 判定 | 当前产品授权 predicate 尚未实现；donor readiness callback 不是产品安全契约。 | S5 components/esp_desktop_buddy_transport_ble/src/buddy_transport_ble.c 在已连接、已订阅且已加密时标记 tx_ready；它本身不能证明完整产品策略。S5 Apache-2.0。 | DROP | 不把 tx_ready 用作应用数据授权。R3 证明不满足项目策略（包括认证和 peer 要求）的链路，即使 transport 就绪也会被拒绝。 |
| Codex Buddy 安全 helper 与默认值 | 当前 main 没有生产配对策略。 | S3 main/buddy_ble.c 的 buddy_ble_link_is_secure 忽略 authenticated；S3 BLE 配置将 MITM 设为 false。S3 根 MIT 和 NOTICE 适用于代码来源。 | DROP | 该 helper 和默认值仅作为反面证据，不能证明可接受的产品安全策略。 |
| 重连与链路恢复 | companion/core/ambient_transport.c 暴露有界 transport 结果，但没有 BLE 状态机。 | S3 main/buddy_ble.c 和 buddy_ble_lifecycle.c 包含 generation 检查、重试和 bond 清理；S5 *_gap.c 在断开后重启 advertising、重置链路状态并丢弃排队帧。S3 为 MIT 且有文件重合；S5 为 Apache-2.0。 | ADAPT DONOR | 在当前 Companion 编排后改造链路生命周期。R3 测试过期 callback、终止失败、重试上限、应用关闭和恢复到新快照。 |
| 项目快照真值与新鲜度 | companion/mac/companion_core.c 构建快照；companion/core/ambient_reducer.c 和 ambient_dedup.c 持有新鲜度、事件顺序和一次性行为。 | S5 Buddy core 有独立的快照/liveness 模型，且包含内容字段；其语义快照在下文排除。 | KEEP OURS | 项目快照字段、新鲜度和事件语义始终以当前实现为准。R1 定义 revision/ack；后续测试通知丢失、快照过期、版本不匹配和完整重同步。 |
| 重连与完整快照恢复技巧 | 当前 Companion 可发布完整有界快照，但没有生产 BLE 重同步状态机。 | S5 components/esp_desktop_buddy/src/buddy_core.c 和 components/esp_desktop_buddy_transport_ble/src/buddy_transport_ble_gap.c 提供快照/liveness 与重连示例；Apache-2.0。不选用其内容 schema。 | ADAPT DONOR | 只在项目自有快照语义后改造恢复技巧。R3 必须证明安全重连后先收到完整当前快照，之后才信任后续事件。 |
| Espressif Buddy 快照/liveness 语义模型 | 当前快照 schema 和 liveness 真值归项目所有。 | S5 components/esp_desktop_buddy/src/buddy_core.c 的快照 getter 包含 message、entries 和 prompt 字段；S5 Apache-2.0。 | DROP | 不引入 donor 的含内容快照模型。可参考传输恢复技巧，但产品快照真值仍归项目所有。 |
| 队列上限与背压 | companion/core/ambient_transport.c 接受调用方消息上限，并显式返回 WOULD_BLOCK/DISCONNECTED；reducer 和 dedup 容量由调用方持有。 | S3 测试覆盖队列溢出和背压；S5 buddy_txq.c 使用有界队列并报告背压错误。Donor 队列大小不是当前 C3 产品测量值。 | KEEP OURS | 上限继续由本项目决定。R1/R3 根据最坏快照/音频流量选择容量，并测试队列满、丢帧和恢复。 |

### C. Passport 固件与产品行为

| 模块 | 当前 codex-buddy 证据 | Donor 来源及许可/技术证据 | 目标决策 | 理由与后续验证 |
| --- | --- | --- | --- | --- |
| 固件架构与板级边界 | main/main.c 启动 passport_ui_model/presenter/shell；main/CMakeLists.txt 只加入产品 UI。components/bsp 持有当前硬件 API 和 pin。 | S1 当前板卡/BSP 源码；S2 产品重写没有根 LICENSE，且替换/删除基线项目文件。S7 main/boards/folotoy/ai-passport/ai_passport_board.cc 在 XiaoZhi 应用内部直接初始化硬件。 | KEEP OURS | 保留当前 BSP 和产品 shell 职责。后续核验特定源码构建，不用 donor 应用替换板级初始化、分区、sdkconfig 或 BSP。 |
| 按键和异步事件处理 | components/bsp/include/bsp_button.h 和 src/bsp_button.c 定义当前事件；项目规则要求 callback 投递有界工作并遵守 LVGL lock。 | S7 main/boards/common/button.cc 和 AI Passport 板级代码把按键工作调度到应用任务；S3/S2 也有产品专用应用逻辑。 | KEEP OURS | 当前 BSP 的时序和 callback 上下文具有权威性。后续在真机测试 press/click/long 和任务交接。 |
| 导航拓扑与状态 UI | main/passport_ui_model.*、passport_ui_presenter.*、passport_ui_shell.* 提供一个持久且与传输无关的状态视图；尚未接入产品导航。 | S3 main/buddy_ui.c 为另一款小屏提供菜单/设置/宠物页；S4 M5StickC UI 和 S7 XiaoZhi 聊天 UI 属于其他产品。 | KEEP OURS | 保留当前产品专用 shell，只定义设置和状态所需的最小导航。后续审查按键拓扑与 LVGL 对象/资源；U3 布局尚未冻结。 |
| 设备设置与 NVS schema | 当前 Passport 产品 UI 尚无设置持久化契约；主机配额重置状态另行存储。 | S3 main/buddy_settings.c 提供有界设备设置/NVS 操作；S7 main/settings.cc 提供带 namespace 的 NVS wrapper，但使用动态 C++ 字符串。S3/S7 根许可证为 MIT。 | ADAPT DONOR | 使用精简、类型安全且有界的产品 schema，并保留当前 BSP 职责。后续测试默认值、损坏/缺失数据、迁移、写入失败和显式 reset；不导入所有者或审批设置。 |
| 电池行为 | components/bsp/include/bsp_battery.h 和 src/bsp_battery.c 持有当前板卡电池报告；UI model 支持读数缺失。 | S7 main/boards/folotoy/ai-passport/cw2017_battery_monitor.* 包含板级 CW2017 profile 和电压换算，受 MIT 许可。 | KEEP OURS | 当前 BSP 是本工作区传感器契约的权威来源。后续在当前板卡验证可用/估算值及故障显示；不复制 profile 或绕过 BSP。 |
| 时间与 reset time 显示 | 当前 quota view 携带来源提供的 reset epoch/marker；Passport 尚无 wall-clock service 或时钟页。 | S4/S5 Buddy protocol 包含时间同步命令；S7 是连 Wi-Fi 的语音产品。这些不是当前状态契约所需。 | KEEP OURS | 时间源和 reset 解释继续由 Companion 持有。只有审查确认需要时才增加设备时钟同步；后续验证时钟改变和时间缺失。 |
| 声音提示与音频硬件访问 | components/bsp/src/bsp_audio.c 持有当前 codec 接口和恢复边界；产品 UI 尚未播放提示音。 | S7 main/audio/audio_service.* 包含 Opus 编解码、音频队列、AEC/唤醒词引擎和网络 service 集成；根许可证为 MIT，但依赖较多。 | KEEP OURS | 短提示音使用当前 BSP。不导入 XiaoZhi 常开/云端语音栈。后续测试提示音调度、音量、音频恢复及其与显示/BLE 资源的竞争。 |
| Sleep/wake 生命周期 | main/demo_low_power.c 是硬件 demo；components/bsp 含音频 sleep/恢复支持；尚未接入产品 sleep 策略。 | S7 电池监视器暴露芯片专用 sleep/active 转换；其余 service 生命周期属于 XiaoZhi 应用。 | KEEP OURS | 在测量功耗和唤醒需求前不冻结产品 sleep 策略。后续在当前板卡测试显示/音频关闭、按键唤醒、BLE 断连/重连和任务 teardown。 |
| Push-to-talk、STT 与 composer 插入 | companion/mac/stt_backend.c 和 composer_injection.c 是 fail-closed 接口；UI model 有语音 overlay，但无采集或真实 backend。 | S7 audio service 在设备端处理并流式传到服务器；S3 Hook/bridge 不是语音 composer 实现。 | KEEP OURS | 保留 Mac 本地 STT 和经验证的插入边界，不包含提交操作。后续测试实际麦克风采集、有界传输、backend 可用性、单一已验证目标以及无 Enter/自动提交路径。 |
| Unpair 与 reset | Passport 没有生产 BLE bond；当前重置持久化仅用于 Companion 配额检测。 | S5 transport 提供 clear_bonds 和尽力断开链路；S3 main/buddy_ble_store.* 快照并删除 NimBLE bond。S5 Apache-2.0，S3 MIT 且有重合。 | ADAPT DONOR | 改造显式清除本地 bond 并报告错误。后续测试 peer 删除、活动链路 teardown、重试耗尽、重启和 reset 范围，且不擦除无关设置。 |
| ESP32-C3/no-PSRAM 资源预算 | S0 main 产品和 BSP 目标、sdkconfig.defaults、主机测试及硬件指南定义实际 8 MB/no-PSRAM 配置。 | S2/S3 报告各自 MTU、buffer、task 和显示假设；S5 可配置 notify chunk/transport task；S7 支持多种板卡并拉入大型音频依赖集。 | KEEP OURS | Donor 容量和性能数据不适用于当前产品。后续在准确板卡和构建上测量内部 heap、最大连续块、stack、DMA 以及音频/BLE 最坏情况。 |
| 设备端权限决定 | 当前产品边界和 permission observer 仅允许观察；没有设备命令授予权限。 | S3 Buddy 固件实现确认及 allow/deny 回复状态；S4/S5 定义权限请求和响应消息。 | DROP | 绝不引入此行为。后续断言设备协议和 reducer 不含审批 UI、命令、回复或权限状态。 |
| PROJECT、ACTIVITY、token 总数、tool 预览、prompt 与 transcript | 当前合成 model 仍有 project/activity 文本，但 vNext 契约会移除；当前配额源从不使用 token 总数。 | S3/S4/S5 协议和 UI 可携带 message、entries、token 总数、tool 名称、hint 和权限 prompt；S6 usage 插件共享状态内容但没有根许可证。 | DROP | 这些字段对设备投影不必要或包含内容。后续测试 allowlist 拒绝并证明序列化不会携带原始文本。 |
| 角色引擎与视觉素材 | 当前紫色/几何角色只是占位符；AGENTS 要求公开素材来源可分发且可追溯。 | S4 Bufo GIF 明确不在其仓库 MIT 许可范围内；S3 pixel sprite 被描述为原创且受 S3 MIT 覆盖；S9 要求逐素材记录来源。 | DROP | 本路线不选择 donor 引擎、吉祥物、GIF、sprite sheet、字体或模型。后续素材工作须另立产品范围并取得权利证据。 |

### D. 交付、验证与来源

| 模块 | 当前 codex-buddy 证据 | Donor 来源及许可/技术证据 | 目标决策 | 理由与后续验证 |
| --- | --- | --- | --- | --- |
| 安装器与打包格式 | 当前源码没有生产 Companion package。 | S3 packaging/macos、packaging/linux、packaging/windows 和 controller build requirements；S3 MIT。它们为 Python 桌面组件打包，且依赖具体平台。 | ADAPT DONOR | 仅在选定 Mac 交付方式后改造打包结构。后续验证全新安装、更新、卸载、路径、权限、签名需求和设置保留。 |
| 后台启动与 autostart | 当前没有生产 Companion daemon 或 launch agent。 | S3 background_install.py 和各平台打包实现用户级启动和 bridge launch；S3 MIT。 | ADAPT DONOR | 改造用户作用域生命周期并确保单进程。后续测试登录/登出、重复启动、崩溃恢复、显式禁用/卸载，且不得静默做系统级安装。 |
| 诊断与支持日志 | 当前 watcher 暴露有界解析计数器；主机模块返回明确状态值。 | S3 hook_health.py、ble_diagnostic.py 和轮转后台日志提供安装/连接诊断；S3 MIT。S6 的附加脚本未许可。 | ADAPT DONOR | 改造健康分类和有界日志，同时删去用户内容、标识符及凭证。后续测试格式错误输入、bridge 中断、BLE 故障及有用且脱敏的支持包。 |
| 自动化主机测试 | 当前测试覆盖 reducer、quota、protocol、transport、reset store、UI 映射/展示和 fake 集成。 | S3 有 Python Hook/controller 与 C 固件主机测试；S5 有组件测试应用；S8 有 simulator 测试。许可按各来源分别记录。 | KEEP OURS | 保留当前测试入口和纯逻辑接口。后续为每个获准生产 adapter 增加测试；donor 测试仅提供案例思路，不证明本项目行为。 |
| 故障与 fault-injection 测试 | `tests/test_bsp_button.c` 注入 ADC/channel/calibration/read/conversion/allocation/callback/delete 故障并检查清理/重试；`tests/test_demo_wifi_runtime.c` 注入九种启动故障并检查回滚；`tests/test_bsp_lvgl_init.c` 覆盖部分初始化失败。 | S3 `tests/test_buddy_ble.c`、`tests/test_buddy_orchestrator.c` 和 `tools/windows_buddy_controller/tests/test_scenarios.py`；S5 `components/esp_desktop_buddy/test_apps/core/main/test_buddy_core.c`。S3 为 MIT，S5 为 Apache-2.0；donor 测试仅适用于各自源码。 | KEEP OURS | 保留本项目的失败路径与 fault-injection 测试方式。未来 donor 负向/错误案例只有在映射到本项目契约后才能作为补充。后续注入 bridge/BLE 启动、策略拒绝、重连、队列满和 teardown 故障，并检查有界恢复。 |
| 固件构建、产物验证与发布集成 | `tools/validate.sh`、`tools/verify_firmware.py`、`tools/archive_firmware.py`、`tests/test_verify_firmware.py` 和 `tests/test_archive_firmware.py` 定义当前构建/产物验证及归档流程。 | S3 `packaging/macos`、`packaging/linux` 和 `packaging/windows` 提供受许可的桌面打包机制（MIT），不定义本项目的固件镜像验证契约。 | KEEP OURS | 现有固件产物验证和归档契约继续由项目持有。Donor 桌面打包/发布机制仅作比较证据；后续发布检查须保留项目的镜像、分区、资源和归档验证。 |
| Simulator 与真机验证 | 当前工作区没有集成 simulator；真机验收属于后续独立门禁。 | S8 simulator 运行 ESP32-C3 固件并模拟 ST7789P3、UP/DOWN/OK/POWER、音频和网络输入；公开 WASM package 另受 Apache-2.0 许可。它不验证真实 BLE 安全或射频。 | ADAPT DONOR | 若 R6 验证工作流可用，将 simulator 作为补充 UI/输入测试接口；R7 真机 BLE/音频/功耗测试仍是必需项。后续固定 simulator/WASM 许可并比较模拟与真机行为。 |
| 许可、来源与素材准入 | S0 根 MIT 和 AGENTS 要求按许可管理源码与素材；R0 未引入 donor 内容。 | S2、S6 无根许可证；S4 Bufo 素材被排除；S9 要求逐项来源证据；S8 WASM 另受 Apache-2.0 许可。 | KEEP OURS | 保留本仓库更严格的来源规则；未来改造代码时记录应保留的声明。后续分别扫描源码、字体、模型、音频、二进制、固件 fixture、生成产物和发行 package。 |

## 历史 Gate 0 观察结果

以下结果由已被替代的架构文档记录。R0 将它们作为历史证据保留，没有重跑 probe、扩大扫描语料，或把它们转成对当前所有 Codex 行为的断言。

- metadata-only rollout 扫描未观察到 SessionConfigured、TurnStarted 或 TurnComplete。后续在 32 个 rollout 文件中找到 46 条持久化 event_msg 记录，其 payload.type 为 turn_aborted。信封字段为 type、ordinal、timestamp、payload；被检查的 abort payload 字段为 type、turn_id、reason、started_at、completed_at、duration_ms。每个被查文件有一个 session_meta.payload.session_id；文件内 ordinal 递增，且不同会话的活动有重叠。该样本支持当时的 watcher 只接受已核验的 abort 形式，但不能证明完整的生产 Hook 契约。
- 持久化 rate_limits.primary 和 rate_limits.secondary 记录含有来源提供的 300 分钟与 10080 分钟窗口 usage/reset 数据。由于没有已确认安全调用的 transport/request framing，无法对照 App Server account/rateLimits/read；没有发送请求，也没有使用 token 估算。
- 对当时可用 rollout 的 event/type label 做 metadata-only 扫描，没有找到 permission、approval、trust、policy、sandbox、allowed 或 denied 信号。扫描语料有限，不证明权限行为不存在。
- Composer Accessibility 状态为 NOT_INSPECTED：可用的 accessibility hierarchy 调用可能返回文本，且没有 metadata-only 过滤方式。没有触发 TCC 提示或更改设置。Composer 选择、可编辑性和插入方式均未核验。
- 在已检查位置没有找到受支持的 whisper.cpp runtime 或预先存在的 base/small 模型组合，因此无法进行本地 STT 对比。没有下载或运行模型，也未使用用户音频。

R1/R2 必须针对将要实现的生产来源和 adapter 收集当前且获同意的证据。历史 probe 不能替代这些工作。

## 资源与证据限制

R0 未验证射频范围、吞吐量、电池续航、配对体验、麦克风/扬声器质量、音频延迟、当前可用 heap、stack 高水位、帧上限、确切 UUID、MTU 或队列大小。Donor 默认值是其 donor 构建的源码事实，不是当前板卡的测量值。

本审计只确认上表 pinned snapshot 中的源码与许可事实，不构成法律意见，不验证所有受支持主机上的运行时行为，也不授予未列出素材或依赖的权利。
