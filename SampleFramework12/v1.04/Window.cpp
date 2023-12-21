//=================================================================================================
//
//  MJP's DX11 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "Window.h"

namespace SampleFramework12
{

Window::Window( HINSTANCE hinstance,
                const wchar* name,
                uint32 style,
                uint32 exStyle,
                uint32 clientWidth,
                uint32 clientHeight,
                const wchar* iconResource,
                const wchar* smallIconResource,
                const wchar* menuResource,
                const wchar* accelResource) : style(style),
                                              exStyle(exStyle),
                                              appName(name),
                                              hinstance(hinstance),
                                              hwnd(nullptr)
{
    if(hinstance == nullptr)
        this->hinstance = GetModuleHandle(nullptr);

    INITCOMMONCONTROLSEX cce;
    cce.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cce.dwICC = ICC_BAR_CLASSES|ICC_COOL_CLASSES|ICC_STANDARD_CLASSES|ICC_STANDARD_CLASSES;
    ::InitCommonControlsEx ( &cce );

    MakeWindow(iconResource, smallIconResource, menuResource);
    SetClientArea(clientWidth, clientHeight);

    if(accelResource)
    {
        accelTable = ::LoadAccelerators(hinstance, accelResource);
        if(!accelTable)
            throw Win32Exception(::GetLastError());
    }
}

Window::~Window()
{
    ::DestroyWindow(hwnd);
    ::UnregisterClass(appName.c_str(), hinstance);
}

HWND Window::GetHwnd() const
{
    return hwnd;
}

HMENU Window::GetMenu() const
{
    return ::GetMenu(hwnd);
}

HINSTANCE Window::GetHinstance() const
{
    return hinstance;
}

bool32 Window::IsAlive() const
{
    return ::IsWindow(hwnd);
}

bool32 Window::IsMinimized() const
{
    return ::IsIconic(hwnd);
}

bool32 Window::HasFocus() const
{
    return GetActiveWindow() == hwnd;
}

void Window::SetWindowStyle(uint32 newStyle)
{
    if(!::SetWindowLongPtr(hwnd, GWL_STYLE, newStyle))
        throw Win32Exception(::GetLastError());

    style = newStyle;
}

void Window::SetExtendedStyle(uint32 newExStyle)
{
    if(!::SetWindowLongPtr(hwnd, GWL_EXSTYLE, newExStyle))
        throw Win32Exception(::GetLastError());

    exStyle = newExStyle;
}

LONG_PTR Window::GetWindowStyle() const
{
    return GetWindowLongPtr(hwnd, GWL_STYLE);
}

LONG_PTR Window::GetExtendedStyle() const
{
    return GetWindowLongPtr(hwnd, GWL_EXSTYLE);
}

void Window::MakeWindow(const wchar* sIconResource, const wchar* sSmallIconResource, const wchar* sMenuResource)
{

    HICON hIcon = nullptr;
    if(sIconResource)
    {
        hIcon = reinterpret_cast<HICON>(::LoadImage(hinstance,
                                                    sIconResource,
                                                    IMAGE_ICON,
                                                    0,
                                                    0,
                                                    LR_DEFAULTCOLOR));
    }

    HICON hSmallIcon = nullptr;
    if(sSmallIconResource)
    {
        hIcon = reinterpret_cast<HICON>(::LoadImage(hinstance,
                                                    sSmallIconResource,
                                                    IMAGE_ICON,
                                                    0,
                                                    0,
                                                    LR_DEFAULTCOLOR));
    }

    HCURSOR hCursor = ::LoadCursorW(nullptr, IDC_ARROW);

    // Register the window class
    WNDCLASSEX wc = {   sizeof(WNDCLASSEX),
                        CS_DBLCLKS,
                        WndProc,
                        0,
                        0,
                        hinstance,
                        hIcon,
                        hCursor,
                        nullptr,
                        sMenuResource,
                        appName.c_str(),
                        hSmallIcon
                    };

    if(!::RegisterClassEx(&wc))
        throw Win32Exception(::GetLastError());

    // Create the application's window
    hwnd = ::CreateWindowEx(exStyle,
                            appName.c_str(),
                            appName.c_str(),
                            style,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            NULL,
                            NULL,
                            hinstance,
                            (void*)this
                            );

    if(!hwnd)
        throw Win32Exception(::GetLastError());
    }

void Window::SetWindowPos(int32 posX, int32 posY)
{
    if(!::SetWindowPos(hwnd, HWND_NOTOPMOST, posX, posY, 0, 0, SWP_NOSIZE))
        throw Win32Exception(::GetLastError());
}

void Window::GetWindowPos(int32& posX, int32& posY) const
{
    RECT windowRect;
    if(!::GetWindowRect(hwnd, &windowRect))
        throw Win32Exception(::GetLastError());
    posX = windowRect.left;
    posY = windowRect.top;
}

void Window::ShowWindow(bool show)
{
    int32 cmdShow = show ? SW_SHOW : SW_HIDE;

    ::ShowWindow(hwnd, cmdShow);
}

void Window::SetClientArea(int32 clientX, int32 clientY)
{
    RECT windowRect;
    ::SetRect( &windowRect, 0, 0, clientX, clientY );

    BOOL isMenu = (::GetMenu(hwnd) != nullptr);
    if(!::AdjustWindowRectEx(&windowRect, style, isMenu, exStyle))
        throw Win32Exception(::GetLastError());

    if(!::SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, SWP_NOMOVE))
        throw Win32Exception(::GetLastError());
}

