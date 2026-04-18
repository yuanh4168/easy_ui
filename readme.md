# easy_UI.hpp · 轻量级 Windows 原生 UI 渲染库

[English](#english) | [中文](#chinese)

---

<a name="english"></a>
## English

### 📌 Overview

**easy_UI.hpp** is a **single‑header, lightweight UI rendering library** for Windows.  
It uses **native Win32 API and GDI** to draw controls directly — **no external dependencies** (no SDL, Qt, or .NET).  
Designed for small tools, embedded front‑ends, and quick prototyping.

### ✨ Features

- 🧩 **Single‑header library** – just `#include "easy_UI.hpp"` and you're ready.
- 🪟 **Native Windows rendering** – creates a real Win32 window with GDI drawing.
- 🎨 **Built‑in controls** – Rectangles, Buttons, Labels (extensible to Image / Custom).
- 🖱️ **Automatic button states** – hover and pressed effects are handled for you.
- 🔔 **Custom callbacks** – attach your own logic to button clicks via lambdas.
- 🧊 **Rounded corners** – per‑element radius support.
- 📐 **Layer management** – Z‑order control (top, bottom, move up/down).
- 🖋️ **Custom fonts** – choose any system font and size (e.g., `L"Segoe UI"`, `L"微软雅黑"`).
- ✨ **Antialiasing toggle** – enable ClearType for smoother text.
- 🚫 **Borderless window** – optional frameless mode.

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

    // Add a button
    easyUI.Create(20, 20, 120, 50, 8, L"Click Me", "btn", 0xFF4CAF50, ElementType::Button);
    easyUI.SetCallback("btn", []() {
        MessageBoxW(nullptr, L"Hello from easy_UI!", L"Info", MB_OK);
    });

    // Show window and run message loop
    easyUI.ShowWindow(nCmdShow);
    return easyUI.Run();
}
```

#### 3. Compile (MinGW / MSVC)
```bash
g++ -std=c++11 -mwindows -o demo.exe main.cpp -lgdi32 -luser32
```

### 📖 API Reference (selection)

| Function | Description |
|----------|-------------|
| `easyUI.InitWindow(w, h, title)` | Creates the main window. |
| `easyUI.SetFont(name, size)` | Sets a custom font for all text. |
| `easyUI.SetAntialiasing(bool)` | Toggles ClearType text rendering. |
| `easyUI.SetBorderless(bool)` | Enables frameless window (call *before* `InitWindow`). |
| `easyUI.Create(...)` | Creates a new UI element. |
| `easyUI.SetCallback(name, lambda)` | Attaches a click handler to a button. |
| `easyUI.Show(name)` / `easyUI.Hide(name)` | Shows/hides an element. |
| `easyUI.LayerTop(name)` / `easyUI.LayerBottom(name)` | Adjusts Z‑order. |
| `easyUI.Renovate(name)` | Marks an element dirty (forces redraw). |
| `easyUI.Run()` | Enters the Windows message loop. |

### 🎯 Use Cases

- **Configuration tools** for embedded devices.
- **Game launchers** or mod managers.
- **Internal developer tools** (log viewers, debug panels).
- **Lightweight overlays** for full‑screen applications.
- **Educational projects** demonstrating Win32 / GDI without heavy frameworks.

### ⚠️ Requirements

- Windows operating system.
- C++11 or later compiler.
- Link with `gdi32.lib` and `user32.lib`.

---

<a name="chinese"></a>
## 中文

### 📌 概述

**easy_UI.hpp** 是一个**单头文件、轻量级的 Windows UI 渲染库**。  
它直接使用 **Win32 API 和 GDI** 进行绘制，**无需任何第三方依赖**（无需 SDL、Qt 或 .NET）。  
适用于小型工具、嵌入式前端以及快速原型开发。

### ✨ 特性

- 🧩 **单头文件库** – 只需 `#include "easy_UI.hpp"` 即可使用。
- 🪟 **原生 Windows 渲染** – 创建真实的 Win32 窗口，通过 GDI 绘制所有控件。
- 🎨 **内置控件类型** – 矩形、按钮、标签（可扩展图像或自定义类型）。
- 🖱️ **自动按钮状态** – 悬停和按下时的颜色变化由库自动处理。
- 🔔 **自定义回调** – 通过 lambda 表达式为按钮绑定点击逻辑。
- 🧊 **圆角支持** – 每个元素可独立设置圆角半径。
- 📐 **层级管理** – 控制元素的 Z 序（置顶、置底、上下移动）。
- 🖋️ **自定义字体** – 可指定系统字体名称和字号（如 `L"微软雅黑"`）。
- ✨ **抗锯齿开关** – 启用 ClearType 使文字边缘更平滑。
- 🚫 **无边框窗口** – 可选的无标题栏模式。

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

    // 添加一个按钮
    easyUI.Create(20, 20, 120, 50, 8, L"点我", "btn", 0xFF4CAF50, ElementType::Button);
    easyUI.SetCallback("btn", []() {
        MessageBoxW(nullptr, L"你点击了按钮！", L"提示", MB_OK);
    });

    // 显示窗口并进入消息循环
    easyUI.ShowWindow(nCmdShow);
    return easyUI.Run();
}
```

#### 3. 编译命令（MinGW / MSVC）
```bash
g++ -std=c++11 -mwindows -o demo.exe main.cpp -lgdi32 -luser32
```

### 📖 API 参考（节选）

| 函数 | 说明 |
|------|------|
| `easyUI.InitWindow(w, h, title)` | 创建主窗口。 |
| `easyUI.SetFont(name, size)` | 设置全局自定义字体。 |
| `easyUI.SetAntialiasing(bool)` | 开启/关闭 ClearType 文字抗锯齿。 |
| `easyUI.SetBorderless(bool)` | 启用无边框窗口（需在 `InitWindow` 前调用）。 |
| `easyUI.Create(...)` | 创建新的 UI 元素。 |
| `easyUI.SetCallback(name, lambda)` | 为按钮绑定点击回调函数。 |
| `easyUI.Show(name)` / `easyUI.Hide(name)` | 显示/隐藏指定元素。 |
| `easyUI.LayerTop(name)` / `easyUI.LayerBottom(name)` | 调整元素 Z 序。 |
| `easyUI.Renovate(name)` | 标记元素为“脏”，触发重绘。 |
| `easyUI.Run()` | 进入 Windows 消息循环。 |

### 🎯 适用场景

- 嵌入式设备的**配置工具**界面。
- 游戏**启动器**或模组管理器。
- 开发者**内部工具**（日志查看器、调试面板）。
- 全屏应用程序的**轻量级悬浮窗**。
- Win32 / GDI 编程的**教学示例**。

### ⚠️ 环境要求

- Windows 操作系统。
- 支持 C++11 或更高版本的编译器。
- 链接 `gdi32.lib` 和 `user32.lib`。

---

<p align="center">
  <b>📁 Single header · ⚡ Zero dependencies · 🖥️ Pure Win32</b>
</p>
