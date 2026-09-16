#pragma once
#pragma once

#include "framework.h"

namespace CodeAsMetal
{
    class MainWindow
    {
    public:
        MainWindow() = default;

        bool Create(HINSTANCE hInstance, int nCmdShow);

        [[nodiscard]]
        HWND Handle() const noexcept;

    private:
        static LRESULT CALLBACK WindowProc(
            HWND hWnd,
            UINT message,
            WPARAM wParam,
            LPARAM lParam);

        bool RegisterWindowClass(HINSTANCE hInstance);

    private:
        HWND m_hWnd = nullptr;
        HINSTANCE m_hInstance = nullptr;
    };
}