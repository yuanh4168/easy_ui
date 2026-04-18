/**
 * @file easy_UI.hpp
 * @brief 轻量化 Windows 原生 UI 渲染库（单头文件实现）
 * 
 * 使用方法：
 *   1. #include "easy_UI.hpp"
 *   2. easy_ui::EasyUI::InitWindow() 或 easyUI.InitWindow()
 *   3. easyUI.Create(...) 添加元素
 *   4. easyUI.SetCallback(...) 设置按钮回调
 *   5. easyUI.Run() 进入消息循环
 * 
 * 编译要求：Windows + C++11 或更高版本
 * 编译命令：g++ -std=c++11 main.cpp -lgdi32 -luser32 -mwindows
 */

#ifndef EASY_UI_HPP
#define EASY_UI_HPP

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <string>
#include <unordered_map>
#include <list>
#include <memory>
#include <algorithm>
#include <functional>
#include <cstdint>

namespace easy_ui {

//=============================================================================
// 基础类型定义
//=============================================================================

using Color = uint32_t;                     // 0xAARRGGBB

enum class ElementType {
    Window, Rect, Button, Label, Image, Custom
};

// 前向声明
class UIElement;
using ElementPtr = std::shared_ptr<UIElement>;
using ButtonCallback = std::function<void()>;

//=============================================================================
// UI 元素类（扩展状态）
//=============================================================================

class UIElement {
public:
    int x, y, width, height, radius;
    std::wstring text;
    std::string name;
    Color color;
    ElementType type;

    bool visible = true;
    bool dirty   = true;
    int  z_order = 0;

    bool is_hovered  = false;
    bool is_pressed  = false;

    ButtonCallback callback;

    UIElement() = default;
    UIElement(int x_, int y_, int w_, int h_, int r_,
              const std::wstring& txt, const std::string& nm,
              Color c, ElementType t)
        : x(x_), y(y_), width(w_), height(h_), radius(r_),
          text(txt), name(nm), color(c), type(t) {}

    void mark_dirty()   { dirty = true; }
    void clear_dirty()  { dirty = false; }

    Color get_draw_color() const {
        if (type != ElementType::Button) return color;
        if (is_pressed) {
            BYTE a = (color >> 24) & 0xFF;
            BYTE r = ((color >> 16) & 0xFF) * 0.8;
            BYTE g = ((color >> 8)  & 0xFF) * 0.8;
            BYTE b = (color & 0xFF) * 0.8;
            return (a << 24) | (r << 16) | (g << 8) | b;
        } else if (is_hovered) {
            BYTE a = (color >> 24) & 0xFF;
            BYTE r = std::min(255, (int)(((color >> 16) & 0xFF) * 1.2));
            BYTE g = std::min(255, (int)(((color >> 8)  & 0xFF) * 1.2));
            BYTE b = std::min(255, (int)((color & 0xFF) * 1.2));
            return (a << 24) | (r << 16) | (g << 8) | b;
        }
        return color;
    }
};

//=============================================================================
// UI 管理器（单例）
//=============================================================================

class UIManager {
public:
    static UIManager& instance() {
        static UIManager mgr;
        return mgr;
    }

    //----------------------------- 全局设置 -----------------------------//
    void SetAntialiasing(bool enable) {
        antialiasing_enabled_ = enable;
        renovate_all();
    }

    void SetBorderless(bool enable) {
        borderless_ = enable;
    }

    bool IsBorderless() const { return borderless_; }

    void SetFont(const std::wstring& fontName, int fontSize) {
        custom_font_name_ = fontName;
        custom_font_size_ = fontSize;
        use_custom_font_ = true;
        renovate_all();
    }

    void SetDefaultFont() {
        use_custom_font_ = false;
        renovate_all();
    }

    //----------------------------- 创建/销毁 -----------------------------//
    ElementPtr create(int x, int y, int width, int height, int radius,
                      const std::wstring& text, const std::string& name,
                      Color color, ElementType type) 
    {
        if (name.empty()) return nullptr;
        if (elements_by_name_.count(name)) destroy(name);

        auto elem = std::make_shared<UIElement>(x, y, width, height, radius,
                                                text, name, color, type);
        elem->z_order = next_z_order_++;
        elements_by_name_[name] = elem;
        layer_list_.push_back(elem);
        return elem;
    }

