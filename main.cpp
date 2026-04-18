#include "easy_UI.hpp"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    using namespace easy_ui;

    // 可选：无边框窗口（需在 InitWindow 前调用）
    // easyUI.SetBorderless(true);

    // 1. 初始化窗口
    if (!easyUI.InitWindow(800, 600, L"easy_UI 命名空间示例")) return 1;

    // 2. 自定义字体（例如使用“微软雅黑”，字号18）
    easyUI.SetFont(L"微软雅黑", 18);

    // 3. 启用抗锯齿
    easyUI.SetAntialiasing(true);

    // 4. 创建 UI 元素
    easyUI.Create(20,  20,  120, 50,  8, L"确定",   "btn_ok",   0xFF4CAF50, ElementType::Button);
    easyUI.Create(160, 20,  120, 50,  8, L"取消",   "btn_cancel",0xFFF44336, ElementType::Button);
    easyUI.Create(20,  90,  260, 160, 12, L"",       "bg_panel", 0xFFE0E0E0, ElementType::Rect);
    easyUI.Create(40,  110, 220, 40,  6, L"欢迎使用 easy_UI", "label", 0xFF2196F3, ElementType::Label);

    // 5. 设置按钮回调
    easyUI.SetCallback("btn_ok", []() {
        MessageBoxW(nullptr, L"你点击了确定！", L"提示", MB_OK);
    });
    easyUI.SetCallback("btn_cancel", []() {
        auto label = easyUI.FindByName("label");
        if (label) {
            label->text = L"已取消";
            easyUI.Renovate("label");
        }
    });

    // 6. 层级管理
    easyUI.LayerBottom("bg_panel");

    // 7. 显示窗口并进入消息循环
    easyUI.ShowWindow(nCmdShow);
    return easyUI.Run();
}