/*
   easy_UI.hpp - 轻量级 Windows UI 渲染库 (C++11 / Win32 + GDI+)
   版本: 4.3
   描述: 单头文件 UI 库，三层架构分离，全功能封装。
   用法: #include "easy_UI.hpp" 并链接 gdiplus.lib
*/

#ifndef EASY_UI_HPP
#define EASY_UI_HPP

#define _CRT_SECURE_NO_WARNINGS
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

namespace eui {

// =============================================================================
// 基础类型
// =============================================================================
struct Color {
    BYTE a, r, g, b;
    Color(BYTE r = 0, BYTE g = 0, BYTE b = 0, BYTE a = 255) : a(a), r(r), g(g), b(b) {}
    Color(COLORREF cref, BYTE alpha = 255) {
        r = GetRValue(cref); g = GetGValue(cref); b = GetBValue(cref); a = alpha;
    }
    operator Gdiplus::Color() const { return Gdiplus::Color(a, r, g, b); }
    operator COLORREF() const { return RGB(r, g, b); }
    static Color FromHex(const char* hex) {
        unsigned int val = 0;
        if (hex[0] == '#') ++hex;
        sscanf(hex, "%x", &val);
        return Color((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
    }
};

struct Theme {
    Color bg, fg, border, accent, hover, pressed, text, textDisabled;
    Color shadow;
    int cornerRadius = 6;
    Theme() {
        bg = Color(45,45,48); fg = Color(63,63,70); border = Color(80,80,85);
        accent = Color(0,122,204); hover = Color(70,70,78); pressed = Color(55,55,60);
        text = Color(240,240,240); textDisabled = Color(150,150,150);
        shadow = Color(0,0,0,80);
    }
};

struct Font {
    Gdiplus::Font* ptr = nullptr;
    std::wstring family;
    float size;
    int style;
    Font(const wchar_t* family = L"Microsoft YaHei", float size = 12.0f, int style = Gdiplus::FontStyleRegular)
        : family(family), size(size), style(style) { ptr = new Gdiplus::Font(family, size, style); }
    ~Font() { delete ptr; }
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Gdiplus::Font* Get() const { return ptr; }
};

enum class BtnState { Normal, Hover, Pressed, Disabled };

struct BtnStyle {
    Color bgNormal, bgHover, bgPressed;
    Color textColor, textDisabled;
    Color border;
    int radius = 6;
    BtnStyle()
        : bgNormal(63,63,70), bgHover(70,70,78), bgPressed(55,55,60),
          textColor(240,240,240), textDisabled(150,150,150),
          border(80,80,85) {}
};

class Control {
public:
    std::string name;
    RECT originalRect = {0,0,0,0};
    bool visible = true;
    bool enabled = true;
    std::function<void()> onClick;
    std::function<void(const std::wstring&)> onTextChanged;

    Control(const std::string& name) : name(name) {}
    virtual ~Control() {}
    virtual void Draw(Gdiplus::Graphics* g) {}
    virtual void OnMouseMove(int x, int y) {}
    virtual void OnLButtonDown(int x, int y) {}
    virtual void OnLButtonUp(int x, int y) {}
    virtual void OnMouseLeave() {}
    virtual void OnKeyDown(WPARAM key) {}

    RECT GetRect() const;
    void GetScale(float& sx, float& sy) const;
    virtual bool PtInRect(int x, int y) const {
        RECT r = GetRect();
        return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    }
};

// =============================================================================
// 全局状态 (Meyers 单例)
// =============================================================================
namespace detail {
    struct GlobalState {
        Theme theme;
        BtnStyle defaultBtnStyle;
        std::unordered_map<std::string, std::unique_ptr<Font>> fonts;
        HWND hwnd = nullptr;
        int baseWidth = 800, baseHeight = 600;
        bool autoScale = false;
        RECT clientRect = {0,0,800,600};
        HDC memDC = nullptr;
        HBITMAP memBitmap = nullptr, oldBitmap = nullptr;
        int bufWidth = 0, bufHeight = 0;
        ULONG_PTR gdiplusToken = 0;
        bool gdiplusInit = false;
        HWND invisibleEdit = nullptr;
        WNDPROC oldEditProc = nullptr;
        std::function<void(const std::wstring&)> onEditFinished;
        std::unordered_map<std::string, std::shared_ptr<Control>> controls;
        Control* hovered = nullptr;
        Control* focused = nullptr;
        Control* pressed = nullptr;
        bool antiAlias = true;

        ~GlobalState() { if (gdiplusInit) Gdiplus::GdiplusShutdown(gdiplusToken); }
    };
    inline GlobalState& GS() { static GlobalState state; return state; }
} // namespace detail

// =============================================================================
// 底层 Core
// =============================================================================
namespace Core {
    inline void InitGDIPlus(ULONG_PTR& token) {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&token, &input, nullptr);
    }
    inline void ShutdownGDIPlus(ULONG_PTR token) { Gdiplus::GdiplusShutdown(token); }

    inline Font* GetFont(const std::string& name) {
        auto& fonts = detail::GS().fonts;
        auto it = fonts.find(name);
        return (it != fonts.end()) ? it->second.get() : nullptr;
    }
    inline void AddFont(const std::string& name, const wchar_t* family, float size, int style = Gdiplus::FontStyleRegular) {
        detail::GS().fonts[name] = std::unique_ptr<Font>(new Font(family, size, style));
    }

    class GfxWrap {
        Gdiplus::Graphics* g;
    public:
        GfxWrap(HDC hdc) : g(new Gdiplus::Graphics(hdc)) {
            g->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g->SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        }
        ~GfxWrap() { delete g; }
        Gdiplus::Graphics* operator->() { return g; }
        Gdiplus::Graphics* Get() { return g; }
    };

    inline void FillRect(Gdiplus::Graphics* g, const RECT& rc, const Color& color) {
        Gdiplus::SolidBrush brush(color);
        g->FillRectangle(&brush, (INT)rc.left, (INT)rc.top, (INT)(rc.right - rc.left), (INT)(rc.bottom - rc.top));
    }
    inline void DrawRect(Gdiplus::Graphics* g, const RECT& rc, const Color& color, float width = 1.0f) {
        Gdiplus::Pen pen(color, width);
        g->DrawRectangle(&pen, (INT)rc.left, (INT)rc.top, (INT)(rc.right - rc.left - 1), (INT)(rc.bottom - rc.top - 1));
    }
    inline void FillRoundRect(Gdiplus::Graphics* g, const RECT& rc, int radius, const Color& color) {
        Gdiplus::GraphicsPath path;
        path.AddArc((INT)rc.left, (INT)rc.top, (INT)radius, (INT)radius, 180, 90);
        path.AddArc((INT)(rc.right - radius - 1), (INT)rc.top, (INT)radius, (INT)radius, 270, 90);
        path.AddArc((INT)(rc.right - radius - 1), (INT)(rc.bottom - radius - 1), (INT)radius, (INT)radius, 0, 90);
        path.AddArc((INT)rc.left, (INT)(rc.bottom - radius - 1), (INT)radius, (INT)radius, 90, 90);
        path.CloseFigure();
        Gdiplus::SolidBrush brush(color);
        g->FillPath(&brush, &path);
    }
    inline void DrawRoundRect(Gdiplus::Graphics* g, const RECT& rc, int radius, const Color& color, float width = 1.0f) {
        Gdiplus::GraphicsPath path;
        path.AddArc((INT)rc.left, (INT)rc.top, (INT)radius, (INT)radius, 180, 90);
        path.AddArc((INT)(rc.right - radius - 1), (INT)rc.top, (INT)radius, (INT)radius, 270, 90);
        path.AddArc((INT)(rc.right - radius - 1), (INT)(rc.bottom - radius - 1), (INT)radius, (INT)radius, 0, 90);
        path.AddArc((INT)rc.left, (INT)(rc.bottom - radius - 1), (INT)radius, (INT)radius, 90, 90);
        path.CloseFigure();
        Gdiplus::Pen pen(color, width);
        g->DrawPath(&pen, &path);
    }
    inline void DrawImage(Gdiplus::Graphics* g, Gdiplus::Image* img, const RECT& rc) {
        g->DrawImage(img, (INT)rc.left, (INT)rc.top, (INT)(rc.right - rc.left), (INT)(rc.bottom - rc.top));
    }
    inline void DrawString(Gdiplus::Graphics* g, const std::wstring& text, const RECT& rc, Font* font,
                           const Color& color, float scaleX = 1.0f, float scaleY = 1.0f,
                           Gdiplus::StringAlignment alignH = Gdiplus::StringAlignmentCenter,
                           Gdiplus::StringAlignment alignV = Gdiplus::StringAlignmentCenter) {
        if (!font) return;
        std::unique_ptr<Font> scaledFont;
        if (scaleX != 1.0f || scaleY != 1.0f) {
            scaledFont = std::unique_ptr<Font>(new Font(font->family.c_str(), font->size * std::min(scaleX, scaleY), font->style));
            font = scaledFont.get();
        }
        Gdiplus::SolidBrush brush(color);
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(alignH);
        fmt.SetLineAlignment(alignV);
        fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        g->DrawString(text.c_str(), -1, font->Get(),
                      Gdiplus::RectF(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top),
                      &fmt, &brush);
    }

    inline LRESULT CALLBACK InvisibleEditProc(HWND, UINT, WPARAM, LPARAM);
    inline void CreateInvisibleEdit();
    inline void ShowInvisibleEdit(const RECT& rc, const std::wstring& initialText);
} // namespace Core

// =============================================================================
// 中层 Primitives
// =============================================================================
namespace Primitives {
    using namespace Core;

