# CLAUDE.md — claude-desktop-buddy (StickS3 port)

这个文件给 Claude Code 提供项目上下文。简明优先，有冲突时以 `REFERENCE.md`（协议真相来源）为准。

---

## 项目是什么

Fork 自 `anthropics/claude-desktop-buddy`。官方参考硬件是 **M5StickC Plus2 (ESP32 classic)**；本 fork 目标硬件是 **M5Stack StickS3 (ESP32-S3-PICO-1-N8R8)**。

核心改造任务是把 Plus2 → StickS3 的 port 跑通，同时保留 BLE 协议层与 GIF 角色包体系的完整兼容性。之后会基于 StickS3 独有硬件（喇叭、麦克风、红外）做扩展玩法。

**物主上下文**：单人非嵌入式背景开发者，习惯 vibe coding。目标是把它当 side project 玩，不追求生产级代码质量，但要可读、可增量迭代。

---

## 硬件差异速查表（Plus2 vs StickS3）

| 维度 | Plus2（原） | StickS3（本） |
|---|---|---|
| 主控 | ESP32-PICO-V3-02 | ESP32-S3-PICO-1-N8R8 |
| Flash / PSRAM | 8MB / 2MB | 8MB / 8MB |
| 屏幕 | 1.14" 135×240 ST7789V2 | 1.14" 135×240 ST7789P3 |
| IMU | MPU6886（6轴） | BMI270（6轴） |
| 音频 | 蜂鸣器 only | ES8311 codec + MEMS mic + AW8737 功放 + 1W 喇叭 |
| 红外 | 仅发射 | 发射 + 接收（接收必须用 RMT 外设，不能 GPIO） |
| USB | 外挂 UART 芯片 | 原生 USB-CDC |
| 电池 | 200mAh | 250mAh |
| BLE | 4.2 | 5.0 |

**推论**：屏幕、按键、IMU 走 `M5Unified` 统一抽象层后代码基本一致；差异主要在 `platformio.ini`（board/flags）和 IMU 型号判定路径（如原代码硬编码 MPU6886 则需改）。音频和红外是 Plus2 上没有的能力，不在 port 主线里，放到 `src/experimental/` 下单独展开。

---

## 开发环境

- macOS
- PlatformIO Core（非 VS Code 插件版；`brew install platformio`）
- 不装 Arduino IDE
- USB-A to USB-C 数据线（C-to-C 在 Stick 系列历史上有识别问题）

**关键命令**：

```bash
# 编译 StickS3 环境
pio run -e m5stack-sticks3

# 烧录固件
pio run -e m5stack-sticks3 -t upload

# 串口监视
pio device monitor -b 115200

# 烧 data/ 到 SPIFFS/LittleFS（字符包本地测试用，跳过 BLE）
pio run -e m5stack-sticks3 -t uploadfs
```

---

## 代码结构（继承自原仓库）

```
src/
  main.cpp          # 主 loop、状态机、UI 屏幕切换
  buddy.cpp         # ASCII 物种派发 + 渲染 helper
  buddies/          # 每个物种一个文件，每个文件暴露 7 个动画函数
                    # 7 状态: sleep, idle, busy, attention, celebrate, dizzy, heart
  ble/              # Nordic UART Service + 字符包传输协议实现（如有此目录）
characters/
  bufo/             # 官方示例 GIF 字符包（保留勿改）
  <my-pack>/        # 自定义字符包放这里
tools/
  prep_character.py   # 源 GIF → 96px 宽标准化
  flash_character.py  # 本地 stage 到 data/ + pio uploadfs（省 BLE 往返）
data/                 # SPIFFS/LittleFS 映像目录（uploadfs 目标）
platformio.ini        # 环境配置（主要改动点）
REFERENCE.md          # BLE 协议规范 — 协议问题的真相来源
```

**动画函数契约**：`buddies/<species>.cpp` 里每个状态一个函数，签名一致（查 `buffalo.cpp` 或 `bufo.cpp` 做模板）。新增物种时复制模板文件、改 7 个函数体、在 `buddy.cpp` 的 dispatch 里登记即可。

---

## BLE 协议不可碰

这些是协议层约束，**任何修改都不该改动它们**：

- Nordic UART Service UUIDs（见 `REFERENCE.md`）
- 字符包传输：`char_begin` / `file` / `chunk` / `file_end` / `char_end`，每步等 ack
- 字符包总大小上限 **1.8MB**
- GIF 规格：**宽 96px 固定**，高 ≤ 140px，透明边距要裁紧
- 设备名必须以 `Claude` 开头（desktop 扫描过滤依赖这个）
- 加密：应标记 NUS characteristics 为 encrypted-only + DisplayOnly IO capability，触发 OS 配对流程

实现新玩法时，如果要扩协议，**只能走 vendor-specific 自定义 GATT service，不要污染 NUS**。

---