    void destroy(const std::string& name) {
        auto it = elements_by_name_.find(name);
        if (it == elements_by_name_.end()) return;
        auto elem = it->second;
        layer_list_.remove(elem);
        elements_by_name_.erase(it);
    }

    void destroy_all() {
        elements_by_name_.clear();
        layer_list_.clear();
        next_z_order_ = 0;
    }

    //----------------------------- 回调设置 -----------------------------//
    void set_callback(const std::string& name, ButtonCallback cb) {
        auto e = find_by_name(name);
        if (e) e->callback = std::move(cb);
    }

    //----------------------------- 显示控制 -----------------------------//
    void show(const std::string& name) {
        if (name.empty()) { show_all(); return; }
        auto e = find_by_name(name);
        if (e && !e->visible) { e->visible = true; e->mark_dirty(); }
    }

    void show_all() {
        for (auto& e : layer_list_) 
            if (!e->visible) { e->visible = true; e->mark_dirty(); }
    }

    void hide(const std::string& name) {
        auto e = find_by_name(name);
        if (e && e->visible) { e->visible = false; e->mark_dirty(); }
    }

    void hide_all() {
        for (auto& e : layer_list_)
            if (e->visible) { e->visible = false; e->mark_dirty(); }
    }

    //----------------------------- 刷新/重绘 -----------------------------//
    void renovate(const std::string& name) {
        if (name.empty()) { renovate_all(); return; }
        auto e = find_by_name(name);
        if (e) e->mark_dirty();
    }

    void renovate_all() {
        for (auto& e : layer_list_) e->mark_dirty();
    }

    //----------------------------- 层级管理 -----------------------------//
    void layer_top(const std::string& name) {
        auto e = find_by_name(name);
        if (!e) return;
        layer_list_.remove(e);
        layer_list_.push_back(e);
        recalc_z_orders();
        e->mark_dirty();
    }

    void layer_bottom(const std::string& name) {
        auto e = find_by_name(name);
        if (!e) return;
        layer_list_.remove(e);
        layer_list_.push_front(e);
        recalc_z_orders();
        e->mark_dirty();
    }

    void layer_move_up(const std::string& name, int steps) {
        if (steps == 0) return;
        auto e = find_by_name(name);
        if (!e) return;
        auto it = std::find(layer_list_.begin(), layer_list_.end(), e);
        if (it == layer_list_.end()) return;

        if (steps > 0) {
            auto next_it = it;
            for (int i = 0; i < steps && std::next(next_it) != layer_list_.end(); ++i)
                ++next_it;
            if (next_it != it)
                layer_list_.splice(std::next(next_it), layer_list_, it);
        } else {
            auto prev_it = it;
            for (int i = 0; i < -steps && prev_it != layer_list_.begin(); ++i)
                --prev_it;
            if (prev_it != it)
                layer_list_.splice(prev_it, layer_list_, it);
        }
        recalc_z_orders();
        e->mark_dirty();
    }

    //----------------------------- 鼠标事件处理 -----------------------------//
    void OnMouseMove(int x, int y) {
        bool need_redraw = false;
        for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
            auto& e = *it;
            if (!e->visible) continue;
            bool hit = (x >= e->x && x < e->x + e->width &&
                        y >= e->y && y < e->y + e->height);
            if (hit) {
                if (e->type == ElementType::Button && !e->is_hovered) {
                    e->is_hovered = true;
                    e->mark_dirty();
                    need_redraw = true;
                }
                for (auto& other : layer_list_) {
                    if (other != e && other->is_hovered) {
                        other->is_hovered = false;
                        other->mark_dirty();
                        need_redraw = true;
                    }
                }
                if (need_redraw) ::InvalidateRect(active_hwnd_, nullptr, FALSE);
                return;
            }
        }
        for (auto& e : layer_list_) {
            if (e->is_hovered) {
                e->is_hovered = false;
                e->mark_dirty();
                need_redraw = true;
            }
        }
        if (need_redraw) ::InvalidateRect(active_hwnd_, nullptr, FALSE);
    }