    inline void DrawBtnBg(Gdiplus::Graphics* g, const RECT& rc, const Color& bg, int radius = 6) {
        FillRoundRect(g, rc, radius, bg);
    }

    inline void DrawBtnState(Gdiplus::Graphics* g, const RECT& rc, BtnState state,
                             const Color& bgNormal, const Color& bgHover,
                             const Color& bgPressed, const Color& border, int radius) {
        Color bg;
        switch (state) {
            case BtnState::Normal:  bg = bgNormal; break;
            case BtnState::Hover:   bg = bgHover; break;
            case BtnState::Pressed: bg = bgPressed; break;
            default:                bg = Color(100,100,100);
        }
        DrawBtnBg(g, rc, bg, radius);
        if (state != BtnState::Disabled) DrawRoundRect(g, rc, radius, border);
    }

    inline void DrawComboArrow(Gdiplus::Graphics* g, const RECT& rc, bool expanded, const Color& color) {
        int arrowW = (int)std::max(4L, (rc.right - rc.left) / 4);
        int arrowH = (int)std::max(4L, (rc.bottom - rc.top) / 3);
        int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
        Gdiplus::Point pts[3];
        if (!expanded) {
            pts[0] = Gdiplus::Point(cx - arrowW, cy - arrowH/2);
            pts[1] = Gdiplus::Point(cx + arrowW, cy - arrowH/2);
            pts[2] = Gdiplus::Point(cx, cy + arrowH/2);
        } else {
            pts[0] = Gdiplus::Point(cx - arrowW, cy + arrowH/2);
            pts[1] = Gdiplus::Point(cx + arrowW, cy + arrowH/2);
            pts[2] = Gdiplus::Point(cx, cy - arrowH/2);
        }
        Gdiplus::SolidBrush brush(color);
        g->FillPolygon(&brush, pts, 3);
    }
} // namespace Primitives

// =============================================================================
// 高层 Controls
// =============================================================================
namespace Controls {
    using namespace Core;
    using namespace Primitives;