## Port 主线（按顺序）

1. **`platformio.ini` 加 `[env:m5stack-sticks3]` 段** — 参考 M5Stack 官方的 `esp32-s3-devkitc-1` + `qio_opi` + `default_8MB.csv` 配置。`build_flags` 必须带 `-DARDUINO_USB_CDC_ON_BOOT=1` 和 `-DARDUINO_USB_MODE=1`（S3 原生 USB-CDC 才能当串口用）。
2. **编译通过** — 先不求功能正确，能出 `.bin`。
3. **烧录 + 串口看到 boot 日志** — 确认 USB-CDC 链路。
4. **屏幕亮、显示默认 ASCII buddy** — `M5Unified` 自动处理 ST7789P3，理论上不用手动写驱动。如果显示偏移或颜色错乱，优先查 `M5.Lcd` 初始化参数而不是去改驱动。
5. **按键 A/B 响应** — M5Unified 的 `M5.BtnA.wasPressed()` 抽象，原代码直接能用。
6. **IMU "摇一摇" 触发 dizzy** — 检查原代码是否硬编码 MPU6886 寄存器访问；若是则改为 `M5.Imu.getAccelData()` 统一接口。
7. **BLE 广播 + Claude Desktop 配对成功** — 协议层代码不用改。
8. **字符包推送 + GIF 播放** — 端到端验证。

每一步跑通就 commit，方便回滚。

---

## 本项目的开发约定

### Claude Code 工作方式偏好

- 改代码前先跑 `git status` 和 `git diff`，基于当前实际状态规划，不假设干净工作区
- 每个 port 阶段的改动独立 commit，commit message 用英文、祈使句、前缀 `[port]` / `[feat]` / `[fix]` / `[chore]`
- 涉及嵌入式概念（内存分区、PSRAM、SPIFFS vs LittleFS、RMT 外设、BLE GATT 等）时，代码改动附带一句原理说明（注释或聊天里都可），便于物主举一反三
- 不自动重构原仓库的代码风格 — 保持和上游 diff 最小，方便后续 rebase
- 文档用中文，代码注释用英文（上游贡献可能性保留）

### 不要做的事

- 不要碰 `REFERENCE.md`（那是上游的协议真相）
- 不要改 `characters/bufo/`（官方示例，保留做对照）
- 不要擅自升 `platform = espressif32` 的版本号 — 会连锁影响 `arduino-esp32` core 版本
- 不要在 `src/` 里写长段注释解释嵌入式基础概念 — 那些放聊天或 PR description 里
- 不要加 `localStorage` / `sessionStorage` / 浏览器 API（这是嵌入式项目，不要精神污染）
- 遇到"不如整个重构一下吧"的冲动，先停下来问物主

### 调试优先级

屏幕不亮 > 串口无输出 > BLE 扫不到 > 配对失败 > 字符包传输失败 > 动画错位 > IMU 不响应 > 音频/红外扩展问题

前几类是基础链路问题，必须先解决；后几类可以绕过继续开发其他部分。

---

## StickS3 独有能力的扩展构想（非 port 主线）

这些放 `src/experimental/` 下，**不进主 loop**，通过编译开关启用：

- **语音合成播报**：approval prompt 出现时让宠物"喊"你（ES8311 + 喇叭，走 M5Unified 的 `M5.Speaker`）
- **麦克风输入**：长按 A 录一段，作为 tool approval 的语音理由传回 desktop（需要协议扩展，走 vendor-specific GATT）
- **红外遥控**：celebrate 状态时向房间发一个"关灯"红外信号（纯粹玩的）
- **自定义 GIF 字符包：瓜瓜 / 皮皮像素形象** — 物主养的两只猫的数字分身。资产制作走 `tools/prep_character.py` 标准流程。

这些扩展的优先级**低于 port 主线**。port 没跑通前不动。

---

## 常见坑（预判）

- **USB 不识别**：换 USB-A to USB-C 线；确认 macOS 上 `ls /dev/cu.*` 出现 `usbmodem*`
- **编译报 `sdkconfig` 相关错**：清 `.pio/` 目录后重编
- **BLE 广播收不到**：检查 macOS 的蓝牙权限（首次需授权 Claude Desktop）
- **GIF 播放卡顿**：确认 `BOARD_HAS_PSRAM` 和 `qio_opi` 都在 `build_flags` 里；StickS3 的 8MB PSRAM 是性能优势，但得显式启用
- **串口输出乱码**：波特率用 115200；如果还乱码，检查 `ARDUINO_USB_CDC_ON_BOOT` 是否为 1

---

## 真相来源优先级

有冲突时按这个顺序仲裁：

1. `REFERENCE.md`（BLE 协议）
2. M5Stack 官方文档（硬件 pin / 外设）
3. `M5Unified` 库 README
4. 本 CLAUDE.md
5. 物主记忆 / 猜测
