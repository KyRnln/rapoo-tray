<div align="center">
  <img src="res/logo.png" width="128" height="128" alt="Battery Monitor logo" />
  <h1>Battery Monitor</h1>
  <p><b>轻量化游戏外设电量 / 状态托盘监控</b></p>
  <p><sub>雷柏（Rapoo）全功能控制 · 多品牌电量聚合 · Windows 11 原生 Fluent 界面</sub></p>

  <p>
    <a href="https://github.com/KyRnln/rapoo-tray/releases"><img src="https://img.shields.io/badge/Release-v2.0.0-blue?style=flat-square" alt="Release" /></a>
    <img src="https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6?style=flat-square&logo=windows" alt="Platform" />
    <img src="https://img.shields.io/badge/Framework-WinUI%203%20%2F%20Windows%20App%20SDK-0078D4?style=flat-square&logo=windows" alt="Framework" />
    <img src="https://img.shields.io/badge/Language-.NET%208%20%2B%20C%2B%2B17-512BD4?style=flat-square&logo=dotnet" alt="Language" />
    <img src="https://img.shields.io/badge/License-MIT-green?style=flat-square" alt="License" />
  </p>
</div>

---

## 💡 项目简介

**Battery Monitor** 是一款面向游戏外设用户的现代化、轻量级状态与电量监控工具。采用 **WinUI 3 (Windows App SDK) + 原生 C++ HID 遥测核心** 双架构：

- **100% 官方 Windows 11 云母 (Mica) 与亚克力 (Desktop Acrylic) 材质**：彻底告别白底、白框与白屏闪烁，与 Windows 11 原生控制中心、音量浮窗体验完全一致。
- **任务栏常驻电量指示**：直接嵌入 Windows 11 任务栏，实时显示当前置顶外设的名称、电量百分比、进度条与设备分类图标。
- **高度个性化定制**：独立个性化设置面板，支持调节常驻进度条宽度、停靠位置、设备图标尺寸、图标上下偏移、文本字号、间距及配色模式（跟随系统主题 / 纯黑 / 强调色）。
- **多设备智能聚合**：支持雷柏（Rapoo）VT/MT 全系列、罗技、雷蛇、Keychron、黑峡谷等无线/有线设备，实时展现 DPI、回报率与充电状态。

---

## 🚀 v2.0.0 WinUI 3 架构全新特性

- **WinUI 3 官方现代架构**：
  - 彻底重构前端展示层为 **Windows App SDK (WinUI 3)**，原生合成管线直连 DWM，完美渲染官方 Mica 与 Acrylic 效果。
  - 弹出卡片自带优雅圆角（`CornerRadius="8"`）与半透明磨砂玻璃质感。
  - 失去焦点自动淡出收起，完美防误触。
- **任务栏常驻进度条交互优化**：
  - **点击常驻进度条**：自任务栏进度条位置**由下往上平滑弹出**设备列表卡片（与状态栏保持精细间距），不再从通知区域生硬弹出。
  - **底部设置入口**：卡片底部新增常驻“设置”按钮，一键直达控制面板。
  - **任务栏托盘图标**：显示项目专属图标，点击不再弹出面板，右键唤起设置与退出。
- **全新独立个性化设置页面**：
  - **宽度调节**：100 ~ 280 像素自由拉动，适配不同长短的设备名称。
  - **停靠位置**：支持靠左侧（小组件侧）、居中空白区、靠右侧（托盘侧）。
  - **设备图标尺寸**：12 ~ 28 像素微调。
  - **设备图标上下偏移**：-8 ~ +8 像素精细对齐。
  - **字体文本大小**：9 ~ 16 磅字体调节。
  - **文字与进度条上下间距**：0 ~ 8 像素调节。
  - **色彩方案**：图标与进度条均支持“跟随系统主题（默认）”、“纯黑色”及“系统强调色”。
- **前后前台切换防闪烁**：
  - 常驻分层窗口原生挂接任务栏宿主，杜绝 Z 序跳变与重复非客户区重算，切换任何前台应用或游戏窗口平稳顺滑、无感保活、永不白闪。
