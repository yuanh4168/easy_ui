/**
 * @file easy_UI.hpp
 * @brief 轻量化 Windows 原生 UI 渲染库（单头文件实现）
 * 
 * 版本：3.3 (修复文字可见性、动画系统、缩放后位置修正)
 * 
 * 编译要求：Windows + C++11 或更高版本
 * 编译命令：g++ -std=c++11 main.cpp -lgdi32 -luser32 -lmsimg32 -lgdiplus -mwindows
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
#include <cmath>

// GDI+ 支持
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

namespace easy_ui {

//=============================================================================
// 自定义消息常量
//=============================================================================
constexpr UINT WM_USER_TASK         = WM_USER + 100;
constexpr UINT WM_USER_TOAST_TIMER  = WM_USER + 101;
constexpr UINT WM_USER_CURSOR_BLINK = WM_USER + 102;

//=============================================================================
// GDI+ 初始化管理
//=============================================================================
class GdiPlusManager {
public:
    static void Init() { static GdiPlusManager instance; }
private:
    GdiPlusManager() { Gdiplus::GdiplusStartupInput input; Gdiplus::GdiplusStartup(&token_, &input, nullptr); }
    ~GdiPlusManager() { Gdiplus::GdiplusShutdown(token_); }
    ULONG_PTR token_;
};

//=============================================================================
// 基础类型与枚举
//=============================================================================

using Color = uint32_t;                     // 0xAARRGGBB

enum class ElementType {
    Window, Rect, Button, Label, Image, Custom,
    HBox, VBox,
    TextBox,
    ComboBox,
    Dialog,
    Toast
};

enum class EventType {
    Click, HoverEnter, HoverLeave,
    TextChanged,
    SelectionChanged
};

enum class AnimationType {
    Fade,
    Slide,
    Scale
};

enum class EasingType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

class UIElement;
using ElementPtr = std::shared_ptr<UIElement>;
using Callback = std::function<void()>;
using TextChangedCallback = std::function<void(const std::wstring&)>;
using SelectionCallback = std::function<void(int, const std::wstring&)>;
using CustomDrawCallback = std::function<void(HDC, const UIElement*)>;

//=============================================================================
// 主题系统
//=============================================================================
struct Theme {
    std::wstring name;
    Color bg_color;
    Color text_color;
    Color button_color;
    Color button_hover;
    Color button_press;
    Color input_bg;
    Color input_border;
    Color combo_bg;
    Color combo_item_hover;
    Color dialog_bg;
    Color toast_bg;
    Color shadow;

    static Theme Light() {
        return {
            L"Light",
            0xFFF0F0F0, 0xFF333333, 0xFFE0E0E0, 0xFFD0D0D0, 0xFFC0C0C0,
            0xFFFFFFFF, 0xFFB0B0B0, 0xFFFFFFFF, 0xFFE0E0E0,
            0xFFFAFAFA, 0xFF333333, 0x80000000
        };
    }
    static Theme Dark() {
        return {
            L"Dark",
            0xFF2D2D30, 0xFFFFFFFF, 0xFF3E3E42, 0xFF4E4E52, 0xFF5E5E62,
            0xFF1E1E1E, 0xFF555555, 0xFF2D2D30, 0xFF3E3E42,
            0xFF2B2B2B, 0xFFEEEEEE, 0x80000000
        };
    }
};

//=============================================================================
// 动画状态
//=============================================================================
struct AnimationState {
    bool active = false;
    AnimationType type = AnimationType::Fade;
    EasingType easing = EasingType::Linear;
    std::chrono::steady_clock::time_point start_time;
    int duration_ms = 0;
    float start_value = 0.0f;
    float target_value = 0.0f;
};

//=============================================================================
// UI 元素类
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
    TextChangedCallback text_changed_cb;
    SelectionCallback selection_cb;

    std::vector<std::string> children;
    int spacing = 5;
    int padding = 5;
    std::string parent_name;

    // 图片
    HBITMAP hBitmap = nullptr;
    Gdiplus::Image* gdiImage = nullptr;
    std::wstring image_path;

    // 文本框
    std::wstring placeholder;
    bool is_password = false;
    int cursor_pos = 0;
    bool cursor_visible = true;

    // 下拉框
    std::vector<std::wstring> options;
    int selected_index = -1;
    bool expanded = false;
    int hovered_option = -1;

    // 动画
    AnimationState anim;
    // 动画临时数据（滑动/缩放专用）
    int anim_original_x = 0, anim_original_y = 0;
    int anim_original_w = 0, anim_original_h = 0;
    int anim_offset_x = 0, anim_offset_y = 0;

    // 自定义绘制
    CustomDrawCallback custom_draw;

    // 原始尺寸（用于窗口缩放）
    int original_x = 0, original_y = 0, original_width = 0, original_height = 0, original_radius = 0;

    UIElement() = default;
    UIElement(int x_, int y_, int w_, int h_, int r_,
              const std::wstring& txt, const std::string& nm,
              Color c, ElementType t)
        : x(x_), y(y_), width(w_), height(h_), radius(r_),
          text(txt), name(nm), color(c), type(t),
          original_x(x_), original_y(y_), original_width(w_),
          original_height(h_), original_radius(r_) {}

    ~UIElement() {
        if (hBitmap) DeleteObject(hBitmap);
        if (gdiImage) delete gdiImage;
    }

    void mark_dirty() { dirty = true; }
    void clear_dirty() { dirty = false; }

    float get_alpha() const {
        if (anim.active && anim.type == AnimationType::Fade) {
            float t = get_anim_progress();
            return anim.start_value + (anim.target_value - anim.start_value) * t;
        }
        return 1.0f;
    }

    float get_anim_progress() const {
        if (!anim.active) return 1.0f;
        auto now = std::chrono::steady_clock::now();
        float t = std::chrono::duration<float>(now - anim.start_time).count() * 1000.0f / anim.duration_ms;
        if (t >= 1.0f) t = 1.0f;
        return apply_easing(t);
    }

    float apply_easing(float t) const {
        switch (anim.easing) {
            case EasingType::EaseIn:      return t * t;
            case EasingType::EaseOut:     return 1.0f - (1.0f - t) * (1.0f - t);
            case EasingType::EaseInOut:   return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
            default: return t;
        }
    }

    Color get_draw_color(const Theme& theme) const {
        Color base = color;
        if (type == ElementType::Button) {
            if (is_pressed) base = theme.button_press;
            else if (is_hovered) base = theme.button_hover;
            else base = theme.button_color;
        }
        else if (type == ElementType::TextBox) {
            base = theme.input_bg;
        }
        else if (type == ElementType::ComboBox) {
            base = theme.combo_bg;
        }
        else if (type == ElementType::Dialog) {
            base = theme.dialog_bg;
        }
        else if (type == ElementType::Toast) {
            base = theme.toast_bg;
        }
        float alpha = get_alpha();
        BYTE a = (BYTE)(((base >> 24) & 0xFF) * alpha);
        BYTE r = (base >> 16) & 0xFF;
        BYTE g = (base >> 8)  & 0xFF;
        BYTE b = base & 0xFF;
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    bool contains_point(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }

    void apply_scale(float scale_x, float scale_y) {
        x = static_cast<int>(original_x * scale_x);
        y = static_cast<int>(original_y * scale_y);
        width = static_cast<int>(original_width * scale_x);
        height = static_cast<int>(original_height * scale_y);
        radius = static_cast<int>(original_radius * ((scale_x + scale_y) * 0.5f));
        mark_dirty();
    }
};

//=============================================================================
// UI 管理器
//=============================================================================
class UIManager {
public:
    static UIManager& instance() {
        static UIManager mgr;
        return mgr;
    }

    void SetTheme(const Theme& theme) {
        current_theme_ = theme;
        renovate_all();
    }
    const Theme& GetTheme() const { return current_theme_; }

    void SetAntialiasing(bool enable) { antialiasing_enabled_ = enable; renovate_all(); }
    void SetBorderless(bool enable) { borderless_ = enable; }
    bool IsBorderless() const { return borderless_; }
    void SetFont(const std::wstring& fontName, int fontSize) {
        custom_font_name_ = fontName;
        custom_font_size_ = fontSize;
        use_custom_font_ = true;
        if (base_font_size_ == 0) base_font_size_ = fontSize;
        renovate_all();
    }
    void SetDefaultFont() { use_custom_font_ = false; renovate_all(); }
    void SetDebugMode(bool enable) { debug_mode_ = enable; renovate_all(); }

    void SetBaseWindowSize(int width, int height) {
        base_window_width_ = width;
        base_window_height_ = height;
        if (!use_custom_font_) {
            use_custom_font_ = true;
            custom_font_name_ = L"Segoe UI";
            custom_font_size_ = 16;
        }
        base_font_size_ = custom_font_size_;
    }

    ElementPtr create(int x, int y, int w, int h, int r, const std::wstring& t, const std::string& n, Color c, ElementType type) {
        if (n.empty()) return nullptr;
        if (elements_by_name_.count(n)) destroy(n);
        auto elem = std::make_shared<UIElement>(x, y, w, h, r, t, n, c, type);
        elem->z_order = next_z_order_++;
        elements_by_name_[n] = elem;
        layer_list_.push_back(elem);
        if (type == ElementType::Button || type == ElementType::TextBox) elem->tab_index = next_tab_index_++;
        return elem;
    }

    ElementPtr create_image(int x, int y, int w, int h, const std::wstring& path, const std::string& n) {
        GdiPlusManager::Init();
        auto elem = create(x, y, w, h, 0, L"", n, 0xFFFFFFFF, ElementType::Image);
        if (elem) {
            elem->image_path = path;
            elem->gdiImage = Gdiplus::Image::FromFile(path.c_str());
            if (!elem->gdiImage || elem->gdiImage->GetLastStatus() != Gdiplus::Ok) {
                delete elem->gdiImage;
                elem->gdiImage = nullptr;
                elem->hBitmap = (HBITMAP)LoadImageW(nullptr, path.c_str(), IMAGE_BITMAP, w, h, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
            }
        }
        return elem;
    }

    ElementPtr create_textbox(int x, int y, int w, int h, const std::wstring& placeholder, const std::string& name) {
        auto elem = create(x, y, w, h, 3, L"", name, 0xFFFFFFFF, ElementType::TextBox);
        if (elem) elem->placeholder = placeholder;
        return elem;
    }

    ElementPtr create_combobox(int x, int y, int w, int h, const std::vector<std::wstring>& opts, const std::string& name) {
        auto elem = create(x, y, w, h, 3, L"", name, 0xFFFFFFFF, ElementType::ComboBox);
        if (elem) {
            elem->options = opts;
            if (!opts.empty()) elem->selected_index = 0;
        }
        return elem;
    }

    void ShowDialog(const std::wstring& title, const std::wstring& message, Callback onOk = nullptr, Callback onCancel = nullptr) {
        dialog_title_ = title;
        dialog_message_ = message;
        dialog_on_ok_ = onOk;
        dialog_on_cancel_ = onCancel;
        show_dialog_ = true;
        InvalidateWindow();
    }

    void ShowToast(const std::wstring& message, int duration_ms = 2000) {
        toast_message_ = message;
        toast_visible_ = true;
        toast_start_ = std::chrono::steady_clock::now();
        toast_duration_ = duration_ms;
        SetTimer(active_hwnd_, 2, 100, nullptr);
        InvalidateWindow();
    }

    void AnimateSlide(const std::string& name, int offset_x, int offset_y, int duration_ms, EasingType easing = EasingType::EaseOut) {
        auto e = find_by_name(name); if (!e) return;
        e->anim.active = true; e->anim.type = AnimationType::Slide; e->anim.easing = easing;
        e->anim.start_time = std::chrono::steady_clock::now(); e->anim.duration_ms = duration_ms;
        e->anim.start_value = 0.0f; e->anim.target_value = 1.0f;
        e->anim_offset_x = offset_x; e->anim_offset_y = offset_y;
        e->anim_original_x = e->x; e->anim_original_y = e->y;
        InvalidateWindow();
    }

    void AnimateScale(const std::string& name, float scale, int duration_ms, EasingType easing = EasingType::EaseOut) {
        auto e = find_by_name(name); if (!e) return;
        e->anim.active = true; e->anim.type = AnimationType::Scale; e->anim.easing = easing;
        e->anim.start_time = std::chrono::steady_clock::now(); e->anim.duration_ms = duration_ms;
        e->anim.start_value = 1.0f; e->anim.target_value = scale;
        e->anim_original_w = e->width; e->anim_original_h = e->height;
        InvalidateWindow();
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

    void set_text(const std::string& n, const std::wstring& t) { auto e = find_by_name(n); if (e) { e->text = t; e->mark_dirty(); InvalidateWindow(); } }
    void set_color(const std::string& n, Color c) { auto e = find_by_name(n); if (e) { e->color = c; e->mark_dirty(); InvalidateWindow(); } }

    // 修正：设置位置时考虑当前缩放比例，将屏幕坐标转换为原始坐标保存
    void set_position(const std::string& n, int x, int y) {
        auto e = find_by_name(n); if (!e) return;
        e->x = x; e->y = y;
        // 获取当前缩放比例
        if (base_window_width_ > 0 && base_window_height_ > 0) {
            RECT rc; GetClientRect(active_hwnd_, &rc);
            float sx = (float)(rc.right - rc.left) / base_window_width_;
            float sy = (float)(rc.bottom - rc.top) / base_window_height_;
            e->original_x = static_cast<int>(x / sx);
            e->original_y = static_cast<int>(y / sy);
        } else {
            e->original_x = x; e->original_y = y;
        }
        e->mark_dirty(); InvalidateWindow();
    }

    void set_size(const std::string& n, int w, int h) {
        auto e = find_by_name(n); if (!e) return;
        e->width = w; e->height = h;
        if (base_window_width_ > 0 && base_window_height_ > 0) {
            RECT rc; GetClientRect(active_hwnd_, &rc);
            float sx = (float)(rc.right - rc.left) / base_window_width_;
            float sy = (float)(rc.bottom - rc.top) / base_window_height_;
            e->original_width = static_cast<int>(w / sx);
            e->original_height = static_cast<int>(h / sy);
        } else {
            e->original_width = w; e->original_height = h;
        }
        e->mark_dirty(); InvalidateWindow();
    }

    void set_callback(const std::string& n, EventType ev, Callback cb) { auto e = find_by_name(n); if (e) e->callbacks[ev] = std::move(cb); }
    void set_text_changed_callback(const std::string& n, TextChangedCallback cb) { auto e = find_by_name(n); if (e) e->text_changed_cb = std::move(cb); }
    void set_selection_callback(const std::string& n, SelectionCallback cb) { auto e = find_by_name(n); if (e) e->selection_cb = std::move(cb); }
    void set_custom_draw(const std::string& n, CustomDrawCallback cb) { auto e = find_by_name(n); if (e) { e->custom_draw = std::move(cb); e->mark_dirty(); InvalidateWindow(); } }

    void show(const std::string& n, int duration_ms = 0) {
        auto e = find_by_name(n); if (!e) return;
        if (!e->visible) {
            e->visible = true;
            if (duration_ms > 0) start_animation(e, AnimationType::Fade, true, duration_ms);
            else e->mark_dirty();
            InvalidateWindow();
        }
    }
    void hide(const std::string& n, int duration_ms = 0) {
        auto e = find_by_name(n); if (!e || !e->visible) return;
        if (duration_ms > 0) start_animation(e, AnimationType::Fade, false, duration_ms);
        else { e->visible = false; e->mark_dirty(); }
        InvalidateWindow();
    }
    void show_all() { for (auto& e : layer_list_) show(e->name); }
    void hide_all() { for (auto& e : layer_list_) hide(e->name); }

    void renovate(const std::string& n) { if (n.empty()) renovate_all(); else { auto e = find_by_name(n); if (e) { e->mark_dirty(); InvalidateWindow(); } } }
    void renovate_all() { for (auto& e : layer_list_) e->mark_dirty(); InvalidateWindow(); }

    void layer_top(const std::string& n) { auto e = find_by_name(n); if (!e) return; layer_list_.remove(e); layer_list_.push_back(e); recalc_z_orders(); e->mark_dirty(); InvalidateWindow(); }
    void layer_bottom(const std::string& n) { auto e = find_by_name(n); if (!e) return; layer_list_.remove(e); layer_list_.push_front(e); recalc_z_orders(); e->mark_dirty(); InvalidateWindow(); }
    void layer_move_up(const std::string& n, int steps) {
        if (steps == 0) return;
        auto e = find_by_name(n); if (!e) return;
        auto it = std::find(layer_list_.begin(), layer_list_.end(), e);
        if (it == layer_list_.end()) return;
        if (steps > 0) {
            auto next_it = it;
            for (int i = 0; i < steps && std::next(next_it) != layer_list_.end(); ++i) ++next_it;
            if (next_it != it) layer_list_.splice(std::next(next_it), layer_list_, it);
        } else {
            auto prev_it = it;
            for (int i = 0; i < -steps && prev_it != layer_list_.begin(); ++i) --prev_it;
            if (prev_it != it) layer_list_.splice(prev_it, layer_list_, it);
        }
        recalc_z_orders(); e->mark_dirty(); InvalidateWindow();
    }

    void add_to_container(const std::string& cname, const std::string& child) {
        auto cont = find_by_name(cname), ch = find_by_name(child);
        if (!cont || !ch) return;
        if (cont->type != ElementType::HBox && cont->type != ElementType::VBox) return;
        ch->parent_name = cname;
        cont->children.push_back(child);
        layout_container(cont.get());
        InvalidateWindow();
    }
    void set_container_spacing(const std::string& n, int s) { auto e = find_by_name(n); if (e) { e->spacing = s; layout_container(e.get()); InvalidateWindow(); } }

    void focus_element(const std::string& n) {
        auto e = find_by_name(n); if (!e) return;
        for (auto& other : layer_list_) if (other->is_focused) { other->is_focused = false; other->mark_dirty(); }
        e->is_focused = true; e->mark_dirty(); focused_element_ = n; InvalidateWindow();
    }
    void focus_next() {
        std::vector<ElementPtr> focusable;
        for (auto& e : layer_list_) if (e->visible && e->tab_index >= 0) focusable.push_back(e);
        if (focusable.empty()) return;
        std::sort(focusable.begin(), focusable.end(), [](const ElementPtr& a, const ElementPtr& b) { return a->tab_index < b->tab_index; });
        auto it = std::find_if(focusable.begin(), focusable.end(), [this](const ElementPtr& e) { return e->name == focused_element_; });
        if (it == focusable.end() || ++it == focusable.end()) it = focusable.begin();
        focus_element((*it)->name);
    }

    void OnMouseMove(int x, int y) {
        bool need = false; ElementPtr top = nullptr;
        for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
            auto& e = *it; if (!e->visible) continue;
            if (e->contains_point(x, y)) { top = e; break; }
        }
        for (auto& e : layer_list_) {
            bool hover = (e == top);
            if (e->is_hovered != hover) {
                e->is_hovered = hover; e->mark_dirty(); need = true;
                EventType ev = hover ? EventType::HoverEnter : EventType::HoverLeave;
                auto cb = e->callbacks.find(ev); if (cb != e->callbacks.end()) cb->second();
            }
        }
        if (need) InvalidateWindow();
    }

    void OnMouseDown(int x, int y) {
        for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
            auto& e = *it; if (!e->visible) continue;
            if (e->contains_point(x, y)) {
                if (e->type == ElementType::Button || e->type == ElementType::TextBox || e->type == ElementType::ComboBox) {
                    e->is_pressed = true; e->mark_dirty();
                    if (e->type == ElementType::TextBox) {
                        focus_element(e->name);
                        e->cursor_pos = (int)e->text.length();
                    }
                    else if (e->type == ElementType::ComboBox) {
                        e->expanded = !e->expanded;
                        focus_element(e->name);
                    }
                    InvalidateWindow();
                    SetCapture(active_hwnd_);
                }
                break;
            }
        }
    }

    void OnMouseUp(int x, int y) {
        ReleaseCapture(); bool need = false;
        for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
            auto& e = *it; if (!e->visible) continue;
            if (e->is_pressed) {
                e->is_pressed = false; e->mark_dirty(); need = true;
                if (e->contains_point(x, y)) {
                    if (e->type == ElementType::Button) {
                        auto cb = e->callbacks.find(EventType::Click); if (cb != e->callbacks.end()) cb->second();
                    }
                    else if (e->type == ElementType::ComboBox && e->expanded) {
                        int opt_h = 25, start_y = e->y + e->height;
                        for (size_t i = 0; i < e->options.size(); ++i) {
                            RECT opt_rect = { e->x, start_y + (int)i * opt_h, e->x + e->width, start_y + ((int)i+1) * opt_h };
                            if (x >= opt_rect.left && x < opt_rect.right && y >= opt_rect.top && y < opt_rect.bottom) {
                                e->selected_index = (int)i; e->text = e->options[i]; e->expanded = false;
                                if (e->selection_cb) e->selection_cb((int)i, e->text);
                                auto cb = e->callbacks.find(EventType::SelectionChanged); if (cb != e->callbacks.end()) cb->second();
                                break;
                            }
                        }
                        e->expanded = false;
                    }
                }
                break;
            }
        }
        if (need) InvalidateWindow();
    }

    void OnKeyDown(WPARAM wParam) {
        if (focused_element_.empty()) return;
        auto e = find_by_name(focused_element_);
        if (!e) return;

        if (e->type == ElementType::TextBox) {
            if (wParam == VK_BACK && !e->text.empty() && e->cursor_pos > 0) {
                e->text.erase(e->cursor_pos - 1, 1); e->cursor_pos--;
                if (e->text_changed_cb) e->text_changed_cb(e->text);
                e->mark_dirty(); InvalidateWindow();
            }
            else if (wParam == VK_RETURN) {
                auto cb = e->callbacks.find(EventType::Click); if (cb != e->callbacks.end()) cb->second();
            }
        }
        else if (e->type == ElementType::ComboBox && e->expanded) {
            if (wParam == VK_UP) { e->hovered_option = std::max(0, e->hovered_option - 1); e->mark_dirty(); InvalidateWindow(); }
            else if (wParam == VK_DOWN) { e->hovered_option = std::min((int)e->options.size()-1, e->hovered_option + 1); e->mark_dirty(); InvalidateWindow(); }
            else if (wParam == VK_RETURN && e->hovered_option >= 0) {
                e->selected_index = e->hovered_option; e->text = e->options[e->hovered_option]; e->expanded = false;
                if (e->selection_cb) e->selection_cb(e->selected_index, e->text);
                e->mark_dirty(); InvalidateWindow();
            }
            else if (wParam == VK_ESCAPE) { e->expanded = false; e->mark_dirty(); InvalidateWindow(); }
        }

        if (wParam == VK_TAB) focus_next();
    }

    void OnChar(WPARAM wParam) {
        if (focused_element_.empty()) return;
        auto e = find_by_name(focused_element_);
        if (e && e->type == ElementType::TextBox) {
            wchar_t ch = (wchar_t)wParam;
            if (ch >= 32) {
                e->text.insert(e->cursor_pos, 1, ch); e->cursor_pos++;
                if (e->text_changed_cb) e->text_changed_cb(e->text);
                e->mark_dirty(); InvalidateWindow();
            }
        }
    }

    void update_animations() {
        bool any = false; auto now = std::chrono::steady_clock::now();
        for (auto& e : layer_list_) {
            if (e->anim.active) {
                any = true; e->mark_dirty();
                float t = std::chrono::duration<float>(now - e->anim.start_time).count() * 1000.0f / e->anim.duration_ms;
                if (t >= 1.0f) {
                    e->anim.active = false;
                    if (e->anim.type == AnimationType::Fade && e->anim.target_value == 0.0f) e->visible = false;
                }
            }
        }
        if (any) InvalidateWindow();
    }

    void OnWindowResize(int new_w, int new_h) {
        if (base_window_width_ <= 0 || base_window_height_ <= 0) return;
        float sx = (float)new_w / base_window_width_, sy = (float)new_h / base_window_height_;
        if (base_font_size_ > 0) {
            int fs = (int)(base_font_size_ * sy); if (fs < 6) fs = 6; if (fs > 72) fs = 72;
            custom_font_size_ = fs;
        }
        for (auto& e : layer_list_) e->apply_scale(sx, sy);
        for (auto& e : layer_list_) if (e->type == ElementType::HBox || e->type == ElementType::VBox) layout_container(e.get());
        renovate_all(); InvalidateWindow();
    }

    void render(HDC hdc) {
        update_animations();
        RECT rc; GetClientRect(active_hwnd_, &rc); int w = rc.right, h = rc.bottom;
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
        HBRUSH bg = CreateSolidBrush(RGB((current_theme_.bg_color>>16)&0xFF, (current_theme_.bg_color>>8)&0xFF, current_theme_.bg_color&0xFF));
        FillRect(memDC, &rc, bg); DeleteObject(bg);
        if (antialiasing_enabled_) { SetTextCharacterExtra(memDC, 0); SetBkMode(memDC, TRANSPARENT); }

        for (auto& e : layer_list_) {
            if (!e->visible || e->type == ElementType::HBox || e->type == ElementType::VBox) continue;
            draw_element(memDC, e.get());
            e->clear_dirty();
        }

        for (auto& e : layer_list_) {
            if (e->type == ElementType::ComboBox && e->expanded) draw_combobox_options(memDC, e.get());
        }

        if (show_dialog_) draw_dialog(memDC, w, h);
        if (toast_visible_) draw_toast(memDC, w, h);
        if (debug_mode_) for (auto& e : layer_list_) if (e->visible) draw_debug_overlay(memDC, e.get());

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp); DeleteObject(memBmp); DeleteDC(memDC);
    }

    ElementPtr find_by_name(const std::string& n) const {
        auto it = elements_by_name_.find(n); return (it != elements_by_name_.end()) ? it->second : nullptr;
    }
    void set_active_hwnd(HWND h) { active_hwnd_ = h; }
    void post_task(std::function<void()> t) {
        { std::lock_guard<std::mutex> lk(task_mutex_); task_queue_.push(std::move(t)); }
        if (active_hwnd_) PostMessage(active_hwnd_, WM_USER_TASK, 0, 0);
    }
    void process_tasks() {
        std::function<void()> t;
        while (true) {
            { std::lock_guard<std::mutex> lk(task_mutex_); if (task_queue_.empty()) break; t = std::move(task_queue_.front()); task_queue_.pop(); }
            if (t) t();
        }
    }

    void OnToastTimer() {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - toast_start_).count() >= toast_duration_) {
            toast_visible_ = false;
            KillTimer(active_hwnd_, 2);
            InvalidateWindow();
        }
    }

    void OnCursorBlink() {
        for (auto& e : layer_list_) {
            if (e->type == ElementType::TextBox && e->is_focused) {
                e->cursor_visible = !e->cursor_visible;
                e->mark_dirty();
            }
        }
        InvalidateWindow();
    }

    void OnDialogClick(int x, int y) {
        if (!show_dialog_) return;
        POINT pt = {x, y};
        if (PtInRect(&dialog_ok_rect_, pt)) {
            show_dialog_ = false;
            if (dialog_on_ok_) dialog_on_ok_();
            InvalidateWindow();
        }
        else if (PtInRect(&dialog_cancel_rect_, pt)) {
            show_dialog_ = false;
            if (dialog_on_cancel_) dialog_on_cancel_();
            InvalidateWindow();
        }
    }

private:
    UIManager() = default;
    Theme current_theme_ = Theme::Light();

    void recalc_z_orders() { int z=0; for (auto& e : layer_list_) e->z_order = z++; next_z_order_ = z; }
    void start_animation(ElementPtr e, AnimationType type, bool in, int ms) {
        e->anim.active = true; e->anim.type = type; e->anim.start_time = std::chrono::steady_clock::now();
        e->anim.duration_ms = ms; e->anim.start_value = in ? 0.0f : 1.0f; e->anim.target_value = in ? 1.0f : 0.0f;
        e->visible = true;
    }
    void layout_container(UIElement* cont) {
        if (cont->type != ElementType::HBox && cont->type != ElementType::VBox) return;
        int off = cont->padding;
        for (auto& cn : cont->children) {
            auto c = find_by_name(cn); if (!c) continue;
            if (cont->type == ElementType::HBox) { c->x = cont->x + off; c->y = cont->y + cont->padding; off += c->width + cont->spacing; }
            else { c->x = cont->x + cont->padding; c->y = cont->y + off; off += c->height + cont->spacing; }
            c->original_x = c->x; c->original_y = c->y; c->mark_dirty();
        }
    }

    void draw_element(HDC hdc, const UIElement* e) {
        float alpha = e->get_alpha();
        bool need_alpha = (alpha < 0.99f) || e->anim.active;

        int offX = e->x, offY = e->y, drawW = e->width, drawH = e->height;
        if (e->anim.active) {
            float t = e->get_anim_progress();
            if (e->anim.type == AnimationType::Slide) {
                offX = e->anim_original_x + (int)(e->anim_offset_x * (1.0f - t));
                offY = e->anim_original_y + (int)(e->anim_offset_y * (1.0f - t));
            }
            else if (e->anim.type == AnimationType::Scale) {
                float scale = e->anim.start_value + (e->anim.target_value - e->anim.start_value) * t;
                drawW = (int)(e->anim_original_w * scale);
                drawH = (int)(e->anim_original_h * scale);
                offX = e->x + (e->width - drawW)/2;
                offY = e->y + (e->height - drawH)/2;
            }
        }

        if (e->custom_draw) {
            if (need_alpha) draw_with_alpha(hdc, e, alpha, offX, offY, drawW, drawH, [&](HDC dest) { e->custom_draw(dest, e); });
            else e->custom_draw(hdc, e);
            return;
        }

        if (e->type == ElementType::Image) {
            if (e->gdiImage) {
                Gdiplus::Graphics g(hdc);
                if (need_alpha) {
                    Gdiplus::ImageAttributes attr;
                    Gdiplus::ColorMatrix cm = { 1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,alpha,0, 0,0,0,0,1 };
                    attr.SetColorMatrix(&cm);
                    g.DrawImage(e->gdiImage, Gdiplus::Rect(offX, offY, drawW, drawH), 0,0,e->gdiImage->GetWidth(),e->gdiImage->GetHeight(), Gdiplus::UnitPixel, &attr);
                } else {
                    g.DrawImage(e->gdiImage, Gdiplus::Rect(offX, offY, drawW, drawH));
                }
            } else if (e->hBitmap) {
                if (need_alpha) draw_with_alpha(hdc, e, alpha, offX, offY, drawW, drawH, [&](HDC dest) { draw_bitmap(dest, e, offX, offY); });
                else draw_bitmap(hdc, e, offX, offY);
            }
            return;
        }

        if (need_alpha) {
            draw_with_alpha(hdc, e, alpha, offX, offY, drawW, drawH, [&](HDC dest) {
                draw_shape(dest, e, offX, offY, drawW, drawH);
                if (e->type != ElementType::TextBox) draw_text(dest, e, offX, offY, drawW, drawH);
                else draw_textbox_content(dest, e, offX, offY, drawW, drawH);
                draw_focus_rect(dest, e, offX, offY, drawW, drawH);
            });
        } else {
            draw_shape(hdc, e, offX, offY, drawW, drawH);
            if (e->type != ElementType::TextBox) draw_text(hdc, e, offX, offY, drawW, drawH);
            else draw_textbox_content(hdc, e, offX, offY, drawW, drawH);
            draw_focus_rect(hdc, e, offX, offY, drawW, drawH);
        }
    }

    void draw_with_alpha(HDC hdc, const UIElement* e, float alpha, int offX, int offY, int w, int h, std::function<void(HDC)> func) {
        if (w<=0||h<=0) return;
        HDC tmpDC = CreateCompatibleDC(hdc);
        HBITMAP tmpBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP oldBmp = (HBITMAP)SelectObject(tmpDC, tmpBmp);
        RECT rc = {0,0,w,h};
        FillRect(tmpDC, &rc, (HBRUSH)GetStockObject(NULL_BRUSH));
        SetBkMode(tmpDC, TRANSPARENT);
        func(tmpDC);
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(alpha*255), AC_SRC_ALPHA };
        AlphaBlend(hdc, offX, offY, w, h, tmpDC, 0, 0, w, h, bf);
        SelectObject(tmpDC, oldBmp); DeleteObject(tmpBmp); DeleteDC(tmpDC);
    }

    void draw_shape(HDC hdc, const UIElement* e, int offX, int offY, int w, int h) {
        Color c = e->get_draw_color(current_theme_);
        HBRUSH br = CreateSolidBrush(RGB((c>>16)&0xFF, (c>>8)&0xFF, c&0xFF));
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, br);
        HPEN pen = (e->type == ElementType::TextBox || e->type == ElementType::ComboBox) ? 
                   CreatePen(PS_SOLID, 1, RGB((current_theme_.input_border>>16)&0xFF, (current_theme_.input_border>>8)&0xFF, current_theme_.input_border&0xFF)) :
                   CreatePen(PS_NULL, 0, 0);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        if (e->radius > 0) RoundRect(hdc, offX, offY, offX+w, offY+h, e->radius*2, e->radius*2);
        else Rectangle(hdc, offX, offY, offX+w, offY+h);
        SelectObject(hdc, oldPen); DeleteObject(pen);
        SelectObject(hdc, oldBr); DeleteObject(br);
    }

    void draw_text(HDC hdc, const UIElement* e, int offX, int offY, int w, int h) {
        if (e->text.empty()) return;
        // 按钮文字强制白色
        Color textColor = (e->type == ElementType::Button) ? 0xFFFFFFFF : current_theme_.text_color;
        SetTextColor(hdc, RGB((textColor>>16)&0xFF, (textColor>>8)&0xFF, textColor&0xFF));
        HFONT fnt = use_custom_font_ ? CreateFontW(custom_font_size_,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0, antialiasing_enabled_?CLEARTYPE_QUALITY:DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, custom_font_name_.c_str()) : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT oldFnt = (HFONT)SelectObject(hdc, fnt);
        RECT rc = { offX, offY, offX+w, offY+h };
        DrawTextW(hdc, e->text.c_str(), -1, &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(hdc, oldFnt);
        if (use_custom_font_) DeleteObject(fnt);
    }

    void draw_textbox_content(HDC hdc, const UIElement* e, int offX, int offY, int w, int h) {
        std::wstring display = e->text;
        if (e->is_password && !display.empty()) display = std::wstring(display.size(), L'*');
        if (display.empty() && !e->placeholder.empty() && !e->is_focused) {
            SetTextColor(hdc, RGB(150,150,150));
            display = e->placeholder;
        } else {
            SetTextColor(hdc, RGB((current_theme_.text_color>>16)&0xFF, (current_theme_.text_color>>8)&0xFF, current_theme_.text_color&0xFF));
        }
        HFONT fnt = use_custom_font_ ? CreateFontW(custom_font_size_,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0, antialiasing_enabled_?CLEARTYPE_QUALITY:DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, custom_font_name_.c_str()) : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT oldFnt = (HFONT)SelectObject(hdc, fnt);
        RECT rc = { offX+5, offY, offX+w-5, offY+h };
        DrawTextW(hdc, display.c_str(), -1, &rc, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        if (e->is_focused && e->cursor_visible) {
            SIZE sz; GetTextExtentPoint32W(hdc, display.substr(0, e->cursor_pos).c_str(), e->cursor_pos, &sz);
            MoveToEx(hdc, offX+5+sz.cx, offY+3, NULL);
            LineTo(hdc, offX+5+sz.cx, offY+h-3);
        }
        SelectObject(hdc, oldFnt);
        if (use_custom_font_) DeleteObject(fnt);
    }

    void draw_combobox_options(HDC hdc, const UIElement* e) {
        int opt_h = 25, y = e->y + e->height;
        for (size_t i = 0; i < e->options.size(); ++i) {
            RECT opt_rect = { e->x, y + (int)i*opt_h, e->x + e->width, y + ((int)i+1)*opt_h };
            bool hover = (e->hovered_option == (int)i);
            Color bg = hover ? current_theme_.combo_item_hover : current_theme_.combo_bg;
            HBRUSH br = CreateSolidBrush(RGB((bg>>16)&0xFF, (bg>>8)&0xFF, bg&0xFF));
            FillRect(hdc, &opt_rect, br); DeleteObject(br);
            SetTextColor(hdc, RGB((current_theme_.text_color>>16)&0xFF, (current_theme_.text_color>>8)&0xFF, current_theme_.text_color&0xFF));
            DrawTextW(hdc, e->options[i].c_str(), -1, &opt_rect, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
    }

    void draw_dialog(HDC hdc, int winW, int winH) {
        HBRUSH mask = CreateSolidBrush(RGB(0,0,0));
        RECT maskRc = {0,0,winW,winH};
        FillRect(hdc, &maskRc, mask); DeleteObject(mask);
        int dlgW = 300, dlgH = 150;
        int dlgX = (winW - dlgW)/2, dlgY = (winH - dlgH)/2;
        RECT dlgRc = {dlgX, dlgY, dlgX+dlgW, dlgY+dlgH};
        HBRUSH dlgBr = CreateSolidBrush(RGB((current_theme_.dialog_bg>>16)&0xFF, (current_theme_.dialog_bg>>8)&0xFF, current_theme_.dialog_bg&0xFF));
        FillRect(hdc, &dlgRc, dlgBr); DeleteObject(dlgBr);
        SetTextColor(hdc, RGB((current_theme_.text_color>>16)&0xFF, (current_theme_.text_color>>8)&0xFF, current_theme_.text_color&0xFF));
        RECT titleRc = {dlgX, dlgY, dlgX+dlgW, dlgY+30};
        DrawTextW(hdc, dialog_title_.c_str(), -1, &titleRc, DT_CENTER|DT_VCENTER);
        RECT msgRc = {dlgX+20, dlgY+40, dlgX+dlgW-20, dlgY+80};
        DrawTextW(hdc, dialog_message_.c_str(), -1, &msgRc, DT_CENTER|DT_WORDBREAK);
        int btnW = 80, btnH = 30;
        int okX = dlgX + (dlgW/2) - btnW - 10, cancelX = dlgX + (dlgW/2) + 10;
        RECT okRc = {okX, dlgY+dlgH-40, okX+btnW, dlgY+dlgH-10};
        RECT cancelRc = {cancelX, dlgY+dlgH-40, cancelX+btnW, dlgY+dlgH-10};
        HBRUSH btnBr = CreateSolidBrush(RGB((current_theme_.button_color>>16)&0xFF, (current_theme_.button_color>>8)&0xFF, current_theme_.button_color&0xFF));
        FillRect(hdc, &okRc, btnBr); FillRect(hdc, &cancelRc, btnBr); DeleteObject(btnBr);
        DrawTextW(hdc, L"确定", -1, &okRc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        DrawTextW(hdc, L"取消", -1, &cancelRc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        dialog_ok_rect_ = okRc; dialog_cancel_rect_ = cancelRc;
    }

    void draw_toast(HDC hdc, int winW, int winH) {
        int w = 200, h = 50, x = winW - w - 20, y = winH - h - 20;
        RECT rc = {x, y, x+w, y+h};
        HBRUSH br = CreateSolidBrush(RGB((current_theme_.toast_bg>>16)&0xFF, (current_theme_.toast_bg>>8)&0xFF, current_theme_.toast_bg&0xFF));
        FillRect(hdc, &rc, br); DeleteObject(br);
        SetTextColor(hdc, RGB((current_theme_.text_color>>16)&0xFF, (current_theme_.text_color>>8)&0xFF, current_theme_.text_color&0xFF));
        DrawTextW(hdc, toast_message_.c_str(), -1, &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }

    void draw_focus_rect(HDC hdc, const UIElement* e, int offX, int offY, int w, int h) {
        if (!e->is_focused) return;
        HPEN pen = CreatePen(PS_DOT, 1, RGB(0,0,0));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, offX+2, offY+2, offX+w-2, offY+h-2);
        SelectObject(hdc, oldPen); SelectObject(hdc, oldBr); DeleteObject(pen);
    }

    void draw_bitmap(HDC hdc, const UIElement* e, int offX, int offY) {
        if (!e->hBitmap) return;
        HDC memBmpDC = CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memBmpDC, e->hBitmap);
        BitBlt(hdc, offX, offY, e->width, e->height, memBmpDC, 0, 0, SRCCOPY);
        SelectObject(memBmpDC, oldBmp); DeleteDC(memBmpDC);
    }

    void draw_debug_overlay(HDC hdc, const UIElement* e) {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(255,0,0));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, e->x, e->y, e->x+e->width, e->y+e->height);
        SelectObject(hdc, oldPen); SelectObject(hdc, oldBr); DeleteObject(pen);
    }

    void InvalidateWindow() { if (active_hwnd_) InvalidateRect(active_hwnd_, nullptr, FALSE); }

    std::unordered_map<std::string, ElementPtr> elements_by_name_;
    std::list<ElementPtr> layer_list_;
    int next_z_order_ = 0, next_tab_index_ = 0;
    bool antialiasing_enabled_ = true, borderless_ = false, debug_mode_ = false;
    HWND active_hwnd_ = nullptr;
    bool use_custom_font_ = false;
    std::wstring custom_font_name_ = L"Segoe UI";
    int custom_font_size_ = 16, base_font_size_ = 0;
    std::string focused_element_;
    std::queue<std::function<void()>> task_queue_;
    std::mutex task_mutex_;
    int base_window_width_ = 0, base_window_height_ = 0;

    bool show_dialog_ = false;
    std::wstring dialog_title_, dialog_message_;
    Callback dialog_on_ok_, dialog_on_cancel_;
    RECT dialog_ok_rect_, dialog_cancel_rect_;

    bool toast_visible_ = false;
    std::wstring toast_message_;
    std::chrono::steady_clock::time_point toast_start_;
    int toast_duration_ = 2000;
};

//=============================================================================
// 窗口管理
//=============================================================================
class EasyUIWindow {
public:
    bool Create(int w=800, int h=600, const wchar_t* t=L"easy_UI") {
        HINSTANCE hi = GetModuleHandle(nullptr); auto& ui = UIManager::instance();
        SetProcessDPIAware(); ui.SetBaseWindowSize(w, h);
        WNDCLASSW wc = {}; wc.lpfnWndProc=WndProc; wc.hInstance=hi; wc.lpszClassName=L"EasyUIWindowClass"; wc.hCursor=LoadCursor(0,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); RegisterClassW(&wc);
        DWORD style = WS_OVERLAPPEDWINDOW; if (ui.IsBorderless()) style = WS_POPUP|WS_VISIBLE;
        RECT rc={0,0,w,h}; AdjustWindowRect(&rc,style,FALSE);
        hwnd_ = CreateWindowExW(0, L"EasyUIWindowClass", t, style, CW_USEDEFAULT,0,rc.right-rc.left,rc.bottom-rc.top,0,0,hi,this);
        if (hwnd_) { ui.set_active_hwnd(hwnd_); SetTimer(hwnd_,1,16,0); SetTimer(hwnd_,3,500,0); return true; } return false;
    }
    void Show(int c=SW_SHOW) { ShowWindow(hwnd_,c); UpdateWindow(hwnd_); }
    int Run() { MSG m; while(GetMessage(&m,0,0,0)) { TranslateMessage(&m); DispatchMessage(&m); } return (int)m.wParam; }
    HWND GetHwnd() const { return hwnd_; }
private:
    HWND hwnd_ = nullptr;
    static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        EasyUIWindow* p = (m==WM_NCCREATE)?(EasyUIWindow*)((CREATESTRUCT*)l)->lpCreateParams:(EasyUIWindow*)GetWindowLongPtr(h,GWLP_USERDATA);
        if (m==WM_NCCREATE) SetWindowLongPtr(h,GWLP_USERDATA,(LONG_PTR)p), p->hwnd_=h, UIManager::instance().set_active_hwnd(h);
        auto& ui = UIManager::instance();
        if (p) switch(m) {
            case WM_SIZE: ui.OnWindowResize(LOWORD(l),HIWORD(l)); return 0;
            case WM_TIMER: 
                if (w==1) ui.update_animations();
                else if (w==2) ui.OnToastTimer();
                else if (w==3) ui.OnCursorBlink();
                return 0;
            case WM_USER_TASK: ui.process_tasks(); return 0;
            case WM_MOUSEMOVE: ui.OnMouseMove(GET_X_LPARAM(l),GET_Y_LPARAM(l)); break;
            case WM_LBUTTONDOWN: {
                int x=GET_X_LPARAM(l), y=GET_Y_LPARAM(l);
                ui.OnDialogClick(x,y);
                ui.OnMouseDown(x,y);
                break;
            }
            case WM_LBUTTONUP: ui.OnMouseUp(GET_X_LPARAM(l),GET_Y_LPARAM(l)); break;
            case WM_KEYDOWN: ui.OnKeyDown(w); break;
            case WM_CHAR: ui.OnChar(w); break;
            case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); static bool first=true; if(first){ui.renovate_all();first=false;} ui.render(dc); EndPaint(h,&ps); return 0; }
            case WM_ERASEBKGND: return TRUE;
            case WM_CLOSE: DestroyWindow(h); return 0;
            case WM_DESTROY: KillTimer(h,1); KillTimer(h,2); KillTimer(h,3); PostQuitMessage(0); return 0;
        }
        return DefWindowProc(h,m,w,l);
    }
};

//=============================================================================
// 对外接口
//=============================================================================
class EasyUI {
public:
    static bool InitWindow(int w=800,int h=600,const wchar_t* t=L"easy_UI") { return window_.Create(w,h,t); }
    static void ShowWindow(int c=SW_SHOW) { window_.Show(c); }
    static int Run() { return window_.Run(); }
    static HWND GetHwnd() { return window_.GetHwnd(); }

    static void SetTheme(const Theme& theme) { UIManager::instance().SetTheme(theme); }
    static const Theme& GetTheme() { return UIManager::instance().GetTheme(); }

    static void SetAntialiasing(bool e) { UIManager::instance().SetAntialiasing(e); }
    static void SetBorderless(bool e) { UIManager::instance().SetBorderless(e); }
    static void SetFont(const std::wstring& n, int s) { UIManager::instance().SetFont(n,s); }
    static void SetDefaultFont() { UIManager::instance().SetDefaultFont(); }
    static void SetDebugMode(bool e) { UIManager::instance().SetDebugMode(e); }

    static ElementPtr Create(int x,int y,int w,int h,int r,const std::wstring& t,const std::string& n,Color c,ElementType tp) { return UIManager::instance().create(x,y,w,h,r,t,n,c,tp); }
    static ElementPtr CreateImage(int x,int y,int w,int h,const std::wstring& p,const std::string& n) { return UIManager::instance().create_image(x,y,w,h,p,n); }
    static ElementPtr CreateTextBox(int x,int y,int w,int h,const std::wstring& placeholder,const std::string& n) { return UIManager::instance().create_textbox(x,y,w,h,placeholder,n); }
    static ElementPtr CreateComboBox(int x,int y,int w,int h,const std::vector<std::wstring>& opts,const std::string& n) { return UIManager::instance().create_combobox(x,y,w,h,opts,n); }

    static void ShowDialog(const std::wstring& title, const std::wstring& msg, Callback onOk = nullptr, Callback onCancel = nullptr) { UIManager::instance().ShowDialog(title,msg,onOk,onCancel); }
    static void ShowToast(const std::wstring& msg, int duration=2000) { UIManager::instance().ShowToast(msg,duration); }

    static void AnimateSlide(const std::string& n, int offX, int offY, int ms, EasingType e=EasingType::EaseOut) { UIManager::instance().AnimateSlide(n,offX,offY,ms,e); }
    static void AnimateScale(const std::string& n, float scale, int ms, EasingType e=EasingType::EaseOut) { UIManager::instance().AnimateScale(n,scale,ms,e); }

    static void SetText(const std::string& n,const std::wstring& t) { UIManager::instance().set_text(n,t); }
    static std::wstring GetText(const std::string& n) { auto e=UIManager::instance().find_by_name(n); return e?e->text:L""; }
    static void SetColor(const std::string& n,Color c) { UIManager::instance().set_color(n,c); }
    static void SetPosition(const std::string& n,int x,int y) { UIManager::instance().set_position(n,x,y); }
    static void SetSize(const std::string& n,int w,int h) { UIManager::instance().set_size(n,w,h); }
    static void SetCallback(const std::string& n,EventType e,Callback c) { UIManager::instance().set_callback(n,e,c); }
    static void SetTextChangedCallback(const std::string& n, TextChangedCallback c) { UIManager::instance().set_text_changed_callback(n,c); }
    static void SetSelectionCallback(const std::string& n, SelectionCallback c) { UIManager::instance().set_selection_callback(n,c); }
    static void SetCustomDraw(const std::string& n,CustomDrawCallback c) { UIManager::instance().set_custom_draw(n,c); }
    static void Show(const std::string& n,int d=0) { UIManager::instance().show(n,d); }
    static void Hide(const std::string& n,int d=0) { UIManager::instance().hide(n,d); }
    static void ShowAll() { UIManager::instance().show_all(); }
    static void HideAll() { UIManager::instance().hide_all(); }
    static void Renovate(const std::string& n) { UIManager::instance().renovate(n); }
    static void RenovateAll() { UIManager::instance().renovate_all(); }
    static void LayerTop(const std::string& n) { UIManager::instance().layer_top(n); }
    static void LayerBottom(const std::string& n) { UIManager::instance().layer_bottom(n); }
    static void LayerMoveUp(const std::string& n,int s) { UIManager::instance().layer_move_up(n,s); }
    static void AddToContainer(const std::string& c,const std::string& ch) { UIManager::instance().add_to_container(c,ch); }
    static void SetContainerSpacing(const std::string& n,int s) { UIManager::instance().set_container_spacing(n,s); }
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
    bool InitWindow(int w=800,int h=600,const wchar_t* t=L"easy_UI") { return easy_ui::EasyUI::InitWindow(w,h,t); }
    void ShowWindow(int c=SW_SHOW) { easy_ui::EasyUI::ShowWindow(c); }
    int Run() { return easy_ui::EasyUI::Run(); }
    HWND GetHwnd() { return easy_ui::EasyUI::GetHwnd(); }
    void SetTheme(const easy_ui::Theme& t) { easy_ui::EasyUI::SetTheme(t); }
    const easy_ui::Theme& GetTheme() { return easy_ui::EasyUI::GetTheme(); }
    void SetAntialiasing(bool e) { easy_ui::EasyUI::SetAntialiasing(e); }
    void SetBorderless(bool e) { easy_ui::EasyUI::SetBorderless(e); }
    void SetFont(const std::wstring& n,int s) { easy_ui::EasyUI::SetFont(n,s); }
    void SetDefaultFont() { easy_ui::EasyUI::SetDefaultFont(); }
    void SetDebugMode(bool e) { easy_ui::EasyUI::SetDebugMode(e); }
    easy_ui::ElementPtr Create(int x,int y,int w,int h,int r,const std::wstring& t,const std::string& n,easy_ui::Color c,easy_ui::ElementType tp) { return easy_ui::EasyUI::Create(x,y,w,h,r,t,n,c,tp); }
    easy_ui::ElementPtr CreateImage(int x,int y,int w,int h,const std::wstring& p,const std::string& n) { return easy_ui::EasyUI::CreateImage(x,y,w,h,p,n); }
    easy_ui::ElementPtr CreateTextBox(int x,int y,int w,int h,const std::wstring& p,const std::string& n) { return easy_ui::EasyUI::CreateTextBox(x,y,w,h,p,n); }
    easy_ui::ElementPtr CreateComboBox(int x,int y,int w,int h,const std::vector<std::wstring>& o,const std::string& n) { return easy_ui::EasyUI::CreateComboBox(x,y,w,h,o,n); }
    void ShowDialog(const std::wstring& t,const std::wstring& m, easy_ui::Callback ok=nullptr, easy_ui::Callback cancel=nullptr) { easy_ui::EasyUI::ShowDialog(t,m,ok,cancel); }
    void ShowToast(const std::wstring& m, int d=2000) { easy_ui::EasyUI::ShowToast(m,d); }
    void AnimateSlide(const std::string& n, int x,int y,int ms, easy_ui::EasingType e=easy_ui::EasingType::EaseOut) { easy_ui::EasyUI::AnimateSlide(n,x,y,ms,e); }
    void AnimateScale(const std::string& n, float s,int ms, easy_ui::EasingType e=easy_ui::EasingType::EaseOut) { easy_ui::EasyUI::AnimateScale(n,s,ms,e); }
    void SetText(const std::string& n,const std::wstring& t) { easy_ui::EasyUI::SetText(n,t); }
    std::wstring GetText(const std::string& n) { return easy_ui::EasyUI::GetText(n); }
    void SetColor(const std::string& n,easy_ui::Color c) { easy_ui::EasyUI::SetColor(n,c); }
    void SetPosition(const std::string& n,int x,int y) { easy_ui::EasyUI::SetPosition(n,x,y); }
    void SetSize(const std::string& n,int w,int h) { easy_ui::EasyUI::SetSize(n,w,h); }
    void SetCallback(const std::string& n,easy_ui::EventType e,easy_ui::Callback c) { easy_ui::EasyUI::SetCallback(n,e,c); }
    void SetTextChangedCallback(const std::string& n, easy_ui::TextChangedCallback c) { easy_ui::EasyUI::SetTextChangedCallback(n,c); }
    void SetSelectionCallback(const std::string& n, easy_ui::SelectionCallback c) { easy_ui::EasyUI::SetSelectionCallback(n,c); }
    void SetCustomDraw(const std::string& n,easy_ui::CustomDrawCallback c) { easy_ui::EasyUI::SetCustomDraw(n,c); }
    void Show(const std::string& n,int d=0) { easy_ui::EasyUI::Show(n,d); }
    void Hide(const std::string& n,int d=0) { easy_ui::EasyUI::Hide(n,d); }
    void ShowAll() { easy_ui::EasyUI::ShowAll(); }
    void HideAll() { easy_ui::EasyUI::HideAll(); }
    void Renovate(const std::string& n) { easy_ui::EasyUI::Renovate(n); }
    void RenovateAll() { easy_ui::EasyUI::RenovateAll(); }
    void LayerTop(const std::string& n) { easy_ui::EasyUI::LayerTop(n); }
    void LayerBottom(const std::string& n) { easy_ui::EasyUI::LayerBottom(n); }
    void LayerMoveUp(const std::string& n,int s) { easy_ui::EasyUI::LayerMoveUp(n,s); }
    void AddToContainer(const std::string& c,const std::string& ch) { easy_ui::EasyUI::AddToContainer(c,ch); }
    void SetContainerSpacing(const std::string& n,int s) { easy_ui::EasyUI::SetContainerSpacing(n,s); }
    void FocusElement(const std::string& n) { easy_ui::EasyUI::FocusElement(n); }
    void Post(std::function<void()> t) { easy_ui::EasyUI::Post(t); }
    easy_ui::ElementPtr FindByName(const std::string& n) { return easy_ui::EasyUI::FindByName(n); }
    void Destroy(const std::string& n) { easy_ui::EasyUI::Destroy(n); }
    void DestroyAll() { easy_ui::EasyUI::DestroyAll(); }
};
static EasyUIGlobal easyUI;

#endif // EASY_UI_HPP