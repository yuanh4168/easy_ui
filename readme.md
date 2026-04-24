[中文介绍](#中文介绍) | [English Introduction](#english-introduction)

---

# 中文介绍

## 概述
`easy_UI` 是一个轻量级单头文件 Windows UI 库（C++11 / Win32 + GDI+），致力于用最少的代码创建常见控件。所有交互和渲染都通过全局对象 `easyUI` 暴露，采用 `easyUI.FunctionName()` 的统一调用风格。库内部封装了 Win32 窗口创建、消息循环、GDI+ 双缓冲渲染，用户无需接触底层消息即可快速搭建界面。

## 功能特性
- **单头文件，零配置**：只需 `#include "easy_UI.hpp"` 并链接 `gdiplus.lib`。
- **完全中文支持**：默认使用微软雅黑字体，无乱码。
- **三层架构**：底层绘图、中层视觉原语、高层交互控件，便于扩展。
- **自动缩放**：窗口尺寸改变时控件位置、大小和文字按比例缩放。
- **双缓冲防闪烁**：提供平滑绘制体验。
- **自定义样式**：支持按控件或全局设置颜色、圆角、字体等。
- **主题切换**：随时更换整体色系。
- **丰富控件**：按钮、标签、文本框、下拉组合框、图片。

## 快速开始
```cpp
#include "easy_UI.hpp"

int main() {
    eui::Application app;
    app.title = L"我的应用";
    app.width = 500;
    app.height = 300;

    app.OnInit = []() {
        using namespace eui;
        easyUI.SetGlobalFont(L"Microsoft YaHei", 14); // 设置中文字体

        // 创建一个按钮（圆角半径 8）
        easyUI.CreateButton("btn", L"点击我", 20, 20, 100, 36, 8);
        easyUI.OnClick("btn", []() {
            easyUI.Text("btn", L"被点击了");
        });

        // 开启自动缩放（以 500×300 为基准）
        easyUI.SetAutoScale(true, 500, 300);
    };

    return easyUI.Run(app);
}
```

## 编译命令
**MinGW-w64 (g++)**
```bash
g++ main.cpp -o test.exe -std=c++11 -mwindows -lgdiplus -static
```
**MSVC**
```bash
cl main.cpp /std:c++11 /EHsc /Fe:test.exe /link gdiplus.lib
```

## 全局对象及类型

### `easyUI` 对象
所有操作均通过全局对象 `easyUI` 进行，无需手动创建实例。

### 主要类型
- `eui::Color` – RGBA 颜色结构  
  `eui::Color(红, 绿, 蓝, 透明度)`  
  例：`eui::Color(255, 0, 0, 255)` 表示红色。
- `eui::Theme` – 主题配置  
  包含：`bg`(背景), `fg`(前景), `border`(边框), `accent`(强调色), `hover`(悬停色), `pressed`(按下色), `text`(文字色), `textDisabled`(禁用文字色), `shadow`(阴影色), `cornerRadius`(默认圆角半径)。
- `eui::BtnStyle` – 按钮样式  
  包含：`bgNormal`, `bgHover`, `bgPressed`(正常/悬停/按下背景色), `textColor`(文字色), `textDisabled`(禁用文字色), `border`(边框色), `radius`(圆角半径)。
- `eui::Application` – 应用描述  
  包含：`title`(窗口标题), `width`/`height`(初始宽高), `OnInit`(初始化回调函数，在此创建控件)。

## 函数参考

### 初始化与运行

#### `easyUI.SetGlobalFont(family, size, style)`
设置全局默认字体，影响所有控件文字。

- `family` (const wchar_t*) : 字体名称，如 `L"Microsoft YaHei"`、`L"SimHei"`
- `size` (float) : 字号，默认 14
- `style` (int) : GDI+ 字体样式，如 `Gdiplus::FontStyleRegular`、`Gdiplus::FontStyleBold`

```cpp
easyUI.SetGlobalFont(L"SimHei", 16.0f, Gdiplus::FontStyleBold);
```

#### `easyUI.Run(app)`
启动应用程序并进入消息循环，阻塞直到窗口关闭。

- `app` (const eui::Application&) : 应用配置
- 返回值 `int` : 退出码

### 控件创建
所有控件创建函数返回控件名称（`std::string`），用于后续操作。可选参数 `radius` 为圆角半径，`-1` 表示使用主题/样式默认值。

#### `easyUI.CreateButton(name, text, x, y, w, h, radius = -1)`
创建一个按钮。

```cpp
easyUI.CreateButton("btnOk", L"确定", 10, 10, 80, 30, 6);
```

#### `easyUI.CreateLabel(name, text, x, y, w, h, radius = -1)`
创建一个标签（`radius` 参数无实际效果，保留接口兼容）。

```cpp
easyUI.CreateLabel("lbl", L"欢迎使用", 100, 10, 200, 30);
```

#### `easyUI.CreateTextBox(name, x, y, w, h, radius = -1)`
创建一个文本框，点击时弹出系统编辑控件进行输入。

```cpp
easyUI.CreateTextBox("txtInput", 10, 50, 200, 30);
```

#### `easyUI.CreateComboBox(name, x, y, w, h, radius = -1)`
创建一个下拉组合框。使用 `AddComboItem` 添加选项。

```cpp
easyUI.CreateComboBox("combo", 10, 100, 150, 30, 10);
```

#### `easyUI.CreateImage(name, path, x, y, w, h)`
创建一个图片控件，支持 PNG、JPG、BMP 等格式。

```cpp
easyUI.CreateImage("logo", L"res/logo.png", 10, 10, 64, 64);
```

### 属性操作

#### `easyUI.Text(name, text)` / `easyUI.Text(name)`
设置或获取控件文本（适用于按钮、标签、文本框）。

```cpp
easyUI.Text("lbl", L"新内容");
std::wstring s = easyUI.Text("txt1");
```

#### `easyUI.Visible(name, vis)`
设置控件可见性。`vis` 为 `true` 时可见。

#### `easyUI.Enable(name, en)`
启用或禁用控件。`en` 为 `true` 时启用，禁用时按钮不可点击且外观变灰。

#### `easyUI.Rect(name, x, y, w, h)`
设置控件的原始矩形（在未缩放时使用），自动缩放时以此矩形为基准。

### 事件回调

#### `easyUI.OnClick(name, fn)`
设置按钮点击回调，`fn` 为无参函数。

```cpp
easyUI.OnClick("btn", []() {
    MessageBoxW(nullptr, L"按钮被按下", L"提示", MB_OK);
});
```

#### `easyUI.OnTextChange(name, fn)`
设置文本框内容变化回调，`fn` 接收新文本字符串。

```cpp
easyUI.OnTextChange("txt", [](const std::wstring& txt) {
    easyUI.Text("lbl", L"输入: " + txt);
});
```

### 组合框专用

#### `easyUI.AddComboItem(name, item)`
向下拉组合框添加一个选项。

```cpp
easyUI.AddComboItem("combo", L"苹果");
```

#### `easyUI.GetComboIndex(name)`
返回当前选中项的索引，未选中时返回 -1。

#### `easyUI.GetComboItems(name)`
返回所有选项的 `std::vector<std::wstring>`。

### 样式与主题

#### `easyUI.SetBtnStyle(name, style)`
为指定按钮设置样式，覆盖全局样式。

```cpp
eui::BtnStyle redStyle;
redStyle.bgNormal = eui::Color(180,60,60);
redStyle.bgHover  = eui::Color(210,80,80);
redStyle.textColor = eui::Color(255,255,255);
redStyle.border = eui::Color(120,40,40);
easyUI.SetBtnStyle("btn1", redStyle);
```

#### `easyUI.SetGlobalBtnStyle(style)`
设置全局默认按钮样式，影响所有未单独设置样式的按钮。

#### `easyUI.SetLabelColor(name, color)`
设置标签文字颜色。

```cpp
easyUI.SetLabelColor("lbl", eui::Color(255,200,0));
```

#### `easyUI.SetTheme(theme)` / `easyUI.Theme()`
设置或获取当前主题对象。主题修改后界面自动刷新。

```cpp
easyUI.Theme().bg = eui::Color(30,30,35);
```

### 全局控制

#### `easyUI.SetAutoScale(enable, baseW, baseH)`
开启或关闭自动缩放。`baseW`、`baseH` 为缩放基准尺寸，通常为窗口初始客户区大小。

```cpp
easyUI.SetAutoScale(true, 800, 600);
```

#### `easyUI.SetAntiAlias(on)`
开启或关闭抗锯齿渲染。`on` 为 `true` 时开启。

## 完整示例
下面是一个包含多种控件、自定义样式、自动缩放和事件响应的完整程序。

```cpp
#include "easy_UI.hpp"

int main() {
    eui::Application app;
    app.title = L"easy_UI 演示";
    app.width = 500;
    app.height = 300;

    app.OnInit = []() {
        using namespace eui;

        // 暗色主题
        Theme t;
        t.bg = Color(35, 35, 40);
        t.fg = Color(60, 60, 65);
        t.accent = Color(0, 160, 220);
        t.text = Color(240, 240, 240);
        easyUI.SetTheme(t);

        // 红色按钮（圆角 15）
        easyUI.CreateButton("btn1", L"大红按钮", 20, 20, 100, 36, 15);
        easyUI.SetBtnStyle("btn1", []() {
            BtnStyle s;
            s.bgNormal = Color(180,60,60);
            s.bgHover = Color(210,80,80);
            s.textColor = Color(255,255,255);
            s.border = Color(120,40,40);
            return s;
        }());

        // 多彩标签
        easyUI.CreateLabel("lbl1", L"多彩标签", 140, 20, 150, 30);
        easyUI.SetLabelColor("lbl1", Color(255,200,100));

        // 文本框
        easyUI.CreateTextBox("txt1", 20, 70, 200, 30);

        // 蓝色按钮（圆角 10）
        easyUI.CreateButton("btn2", L"蓝色按钮", 240, 70, 80, 30, 10);
        easyUI.SetBtnStyle("btn2", []() {
            BtnStyle s;
            s.bgNormal = Color(50,100,180);
            s.bgHover = Color(70,120,210);
            s.textColor = Color(255,255,255);
            s.border = Color(40,80,160);
            return s;
        }());

        // 下拉组合框（圆角 12）
        easyUI.CreateComboBox("combo1", 20, 120, 150, 30, 12);
        easyUI.AddComboItem("combo1", L"🍎 苹果");
        easyUI.AddComboItem("combo1", L"🍌 香蕉");
        easyUI.AddComboItem("combo1", L"🍒 樱桃");

        // 金色按钮
        easyUI.CreateButton("btnGold", L"金色按钮", 200, 120, 100, 30);
        easyUI.SetBtnStyle("btnGold", []() {
            BtnStyle s;
            s.bgNormal = Color(200,160,40);
            s.bgHover = Color(220,180,60);
            s.textColor = Color(30,30,30);
            s.border = Color(160,120,20);
            return s;
        }());

        // 信息标签
        easyUI.CreateLabel("lblInfo", L"当前操作信息", 20, 210, 460, 30);
        easyUI.SetLabelColor("lblInfo", Color(100,255,100));

        // 事件绑定
        easyUI.OnClick("btn1", []() {
            static int n = 0;
            wchar_t buf[64];
            swprintf(buf, 64, L"红色按钮 %d 次", ++n);
            easyUI.Text("lbl1", buf);
        });
        easyUI.OnClick("btn2", []() {
            easyUI.Text("lbl1", L"获取: " + easyUI.Text("txt1"));
        });
        easyUI.OnClick("btnGold", []() {
            static bool on = false; on = !on;
            easyUI.SetAntiAlias(on);
            easyUI.Text("lblInfo", on ? L"抗锯齿：开启" : L"抗锯齿：关闭");
        });
        easyUI.OnTextChange("txt1", [](const std::wstring& txt) {
            easyUI.Text("lblInfo", L"输入内容：" + (txt.empty() ? L"(空)" : txt));
        });

        easyUI.SetAutoScale(true, 500, 300);
    };

    return easyUI.Run(app);
}
```

---

# English Introduction

## Overview
`easy_UI` is a lightweight single-header Windows UI library (C++11 / Win32 + GDI+). It allows you to create common controls with minimal code. All interactions and rendering are exposed through the global `easyUI` object using a `easyUI.FunctionName()` style. The library handles Win32 window creation, message loop, and GDI+ double‑buffering internally, so you can build interfaces without dealing with low‑level messages.

## Features
- **Single header, zero configuration**: just `#include "easy_UI.hpp"` and link `gdiplus.lib`.
- **Full Unicode support**: works seamlessly with any language (default font is Microsoft YaHei for CJK).
- **Three‑layer architecture**: core drawing, reusable visual primitives, interactive controls – easy to extend.
- **Automatic scaling**: controls and text scale proportionally when the window is resized.
- **Double‑buffering**: flicker‑free rendering.
- **Customizable styles**: set colors, corner radii, and fonts per control or globally.
- **Theme switching**: change the entire color scheme at runtime.
- **Rich controls**: Button, Label, TextBox, ComboBox (dropdown), Image.

## Quick Start
```cpp
#include "easy_UI.hpp"

int main() {
    eui::Application app;
    app.title = L"My App";
    app.width = 500;
    app.height = 300;

    app.OnInit = []() {
        using namespace eui;
        easyUI.SetGlobalFont(L"Segoe UI", 14); // Set a modern font

        // Create a button with corner radius 8
        easyUI.CreateButton("btn", L"Click me", 20, 20, 100, 36, 8);
        easyUI.OnClick("btn", []() {
            easyUI.Text("btn", L"Clicked");
        });

        // Enable auto scaling based on 500x300
        easyUI.SetAutoScale(true, 500, 300);
    };

    return easyUI.Run(app);
}
```

## Compilation
**MinGW-w64 (g++)**
```bash
g++ main.cpp -o test.exe -std=c++11 -mwindows -lgdiplus -static
```
**MSVC**
```bash
cl main.cpp /std:c++11 /EHsc /Fe:test.exe /link gdiplus.lib
```

## Global Object & Types

### `easyUI` Object
All operations are performed through the global `easyUI` object.

### Main Types
- `eui::Color` – RGBA color structure  
  `eui::Color(red, green, blue, alpha)`  
  e.g., `eui::Color(255, 0, 0, 255)` is opaque red.
- `eui::Theme` – Theme configuration  
  Members: `bg`, `fg`, `border`, `accent`, `hover`, `pressed`, `text`, `textDisabled`, `shadow`, `cornerRadius`.
- `eui::BtnStyle` – Button style  
  Members: `bgNormal`, `bgHover`, `bgPressed`, `textColor`, `textDisabled`, `border`, `radius`.
- `eui::Application` – Application descriptor  
  Members: `title`, `width`, `height`, `OnInit` (callback where you create controls).

## Function Reference

### Initialization & Run

#### `easyUI.SetGlobalFont(family, size, style)`
Sets the global default font used by all controls.

- `family` (const wchar_t*) – font family name, e.g. `L"Segoe UI"`
- `size` (float) – font size (default 14)
- `style` (int) – GDI+ font style (e.g. `Gdiplus::FontStyleRegular`)

```cpp
easyUI.SetGlobalFont(L"Consolas", 16.0f, Gdiplus::FontStyleBold);
```

#### `easyUI.Run(app)`
Starts the application message loop. Returns the exit code.

### Control Creation
All creation functions return the control name (`std::string`). The optional `radius` parameter sets the corner radius; `-1` means “use default from theme/style”.

#### `easyUI.CreateButton(name, text, x, y, w, h, radius = -1)`
Creates a button.

```cpp
easyUI.CreateButton("btnOk", L"OK", 10, 10, 80, 30, 6);
```

#### `easyUI.CreateLabel(name, text, x, y, w, h, radius = -1)`
Creates a label. (`radius` is ignored but kept for interface consistency.)

#### `easyUI.CreateTextBox(name, x, y, w, h, radius = -1)`
Creates a text box. Clicking it opens a system edit control for input.

#### `easyUI.CreateComboBox(name, x, y, w, h, radius = -1)`
Creates a drop‑down combo box. Use `AddComboItem` to populate it.

```cpp
easyUI.CreateComboBox("combo", 10, 100, 150, 30, 10);
```

#### `easyUI.CreateImage(name, path, x, y, w, h)`
Creates an image control (supports PNG, JPG, BMP).

```cpp
easyUI.CreateImage("logo", L"res/logo.png", 10, 10, 64, 64);
```

### Property Manipulation

#### `easyUI.Text(name, text)` / `easyUI.Text(name)`
Set or get the text of a button, label, or text box.

```cpp
easyUI.Text("lbl", L"New content");
std::wstring s = easyUI.Text("txt1");
```

#### `easyUI.Visible(name, vis)`
Show (`true`) or hide (`false`) a control.

#### `easyUI.Enable(name, en)`
Enable or disable a control. Disabled buttons are grayed out and cannot be clicked.

#### `easyUI.Rect(name, x, y, w, h)`
Sets the original (unscaled) rectangle of a control.

### Event Callbacks

#### `easyUI.OnClick(name, fn)`
Registers a click callback for a button. `fn` is a `void()` function.

#### `easyUI.OnTextChange(name, fn)`
Registers a text‑changed callback for a text box. `fn` receives the new `std::wstring`.

### ComboBox Specific

#### `easyUI.AddComboItem(name, item)`
Adds an item to the drop‑down list.

#### `easyUI.GetComboIndex(name)`
Returns the index of the selected item, or `-1` if none is selected.

#### `easyUI.GetComboItems(name)`
Returns a `std::vector<std::wstring>` containing all items.

### Style & Theme

#### `easyUI.SetBtnStyle(name, style)`
Applies a custom `BtnStyle` to a specific button (overrides global style).

```cpp
eui::BtnStyle red;
red.bgNormal = eui::Color(180,60,60);
red.bgHover  = eui::Color(210,80,80);
red.textColor = eui::Color(255,255,255);
easyUI.SetBtnStyle("btn1", red);
```

#### `easyUI.SetGlobalBtnStyle(style)`
Sets the default button style for all buttons that do not have an explicit style.

#### `easyUI.SetLabelColor(name, color)`
Changes the text color of a label.

#### `easyUI.SetTheme(theme)` / `easyUI.Theme()`
Set or retrieve the current theme. Modifying the returned reference triggers an immediate redraw.

### Global Control

#### `easyUI.SetAutoScale(enable, baseW, baseH)`
Enables or disables automatic scaling. `baseW` and `baseH` are the reference dimensions (usually the initial client area size).

```cpp
easyUI.SetAutoScale(true, 800, 600);
```

#### `easyUI.SetAntiAlias(on)`
Turns anti‑aliased rendering on (`true`) or off.

## Full Example
A complete demo showing multiple controls, custom styles, auto‑scaling, and events.

```cpp
#include "easy_UI.hpp"

int main() {
    eui::Application app;
    app.title = L"easy_UI Demo";
    app.width = 500;
    app.height = 300;

    app.OnInit = []() {
        using namespace eui;

        Theme t;
        t.bg = Color(35, 35, 40);
        t.fg = Color(60, 60, 65);
        t.accent = Color(0, 160, 220);
        t.text = Color(240, 240, 240);
        easyUI.SetTheme(t);

        easyUI.CreateButton("btn1", L"Red Button", 20, 20, 100, 36, 15);
        easyUI.SetBtnStyle("btn1", []() {
            BtnStyle s;
            s.bgNormal = Color(180,60,60); s.bgHover = Color(210,80,80);
            s.textColor = Color(255,255,255); s.border = Color(120,40,40);
            return s;
        }());

        easyUI.CreateLabel("lbl1", L"Colorful Label", 140, 20, 150, 30);
        easyUI.SetLabelColor("lbl1", Color(255,200,100));

        easyUI.CreateTextBox("txt1", 20, 70, 200, 30);

        easyUI.CreateButton("btn2", L"Blue Button", 240, 70, 80, 30, 10);
        easyUI.SetBtnStyle("btn2", []() {
            BtnStyle s;
            s.bgNormal = Color(50,100,180); s.bgHover = Color(70,120,210);
            s.textColor = Color(255,255,255); s.border = Color(40,80,160);
            return s;
        }());

        easyUI.CreateComboBox("combo1", 20, 120, 150, 30, 12);
        easyUI.AddComboItem("combo1", L"Apple");
        easyUI.AddComboItem("combo1", L"Banana");
        easyUI.AddComboItem("combo1", L"Cherry");

        easyUI.CreateButton("btnGold", L"Gold Button", 200, 120, 100, 30);
        easyUI.SetBtnStyle("btnGold", []() {
            BtnStyle s;
            s.bgNormal = Color(200,160,40); s.bgHover = Color(220,180,60);
            s.textColor = Color(30,30,30); s.border = Color(160,120,20);
            return s;
        }());

        easyUI.CreateLabel("lblInfo", L"Info area", 20, 210, 460, 30);
        easyUI.SetLabelColor("lblInfo", Color(100,255,100));

        easyUI.OnClick("btn1", []() {
            static int n = 0;
            wchar_t buf[64];
            swprintf(buf, 64, L"Red clicked %d times", ++n);
            easyUI.Text("lbl1", buf);
        });
        easyUI.OnClick("btn2", []() {
            easyUI.Text("lbl1", L"Get: " + easyUI.Text("txt1"));
        });
        easyUI.OnClick("btnGold", []() {
            static bool on = false; on = !on;
            easyUI.SetAntiAlias(on);
            easyUI.Text("lblInfo", on ? L"Anti-alias: ON" : L"Anti-alias: OFF");
        });
        easyUI.OnTextChange("txt1", [](const std::wstring& txt) {
            easyUI.Text("lblInfo", L"Input: " + (txt.empty() ? L"(empty)" : txt));
        });

        easyUI.SetAutoScale(true, 500, 300);
    };

    return easyUI.Run(app);
}
```

## License
MIT License. Free for both personal and commercial use.