- **全局原生 Light-Dismiss 外部点击收起**：
  - 弹出卡片具备轻量全局鼠标事件感知，点击桌面、任务栏或其他任意应用空白处即刻平滑收起，体验与 Windows 11 原生控制中心完全一致。
- **开机自启动自愈看门狗 (Auto-Heal Watchdog)**：
  - 自动检测并迁移注册表历史残留，结合底层 1 秒心跳看门狗与阶梯式任务栏排版监听，开机自启或 Explorer 重启后进度条 100% 自动挂载，告别“重启后必须退出再打开”。
- **多设备并行监控**：同时识别并展示多只鼠标 / 键盘 / 耳机 / 手柄 / 底座。
- **多品牌电量聚合**：支持罗技 HID++ 2.0、雷蛇专有 91 字节 Feature Report 协议、西伯利亚耳机、ATK / VXE、微软、索尼、苹果、Keychron、Pulsar、Lamzu 及通用蓝牙 BLE 外设的电量读取。
- **雷蛇（Razer）专有协议与休眠保活**：针对 2.4G HyperSpeed 无线鼠标（如 Orochi V2、Viper、DeathAdder 等）实现底层 91 字节指令直读，并在接收器闲置休眠（状态码 `0x04`）时持久保留缓存电量与状态，永不闪烁丢失，唤醒后毫秒级恢复。
- **黑峡谷（Hyeku）2.4G 无线键盘适配**：原生识别昂瑞微（OnMicro）HS6209 方案 2.4G 接收器（`VID_32C2`），自动归类为键盘外设并渲染 Fluent 键盘图标，支持名称重命名与设备排序置顶。
- **雷柏型号库大幅扩充**：覆盖 VT0 ~ VT9 全系列（含 V2 / MAX / 大师版 / Air / PRO / SE）以及 P5/P6 PLUS 充电底座、V700DIY 系列键盘。
- **全新 Windows 11 原生电池托盘图标**：单色描边外壳 + 语义配色填充（充电绿 / 低电琥珀 / 危急红 / 离线虚线），4× 超采样结合精确预乘 Alpha 下采样，系统深浅色主题无缝自适应，任意 DPI 下锐利清晰。
- **托盘右键菜单精简**：聚焦为 **设置 / 开机自启动 / 退出程序** 三项 Win11 原生亚克力菜单。
- **回报率与设备状态提示**：托盘 Tooltip 显示当前回报率、连接模式与多设备在线数量。

> 💡 **绿色全自包含运行**：v2.0.0 起基于 WinUI 3 (Windows App SDK) 构建，已包含所有必要组件，无需手动安装任何依赖或运行时，双击即可直接使用。

---

## 🖱️ 鼠标交互

| 操作对象与手势 | 响应动作 | 视觉反馈 |
| :--- | :--- | :--- |
| **左键单击任务栏常驻进度条** | 自下而上唤起 Windows 11 全云母浮窗 | 伴随平滑入场动画，展示全部设备的型号、连接模式、电量进度条、DPI 与回报率，底部带“设置”按钮；失焦自动淡出 |
| **左键单击系统托盘图标** | 仅聚焦项目图标 | 托盘图标采用项目专属设计，点击不弹出面板，保持任务栏整洁 |
| **右键单击托盘图标 / 常驻条** | 打开 WinUI 3 原生云母设置中心 | 提供 **个性化定制（宽度/位置/字号/间距/色彩） / 设备管理（重命名/置顶） / 常规与自启 / 关于** 四大面板 |
| **浮动面板内操作** | 点击底部“设置”或右上“刷新” | 毫秒级即时响应，与系统右键菜单保持精致统一的 8px 底部状态栏间距 |

---

## 🎯 功能特性

1. **WinUI 3 全云母 (Mica) 架构**：
   - 彻底摒弃传统 WebView2 浮层，直连 DWM 原生渲染管线，无白底、无白框、无白屏闪烁。
   - 浮动面板背景全透融合壁纸，仅内部设备卡片保留实体 Fluent 浮岛底色。