    class Button : public Control {
    public:
        std::wstring text;
        BtnState state = BtnState::Normal;
        std::unique_ptr<BtnStyle> style;
        int customRadius = -1;

        Button(const std::string& name, const std::wstring& text) : Control(name), text(text) {}
        void Draw(Gdiplus::Graphics* g) override {
            auto& gs = detail::GS();
            BtnStyle& st = style ? *style : gs.defaultBtnStyle;
            int r = (customRadius >= 0) ? customRadius : st.radius;
            RECT rc = GetRect();
            float sx, sy; GetScale(sx, sy);
            DrawBtnState(g, rc, state, st.bgNormal, st.bgHover, st.bgPressed, st.border, r);
            Color tc = enabled ? st.textColor : st.textDisabled;
            DrawString(g, text, rc, GetFont("default"), tc, sx, sy);
        }
        void OnMouseMove(int,int) override { state = BtnState::Hover; }
        void OnMouseLeave() override { state = BtnState::Normal; }
        void OnLButtonDown(int,int) override { state = BtnState::Pressed; }
        void OnLButtonUp(int,int) override { state = BtnState::Hover; }
    };

    class Label : public Control {
    public:
        std::wstring text;
        Color textColor = Color(240,240,240);
        Gdiplus::StringAlignment alignH = Gdiplus::StringAlignmentNear;
        Label(const std::string& name, const std::wstring& text) : Control(name), text(text) {}
        void Draw(Gdiplus::Graphics* g) override {
            RECT r = GetRect();
            float sx, sy; GetScale(sx, sy);
            DrawString(g, text, r, GetFont("default"), textColor, sx, sy, alignH);
        }
    };