    void OnMouseDown(int x, int y) {
        for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
            auto& e = *it;
            if (!e->visible) continue;
            if (x >= e->x && x < e->x + e->width &&
                y >= e->y && y < e->y + e->height) {
                if (e->type == ElementType::Button) {
                    e->is_pressed = true;
                    e->mark_dirty();
                    ::InvalidateRect(active_hwnd_, nullptr, FALSE);
                }
                break;
            }
        }
    }

    void OnMouseUp(int x, int y) {
        bool need_redraw = false;
        for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
            auto& e = *it;
            if (!e->visible) continue;
            if (e->type == ElementType::Button && e->is_pressed) {
                e->is_pressed = false;
                e->mark_dirty();
                need_redraw = true;
                if (x >= e->x && x < e->x + e->width &&
                    y >= e->y && y < e->y + e->height) {
                    if (e->callback) e->callback();
                }
                break;
            }
        }
        if (need_redraw) ::InvalidateRect(active_hwnd_, nullptr, FALSE);
    }

    //----------------------------- 渲染接口 -----------------------------//
    void render(HDC hdc) {
        if (antialiasing_enabled_) {
            SetTextCharacterExtra(hdc, 0);
            SetBkMode(hdc, TRANSPARENT);
        }
        for (auto& e : layer_list_) {
            if (!e->visible) continue;
            if (!e->dirty) continue;
            draw_element(hdc, e.get());
            e->clear_dirty();
        }
    }

    //----------------------------- 辅助函数 -----------------------------//
    ElementPtr find_by_name(const std::string& name) const {
        auto it = elements_by_name_.find(name);
        return (it != elements_by_name_.end()) ? it->second : nullptr;
    }

    void set_active_hwnd(HWND hwnd) { active_hwnd_ = hwnd; }

private:
    UIManager() = default;

    void recalc_z_orders() {
        int z = 0;
        for (auto& e : layer_list_) e->z_order = z++;
        next_z_order_ = z;
    }

    void draw_element(HDC hdc, const UIElement* e) {
        Color draw_color = e->get_draw_color();
        BYTE a = (draw_color >> 24) & 0xFF;
        BYTE r = (draw_color >> 16) & 0xFF;
        BYTE g = (draw_color >> 8)  & 0xFF;
        BYTE b = draw_color & 0xFF;

        HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
        HPEN hPen = CreatePen(PS_NULL, 0, 0);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

        if (e->radius > 0) {
            RoundRect(hdc, e->x, e->y, e->x + e->width, e->y + e->height,
                      e->radius * 2, e->radius * 2);
        } else {
            Rectangle(hdc, e->x, e->y, e->x + e->width, e->y + e->height);
        }

        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hBrush);

        if (!e->text.empty()) {
            SetTextColor(hdc, RGB(255, 255, 255));
            HFONT hFont = nullptr;
            if (use_custom_font_) {
                hFont = CreateFontW(
                    custom_font_size_, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    antialiasing_enabled_ ? CLEARTYPE_QUALITY : DEFAULT_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, custom_font_name_.c_str());
            } else {
                hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            }
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            RECT rect = { e->x, e->y, e->x + e->width, e->y + e->height };
            DrawTextW(hdc, e->text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOldFont);
            if (use_custom_font_) DeleteObject(hFont);
        }
    }

    std::unordered_map<std::string, ElementPtr> elements_by_name_;
    std::list<ElementPtr> layer_list_;
    int next_z_order_ = 0;
    bool antialiasing_enabled_ = true;
    bool borderless_ = false;
    HWND active_hwnd_ = nullptr;

    // 字体设置
    bool use_custom_font_ = false;
    std::wstring custom_font_name_ = L"Segoe UI";
    int custom_font_size_ = 16;
};

//=============================================================================
// 窗口管理（内嵌 Win32 实现）
//=============================================================================

class EasyUIWindow {
public:
    EasyUIWindow() = default;