2. **Windows 11 任务栏常驻电量指示**：
   - 直接无缝嵌入 Windows 11 任务栏，实时展现设备电量与分类图标。
   - 自由定制宽度（100~280px）、位置（左/中/右）、图标大小（12~28px）、图标上下偏移（-8~+8px）、字号（9~16pt）与间距（0~8px）。
   - 图标与进度条均默认跟随系统主题，亦可自选纯黑或强调色。
3. **硬件级回报率与休眠控制（雷柏二代架构）**：
   - 支持 125Hz、250Hz、500Hz、1000Hz（标准）及 2000Hz、4000Hz、8000Hz（电竞高刷）即时切换。
   - 休眠时间支持 2 ~ 120 分钟调节，配置即刻下发至硬件。
4. **多设备与多品牌**：
   - 多设备并行识别与聚合展示，支持重命名、排序与置顶。
   - 跨品牌读取标准化电量（HID++ 2.0 / 标准 HID 电池 / 蓝牙 GATT）。
5. **绿色便携，无需安装**：
   - 编译输出为 100% 全自包含（Self-Contained）独立便携程序，内置所有运行时，双击即开。
   - 开机自启基于当前用户注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 实现，无需管理员权限，启动无弹窗干扰。

---

## 📋 支持设备列表 (Supported Devices)

### 🖱️ 雷柏 Rapoo（二代 Nordic 架构 · 完整功能）

| 设备系列 | 连接模式 | VID | PID | 支持状态 |
| :--- | :--- | :--- | :--- | :---: |
| **VT0 / VT1 / VT2 / VT2S 系列** | 2.4G 无线 / USB 有线 | `0x24AE` | `0x14xx` / `0x46xx` | ✅ 完整功能 |
| **VT3 / VT3S / VT4 系列** | 2.4G 无线 / USB 有线 | `0x24AE` | `0x14xx` / `0x46xx` | ✅ 完整功能 |
| **VT6 / VT7 / VT7S / VT8 / VT9 / VT9S 系列** | 2.4G 无线 / USB 有线 | `0x24AE` | `0x14xx` / `0x46xx` | ✅ 完整功能 |
| **P5 PLUS / P6 PLUS 充电底座** | USB / 无线 | `0x24AE` | `0x4827` / `0x4825` | 🔑 待验证 |
| **V700DIY-98 / 87 / 75 键盘** | 2.4G 无线 / USB 有线 | `0x24AE` | `0x19xx` / `0x48xx` | 🔑 待验证 |