    class TextBox : public Control {
    public:
        std::wstring text;
        TextBox(const std::string& name) : Control(name) {}
        void Draw(Gdiplus::Graphics* g) override {
            RECT r = GetRect();
            auto& gs = detail::GS();
            FillRect(g, r, gs.theme.fg);
            DrawRect(g, r, (gs.focused == this) ? gs.theme.accent : gs.theme.border);
            RECT tr = r; tr.left += 3; tr.right -= 3;
            float sx, sy; GetScale(sx, sy);
            DrawString(g, text.empty() ? L" " : text, tr, GetFont("default"), gs.theme.text, sx, sy, Gdiplus::StringAlignmentNear);
        }
        void OnLButtonDown(int,int) override {
            Core::ShowInvisibleEdit(GetRect(), text);
            detail::GS().onEditFinished = [this](const std::wstring& newText) {
                text = newText;
                if (onTextChanged) onTextChanged(text);
                InvalidateRect(detail::GS().hwnd, nullptr, FALSE);
            };
        }
    };

    class ComboBox : public Control {
    public:
        std::vector<std::wstring> items;
        int selectedIndex = -1;
        bool expanded = false;
        int itemHeight = 24;
        int hoveredIndex = -1;
        int customRadius = -1;

        ComboBox(const std::string& name) : Control(name) {}

        bool PtInRect(int x, int y) const override {
            if (!visible) return false;
            RECT r = GetRect();
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return true;
            if (expanded) {
                float sx, sy; GetScale(sx, sy);
                int scaledItemHeight = (int)(itemHeight * sy);
                if (scaledItemHeight < 20) scaledItemHeight = 20;
                int totalHeight = (int)items.size() * scaledItemHeight;
                RECT drop = { r.left, r.bottom, r.right, r.bottom + totalHeight };
                if (x >= drop.left && x < drop.right && y >= drop.top && y < drop.bottom) return true;
            }
            return false;
        }

        void Draw(Gdiplus::Graphics* g) override {
            RECT r = GetRect();
            auto& gs = detail::GS();
            int rad = (customRadius >= 0) ? customRadius : gs.defaultBtnStyle.radius;
            DrawBtnBg(g, r, gs.defaultBtnStyle.bgNormal, rad);

            int arrowWidth = (int)std::max(16L, (r.right - r.left) / 5);
            RECT textRect = { r.left, r.top, r.right - arrowWidth, r.bottom };
            RECT arrowRect = { r.right - arrowWidth, r.top, r.right, r.bottom };

            std::wstring txt = (selectedIndex >=0 && selectedIndex < (int)items.size()) ? items[selectedIndex] : L"";
            float sx, sy; GetScale(sx, sy);
            DrawString(g, txt, textRect, GetFont("default"), gs.defaultBtnStyle.textColor, sx, sy, Gdiplus::StringAlignmentNear);
            DrawComboArrow(g, arrowRect, expanded, gs.defaultBtnStyle.textColor);

            if (expanded) {
                int scaledItemHeight = (int)(itemHeight * sy);
                if (scaledItemHeight < 20) scaledItemHeight = 20;
                int y = r.bottom;
                for (size_t i = 0; i < items.size(); ++i) {
                    RECT ir = { r.left, y, r.right, y + scaledItemHeight };
                    FillRect(g, ir, ((int)i == hoveredIndex) ? gs.theme.hover : gs.theme.fg);
                    DrawString(g, items[i], ir, GetFont("default"), gs.defaultBtnStyle.textColor, sx, sy, Gdiplus::StringAlignmentNear);
                    y += scaledItemHeight;
                }
            }
        }