    bool Create(int width = 800, int height = 600, const wchar_t* title = L"easy_UI Window") {
        HINSTANCE hInst = GetModuleHandle(nullptr);
        auto& ui = UIManager::instance();

        WNDCLASSW wc = {};
        wc.lpfnWndProc   = EasyUIWindow::WndProc;
        wc.hInstance     = hInst;
        wc.lpszClassName = L"EasyUIWindowClass";
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassW(&wc);

        DWORD style = WS_OVERLAPPEDWINDOW;
        if (ui.IsBorderless()) {
            style = WS_POPUP | WS_VISIBLE;
        }

        RECT rect = { 0, 0, width, height };
        AdjustWindowRect(&rect, style, FALSE);
        hwnd_ = CreateWindowExW(
            0, L"EasyUIWindowClass", title, style,
            CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
            nullptr, nullptr, hInst, this
        );

        if (hwnd_) {
            ui.set_active_hwnd(hwnd_);
            return true;
        }
        return false;
    }

    void Show(int nCmdShow = SW_SHOW) {
        ShowWindow(hwnd_, nCmdShow);
        UpdateWindow(hwnd_);
    }

    int Run() {
        MSG msg = {};
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return (int)msg.wParam;
    }

    HWND GetHwnd() const { return hwnd_; }

private:
    HWND hwnd_ = nullptr;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        EasyUIWindow* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<EasyUIWindow*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
            pThis->hwnd_ = hwnd;
            UIManager::instance().set_active_hwnd(hwnd);
        } else {
            pThis = reinterpret_cast<EasyUIWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        auto& ui = UIManager::instance();

        if (pThis) {
            switch (msg) {
                case WM_MOUSEMOVE:
                    ui.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    break;
                case WM_LBUTTONDOWN:
                    ui.OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    break;
                case WM_LBUTTONUP:
                    ui.OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    break;
                case WM_PAINT: {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hwnd, &ps);
                    ui.render(hdc);
                    EndPaint(hwnd, &ps);
                    return 0;
                }
                case WM_ERASEBKGND:
                    return 1;
                case WM_CLOSE:
                    DestroyWindow(hwnd);
                    return 0;
                case WM_DESTROY:
                    PostQuitMessage(0);
                    return 0;
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
};

//=============================================================================
// 对外接口类（所有功能均通过此类静态方法调用）
//=============================================================================

class EasyUI {
public:
    // 窗口生命周期
    static bool InitWindow(int width = 800, int height = 600, const wchar_t* title = L"easy_UI") {
        return window_.Create(width, height, title);
    }

    static void ShowWindow(int nCmdShow = SW_SHOW) {
        window_.Show(nCmdShow);
    }

    static int Run() {
        return window_.Run();
    }

    static HWND GetHwnd() {
        return window_.GetHwnd();
    }

    // 全局设置
    static void SetAntialiasing(bool enable) {
        UIManager::instance().SetAntialiasing(enable);
        InvalidateWindow();
    }

    static void SetBorderless(bool enable) {
        UIManager::instance().SetBorderless(enable);
    }

    static void SetFont(const std::wstring& fontName, int fontSize) {
        UIManager::instance().SetFont(fontName, fontSize);
        InvalidateWindow();
    }

    static void SetDefaultFont() {
        UIManager::instance().SetDefaultFont();
        InvalidateWindow();
    }

    // UI 元素操作
    static ElementPtr Create(int x, int y, int width, int height, int radius,
                             const std::wstring& text, const std::string& name,
                             Color color, ElementType type) {
        auto e = UIManager::instance().create(x, y, width, height, radius, text, name, color, type);
        InvalidateWindow();
        return e;
    }

    static void SetCallback(const std::string& name, ButtonCallback cb) {
        UIManager::instance().set_callback(name, std::move(cb));
    }

    static void Destroy(const std::string& name) {
        UIManager::instance().destroy(name);
        InvalidateWindow();
    }

    static void DestroyAll() {
        UIManager::instance().destroy_all();
        InvalidateWindow();
    }

    static void Show(const std::string& name) {
        UIManager::instance().show(name);
        InvalidateWindow();
    }

    static void ShowAll() {
        UIManager::instance().show_all();
        InvalidateWindow();
    }

    static void Hide(const std::string& name) {
        UIManager::instance().hide(name);
        InvalidateWindow();
    }

    static void HideAll() {
        UIManager::instance().hide_all();
        InvalidateWindow();
    }

    static void Renovate(const std::string& name) {
        UIManager::instance().renovate(name);
        InvalidateWindow();
    }

    static void RenovateAll() {
        UIManager::instance().renovate_all();
        InvalidateWindow();
    }

    static void LayerTop(const std::string& name) {
        UIManager::instance().layer_top(name);
        InvalidateWindow();
    }

    static void LayerBottom(const std::string& name) {
        UIManager::instance().layer_bottom(name);
        InvalidateWindow();
    }

    static void LayerMoveUp(const std::string& name, int steps) {
        UIManager::instance().layer_move_up(name, steps);
        InvalidateWindow();
    }

    static ElementPtr FindByName(const std::string& name) {
        return UIManager::instance().find_by_name(name);
    }

private:
    static void InvalidateWindow() {
        InvalidateRect(window_.GetHwnd(), nullptr, FALSE);
    }

    static EasyUIWindow window_;
};

// 静态成员定义
EasyUIWindow EasyUI::window_;

} // namespace easy_ui