> 完整型号映射（含各 V2 / MAX / 大师版 / Air / PRO / SE 变体）见 [`src/device_manager.cpp`](file:///c:/Users/TREX/Downloads/code/rapoo-tray/src/device_manager.cpp) 中的 `VERIFIED_MODELS` 映射表。

### 🎧 多品牌外设（标准化电量聚合）

| 品牌 | 识别方式 | 可读取内容 | 支持状态 |
| :--- | :--- | :--- | :---: |
| **罗技 Logitech** | HID++ 2.0 统一电量 (`VID_046D`) | 电量、充电状态 | ✅ 支持 |
| **西伯利亚 Xiberia** | 私有耳机协议 (`VID_36F9`) | 电量、在线状态 | ✅ 支持 |
| **雷蛇 Razer** | 专有 91 字节 Feature Report 协议 (`VID_1532`) | 电量、充电状态、2.4G HyperSpeed 连接识别、休眠保活 | ✅ 支持 |
| **黑峡谷 Hyeku** | 2.4G 无线接收器识别 (`VID_32C2`) | 键盘设备聚合、2.4G 无线状态监控、自定义命名与置顶 | ✅ 支持 |
| **ATK / VXE** | 标准 HID 电池 (`VID_373B`) | 电量 | ✅ 支持 |
| **微软 Microsoft** | 标准 HID 电池 (`VID_045E`) | 电量 | ✅ 支持 |
| **索尼 Sony (DualSense)** | 标准 HID 电池 (`VID_054C`) | 电量 | ✅ 支持 |
| **苹果 Apple (AirPods / Magic)** | 标准 HID 电池 (`VID_05AC`) | 电量 | ✅ 支持 |
| **Keychron** | 标准 HID 电池 (`VID_3434`) | 电量 | ✅ 支持 |
| **Pulsar / Lamzu** | 名称识别 | 电量 | ✅ 支持 |
| **通用蓝牙 BLE / 标准 HID 电池外设** | 蓝牙 GATT / HID Battery System | 电量 | 🔑 视设备而定 |

### 💡 通用兼容机制与型号提报指南

1. **二代 Nordic 架构全功能即插即用**：
   雷柏二代游戏外设（基于 Nordic 54L15 / 3950 方案）在底层报文协议（UsagePage `0xFF00`、Report ID `0x07` 与控制指令集）上完全统一。即使您的设备尚未打标，程序也会自动以 **`通用`** 模式挂载运行，**电量读取、DPI 切换、回报率设置与休眠控制均支持使用**。
2. **一键提取设备硬件 PID 并提交 PR（仅需 1 行代码）**：
   插入设备后，直接双击运行项目根目录下的 **`获取鼠标PID.bat`**（或执行 `tools/get_mouse_pid.ps1`），脚本会自动提取硬件 PID 并生成代码片段复制到剪贴板；也可在 PowerShell 中手动执行：
   ```powershell
   Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -like "*24AE*" } | Select-Object FriendlyName, InstanceId
   ```
   在 `src/device_manager.cpp` 的 `VERIFIED_MODELS` 映射表中添加对应行（如 `{ L"14xx", L"雷柏 VT9 Pro" }`），提交 PR 即可合并！

---

## 🔌 TrafficMonitor 插件使用指南

本项目提供官方适配的 TrafficMonitor 扩展插件（`rapoo-plugin.dll`），可在系统监控软件 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 的任务栏窗口与主悬浮窗中显示鼠标状态。

### 显示项目
- **鼠标电量**（例如：`M: 85%`，充电时显示为 `M: ⚡85%`）
- **鼠标 DPI**（例如：`DPI: 800`）
- **悬停信息 (Tooltip)**：鼠标悬停在监控项上可查看型号、连接模式（2.4G/USB）、详细电量与当前档位。

### 安装步骤
1. 前往 [Releases](https://github.com/KyRnln/rapoo-tray/releases) 下载 `rapoo-plugin.dll`。
2. 将 `rapoo-plugin.dll` 放置于 TrafficMonitor 根目录下的 `plugins` 文件夹中。
3. 重启 TrafficMonitor（或在右键菜单中选择“更多功能” -> “插件管理” -> “重新加载插件”）。
4. 在“选项” -> “任务栏窗口设置”或“主窗口设置”中，勾选“鼠标电量”和“鼠标DPI”即可。

---

## 📥 下载与安装

请前往 [GitHub Releases](https://github.com/KyRnln/rapoo-tray/releases) 下载最新发行版：

- **单文件便携版 (`rapoo-tray.exe`)**：绿色免安装独立托盘程序。
- **一键安装向导 (`rapoo-tray-setup.exe`)**：自动部署至本地目录，创建快捷方式并支持控制面板卸载。
- **TrafficMonitor 插件 (`rapoo-plugin.dll`)**：供 TrafficMonitor 用户按需下载使用。

> 首次使用悬浮窗 / 设置窗口前，请确认系统已安装 **WebView2 Runtime**（Windows 11 默认内置）。

---

## 🔬 技术协议解析 (Protocol Reverse Engineering)

程序通过 Windows 原生 HID API 与设备专有端点建立通信：

### 1. 被动状态接收端点 (UsagePage: `0xFF00`, Usage: `0x0002`)
通过中断传输接收 Report ID 为 `0x07` 的实时状态包（支持 `0xB0` 前缀与直接载荷两种帧格式）：
- `Byte [1]`：设备标识 / 帧前缀（`0xB0` 为扩展帧，`0xB1`~`0xB3` 为非状态帧）
- 载荷 `[1]`：当前 DPI 档位索引（从 0 开始）
- 载荷 `[2..3]`：X 轴 DPI 数值（16 位小端序整数）
- 载荷 `[6]`：活跃 / 充电状态标志
- 载荷 `[7]`：物理电量百分比（`0x00` ~ `0x64`，即 0% ~ 100%）

### 2. 双向控制与心跳检测通道 (UsagePage: `0xFF00`, Usage: `0x000E` / `0x000F`)
通过 33 字节 Command Report (`0x06`) 与 Feature Report (`0x08`) 实现寄存器读写、回报率读取与离线状态判定：
- **写入寄存器**：向 Usage `0x000E` 写入 `[0x06, 0xA5, 0xA5, 0x01, Addr, Bank, Value, ...]`。
- **读取寄存器与心跳**：向 Usage `0x000E` 发送读取命令后，通过 Usage `0x000F` 调用 `HidD_GetFeature` 获取 33 字节应答。若设备休眠或关机，通道立即产生超时应答，程序由此判定设备进入离线状态。

### 3. 多品牌电量查询
- **罗技 HID++ 2.0**：通过长报文 (`0x11`) 查询 IRoot 特性索引，再调用统一电量特性 `0x1004` / `0x1000`。
- **标准 HID 电池**：读取 `HID_USAGE_PAGE_GENERIC_DESKTOP` 下的 `Battery Strength` 上报值。
- **Xiberia 耳机**：向 UsagePage `0xFF90`（Report ID `0x55`）发送状态查询帧并解析电量与在线标志。
- **雷蛇 Razer**：向 91 字节专有 Feature Report 端点发送查询指令，`cmdClass = 0x07, cmdId = 0x80` 获取 0~255 电量数值，`0x07, 0x84` 读取充电状态，校验采用 Byte 3~88 的异或 CRC。对 2.4G HyperSpeed 接收器休眠超时状态（`0x04`）实现无感缓存保活与平滑重连。

---

## 🛠️ 源码编译 (Build from Source)

环境要求：Windows 10 / 11 (x64)，配备 .NET 8 SDK 与 MinGW-w64 (GCC 9+)。

### 1. 一键编译全自包含便携独立程序 (Portable Release)
运行根目录下的自动化构建脚本：
```cmd
build.bat
```
- **输出可执行文件**：`bin\Release\BatteryMonitor.exe`
- **特性**：全自包含（Self-Contained），内置所有运行时与 Windows App SDK 组件，**完全免安装**，双击即可直接运行使用。

### 2. 编译 TrafficMonitor 插件 (可选)
```cmd
cd plugin
build_plugin.bat
```
输出产物：`bin\rapoo-plugin.dll`

---

## 🤝 鸣谢与致敬 (Credits & Acknowledgments)

- **[@Nuitfanee](https://github.com/Nuitfanee)** ([ClickSync](https://github.com/Nuitfanee/ClickSync))：提供了雷柏双向控制协议逆向与寄存器地址映射的关键技术启发。
- **[@dts-dan](https://github.com/dts-dan)** ([logi-tray](https://github.com/dts-dan/logi-tray))：提供了托盘电池图标的设计灵感与创意启发。
- **[@hsb689](https://github.com/hsb689)**：提供了雷柏 VT3S 系列硬件 PID 数据。
- **[@sAchNMN](https://github.com/sAchNMN)**：提供了雷柏 VT3 MAX 系列硬件 PID 数据。
- **[@lbyisme](https://github.com/lbyisme)**：提供了雷柏 VT3 V2 系列硬件 PID 数据。

---

## ⚠️ 免责声明 (Disclaimer)

1. 本项目为个人开源业余作品，旨在为无线外设用户提供轻量、低占用的系统级托盘助手，**非雷柏官方出品**。
2. “雷柏”与“RAPOO”商标及产品名称所有权归深圳市雷柏科技股份有限公司所有。
3. 软件按“原样”（AS IS）提供，作者不对使用过程中可能出现的任何偶发问题承担法律责任。

---

## 📄 开源许可证 (License)

本项目基于 [MIT License](LICENSE) 开源。