        void OnMouseMove(int x, int y) override {
            if (!expanded) return;
            RECT r = GetRect();
            float sx, sy; GetScale(sx, sy);
            int scaledItemHeight = (int)(itemHeight * sy);
            if (scaledItemHeight < 20) scaledItemHeight = 20;
            int baseY = r.bottom;
            int idx = (y - baseY) / scaledItemHeight;
            if (x >= r.left && x < r.right && idx >= 0 && idx < (int)items.size()) hoveredIndex = idx;
            else hoveredIndex = -1;
            InvalidateRect(detail::GS().hwnd, nullptr, FALSE);
        }

        void OnLButtonDown(int x, int y) override {
            RECT r = GetRect();
            float sx, sy; GetScale(sx, sy);
            int scaledItemHeight = (int)(itemHeight * sy);
            if (scaledItemHeight < 20) scaledItemHeight = 20;

            if (expanded) {
                int baseY = r.bottom;
                if (y >= baseY && x >= r.left && x < r.right) {
                    int idx = (y - baseY) / scaledItemHeight;
                    if (idx >= 0 && idx < (int)items.size()) selectedIndex = idx;
                }
                expanded = false;
            } else {
                expanded = true;
            }
            InvalidateRect(detail::GS().hwnd, nullptr, FALSE);
            UpdateWindow(detail::GS().hwnd);
        }

        void OnMouseLeave() override { hoveredIndex = -1; }
    };

    class Image : public Control {
    public:
        Gdiplus::Image* image = nullptr;
        Image(const std::string& name, const wchar_t* path) : Control(name) { image = new Gdiplus::Image(path); }
        ~Image() { delete image; }
        void Draw(Gdiplus::Graphics* g) override { DrawImage(g, image, GetRect()); }
    };