//=============================================================================
// 全局便捷对象（使用方式：easyUI.Create(...)）
//=============================================================================

// 注意：此对象仅用于简化调用，实际功能均委托给 easy_ui::EasyUI 静态方法。
struct EasyUIGlobal {
    // 窗口
    bool InitWindow(int width = 800, int height = 600, const wchar_t* title = L"easy_UI") {
        return easy_ui::EasyUI::InitWindow(width, height, title);
    }
    void ShowWindow(int nCmdShow = SW_SHOW) { easy_ui::EasyUI::ShowWindow(nCmdShow); }
    int Run() { return easy_ui::EasyUI::Run(); }
    HWND GetHwnd() { return easy_ui::EasyUI::GetHwnd(); }

    // 设置
    void SetAntialiasing(bool enable) { easy_ui::EasyUI::SetAntialiasing(enable); }
    void SetBorderless(bool enable) { easy_ui::EasyUI::SetBorderless(enable); }
    void SetFont(const std::wstring& fontName, int fontSize) { easy_ui::EasyUI::SetFont(fontName, fontSize); }
    void SetDefaultFont() { easy_ui::EasyUI::SetDefaultFont(); }

    // 元素操作
    easy_ui::ElementPtr Create(int x, int y, int width, int height, int radius,
                               const std::wstring& text, const std::string& name,
                               easy_ui::Color color, easy_ui::ElementType type) {
        return easy_ui::EasyUI::Create(x, y, width, height, radius, text, name, color, type);
    }
    void SetCallback(const std::string& name, easy_ui::ButtonCallback cb) {
        easy_ui::EasyUI::SetCallback(name, std::move(cb));
    }
    void Destroy(const std::string& name) { easy_ui::EasyUI::Destroy(name); }
    void DestroyAll() { easy_ui::EasyUI::DestroyAll(); }
    void Show(const std::string& name) { easy_ui::EasyUI::Show(name); }
    void ShowAll() { easy_ui::EasyUI::ShowAll(); }
    void Hide(const std::string& name) { easy_ui::EasyUI::Hide(name); }
    void HideAll() { easy_ui::EasyUI::HideAll(); }
    void Renovate(const std::string& name) { easy_ui::EasyUI::Renovate(name); }
    void RenovateAll() { easy_ui::EasyUI::RenovateAll(); }
    void LayerTop(const std::string& name) { easy_ui::EasyUI::LayerTop(name); }
    void LayerBottom(const std::string& name) { easy_ui::EasyUI::LayerBottom(name); }
    void LayerMoveUp(const std::string& name, int steps) { easy_ui::EasyUI::LayerMoveUp(name, steps); }
    easy_ui::ElementPtr FindByName(const std::string& name) { return easy_ui::EasyUI::FindByName(name); }
};

// 全局对象（推荐使用方式）
static EasyUIGlobal easyUI;

#endif // EASY_UI_HPP