void Window::GetClientArea(int32& clientX, int32& clientY) const
{
    RECT clientRect;
    if(!::GetClientRect(hwnd, &clientRect))
        throw Win32Exception(::GetLastError());

    clientX = clientRect.right;
    clientY = clientRect.bottom;
}

void Window::SetWindowTitle(const wchar* title)
{
    if(!::SetWindowText(hwnd, title))
        throw Win32Exception(::GetLastError());
}

void Window::SetScrollRanges(int32 scrollRangeX, int32 scrollRangeY, int32 posX, int32 posY)
{
    int32 clientX, clientY;
    GetClientArea(clientX, clientY);

    // Horizontal first
    SCROLLINFO scrollInfo = { };
    scrollInfo.cbSize = sizeof(SCROLLINFO);
    scrollInfo.fMask = SIF_PAGE|SIF_POS|SIF_RANGE;
    scrollInfo.nMin = 0;
    scrollInfo.nMax = scrollRangeX;
    scrollInfo.nPos = posX;
    scrollInfo.nTrackPos = 0;
    scrollInfo.nPage = static_cast<int32>(((FLOAT) clientX / scrollRangeX) * clientX);
    ::SetScrollInfo(hwnd, SB_HORZ, &scrollInfo, true);

    // Then vertical
    scrollInfo.nMax = scrollRangeX;
    scrollInfo.nPos = posY;
    scrollInfo.nPage = static_cast<int32>(((FLOAT) clientY / scrollRangeX) * clientY);
    ::SetScrollInfo(hwnd, SB_VERT, &scrollInfo, true);
}

void Window::SetBorderless(bool borderless_)
{
    if(borderless == borderless_)
        return;

    if(borderless_)
    {
        // Get the desktop coordinates
        POINT point = { };
        HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
        if(monitor == 0)
            return;

        MONITORINFOEX info = { };
        info.cbSize = sizeof(MONITORINFOEX);
        if(GetMonitorInfo(monitor, &info) == 0)
            return;

        int32 monitorWidth = info.rcMonitor.right - info.rcMonitor.left;
        int32 monitorHeight = info.rcMonitor.bottom - info.rcMonitor.top;

        // Save off the current window size
        RECT wrect = { };
        GetWindowRect(hwnd, &wrect);
        nonFSWidth = uint32(wrect.right - wrect.left);
        nonFSHeight = uint32(wrect.bottom - wrect.top);
        nonFSPosX = wrect.left;
        nonFSPosY = wrect.top;
        nonFSStyle = GetWindowLongPtr(hwnd, GWL_STYLE);

        SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        ::SetWindowPos(hwnd, nullptr, info.rcMonitor.left, info.rcMonitor.top, monitorWidth, monitorHeight, 0);
    }
    else
    {
        // Restore the previous window style
        SetWindowLongPtr(hwnd, GWL_STYLE, nonFSStyle);

        // Place the window to the saved position, and resize it
        ::SetWindowPos(hwnd, nullptr, nonFSPosX, nonFSPosY, nonFSWidth, nonFSHeight, 0);
    }

    borderless = borderless_;
}

bool Window::Borderless() const
{
    return borderless;
}

int32 Window::CreateMessageBox(const wchar* message, const wchar* title, uint32 type)
{
    if(title == nullptr)
        return ::MessageBox(hwnd, message, appName.c_str(), type);
    else
        return ::MessageBox(hwnd, message, title, type);
}

void Window::Maximize()
{
    ::ShowWindow( hwnd, SW_MAXIMIZE );
}

void Window::Destroy()
{
    ::DestroyWindow(hwnd);
    ::UnregisterClass(appName.c_str(), hinstance);
}

LRESULT Window::MessageHandler(HWND hWnd, uint32 uMsg, WPARAM wParam, LPARAM lParam)
{
    for(uint64 i = 0; i < messageCallbacks.Count(); ++i)
    {
        Callback callback = messageCallbacks[i];
        MsgFunction msgFunction = callback.Function;
        msgFunction(callback.Context, hWnd, uMsg, wParam, lParam);
    }

    switch(uMsg)
    {
        // Window is being destroyed
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

        // Window is being closed
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            return 0;
        }
    }

    return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT WINAPI Window::WndProc(HWND hWnd, uint32 uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_NCCREATE:
        {
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
            return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
    }

    Window* pObj = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if(pObj)
        return pObj->MessageHandler(hWnd, uMsg, wParam, lParam);
    else
        return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}


void Window::MessageLoop()
{
    // Main message loop:
    MSG msg;

    while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if(!accelTable || !TranslateAccelerator(msg.hwnd, accelTable, &msg))
        {
            ::TranslateMessage( &msg );
            ::DispatchMessage( &msg );
        }
    }
}

void Window::RegisterMessageCallback(MsgFunction msgFunction, void* context)
{
    Callback callback;
    callback.Function = msgFunction;
    callback.Context = context;
    messageCallbacks.Add(callback);
}

}