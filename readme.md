
# easy_UI.hpp · 轻量级 Windows 原生 UI 渲染库

[English](#english) | [中文](#chinese)

---

<a name="english"></a>
## English

### 📌 Overview

**easy_UI.hpp** is a **single‑header, lightweight UI rendering library** for Windows.  
It uses **native Win32 API, GDI, and GDI+** to draw controls directly — **no external dependencies** (no SDL, Qt, or .NET).  
Designed for small tools, embedded front‑ends, and quick prototyping.

**Latest version (v2.8)** brings:
- 🖼️ **PNG / JPG image support** (via GDI+ with alpha transparency)
- ✨ **Smooth fade in/out animations**
- 📏 **Automatic UI scaling** when the window is resized
- 🖥️ **Double‑buffered rendering** – zero flicker

### ✨ Features

- 🧩 **Single‑header library** – just `#include "easy_UI.hpp"` and you're ready.
- 🪟 **Native Windows rendering** – creates a real Win32 window with GDI / GDI+ drawing.
- 🎨 **Built‑in controls** – Rectangles, Buttons, Labels, Images, Custom draw.
- 🖼️ **Image support** – load PNG, JPG, BMP (alpha channel preserved).
- 🎞️ **Animation system** – show/hide elements with fade effects.
- 🖱️ **Automatic button states** – hover and pressed effects are handled for you.
- 🔔 **Custom callbacks** – attach your own logic via lambdas (click, hover enter/leave).
- 🧊 **Rounded corners** – per‑element radius support.
- 📐 **Layer management** – Z‑order control (top, bottom, move up/down).
- 📏 **Responsive scaling** – all elements and fonts scale proportionally with the window.
- 🖋️ **Custom fonts** – choose any system font and size (e.g., `L"Segoe UI"`, `L"微软雅黑"`).
- ✨ **Antialiasing toggle** – enable ClearType for smoother text.
- 🚫 **Borderless window** – optional frameless mode.
- 🧵 **Thread‑safe task posting** – safely update UI from worker threads.

### 🚀 Quick Start

#### 1. Include the header
```cpp
#include "easy_UI.hpp"
```

#### 2. Write your `WinMain`
```cpp
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int nCmdShow) {
    using namespace easy_ui;

    // Create a window (800x600)
    if (!easyUI.InitWindow(800, 600, L"easy_UI Demo")) return 1;

    // Optional: custom font and antialiasing
    easyUI.SetFont(L"Segoe UI", 18);
    easyUI.SetAntialiasing(true);

    // Add a button with rounded corners
    easyUI.Create(20, 20, 120, 50, 8, L"Click Me", "btn", 0xFF4CAF50, ElementType::Button);
    easyUI.SetCallback("btn", EventType::Click, []() {
        MessageBoxW(nullptr, L"Hello from easy_UI!", L"Info", MB_OK);
    });

    // Add a PNG image (with transparency)
    easyUI.CreateImage(20, 100, 64, 64, L"icon.png", "img_logo");

    // Show window and run message loop
    easyUI.ShowWindow(nCmdShow);
    return easyUI.Run();
}
```

#### 3. Compile (MinGW / MSVC)
```bash
g++ -std=c++11 -mwindows -o demo.exe main.cpp -lgdi32 -luser32 -lmsimg32 -lgdiplus
```

### 📖 API Reference (selection)

| Function | Description |
|----------|-------------|
| `easyUI.InitWindow(w, h, title)` | Creates the main window. |
| `easyUI.SetFont(name, size)` | Sets a custom font for all text. |
| `easyUI.SetAntialiasing(bool)` | Toggles ClearType text rendering. |
| `easyUI.SetBorderless(bool)` | Enables frameless window (call *before* `InitWindow`). |
| `easyUI.Create(...)` | Creates a new UI element (rect, button, label, custom). |
| `easyUI.CreateImage(x, y, w, h, path, name)` | Creates an image element (PNG, JPG, BMP). |
| `easyUI.SetCallback(name, EventType, lambda)` | Attaches a handler for click, hover enter, or hover leave. |
| `easyUI.SetCustomDraw(name, callback)` | Provides a custom GDI drawing function. |
| `easyUI.Show(name, duration_ms)` | Shows an element (optional fade‑in animation). |
| `easyUI.Hide(name, duration_ms)` | Hides an element (optional fade‑out animation). |
| `easyUI.LayerTop(name)` / `easyUI.LayerBottom(name)` | Adjusts Z‑order. |
| `easyUI.Renovate(name)` | Marks an element dirty (forces redraw). |
| `easyUI.Post(task)` | Schedules a task to run on the UI thread (thread‑safe). |
| `easyUI.Run()` | Enters the Windows message loop. |

### 🎯 Use Cases

- **Configuration tools** for embedded devices.
- **Game launchers** or mod managers.
- **Internal developer tools** (log viewers, debug panels).
- **Lightweight overlays** for full‑screen applications.
- **Educational projects** demonstrating Win32 / GDI / GDI+.

### ⚠️ Requirements

- Windows operating system.
- C++11 or later compiler.
- Link with `gdi32.lib`, `user32.lib`, `msimg32.lib`, and `gdiplus.lib`.

---

<a name="chinese"></a>
## 中文

### 📌 概述

**easy_UI.hpp** 是一个**单头文件、轻量级的 Windows UI 渲染库**。  
它直接使用 **Win32 API、GDI 及 GDI+** 进行绘制，**无需任何第三方依赖**（无需 SDL、Qt 或 .NET）。  
适用于小型工具、嵌入式前端以及快速原型开发。

**最新版本 (v2.8) 新增**：
- 🖼️ **PNG / JPG 图片支持**（通过 GDI+，完美保留透明通道）
- ✨ **平滑淡入淡出动画**
- 📏 **窗口缩放自动适应**（UI 元素与字体等比缩放）
- 🖥️ **双缓冲渲染** – 彻底消除闪烁

### ✨ 特性

- 🧩 **单头文件库** – 只需 `#include "easy_UI.hpp"` 即可使用。
- 🪟 **原生 Windows 渲染** – 创建真实的 Win32 窗口，通过 GDI / GDI+ 绘制所有控件。
- 🎨 **内置控件类型** – 矩形、按钮、标签、图片、自定义绘制。
- 🖼️ **图片支持** – 可加载 PNG、JPG、BMP 图片（保留 Alpha 透明度）。
- 🎞️ **动画系统** – 显示/隐藏元素时支持淡入淡出效果。
- 🖱️ **自动按钮状态** – 悬停和按下时的颜色变化由库自动处理。
- 🔔 **自定义回调** – 通过 lambda 表达式绑定点击、悬停进入/离开事件。
- 🧊 **圆角支持** – 每个元素可独立设置圆角半径。
- 📐 **层级管理** – 控制元素的 Z 序（置顶、置底、上下移动）。
- 📏 **响应式缩放** – 窗口大小改变时，所有元素及文字自动等比缩放。
- 🖋️ **自定义字体** – 可指定系统字体名称和字号（如 `L"微软雅黑"`）。
- ✨ **抗锯齿开关** – 启用 ClearType 使文字边缘更平滑。
- 🚫 **无边框窗口** – 可选的无标题栏模式。
- 🧵 **线程安全任务投递** – 可在工作线程中安全更新 UI。

### 🚀 快速开始

#### 1. 包含头文件
```cpp
#include "easy_UI.hpp"
```

#### 2. 编写 `WinMain` 入口
```cpp
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int nCmdShow) {
    using namespace easy_ui;

    // 创建窗口 (800x600)
    if (!easyUI.InitWindow(800, 600, L"easy_UI 示例")) return 1;

    // 可选：自定义字体和抗锯齿
    easyUI.SetFont(L"微软雅黑", 18);
    easyUI.SetAntialiasing(true);

    // 添加一个圆角按钮
    easyUI.Create(20, 20, 120, 50, 8, L"点我", "btn", 0xFF4CAF50, ElementType::Button);
    easyUI.SetCallback("btn", EventType::Click, []() {
        MessageBoxW(nullptr, L"你点击了按钮！", L"提示", MB_OK);
    });

    // 添加一张 PNG 图片（支持透明）
    easyUI.CreateImage(20, 100, 64, 64, L"icon.png", "img_logo");

    // 显示窗口并进入消息循环
    easyUI.ShowWindow(nCmdShow);
    return easyUI.Run();
}
```

#### 3. 编译命令（MinGW / MSVC）
```bash
g++ -std=c++11 -mwindows -o demo.exe main.cpp -lgdi32 -luser32 -lmsimg32 -lgdiplus
```

### 📖 API 参考（节选）

| 函数 | 说明 |
|------|------|
| `easyUI.InitWindow(w, h, title)` | 创建主窗口。 |
| `easyUI.SetFont(name, size)` | 设置全局自定义字体。 |
| `easyUI.SetAntialiasing(bool)` | 开启/关闭 ClearType 文字抗锯齿。 |
| `easyUI.SetBorderless(bool)` | 启用无边框窗口（需在 `InitWindow` 前调用）。 |
| `easyUI.Create(...)` | 创建新的 UI 元素（矩形、按钮、标签等）。 |
| `easyUI.CreateImage(x, y, w, h, path, name)` | 创建图片元素（支持 PNG、JPG、BMP）。 |
| `easyUI.SetCallback(name, EventType, lambda)` | 绑定事件回调（点击、悬停进入、悬停离开）。 |
| `easyUI.SetCustomDraw(name, callback)` | 设置自定义 GDI 绘制函数。 |
| `easyUI.Show(name, duration_ms)` | 显示元素（可指定淡入动画时长）。 |
| `easyUI.Hide(name, duration_ms)` | 隐藏元素（可指定淡出动画时长）。 |
| `easyUI.LayerTop(name)` / `easyUI.LayerBottom(name)` | 调整元素 Z 序。 |
| `easyUI.Renovate(name)` | 标记元素为“脏”，触发重绘。 |
| `easyUI.Post(task)` | 将任务投递到 UI 线程执行（线程安全）。 |
| `easyUI.Run()` | 进入 Windows 消息循环。 |

### 🎯 适用场景

- 嵌入式设备的**配置工具**界面。
- 游戏**启动器**或模组管理器。
- 开发者**内部工具**（日志查看器、调试面板）。
- 全屏应用程序的**轻量级悬浮窗**。
- Win32 / GDI / GDI+ 编程的**教学示例**。

### ⚠️ 环境要求

- Windows 操作系统。
- 支持 C++11 或更高版本的编译器。
- 链接 `gdi32.lib`、`user32.lib`、`msimg32.lib` 和 `gdiplus.lib`。

---

<p align="center">
  <b>📁 Single header · ⚡ Zero dependencies · 🖥️ Pure Win32 + GDI+</b>
</p>