    // 控件管理器
    inline void AddControl(std::shared_ptr<Control> ctrl) { detail::GS().controls[ctrl->name] = ctrl; }
    inline Control* FindControl(const std::string& name) {
        auto& m = detail::GS().controls;
        auto it = m.find(name);
        return (it != m.end()) ? it->second.get() : nullptr;
    }
    inline void DrawAll(Gdiplus::Graphics* g) {
        for (auto& kv : detail::GS().controls)
            if (kv.second->visible) kv.second->Draw(g);
    }
    inline void DispatchMouseMove(int x, int y) {
        auto& gs = detail::GS();
        for (auto& kv : gs.controls) {
            auto c = kv.second.get();
            if (c->visible && c->PtInRect(x, y)) {
                if (gs.hovered != c) { if (gs.hovered) gs.hovered->OnMouseLeave(); gs.hovered = c; }
                c->OnMouseMove(x, y);
                return;
            }
        }
        if (gs.hovered) { gs.hovered->OnMouseLeave(); gs.hovered = nullptr; }
    }
    inline void DispatchLButtonDown(int x, int y) {
        auto& gs = detail::GS();
        for (auto& kv : gs.controls) {
            auto c = kv.second.get();
            if (c->visible && c->PtInRect(x, y)) {
                gs.focused = c; gs.pressed = c;
                c->OnLButtonDown(x, y);
                return;
            }
        }
        gs.focused = gs.pressed = nullptr;
    }
    inline void DispatchLButtonUp(int x, int y) {
        auto& gs = detail::GS();
        if (gs.pressed) {
            gs.pressed->OnLButtonUp(x, y);
            if (gs.pressed->PtInRect(x, y) && gs.pressed->onClick) gs.pressed->onClick();
            gs.pressed = nullptr;
        }
    }
    inline void DispatchKeyDown(WPARAM key) {
        if (detail::GS().focused) detail::GS().focused->OnKeyDown(key);
    }

} // namespace Controls

// Control 辅助方法
inline RECT Control::GetRect() const {
    auto& gs = detail::GS();
    if (!gs.autoScale) return originalRect;
    float sx = (float)gs.clientRect.right / gs.baseWidth;
    float sy = (float)gs.clientRect.bottom / gs.baseHeight;
    RECT r;
    r.left   = (LONG)(originalRect.left * sx);
    r.top    = (LONG)(originalRect.top * sy);
    r.right  = (LONG)(originalRect.right * sx);
    r.bottom = (LONG)(originalRect.bottom * sy);
    return r;
}
inline void Control::GetScale(float& sx, float& sy) const {
    auto& gs = detail::GS();
    sx = gs.autoScale ? (float)gs.clientRect.right / gs.baseWidth : 1.0f;
    sy = gs.autoScale ? (float)gs.clientRect.bottom / gs.baseHeight : 1.0f;
}

// 隐形输入框实现
namespace Core {
    inline LRESULT CALLBACK InvisibleEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        auto& gs = detail::GS();
        if (msg == WM_KILLFOCUS || (msg == WM_CHAR && wParam == VK_RETURN)) {
            wchar_t buf[1024];
            GetWindowTextW(hwnd, buf, 1024);
            if (gs.onEditFinished) gs.onEditFinished(buf);
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        return CallWindowProc(gs.oldEditProc, hwnd, msg, wParam, lParam);
    }
    inline void CreateInvisibleEdit() {
        auto& gs = detail::GS();
        if (gs.invisibleEdit) return;
        gs.invisibleEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | WS_BORDER,
                                         0,0,100,20, gs.hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        gs.oldEditProc = (WNDPROC)SetWindowLongPtr(gs.invisibleEdit, GWLP_WNDPROC, (LONG_PTR)InvisibleEditProc);
        ShowWindow(gs.invisibleEdit, SW_HIDE);
    }
    inline void ShowInvisibleEdit(const RECT& rc, const std::wstring& initText) {
        auto& gs = detail::GS();
        if (!gs.invisibleEdit) CreateInvisibleEdit();
        SetWindowTextW(gs.invisibleEdit, initText.c_str());
        SetWindowPos(gs.invisibleEdit, HWND_TOP, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_SHOWWINDOW);
        SetFocus(gs.invisibleEdit);
        SendMessage(gs.invisibleEdit, EM_SETSEL, 0, -1);
    }
}

// Application 结构体
struct Application {
    std::wstring title = L"easy_UI Application";
    int width = 800, height = 600;
    std::function<void()> OnInit;
};

// InternalWndProc 前向声明
LRESULT CALLBACK InternalWndProc(HWND, UINT, WPARAM, LPARAM);

} // namespace eui

// =============================================================================
// 全局对象 easyUI（位于全局命名空间）
// =============================================================================

struct EasyUI {
    void SetGlobalFont(const wchar_t* family, float size = 14, int style = Gdiplus::FontStyleRegular) {
        eui::Core::AddFont("default", family, size, style);
    }

    std::string CreateButton(const std::string& name, const std::wstring& text, int x, int y, int w, int h, int radius = -1) {
        auto btn = std::make_shared<eui::Controls::Button>(name, text);
        btn->originalRect = { x, y, x + w, y + h };
        btn->customRadius = radius;
        eui::Controls::AddControl(btn);
        return name;
    }
    std::string CreateLabel(const std::string& name, const std::wstring& text, int x, int y, int w, int h, int radius = -1) {
        auto lbl = std::make_shared<eui::Controls::Label>(name, text);
        lbl->originalRect = { x, y, x + w, y + h };
        (void)radius;
        eui::Controls::AddControl(lbl);
        return name;
    }
    std::string CreateTextBox(const std::string& name, int x, int y, int w, int h, int radius = -1) {
        auto tb = std::make_shared<eui::Controls::TextBox>(name);
        tb->originalRect = { x, y, x + w, y + h };
        (void)radius;
        eui::Controls::AddControl(tb);
        return name;
    }
    std::string CreateComboBox(const std::string& name, int x, int y, int w, int h, int radius = -1) {
        auto cb = std::make_shared<eui::Controls::ComboBox>(name);
        cb->originalRect = { x, y, x + w, y + h };
        cb->customRadius = radius;
        eui::Controls::AddControl(cb);
        return name;
    }
    std::string CreateImage(const std::string& name, const wchar_t* path, int x, int y, int w, int h) {
        auto img = std::make_shared<eui::Controls::Image>(name, path);
        img->originalRect = { x, y, x + w, y + h };
        eui::Controls::AddControl(img);
        return name;
    }

