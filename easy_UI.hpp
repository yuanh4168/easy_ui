/**
 * @file easy_UI.hpp
 * @brief 轻量化 Windows 原生 UI 渲染库（单头文件实现）
 * 
 * 版本：2.8 (修复形状绘制位置错误，元素不再重叠在左上角)
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

// GDI+ 支持
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

namespace easy_ui {

//=============================================================================
// 自定义消息常量
//=============================================================================
constexpr UINT WM_USER_TASK = WM_USER + 100;

//=============================================================================
// GDI+ 初始化管理
//=============================================================================
class GdiPlusManager {
public:
    static void Init() {
        static GdiPlusManager instance;
    }
private:
    GdiPlusManager() {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&token_, &input, nullptr);
    }
    ~GdiPlusManager() {
        Gdiplus::GdiplusShutdown(token_);
    }
    ULONG_PTR token_;
};

//=============================================================================
// 基础类型定义
//=============================================================================

using Color = uint32_t;                     // 0xAARRGGBB

enum class ElementType {
    Window, Rect, Button, Label, Image, Custom,
    HBox, VBox
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

    std::vector<std::string> children;
    int spacing = 5;
    int padding = 5;
    std::string parent_name;

    HBITMAP hBitmap = nullptr;
    Gdiplus::Image* gdiImage = nullptr;
    std::wstring image_path;

    AnimationState anim;
    CustomDrawCallback custom_draw;

    // 原始尺寸（用于窗口缩放）
    int original_x = 0;
    int original_y = 0;
    int original_width = 0;
    int original_height = 0;
    int original_radius = 0;

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
        if (anim.active) {
            auto now = std::chrono::steady_clock::now();
            float t = std::chrono::duration<float>(now - anim.start_time).count() * 1000.0f / anim.duration_ms;
            if (t >= 1.0f) t = 1.0f;
            return anim.start_alpha + (anim.target_alpha - anim.start_alpha) * t;
        }
        return 1.0f;
    }

    Color get_draw_color() const {
        Color result = color;
        if (type == ElementType::Button) {
            BYTE a = (result >> 24) & 0xFF;
            BYTE r = (result >> 16) & 0xFF;
            BYTE g = (result >> 8)  & 0xFF;
            BYTE b = result & 0xFF;
            if (is_pressed) {
                r = (BYTE)(r * 0.8);
                g = (BYTE)(g * 0.8);
                b = (BYTE)(b * 0.8);
            } else if (is_hovered) {
                r = (BYTE)std::min(255, (int)(r * 1.2));
                g = (BYTE)std::min(255, (int)(g * 1.2));
                b = (BYTE)std::min(255, (int)(b * 1.2));
            }
            result = (a << 24) | (r << 16) | (g << 8) | b;
        }
        float alpha = get_alpha();
        BYTE a = (BYTE)(((result >> 24) & 0xFF) * alpha);
        BYTE r = (result >> 16) & 0xFF;
        BYTE g = (result >> 8)  & 0xFF;
        BYTE b = result & 0xFF;
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
        if (type == ElementType::Button) elem->tab_index = next_tab_index_++;
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
    void set_position(const std::string& n, int x, int y) { auto e = find_by_name(n); if (e) { e->x = x; e->y = y; e->original_x = x; e->original_y = y; e->mark_dirty(); InvalidateWindow(); } }
    void set_size(const std::string& n, int w, int h) { auto e = find_by_name(n); if (e) { e->width = w; e->height = h; e->original_width = w; e->original_height = h; e->mark_dirty(); InvalidateWindow(); } }
    void set_callback(const std::string& n, EventType ev, Callback cb) { auto e = find_by_name(n); if (e) e->callbacks[ev] = std::move(cb); }
    void set_custom_draw(const std::string& n, CustomDrawCallback cb) { auto e = find_by_name(n); if (e) { e->custom_draw = std::move(cb); e->mark_dirty(); InvalidateWindow(); } }

    void show(const std::string& n, int duration_ms = 0) {
        auto e = find_by_name(n); if (!e) return;
        if (!e->visible) {
            e->visible = true;
            if (duration_ms > 0) start_animation(e, true, duration_ms);
            else e->mark_dirty();
            InvalidateWindow();
        }
    }
    void hide(const std::string& n, int duration_ms = 0) {
        auto e = find_by_name(n); if (!e || !e->visible) return;
        if (duration_ms > 0) start_animation(e, false, duration_ms);
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
    void trigger_focused_click() {
        auto e = find_by_name(focused_element_);
        if (e && e->visible && e->type == ElementType::Button) {
            auto cb = e->callbacks.find(EventType::Click);
            if (cb != e->callbacks.end()) cb->second();
        }
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
            if (e->contains_point(x, y) && e->type == ElementType::Button) {
                e->is_pressed = true; e->mark_dirty(); InvalidateWindow(); SetCapture(active_hwnd_); break;
            }
        }
    }
    void OnMouseUp(int x, int y) {
        ReleaseCapture(); bool need = false;
        for (auto it = layer_list_.rbegin(); it != layer_list_.rend(); ++it) {
            auto& e = *it; if (!e->visible) continue;
            if (e->type == ElementType::Button && e->is_pressed) {
                e->is_pressed = false; e->mark_dirty(); need = true;
                if (e->contains_point(x, y)) {
                    auto cb = e->callbacks.find(EventType::Click); if (cb != e->callbacks.end()) cb->second();
                }
                break;
            }
        }
        if (need) InvalidateWindow();
    }
    void OnKeyDown(WPARAM wParam) {
        if (wParam == VK_TAB) focus_next();
        else if (wParam == VK_RETURN || wParam == VK_SPACE) trigger_focused_click();
    }

    void update_animations() {
        bool any = false; auto now = std::chrono::steady_clock::now();
        for (auto& e : layer_list_) {
            if (e->anim.active) {
                any = true; e->mark_dirty();
                float t = std::chrono::duration<float>(now - e->anim.start_time).count() * 1000.0f / e->anim.duration_ms;
                if (t >= 1.0f) { e->anim.active = false; if (!e->anim.fading_in) e->visible = false; }
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
        HBRUSH bg = GetSysColorBrush(COLOR_WINDOW);
        FillRect(memDC, &rc, bg);
        if (antialiasing_enabled_) { SetTextCharacterExtra(memDC, 0); SetBkMode(memDC, TRANSPARENT); }
        for (auto& e : layer_list_) {
            if (!e->visible || e->type == ElementType::HBox || e->type == ElementType::VBox) continue;
            draw_element(memDC, e.get());
            e->clear_dirty();
        }
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

private:
    UIManager() = default;
    void recalc_z_orders() { int z=0; for (auto& e : layer_list_) e->z_order = z++; next_z_order_ = z; }
    void start_animation(ElementPtr e, bool in, int ms) {
        e->anim.active = true; e->anim.fading_in = in; e->anim.start_time = std::chrono::steady_clock::now();
        e->anim.duration_ms = ms; e->anim.start_alpha = in ? 0.0f : 1.0f; e->anim.target_alpha = in ? 1.0f : 0.0f;
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

    // 绘制元素（决定是否使用透明度临时位图）
    void draw_element(HDC hdc, const UIElement* e) {
        float alpha = e->get_alpha();
        bool need_alpha = (alpha < 0.99f) || e->anim.active;

        if (e->custom_draw) {
            if (need_alpha) draw_with_alpha(hdc, e, alpha, [&](HDC dest) { e->custom_draw(dest, e); });
            else e->custom_draw(hdc, e);
            return;
        }

        if (e->type == ElementType::Image) {
            if (e->gdiImage) {
                Gdiplus::Graphics g(hdc);
                g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                if (need_alpha) {
                    Gdiplus::ImageAttributes attr;
                    Gdiplus::ColorMatrix cm = { 1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,alpha,0, 0,0,0,0,1 };
                    attr.SetColorMatrix(&cm);
                    g.DrawImage(e->gdiImage, Gdiplus::Rect(e->x, e->y, e->width, e->height), 0,0,e->gdiImage->GetWidth(),e->gdiImage->GetHeight(), Gdiplus::UnitPixel, &attr);
                } else {
                    g.DrawImage(e->gdiImage, Gdiplus::Rect(e->x, e->y, e->width, e->height));
                }
            } else if (e->hBitmap) {
                if (need_alpha) draw_with_alpha(hdc, e, alpha, [&](HDC dest) { draw_bitmap(dest, e, 0, 0); });
                else draw_bitmap(hdc, e, e->x, e->y);
            }
            return;
        }

        // 普通形状
        if (need_alpha) {
            draw_with_alpha(hdc, e, alpha, [&](HDC dest) {
                draw_shape(dest, e, 0, 0);
                draw_text(dest, e, 0, 0);
                draw_focus_rect(dest, e, 0, 0);
            });
        } else {
            draw_shape(hdc, e, e->x, e->y);
            draw_text(hdc, e, e->x, e->y);
            draw_focus_rect(hdc, e, e->x, e->y);
        }
    }

    void draw_with_alpha(HDC hdc, const UIElement* e, float alpha, std::function<void(HDC)> drawFunc) {
        int w = e->width, h = e->height; if (w<=0||h<=0) return;
        HDC tmpDC = CreateCompatibleDC(hdc);
        HBITMAP tmpBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP oldBmp = (HBITMAP)SelectObject(tmpDC, tmpBmp);
        RECT rc = {0,0,w,h};
        FillRect(tmpDC, &rc, (HBRUSH)GetStockObject(NULL_BRUSH));
        SetBkMode(tmpDC, TRANSPARENT);
        drawFunc(tmpDC);
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(alpha*255), AC_SRC_ALPHA };
        AlphaBlend(hdc, e->x, e->y, w, h, tmpDC, 0, 0, w, h, bf);
        SelectObject(tmpDC, oldBmp); DeleteObject(tmpBmp); DeleteDC(tmpDC);
    }

    void draw_shape(HDC hdc, const UIElement* e, int offX, int offY) {
        Color c = e->get_draw_color();
        HBRUSH br = CreateSolidBrush(RGB((c>>16)&0xFF, (c>>8)&0xFF, c&0xFF));
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, br);
        HPEN pen = CreatePen(PS_NULL, 0, 0);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        if (e->radius > 0) RoundRect(hdc, offX, offY, offX+e->width, offY+e->height, e->radius*2, e->radius*2);
        else Rectangle(hdc, offX, offY, offX+e->width, offY+e->height);
        SelectObject(hdc, oldPen); DeleteObject(pen);
        SelectObject(hdc, oldBr); DeleteObject(br);
    }

    void draw_text(HDC hdc, const UIElement* e, int offX, int offY) {
        if (e->text.empty()) return;
        SetTextColor(hdc, RGB(255,255,255));
        HFONT fnt = use_custom_font_ ? CreateFontW(custom_font_size_,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0, antialiasing_enabled_?CLEARTYPE_QUALITY:DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, custom_font_name_.c_str()) : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT oldFnt = (HFONT)SelectObject(hdc, fnt);
        RECT rc = { offX, offY, offX+e->width, offY+e->height };
        DrawTextW(hdc, e->text.c_str(), -1, &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(hdc, oldFnt);
        if (use_custom_font_) DeleteObject(fnt);
    }

    void draw_focus_rect(HDC hdc, const UIElement* e, int offX, int offY) {
        if (!e->is_focused) return;
        HPEN pen = CreatePen(PS_DOT, 1, RGB(0,0,0));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, offX+2, offY+2, offX+e->width-2, offY+e->height-2);
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
        std::wstring info = e->text.empty() ? std::wstring(e->name.begin(), e->name.end()) : e->text;
        info += L" (" + std::to_wstring(e->x) + L"," + std::to_wstring(e->y) + L")";
        SetTextColor(hdc, RGB(255,0,0)); SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, e->x, e->y-16, info.c_str(), (int)info.length());
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
        if (hwnd_) { ui.set_active_hwnd(hwnd_); SetTimer(hwnd_,1,16,0); return true; } return false;
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
            case WM_TIMER: ui.update_animations(); return 0;
            case WM_USER_TASK: ui.process_tasks(); return 0;
            case WM_MOUSEMOVE: ui.OnMouseMove(GET_X_LPARAM(l),GET_Y_LPARAM(l)); break;
            case WM_LBUTTONDOWN: ui.OnMouseDown(GET_X_LPARAM(l),GET_Y_LPARAM(l)); break;
            case WM_LBUTTONUP: ui.OnMouseUp(GET_X_LPARAM(l),GET_Y_LPARAM(l)); break;
            case WM_KEYDOWN: ui.OnKeyDown(w); break;
            case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); static bool first=true; if(first){ui.renovate_all();first=false;} ui.render(dc); EndPaint(h,&ps); return 0; }
            case WM_ERASEBKGND: return TRUE;
            case WM_CLOSE: DestroyWindow(h); return 0;
            case WM_DESTROY: KillTimer(h,1); PostQuitMessage(0); return 0;
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
    static void SetAntialiasing(bool e) { UIManager::instance().SetAntialiasing(e); }
    static void SetBorderless(bool e) { UIManager::instance().SetBorderless(e); }
    static void SetFont(const std::wstring& n, int s) { UIManager::instance().SetFont(n,s); }
    static void SetDefaultFont() { UIManager::instance().SetDefaultFont(); }
    static void SetDebugMode(bool e) { UIManager::instance().SetDebugMode(e); }
    static ElementPtr Create(int x,int y,int w,int h,int r,const std::wstring& t,const std::string& n,Color c,ElementType tp) { return UIManager::instance().create(x,y,w,h,r,t,n,c,tp); }
    static ElementPtr CreateImage(int x,int y,int w,int h,const std::wstring& p,const std::string& n) { return UIManager::instance().create_image(x,y,w,h,p,n); }
    static void SetText(const std::string& n,const std::wstring& t) { UIManager::instance().set_text(n,t); }
    static void SetColor(const std::string& n,Color c) { UIManager::instance().set_color(n,c); }
    static void SetPosition(const std::string& n,int x,int y) { UIManager::instance().set_position(n,x,y); }
    static void SetSize(const std::string& n,int w,int h) { UIManager::instance().set_size(n,w,h); }
    static void SetCallback(const std::string& n,EventType e,Callback c) { UIManager::instance().set_callback(n,e,c); }
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
    void SetAntialiasing(bool e) { easy_ui::EasyUI::SetAntialiasing(e); }
    void SetBorderless(bool e) { easy_ui::EasyUI::SetBorderless(e); }
    void SetFont(const std::wstring& n,int s) { easy_ui::EasyUI::SetFont(n,s); }
    void SetDefaultFont() { easy_ui::EasyUI::SetDefaultFont(); }
    void SetDebugMode(bool e) { easy_ui::EasyUI::SetDebugMode(e); }
    easy_ui::ElementPtr Create(int x,int y,int w,int h,int r,const std::wstring& t,const std::string& n,easy_ui::Color c,easy_ui::ElementType tp) { return easy_ui::EasyUI::Create(x,y,w,h,r,t,n,c,tp); }
    easy_ui::ElementPtr CreateImage(int x,int y,int w,int h,const std::wstring& p,const std::string& n) { return easy_ui::EasyUI::CreateImage(x,y,w,h,p,n); }
    void SetText(const std::string& n,const std::wstring& t) { easy_ui::EasyUI::SetText(n,t); }
    void SetColor(const std::string& n,easy_ui::Color c) { easy_ui::EasyUI::SetColor(n,c); }
    void SetPosition(const std::string& n,int x,int y) { easy_ui::EasyUI::SetPosition(n,x,y); }
    void SetSize(const std::string& n,int w,int h) { easy_ui::EasyUI::SetSize(n,w,h); }
    void SetCallback(const std::string& n,easy_ui::EventType e,easy_ui::Callback c) { easy_ui::EasyUI::SetCallback(n,e,c); }
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