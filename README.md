<div align="center">
  <img src="res/logo.png" width="128" height="128" alt="rapoo-tray logo" />
  <h1>rapoo-tray</h1>
  <p><b>轻量化游戏外设电量 / 状态托盘监控</b></p>
  <p><sub>雷柏（Rapoo）全功能控制 · 多品牌电量聚合 · Windows 11 原生 Fluent 界面</sub></p>

  <p>
    <a href="https://github.com/KyRnln/rapoo-tray/releases"><img src="https://img.shields.io/badge/Release-v1.4.0-blue?style=flat-square" alt="Release" /></a>
    <img src="https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6?style=flat-square&logo=windows" alt="Platform" />
    <img src="https://img.shields.io/badge/Language-C%2B%2B17%20%2F%20Win32-00599C?style=flat-square&logo=c%2B%2B" alt="Language" />
    <img src="https://img.shields.io/badge/UI-Windows%2011%20Fluent%20%2F%20WebView2-0067C0?style=flat-square" alt="UI" />
    <img src="https://img.shields.io/badge/License-MIT-green?style=flat-square" alt="License" />
  </p>
</div>

---

## 💡 项目简介

**rapoo-tray** 是一款面向游戏外设用户的轻量化状态监控工具。基于 C++17 与 Win32 API 编写，不依赖 .NET / Electron 等重型框架，常驻内存极低。程序以系统托盘独立运行，或作为插件集成至 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor)。

- 对**雷柏（Rapoo）二代 Nordic 架构**外设提供**完整功能**：电量、DPI、回报率读取，以及回报率 / 休眠时间等硬件参数下发。
- 对**罗技、雷蛇、西伯利亚、Keychron**等**多品牌外设**提供标准化电量聚合（HID++ 2.0、标准 HID 电池、蓝牙 GATT）。
- 交互界面采用 **WebView2 + Windows 11 Fluent 设计语言**：原生通知卡片悬浮窗与独立设置窗口。

---

## 🚀 v1.4.0 新版本特性

- **全新 Windows 11 通知卡片悬浮窗（WebView2）**：
  - 左键单击托盘图标，右下角滑入原生通知卡片，实时展示全部设备的电量、连接模式、DPI 与回报率。
  - 多设备自动堆叠、整卡滑入 / 滑出动画、鼠标悬停暂停自动收起、跟随系统深浅色与主题强调色。
  - 取代旧版固定宽框 OSD，卡片高度按内容动态计算。
- **全新独立设置窗口（WinUI 3 / WebView2）**：
  - 右键菜单进入，Windows 11 Mica 材质背景、NavigationView 导航布局、卡片化级联入场动画。
  - **设备重命名**：双击设备名即可自定义显示名称（留空恢复默认）。
  - **拖动排序 / 置顶**：按住拖拽手柄调整多设备顺序，首位设备作为托盘电池主显示。
  - **开机自启动**开关与设备实时遥测面板。
- **多设备并行监控**：同时识别并展示多只鼠标 / 键盘 / 耳机 / 手柄 / 底座。
- **多品牌电量聚合**：支持罗技 HID++ 2.0、雷蛇专有 91 字节 Feature Report 协议、西伯利亚耳机、ATK / VXE、微软、索尼、苹果、Keychron、Pulsar、Lamzu 及通用蓝牙 BLE 外设的电量读取。
- **雷蛇（Razer）专有协议与休眠保活**：针对 2.4G HyperSpeed 无线鼠标（如 Orochi V2、Viper、DeathAdder 等）实现底层 91 字节指令直读，并在接收器闲置休眠（状态码 `0x04`）时持久保留缓存电量与状态，永不闪烁丢失，唤醒后毫秒级恢复。
- **雷柏型号库大幅扩充**：覆盖 VT0 ~ VT9 全系列（含 V2 / MAX / 大师版 / Air / PRO / SE）以及 P5/P6 PLUS 充电底座、V700DIY 系列键盘。
- **全新 Windows 11 原生电池托盘图标**：单色描边外壳 + 语义配色填充（充电绿 / 低电琥珀 / 危急红 / 离线虚线），4× 超采样结合精确预乘 Alpha 下采样，系统深浅色主题无缝自适应，任意 DPI 下锐利清晰。
- **托盘右键菜单精简**：聚焦为 **设置 / 开机自启动 / 退出程序** 三项 Win11 原生亚克力菜单。
- **回报率与设备状态提示**：托盘 Tooltip 显示当前回报率、连接模式与多设备在线数量。

> ⚠️ 悬浮窗与设置窗口依赖 **Microsoft Edge WebView2 Runtime**。Windows 11 及新版 Windows 10 已内置；若未安装，请前往 [微软官方下载 Evergreen Runtime](https://developer.microsoft.com/microsoft-edge/webview2/)（托盘图标与电量监控不依赖 WebView2，仍可正常工作）。

---

## 🖱️ 鼠标交互

| 操作手势 | 响应动作 | 视觉反馈 |
| :--- | :--- | :--- |
| **鼠标左键单击** | 唤起右下角 Windows 11 原生通知卡片 | 滑入展示全部设备的型号、连接模式、电量进度条、DPI 与回报率；5 秒后自动滑出 |
| **鼠标右键单击** | 打开 Win11 原生亚克力托盘菜单 | 提供 **设置 / 开机自启动 / 退出程序** 三个入口 |
| **通知卡片内悬停** | 暂停自动收起 | 鼠标移入卡片时停止倒计时，移出后重新计时 |

在**设置窗口**中：双击设备名称即可重命名，按住右侧拖拽手柄可调整多设备显示顺序（首位设备即托盘电池主显示）。

---

## 🎯 功能特性

1. **极简轻量，原生低耗**：
   - 核心由纯 Win32 API 构建；仅界面层使用系统 WebView2 运行时。
   - 后台常驻占用极低，空闲 CPU 占用不超过 **0.2%**。
2. **Windows 11 原生电池托盘图标**：
   - 单色描边外壳，实时填充电量：$\le 20\%$ 危急红、$\le 30\%$ 低电琥珀、充电中翡翠绿、离线 / 休眠灰色虚线。
   - 充电时叠加清晰的闪电图标，并在多设备时于 Tooltip 显示在线数量。
   - 图标配置与设备顺序、自定义名称自动持久化保存于 Windows 注册表。
3. **硬件级回报率与休眠控制（雷柏二代架构）**：
   - 支持 125Hz、250Hz、500Hz、1000Hz（标准）及 2000Hz、4000Hz、8000Hz（电竞高刷）即时切换。
   - 休眠时间支持 2 ~ 120 分钟调节，配置即刻下发至硬件。
4. **多设备与多品牌**：
   - 多设备并行识别与聚合展示，支持重命名、排序与置顶。
   - 跨品牌读取标准化电量（HID++ 2.0 / 标准 HID 电池 / 蓝牙 GATT）。
5. **开机自启**：
   - 基于当前用户注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 实现，无需管理员权限，启动无弹窗干扰。

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

环境要求：Windows 10 / 11，配备 MinGW-w64 (GCC 9+) 或 MSVC (Visual Studio 2019+)。

### 1. 编译可执行程序
运行根目录下的一键编译脚本：
```cmd
build.bat
```
输出产物：`bin\rapoo-tray.exe`

### 2. 编译安装程序
```cmd
cd installer
build_installer.bat
```
输出产物：`dist\rapoo-tray-setup.exe`

### 3. 编译 TrafficMonitor 插件
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
