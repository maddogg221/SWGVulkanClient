#include "renderer/Window.h"

#include <stdexcept>

#include <windowsx.h> // GET_X_LPARAM / GET_Y_LPARAM

namespace renderer {

namespace {
constexpr wchar_t kWindowClassName[] = L"SwgVisualizerWindowClass";

// Real bug found live: isKeyDown()/isMouseButtonDown() are static (queried
// from all over the codebase without a Window instance in hand) and used
// GetAsyncKeyState() directly, which is a SYSTEM-WIDE key state, not scoped
// to this window having focus - typing a message in a completely different
// application (this tool's own AI chat client, in the real case that found
// this) that happened to contain one of this project's single-letter debug
// toggle keys (e.g. 'R') fired that toggle even though the game window
// never had focus, corrupting an in-progress live test. Tracked here as a
// plain HWND (this tool only ever constructs one Window - see the class's
// own header comment) rather than adding it to the Window instance itself,
// since isKeyDown() is called as a static from many places with no Window&
// available.
HWND g_focusHwnd = nullptr;
} // namespace

LRESULT CALLBACK Window::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return 0;
        }
        case WM_DESTROY:
            if (self != nullptr) {
                self->quit_ = true;
            }
            PostQuitMessage(0);
            return 0;
        case WM_MOUSEMOVE: {
            if (self != nullptr) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                if (self->haveLastMouse_) {
                    self->accumDx_ += (x - self->lastMouseX_);
                    self->accumDy_ += (y - self->lastMouseY_);
                }
                self->lastMouseX_ = x;
                self->lastMouseY_ = y;
                self->haveLastMouse_ = true;
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            if (self != nullptr) {
                self->accumWheelDelta_ += GET_WHEEL_DELTA_WPARAM(wParam);
            }
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

Window::Window(int width, int height, const wchar_t* title) : width_(width), height_(height) {
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = &Window::wndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    // Registering twice (e.g. a second Window instance in the same process)
    // is harmless - RegisterClassExW fails with ERROR_CLASS_ALREADY_EXISTS,
    // which this tool only ever constructs one Window so never hits, but
    // ignoring the return value here rather than checking it keeps this
    // constructor robust either way.
    RegisterClassExW(&wc);

    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowExW(0, kWindowClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                             CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr,
                             nullptr, instance, this);
    if (hwnd_ == nullptr) {
        throw std::runtime_error("Window: CreateWindowExW failed");
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    g_focusHwnd = hwnd_;
}

Window::~Window() {
    if (g_focusHwnd == hwnd_) {
        g_focusHwnd = nullptr;
    }
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
    }
    UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
}

bool Window::pumpMessages() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            quit_ = true;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !quit_;
}

bool Window::isKeyDown(int virtualKeyCode) {
    if (GetForegroundWindow() != g_focusHwnd) return false;
    return (GetAsyncKeyState(virtualKeyCode) & 0x8000) != 0;
}

bool Window::isMouseButtonDown(int button) {
    if (GetForegroundWindow() != g_focusHwnd) return false;
    int vk = (button == 0) ? VK_LBUTTON : (button == 1) ? VK_RBUTTON : VK_MBUTTON;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void Window::takeMouseDelta(int& dx, int& dy) {
    dx = accumDx_;
    dy = accumDy_;
    accumDx_ = 0;
    accumDy_ = 0;
}

void Window::cursorPosition(int& x, int& y) const {
    x = lastMouseX_;
    y = lastMouseY_;
}

int Window::takeMouseWheelDelta() {
    int delta = accumWheelDelta_;
    accumWheelDelta_ = 0;
    return delta;
}

} // namespace renderer