    void Text(const std::string& name, const std::wstring& text) {
        auto c = eui::Controls::FindControl(name);
        if (auto btn = dynamic_cast<eui::Controls::Button*>(c)) btn->text = text;
        else if (auto lbl = dynamic_cast<eui::Controls::Label*>(c)) lbl->text = text;
        else if (auto tb = dynamic_cast<eui::Controls::TextBox*>(c)) tb->text = text;
        InvalidateRect(eui::detail::GS().hwnd, nullptr, FALSE);
    }
    std::wstring Text(const std::string& name) {
        auto c = eui::Controls::FindControl(name);
        if (auto btn = dynamic_cast<eui::Controls::Button*>(c)) return btn->text;
        if (auto lbl = dynamic_cast<eui::Controls::Label*>(c)) return lbl->text;
        if (auto tb = dynamic_cast<eui::Controls::TextBox*>(c)) return tb->text;
        return L"";
    }
    void Visible(const std::string& name, bool vis) {
        if (auto c = eui::Controls::FindControl(name)) c->visible = vis;
        InvalidateRect(eui::detail::GS().hwnd, nullptr, FALSE);
    }
    void Enable(const std::string& name, bool en) {
        if (auto c = eui::Controls::FindControl(name)) c->enabled = en;
        InvalidateRect(eui::detail::GS().hwnd, nullptr, FALSE);
    }
    void Rect(const std::string& name, int x, int y, int w, int h) {
        if (auto c = eui::Controls::FindControl(name)) c->originalRect = {x,y,x+w,y+h};
        InvalidateRect(eui::detail::GS().hwnd, nullptr, FALSE);
    }
    void OnClick(const std::string& name, std::function<void()> fn) {
        if (auto c = eui::Controls::FindControl(name)) c->onClick = fn;
    }
    void OnTextChange(const std::string& name, std::function<void(const std::wstring&)> fn) {
        if (auto c = eui::Controls::FindControl(name)) c->onTextChanged = fn;
    }

    void AddComboItem(const std::string& name, const std::wstring& item) {
        auto c = eui::Controls::FindControl(name);
        if (auto cb = dynamic_cast<eui::Controls::ComboBox*>(c)) cb->items.push_back(item);
    }
    int GetComboIndex(const std::string& name) {
        auto c = eui::Controls::FindControl(name);
        if (auto cb = dynamic_cast<eui::Controls::ComboBox*>(c)) return cb->selectedIndex;
        return -1;
    }
    std::vector<std::wstring> GetComboItems(const std::string& name) {
        auto c = eui::Controls::FindControl(name);
        if (auto cb = dynamic_cast<eui::Controls::ComboBox*>(c)) return cb->items;
        return {};
    }

    void SetBtnStyle(const std::string& name, const eui::BtnStyle& style) {
        auto c = eui::Controls::FindControl(name);
        if (auto btn = dynamic_cast<eui::Controls::Button*>(c)) btn->style.reset(new eui::BtnStyle(style));
        InvalidateRect(eui::detail::GS().hwnd, nullptr, FALSE);
    }
    void SetGlobalBtnStyle(const eui::BtnStyle& style) {
        eui::detail::GS().defaultBtnStyle = style;
        InvalidateRect(eui::detail::GS().hwnd, nullptr, FALSE);
    }
    void SetLabelColor(const std::string& name, const eui::Color& color) {
        auto c = eui::Controls::FindControl(name);
        if (auto lbl = dynamic_cast<eui::Controls::Label*>(c)) lbl->textColor = color;
        InvalidateRect(eui::detail::GS().hwnd, nullptr, FALSE);
    }

