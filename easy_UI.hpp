/**
 * @file easy_UI.hpp
 * @brief 轻量化 Windows 原生 UI 渲染库（单头文件实现）
 * 
 * 版本：2.3 (修复首次绘制空白及悬停重绘问题)
 * 
 * 编译要求：Windows + C++11 或更高版本
 * 编译命令：g++ -std=c++11 main.cpp -lgdi32 -luser32 -lmsimg32 -mwindows
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
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <queue>
#include <mutex>
#include <chrono>

#pragma comment(lib, "msimg32.lib")

namespace easy_ui {

//=============================================================================
// 自定义消息常量（窗口过程使用）
//=============================================================================
constexpr UINT WM_USER_TASK = WM_USER + 100;   // 跨线程任务投递消息

//=============================================================================
// 基础类型定义
//=============================================================================

using Color = uint32_t;                     // 0xAARRGGBB

enum class ElementType {
    Window, Rect, Button, Label, Image, Custom,
    HBox, VBox                                // 布局容器
};

enum class EventType {
    Click, HoverEnter, HoverLeave
};

class UIElement;
using ElementPtr = std::shared_ptr<UIElement>;
using Callback = std::function<void()>;
using CustomDrawCallback = std::function<void(HDC, const UIElement*)>;

//=============================================================================
// 动画状态
//=============================================================================

struct AnimationState {
    bool active = false;
    bool fading_in = false;
    std::chrono::steady_clock::time_point start_time;
    int duration_ms = 0;
    float start_alpha = 1.0f;
    float target_alpha = 1.0f;
};

//=============================================================================
// UI 元素类（扩展）
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
    int  tab_index = -1;

    bool is_hovered  = false;
    bool is_pressed  = false;
    bool is_focused  = false;

    std::unordered_map<EventType, Callback> callbacks;

    std::vector<std::string> children;
    int spacing = 5;
    int padding = 5;
    std::string parent_name;                 // 父容器名称

    HBITMAP hBitmap = nullptr;
    std::wstring image_path;

    AnimationState anim;
    CustomDrawCallback custom_draw;

    UIElement() = default;
    UIElement(int x_, int y_, int w_, int h_, int r_,
              const std::wstring& txt, const std::string& nm,
              Color c, ElementType t)
        : x(x_), y(y_), width(w_), height(h_), radius(r_),
          text(txt), name(nm), color(c), type(t) {}

    ~UIElement() {
        if (hBitmap) DeleteObject(hBitmap);
    }

    void mark_dirty() { dirty = true; }
    void clear_dirty() { dirty = false; }

    Color get_draw_color() const {
        Color result = color;
        if (type == ElementType::Button) {
            BYTE a = (result >> 24) & 0xFF;
            BYTE r = (result >> 16) & 0xFF;
            BYTE g = (result >> 8)  & 0xFF;
            BYTE b = result & 0xFF;
            if (is_pressed) {
                r = r * 0.8;
                g = g * 0.8;
                b = b * 0.8;
            } else if (is_hovered) {
                r = std::min(255, (int)(r * 1.2));
                g = std::min(255, (int)(g * 1.2));
                b = std::min(255, (int)(b * 1.2));
            }
            result = (a << 24) | (r << 16) | (g << 8) | b;
        }
        if (anim.active) {
            auto now = std::chrono::steady_clock::now();
            float t = std::chrono::duration<float>(now - anim.start_time).count() * 1000.0f / anim.duration_ms;
            if (t >= 1.0f) {
                t = 1.0f;
            }
            float alpha = anim.start_alpha + (anim.target_alpha - anim.start_alpha) * t;
            BYTE a = ((result >> 24) & 0xFF) * alpha;
            BYTE r = (result >> 16) & 0xFF;
            BYTE g = (result >> 8)  & 0xFF;
            BYTE b = result & 0xFF;
            result = (a << 24) | (r << 16) | (g << 8) | b;
        }
        return result;
    }

    bool contains_point(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
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

    void SetDebugMode(bool enable) {
        debug_mode_ = enable;
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
        if (type == ElementType::Button) {
            elem->tab_index = next_tab_index_++;
        }
        return elem;
    }

    ElementPtr create_image(int x, int y, int width, int height,
                            const std::wstring& image_path, const std::string& name) {
        auto elem = create(x, y, width, height, 0, L"", name, 0xFFFFFFFF, ElementType::Image);
        if (elem) {
            elem->image_path = image_path;
            elem->hBitmap = (HBITMAP)LoadImageW(nullptr, image_path.c_str(), IMAGE_BITMAP,
                                                width, height, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        }
        return elem;
    }

    void destroy(const std::string& name) {
        auto it = elements_by_name_.find(name);
        if (it == elements_by_name_.end()) return;
        auto elem = it->second;
        if (!elem->parent_name.empty()) {
            auto parent = find_by_name(elem->parent_name);
            if (parent) {
                auto& vec = parent->children;
                vec.erase(std::remove(vec.begin(), vec.end(), name), vec.end());
            }
        }
        layer_list_.remove(elem);
        elements_by_name_.erase(it);
    }

    void destroy_all() {
        elements_by_name_.clear();
        layer_list_.clear();
        next_z_order_ = 0;
        next_tab_index_ = 0;
        focused_element_.clear();
    }

    //----------------------------- 属性修改接口 -----------------------------//
    void set_text(const std::string& name, const std::wstring& text) {
        auto e = find_by_name(name);
        if (e) { e->text = text; e->mark_dirty(); InvalidateWindow(); }
    }

    void set_color(const std::string& name, Color color) {
        auto e = find_by_name(name);
        if (e) { e->color = color; e->mark_dirty(); InvalidateWindow(); }
    }

    void set_position(const std::string& name, int x, int y) {
        auto e = find_by_name(name);
        if (e) { e->x = x; e->y = y; e->mark_dirty(); InvalidateWindow(); }
    }

    void set_size(const std::string& name, int w, int h) {
        auto e = find_by_name(name);
        if (e) { e->width = w; e->height = h; e->mark_dirty(); InvalidateWindow(); }
    }

    //----------------------------- 回调设置 -----------------------------//
    void set_callback(const std::string& name, EventType event, Callback cb) {
        auto e = find_by_name(name);
        if (e) e->callbacks[event] = std::move(cb);
    }

    void set_custom_draw(const std::string& name, CustomDrawCallback cb) {
        auto e = find_by_name(name);
        if (e) { e->custom_draw = std::move(cb); e->mark_dirty(); InvalidateWindow(); }
    }

    //----------------------------- 显示控制（带动画） -----------------------------//
    void show(const std::string& name, int duration_ms = 0) {
        auto e = find_by_name(name);
        if (!e) return;
        if (!e->visible) {
            e->visible = true;
            if (duration_ms > 0) {
                start_animation(e, true, duration_ms);
            } else {
                e->mark_dirty();
            }
            InvalidateWindow();
        }
    }

    void hide(const std::string& name, int duration_ms = 0) {
        auto e = find_by_name(name);
        if (!e || !e->visible) return;
        if (duration_ms > 0) {
            start_animation(e, false, duration_ms);
        } else {
            e->visible = false;
            e->mark_dirty();
        }
        InvalidateWindow();
    }

    void show_all() {
        for (auto& e : layer_list_) show(e->name);
    }

    void hide_all() {
        for (auto& e : layer_list_) hide(e->name);
    }

    //----------------------------- 刷新/重绘 -----------------------------//
    void renovate(const std::string& name) {
        if (name.empty()) { renovate_all(); return; }
        auto e = find_by_name(name);
        if (e) { e->mark_dirty(); InvalidateWindow(); }
    }

    void renovate_all() {
        for (auto& e : layer_list_) e->mark_dirty();
        InvalidateWindow();
    }

    //----------------------------- 层级管理 -----------------------------//
    void layer_top(const std::string& name) {
        auto e = find_by_name(name);
        if (!e) return;
        layer_list_.remove(e);
        layer_list_.push_back(e);
        recalc_z_orders();
        e->mark_dirty(); InvalidateWindow();
    }

    void layer_bottom(const std::string& name) {
        auto e = find_by_name(name);
        if (!e) return;
        layer_list_.remove(e);
        layer_list_.push_front(e);
        recalc_z_orders();
        e->mark_dirty(); InvalidateWindow();
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
        e->mark_dirty(); InvalidateWindow();
    }

    //----------------------------- 布局容器 -----------------------------//
    void add_to_container(const std::string& container_name, const std::string& child_name) {
        auto cont = find_by_name(container_name);
        auto child = find_by_name(child_name);
        if (!cont || !child) return;
        if (cont->type != ElementType::HBox && cont->type != ElementType::VBox) return;
        child->parent_name = container_name;
        cont->children.push_back(child_name);
        layout_container(cont.get());
        InvalidateWindow();
    }

    void set_container_spacing(const std::string& name, int spacing) {
        auto e = find_by_name(name);
        if (e) { e->spacing = spacing; layout_container(e.get()); InvalidateWindow(); }
    }

    //----------------------------- 焦点与键盘导航 -----------------------------//
    void focus_element(const std::string& name) {
        auto e = find_by_name(name);
        if (!e) return;
        for (auto& other : layer_list_) {
            if (other->is_focused) {
                other->is_focused = false;
                other->mark_dirty();
            }
        }
        e->is_focused = true;
        e->mark_dirty();
        focused_element_ = name;
        InvalidateWindow();
    }

    void focus_next() {
        std::vector<ElementPtr> focusable;
        for (auto& e : layer_list_) {
            if (e->visible && e->tab_index >= 0)
                focusable.push_back(e);
        }
        if (focusable.empty()) return;
        std::sort(focusable.begin(), focusable.end(),
                  [](const ElementPtr& a, const ElementPtr& b) { return a->tab_index < b->tab_index; });
        
        auto it = std::find_if(focusable.begin(), focusable.end(),
                               [this](const ElementPtr& e) { return e->name == focused_element_; });
        if (it == focusable.end() || ++it == focusable.end())
            it = focusable.begin();
        focus_element((*it)->name);
    }

    void trigger_focused_click() {
        auto e = find_by_name(focused_element_);
        if (e && e->visible && e->type == ElementType::Button) {
            auto cb_it = e->callbacks.find(EventType::Click);
            if (cb_it != e->callbacks.end()) cb_it->second();
        }
    }

    //----------------------------- 鼠标事件处理 -----------------------------//
    void OnMouseMove(int x, int y) {
        bool need_redraw = false;
        ElementPtr top_hit = nullptr;
        for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
            auto& e = *it;
            if (!e->visible) continue;
            if (e->contains_point(x, y)) {
                top_hit = e;
                break;
            }
        }
        for (auto& e : layer_list_) {
            bool should_hover = (e == top_hit);
            if (e->is_hovered != should_hover) {
                e->is_hovered = should_hover;
                e->mark_dirty();
                need_redraw = true;
                EventType evt = should_hover ? EventType::HoverEnter : EventType::HoverLeave;
                auto cb = e->callbacks.find(evt);
                if (cb != e->callbacks.end()) cb->second();
            }
        }
        if (need_redraw) InvalidateWindow();
    }

    void OnMouseDown(int x, int y) {
        for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
            auto& e = *it;
            if (!e->visible) continue;
            if (e->contains_point(x, y)) {
                if (e->type == ElementType::Button) {
                    e->is_pressed = true;
                    e->mark_dirty();
                    InvalidateWindow();
                    SetCapture(active_hwnd_);
                }
                break;
            }
        }
    }

    void OnMouseUp(int x, int y) {
        ReleaseCapture();
        bool need_redraw = false;
        for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
            auto& e = *it;
            if (!e->visible) continue;
            if (e->type == ElementType::Button && e->is_pressed) {
                e->is_pressed = false;
                e->mark_dirty();
                need_redraw = true;
                if (e->contains_point(x, y)) {
                    auto cb = e->callbacks.find(EventType::Click);
                    if (cb != e->callbacks.end()) cb->second();
                }
                break;
            }
        }
        if (need_redraw) InvalidateWindow();
    }

    //----------------------------- 键盘事件 -----------------------------//
    void OnKeyDown(WPARAM wParam) {
        switch (wParam) {
            case VK_TAB:
                focus_next();
                break;
            case VK_RETURN:
            case VK_SPACE:
                trigger_focused_click();
                break;
        }
    }

    //----------------------------- 动画更新 -----------------------------//
    void update_animations() {
        bool any_active = false;
        auto now = std::chrono::steady_clock::now();
        for (auto& e : layer_list_) {
            if (e->anim.active) {
                any_active = true;
                e->mark_dirty();
                float t = std::chrono::duration<float>(now - e->anim.start_time).count() * 1000.0f / e->anim.duration_ms;
                if (t >= 1.0f) {
                    e->anim.active = false;
                    if (!e->anim.fading_in) e->visible = false;
                }
            }
        }
        if (any_active) InvalidateWindow();
    }

    //----------------------------- 渲染接口（修复版：无条件绘制所有可见元素）-----------------------------//
    void render(HDC hdc) {
        update_animations();

        if (antialiasing_enabled_) {
            SetTextCharacterExtra(hdc, 0);
            SetBkMode(hdc, TRANSPARENT);
        }

        // 始终绘制所有可见的非容器元素（不再依赖 dirty 标志，避免画面残缺）
        for (auto& e : layer_list_) {
            if (!e->visible) continue;
            if (e->type == ElementType::HBox || e->type == ElementType::VBox) continue;
            draw_element(hdc, e.get());
            e->clear_dirty();
        }

        if (debug_mode_) {
            for (auto& e : layer_list_) {
                if (!e->visible) continue;
                draw_debug_overlay(hdc, e.get());
            }
        }
    }

    //----------------------------- 辅助函数 -----------------------------//
    ElementPtr find_by_name(const std::string& name) const {
        auto it = elements_by_name_.find(name);
        return (it != elements_by_name_.end()) ? it->second : nullptr;
    }

    void set_active_hwnd(HWND hwnd) { active_hwnd_ = hwnd; }
    HWND get_active_hwnd() const { return active_hwnd_; }

    void post_task(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(task_mutex_);
        task_queue_.push(std::move(task));
        if (active_hwnd_) PostMessage(active_hwnd_, WM_USER_TASK, 0, 0);
    }

    void process_tasks() {
        std::function<void()> task;
        while (true) {
            {
                std::lock_guard<std::mutex> lock(task_mutex_);
                if (task_queue_.empty()) break;
                task = std::move(task_queue_.front());
                task_queue_.pop();
            }
            if (task) task();
        }
    }

private:
    UIManager() = default;

    void recalc_z_orders() {
        int z = 0;
        for (auto& e : layer_list_) e->z_order = z++;
        next_z_order_ = z;
    }

    void start_animation(ElementPtr e, bool fade_in, int duration_ms) {
        e->anim.active = true;
        e->anim.fading_in = fade_in;
        e->anim.start_time = std::chrono::steady_clock::now();
        e->anim.duration_ms = duration_ms;
        e->anim.start_alpha = fade_in ? 0.0f : 1.0f;
        e->anim.target_alpha = fade_in ? 1.0f : 0.0f;
        e->visible = true;
    }

    void layout_container(UIElement* cont) {
        if (cont->type != ElementType::HBox && cont->type != ElementType::VBox) return;
        int offset = cont->padding;
        for (const auto& child_name : cont->children) {
            auto child = find_by_name(child_name);
            if (!child) continue;
            if (cont->type == ElementType::HBox) {
                child->x = cont->x + offset;
                child->y = cont->y + cont->padding;
                offset += child->width + cont->spacing;
            } else {
                child->x = cont->x + cont->padding;
                child->y = cont->y + offset;
                offset += child->height + cont->spacing;
            }
            child->mark_dirty();
        }
    }

    void draw_element(HDC hdc, const UIElement* e) {
        if (e->custom_draw) {
            e->custom_draw(hdc, e);
            return;
        }

        Color draw_color = e->get_draw_color();
        BYTE r = (draw_color >> 16) & 0xFF;
        BYTE g = (draw_color >> 8)  & 0xFF;
        BYTE b = draw_color & 0xFF;

        if (e->type == ElementType::Image && e->hBitmap) {
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, e->hBitmap);
            BLENDFUNCTION blend = { AC_SRC_OVER, 0, (BYTE)((draw_color >> 24) & 0xFF), AC_SRC_ALPHA };
            AlphaBlend(hdc, e->x, e->y, e->width, e->height,
                       memDC, 0, 0, e->width, e->height, blend);
            SelectObject(memDC, oldBmp);
            DeleteDC(memDC);
        } else {
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
        }

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

        if (e->is_focused) {
            HPEN hPen = CreatePen(PS_DOT, 1, RGB(0, 0, 0));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, e->x + 2, e->y + 2, e->x + e->width - 2, e->y + e->height - 2);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);
        }
    }

    void draw_debug_overlay(HDC hdc, const UIElement* e) {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, e->x, e->y, e->x + e->width, e->y + e->height);
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);

        std::wstring info = e->text.empty() ? std::wstring(e->name.begin(), e->name.end()) : e->text;
        info += L" (" + std::to_wstring(e->x) + L"," + std::to_wstring(e->y) + L")";
        SetTextColor(hdc, RGB(255, 0, 0));
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, e->x, e->y - 16, info.c_str(), info.length());
    }

    void InvalidateWindow() {
        if (active_hwnd_) InvalidateRect(active_hwnd_, nullptr, FALSE);
    }

    std::unordered_map<std::string, ElementPtr> elements_by_name_;
    std::list<ElementPtr> layer_list_;
    int next_z_order_ = 0;
    int next_tab_index_ = 0;
    bool antialiasing_enabled_ = true;
    bool borderless_ = false;
    bool debug_mode_ = false;
    HWND active_hwnd_ = nullptr;

    bool use_custom_font_ = false;
    std::wstring custom_font_name_ = L"Segoe UI";
    int custom_font_size_ = 16;

    std::string focused_element_;

    std::queue<std::function<void()>> task_queue_;
    std::mutex task_mutex_;
};

//=============================================================================
// 窗口管理
//=============================================================================

class EasyUIWindow {
public:
    EasyUIWindow() = default;

    bool Create(int width = 800, int height = 600, const wchar_t* title = L"easy_UI Window") {
        HINSTANCE hInst = GetModuleHandle(nullptr);
        auto& ui = UIManager::instance();

        SetProcessDPIAware();

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
            SetTimer(hwnd_, 1, 16, nullptr);
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
                case WM_TIMER:
                    ui.update_animations();
                    return 0;
                case WM_USER_TASK:
                    ui.process_tasks();
                    return 0;
                case WM_MOUSEMOVE:
                    ui.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    break;
                case WM_LBUTTONDOWN:
                    ui.OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    break;
                case WM_LBUTTONUP:
                    ui.OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                    break;
                case WM_KEYDOWN:
                    ui.OnKeyDown(wParam);
                    break;
                case WM_PAINT: {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hwnd, &ps);
                    // 修复：首次绘制时强制刷新所有元素，确保窗口一出现就显示内容
                    static bool s_first_paint = true;
                    if (s_first_paint) {
                        ui.renovate_all();
                        s_first_paint = false;
                    }
                    ui.render(hdc);
                    EndPaint(hwnd, &ps);
                    return 0;
                }
                case WM_ERASEBKGND:
                    // 修复：让系统正常填充背景，避免首次显示空白
                    return DefWindowProc(hwnd, msg, wParam, lParam);
                case WM_CLOSE:
                    DestroyWindow(hwnd);
                    return 0;
                case WM_DESTROY:
                    KillTimer(hwnd, 1);
                    PostQuitMessage(0);
                    return 0;
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
};

//=============================================================================
// 对外接口类
//=============================================================================

class EasyUI {
public:
    static bool InitWindow(int width = 800, int height = 600, const wchar_t* title = L"easy_UI") {
        return window_.Create(width, height, title);
    }

    static void ShowWindow(int nCmdShow = SW_SHOW) { window_.Show(nCmdShow); }
    static int Run() { return window_.Run(); }
    static HWND GetHwnd() { return window_.GetHwnd(); }

    static void SetAntialiasing(bool enable) { UIManager::instance().SetAntialiasing(enable); }
    static void SetBorderless(bool enable) { UIManager::instance().SetBorderless(enable); }
    static void SetFont(const std::wstring& fontName, int fontSize) { UIManager::instance().SetFont(fontName, fontSize); }
    static void SetDefaultFont() { UIManager::instance().SetDefaultFont(); }
    static void SetDebugMode(bool enable) { UIManager::instance().SetDebugMode(enable); }

    static ElementPtr Create(int x, int y, int w, int h, int r, const std::wstring& t, const std::string& n, Color c, ElementType tp) {
        return UIManager::instance().create(x, y, w, h, r, t, n, c, tp);
    }

    static ElementPtr CreateImage(int x, int y, int w, int h, const std::wstring& p, const std::string& n) {
        return UIManager::instance().create_image(x, y, w, h, p, n);
    }

    static void SetText(const std::string& n, const std::wstring& t) { UIManager::instance().set_text(n, t); }
    static void SetColor(const std::string& n, Color c) { UIManager::instance().set_color(n, c); }
    static void SetPosition(const std::string& n, int x, int y) { UIManager::instance().set_position(n, x, y); }
    static void SetSize(const std::string& n, int w, int h) { UIManager::instance().set_size(n, w, h); }

    static void SetCallback(const std::string& n, EventType e, Callback c) { UIManager::instance().set_callback(n, e, c); }
    static void SetCustomDraw(const std::string& n, CustomDrawCallback c) { UIManager::instance().set_custom_draw(n, c); }

    static void Show(const std::string& n, int d = 0) { UIManager::instance().show(n, d); }
    static void Hide(const std::string& n, int d = 0) { UIManager::instance().hide(n, d); }
    static void ShowAll() { UIManager::instance().show_all(); }
    static void HideAll() { UIManager::instance().hide_all(); }

    static void Renovate(const std::string& n) { UIManager::instance().renovate(n); }
    static void RenovateAll() { UIManager::instance().renovate_all(); }

    static void LayerTop(const std::string& n) { UIManager::instance().layer_top(n); }
    static void LayerBottom(const std::string& n) { UIManager::instance().layer_bottom(n); }
    static void LayerMoveUp(const std::string& n, int s) { UIManager::instance().layer_move_up(n, s); }

    static void AddToContainer(const std::string& c, const std::string& ch) { UIManager::instance().add_to_container(c, ch); }
    static void SetContainerSpacing(const std::string& n, int s) { UIManager::instance().set_container_spacing(n, s); }

    static void FocusElement(const std::string& n) { UIManager::instance().focus_element(n); }
    static void Post(std::function<void()> t) { UIManager::instance().post_task(t); }

    static ElementPtr FindByName(const std::string& n) { return UIManager::instance().find_by_name(n); }
    static void Destroy(const std::string& n) { UIManager::instance().destroy(n); }
    static void DestroyAll() { UIManager::instance().destroy_all(); }

private:
    static EasyUIWindow window_;
};

EasyUIWindow EasyUI::window_;

} // namespace easy_ui

//=============================================================================
// 全局便捷对象
//=============================================================================

struct EasyUIGlobal {
    bool InitWindow(int w=800, int h=600, const wchar_t* t=L"easy_UI") { return easy_ui::EasyUI::InitWindow(w,h,t); }
    void ShowWindow(int c=SW_SHOW) { easy_ui::EasyUI::ShowWindow(c); }
    int Run() { return easy_ui::EasyUI::Run(); }
    HWND GetHwnd() { return easy_ui::EasyUI::GetHwnd(); }

    void SetAntialiasing(bool e) { easy_ui::EasyUI::SetAntialiasing(e); }
    void SetBorderless(bool e) { easy_ui::EasyUI::SetBorderless(e); }
    void SetFont(const std::wstring& n, int s) { easy_ui::EasyUI::SetFont(n,s); }
    void SetDefaultFont() { easy_ui::EasyUI::SetDefaultFont(); }
    void SetDebugMode(bool e) { easy_ui::EasyUI::SetDebugMode(e); }

    easy_ui::ElementPtr Create(int x, int y, int w, int h, int r, const std::wstring& t, const std::string& n, easy_ui::Color c, easy_ui::ElementType tp) {
        return easy_ui::EasyUI::Create(x,y,w,h,r,t,n,c,tp);
    }
    easy_ui::ElementPtr CreateImage(int x,int y,int w,int h,const std::wstring& p,const std::string& n) {
        return easy_ui::EasyUI::CreateImage(x,y,w,h,p,n);
    }
    void SetText(const std::string& n, const std::wstring& t) { easy_ui::EasyUI::SetText(n,t); }
    void SetColor(const std::string& n, easy_ui::Color c) { easy_ui::EasyUI::SetColor(n,c); }
    void SetPosition(const std::string& n, int x, int y) { easy_ui::EasyUI::SetPosition(n,x,y); }
    void SetSize(const std::string& n, int w, int h) { easy_ui::EasyUI::SetSize(n,w,h); }

    void SetCallback(const std::string& n, easy_ui::EventType e, easy_ui::Callback c) { easy_ui::EasyUI::SetCallback(n,e,c); }
    void SetCustomDraw(const std::string& n, easy_ui::CustomDrawCallback c) { easy_ui::EasyUI::SetCustomDraw(n,c); }

    void Show(const std::string& n, int d=0) { easy_ui::EasyUI::Show(n,d); }
    void Hide(const std::string& n, int d=0) { easy_ui::EasyUI::Hide(n,d); }
    void ShowAll() { easy_ui::EasyUI::ShowAll(); }
    void HideAll() { easy_ui::EasyUI::HideAll(); }

    void Renovate(const std::string& n) { easy_ui::EasyUI::Renovate(n); }
    void RenovateAll() { easy_ui::EasyUI::RenovateAll(); }

    void LayerTop(const std::string& n) { easy_ui::EasyUI::LayerTop(n); }
    void LayerBottom(const std::string& n) { easy_ui::EasyUI::LayerBottom(n); }
    void LayerMoveUp(const std::string& n, int s) { easy_ui::EasyUI::LayerMoveUp(n,s); }

    void AddToContainer(const std::string& c, const std::string& ch) { easy_ui::EasyUI::AddToContainer(c,ch); }
    void SetContainerSpacing(const std::string& n, int s) { easy_ui::EasyUI::SetContainerSpacing(n,s); }

    void FocusElement(const std::string& n) { easy_ui::EasyUI::FocusElement(n); }
    void Post(std::function<void()> t) { easy_ui::EasyUI::Post(t); }

    easy_ui::ElementPtr FindByName(const std::string& n) { return easy_ui::EasyUI::FindByName(n); }
    void Destroy(const std::string& n) { easy_ui::EasyUI::Destroy(n); }
    void DestroyAll() { easy_ui::EasyUI::DestroyAll(); }
};

static EasyUIGlobal easyUI;

#endif // EASY_UI_HPP