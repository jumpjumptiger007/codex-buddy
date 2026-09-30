<p align="right">
  <strong>简体中文</strong> · <a href="AGENTS.md">English</a>
</p>

# AI Agent 仓库规范

本文是本仓库 AI 辅助工作的长期入口。稳定架构和详细设计应写入项目文档；本文件保持简洁并聚焦可执行的协作规范。

## 产品与硬件边界

- AI Passport 应保持为轻客户端。Passport 负责显示/UI、角色动画、按键、麦克风采集、扬声器提示、BLE 传输，以及小型确定性状态/reducer 逻辑。Mac Companion 负责 Codex 集成、配额获取、解释 Codex 生命周期、生成通知、本地 STT、向 Codex composer 注入文本，以及 BLE 重连/编排。未经明确的架构决策，不得把 Codex 专用解析或重量级/易变的桌面集成移到 ESP32。稳定的边界决策记录在项目架构文档中。
- 目标硬件为 ESP32-C3、8 MB Flash、无 PSRAM，使用 ESP-IDF 5.5.3。按此预算内部 RAM。
- 除非产品明确要求其他有效且有文档说明的布局，否则保留有效的默认 8 MB 分区布局。详细分区规则见 `docs/development/engineering/firmware-layout.md`。
- 可复用的板级硬件能力放在 `components/bsp`；应用 UI、状态、动画、协议行为和应用任务均应放在可复用 BSP 代码之外。硬件事实以产品规格与实测结果、`components/bsp/include/bsp_pins.h`、BSP 头文件与实现、硬件指南为依据。不得猜测未定义的板级细节。
- LVGL 非线程安全。LVGL 任务之外访问 LVGL 对象时必须持有 `bsp_lvgl_lock()`。
- 按键回调不得阻塞。音频、存储、网络等慢操作应移至工作任务。
- 销毁受影响的 UI 前，先停止可能访问该 UI 的任务、定时器、回调和其他生产者。
- 可脱离硬件测试的状态、reducer、协议、计时和布局逻辑应与 ESP-IDF、LVGL 解耦；适用逻辑应由 host tests 覆盖。
- 产品 UI 必须围绕产品需求设计。不得把基线硬件测试菜单、页面或视觉外壳复用为产品 UI；BSP API 和非 UI 逻辑仍可复用。

## 安全、数据与可分发素材

- Passport 是产品设备，不是审批控制器。不得通过其 UI 或 BLE 连接授予仓库、agent、部署或其他特权操作的审批。
- BLE payload、buffer、队列和保留数据都必须有明确上限。解析前校验长度和格式；只传输当前功能所需的数据。不得向 Passport 发送任意 prompt、transcript、shell command、diff、tool output、assistant content、认证材料或 secret。这不禁止后续明确建模的语音音频传输或有界项目/状态字段。不得记录或提交凭证、用户/设备私有数据或未脱敏日志。
- 公开角色引擎/代码和公开示例素材必须保持可分发。Spider-Man 及其他私有或不可分发的角色素材不得进入公开 Git 历史或公开发行包。
- 引入任何私有/本地角色源素材包之前，必须先在 `.gitignore` 中明确保护其源路径。本文件不决定该路径。公开生成素材不得在未明确说明的情况下派生自私有、不可分发来源。

## 必需的 Passport 技能

五个核心技能为 `passport-develop`、`passport-setup`、`passport-build`、`passport-device-test` 和 `passport-debug`。固件开发前确认这些技能可用，并且每项任务只使用与当前任务相关的技能。

如果必需技能或工具不可用，应遵守当前任务和环境的授权边界。技能要求本身不构成修改全局或用户配置、安装系统软件包或更改宿主环境的授权。

## 任务路由

- 固件/代码工作：阅读 `docs/development/ai-guide.md`、受影响的公开头文件和相邻实现。
- 板卡、BSP、显示、音频、按键或电池工作：查阅相关硬件指南和 BSP 源码。
- 构建、依赖或分区工作：查阅 `docs/development/engineering/build-and-test.md` 和 `docs/development/engineering/firmware-layout.md`。
- 维护文档：遵循 `docs/contribution/doc-conventions.md`。
- 只读取当前任务相关的上下文；不要机械地加载仓库全部文档。

## Git 与授权

- 本项目使用独立仓库，不采用上游 fork 工作流。`upstream` 是 `FoloToy/ai-passport`，用于基线/参考同步及 donor/demo 检查。`origin` 配置后指向本项目自己的仓库。
- 本仓库继承的上游/参考分支说明应按这里的 `upstream` 远端解释；例如继承说明中的 `origin/demo/*` 在此处对应 `upstream/demo/*`。
- 修改前检查 `git status --short --branch`。保留无关的用户修改，不得覆盖、清理或将它们混入当前任务变更。
- 不得执行破坏性 Git 操作或改写共享历史。只有当前任务或项目工作流明确授权时，才可 commit、push、flash、擦除、部署、发布或公开内容。
- 不得提交凭证、私有角色素材、个人数据或未脱敏日志。

## 验证与 C2C

- 根据变更使用仓库验证入口：`./tools/validate.sh --static` 用于仓库/主机验证，`./tools/validate.sh --firmware` 用于固件构建验证，必要时使用 `./tools/validate.sh` 执行完整门禁。
- 迭代时运行最小相关检查。文档和纯主机逻辑变更至少应通过适用的静态验证；固件交付遵循固件/完整门禁策略。
- 分别报告 `Build`、`Host tests`、`Device tests` 和 `Unverified`。
- 当前工作流要求 C2C 审查时，使用项目绑定的 C2C connector，让 ChatGPT 直接检查实际工作区、Git 状态、差异、文件和已记录的执行输出。不得要求用户粘贴 C2C 已能读取的仓库文件正文、差异或日志。
- Codex 执行有明确边界的任务；ChatGPT 独立审查。审查后不得自动开始下一个架构任务。

## 维护中的 Markdown

维护中的 Markdown 默认 `.md` 路径使用英文，并使用配对的 `.zh_CN.md` 文件提供简体中文。两种语言保持对齐，并保留互相切换的链接。