    void SetAutoScale(bool enable, int baseW = 0, int baseH = 0) {
        auto& gs = eui::detail::GS();
        gs.autoScale = enable;
        if (baseW > 0) gs.baseWidth = baseW;
        if (baseH > 0) gs.baseHeight = baseH;
        InvalidateRect(gs.hwnd, nullptr, FALSE);
    }
    void SetAntiAlias(bool on) {
        eui::detail::GS().antiAlias = on;
        InvalidateRect(eui::detail::GS().hwnd, nullptr, FALSE);
    }
    void SetTheme(const eui::Theme& theme) { eui::detail::GS().theme = theme; InvalidateRect(eui::detail::GS().hwnd, nullptr, FALSE); }
    eui::Theme& Theme() { return eui::detail::GS().theme; }

    // 静态成员函数：运行应用
    static int Run(const eui::Application& app);
};

static EasyUI easyUI;

// 实现 Run（需要放在 InternalWndProc 定义之后）
namespace eui {
    // 窗口过程实现
    inline LRESULT CALLBACK InternalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        auto& gs = detail::GS();
        switch (msg) {
            case WM_CLOSE: DestroyWindow(hwnd); break;
            case WM_DESTROY: PostQuitMessage(0); break;
            case WM_ERASEBKGND: return 1;
        }
        if (!gs.gdiplusInit) return DefWindowProc(hwnd, msg, wParam, lParam);

        switch (msg) {
            case WM_SIZE:
                gs.clientRect.right = LOWORD(lParam);
                gs.clientRect.bottom = HIWORD(lParam);
                InvalidateRect(hwnd, NULL, TRUE);
                UpdateWindow(hwnd);
                break;
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                int w = gs.clientRect.right, h = gs.clientRect.bottom;
                if (w > 0 && h > 0) {
                    if (!gs.memDC || gs.bufWidth != w || gs.bufHeight != h) {
                        if (gs.memDC) {
                            SelectObject(gs.memDC, gs.oldBitmap);
                            DeleteObject(gs.memBitmap);
                            DeleteDC(gs.memDC);
                        }
                        gs.memDC = CreateCompatibleDC(hdc);
                        gs.memBitmap = CreateCompatibleBitmap(hdc, w, h);
                        gs.oldBitmap = (HBITMAP)SelectObject(gs.memDC, gs.memBitmap);
                        gs.bufWidth = w; gs.bufHeight = h;
                        RECT full = {0,0,w,h};
                        Gdiplus::Graphics gtmp(gs.memDC);
                        Core::FillRect(&gtmp, full, gs.theme.bg);
                    }
                    Core::GfxWrap gfx(gs.memDC);
                    if (!gs.antiAlias) gfx->SetSmoothingMode(Gdiplus::SmoothingModeNone);
                    Core::FillRect(gfx.Get(), gs.clientRect, gs.theme.bg);
                    Controls::DrawAll(gfx.Get());
                    BitBlt(hdc, 0, 0, w, h, gs.memDC, 0, 0, SRCCOPY);
                }
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_MOUSEMOVE:
                Controls::DispatchMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            case WM_LBUTTONDOWN:
                Controls::DispatchLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            case WM_LBUTTONUP:
                Controls::DispatchLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            case WM_KEYDOWN:
                Controls::DispatchKeyDown(wParam);
                break;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int EasyUI::Run(const eui::Application& app) {
    auto& gs = eui::detail::GS();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW|CS_VREDRAW, eui::InternalWndProc, 0,0,
                      GetModuleHandle(nullptr), nullptr, LoadCursor(nullptr, IDC_ARROW),
                      (HBRUSH)(COLOR_WINDOW+1), nullptr, L"EasyUIWindow", nullptr };
    RegisterClassEx(&wc);

    gs.baseWidth = app.width;
    gs.baseHeight = app.height;
    gs.clientRect = {0, 0, app.width, app.height};

    HWND hwnd = CreateWindowEx(0, L"EasyUIWindow", app.title.c_str(),
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, app.width, app.height,
                               nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwnd) return -1;

    gs.hwnd = hwnd;
    eui::Core::InitGDIPlus(gs.gdiplusToken);
    gs.gdiplusInit = true;
    eui::Core::AddFont("default", L"Microsoft YaHei", 14);
    easyUI.SetGlobalFont(L"Microsoft YaHei", 14);

    if (app.OnInit) app.OnInit();

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    gs.controls.clear();
    if (gs.gdiplusInit) eui::Core::ShutdownGDIPlus(gs.gdiplusToken);
    return 0;
}

#endif // EASY_UI_HPP