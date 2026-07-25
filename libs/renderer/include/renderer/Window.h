#pragma once

#include <cstdint>

#include <Windows.h>

namespace renderer {

// A single Win32 window plus polled input state (keyboard + mouse). No
// cursor hiding/re-centering trickery for this first pass - mouse-look only
// happens while the right mouse button is held (checked each frame via
// isMouseButtonDown(1)), the same "hold to look around" convention common in
// level-editor/scene-view tools. Simpler and more robust to get right on a
// first attempt than a captured/re-centered FPS-style cursor.
class Window {
public:
    Window(int width, int height, const wchar_t* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Drains the Win32 message queue. Returns false once WM_QUIT has been
    // posted (e.g. the window was closed) - the caller's render loop should
    // exit when this returns false.
    bool pumpMessages();

    HWND handle() const { return hwnd_; }
    int width() const { return width_; }
    int height() const { return height_; }

    // Virtual-key code (VK_...) queried via GetAsyncKeyState, gated on this
    // window actually being the foreground/focused window (real bug found
    // live: without this gate, typing in a completely different
    // application that happened to contain one of this tool's single-letter
    // debug toggle keys fired that toggle even with the game window
    // unfocused).
    static bool isKeyDown(int virtualKeyCode);
    // button: 0 = left, 1 = right, 2 = middle. Same foreground-window gate
    // as isKeyDown().
    static bool isMouseButtonDown(int button);

    // Mouse movement (in pixels) since the last call to this function -
    // resets the accumulator each call, so call it exactly once per frame.
    void takeMouseDelta(int& dx, int& dy);

    // Absolute cursor position, in client-area pixels, as of the most
    // recent WM_MOUSEMOVE - unlike takeMouseDelta() this does NOT reset
    // anything (safe to call every frame regardless of whether the mouse
    // actually moved this frame), for screen-to-world ray picking (Phase 17
    // Step 5). (0, 0) with haveMouse_ still false is a real possibility
    // right after window creation, before the first WM_MOUSEMOVE arrives.
    void cursorPosition(int& x, int& y) const;

    // Accumulated WM_MOUSEWHEEL delta (multiples of WHEEL_DELTA == 120, one
    // notch) since the last call - resets to 0 each call, call once per
    // frame. Positive = away from the user (zoom in, by convention).
    int takeMouseWheelDelta();

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool quit_ = false;

    // Updated from WM_MOUSEMOVE inside wndProc(), drained by
    // takeMouseDelta(). Raw screen-space deltas are computed by comparing
    // successive WM_MOUSEMOVE positions - simple and sufficient for a
    // hold-right-mouse-to-look camera (no need for WM_INPUT/raw input at
    // this fidelity level).
    int lastMouseX_ = 0;
    int lastMouseY_ = 0;
    bool haveLastMouse_ = false;
    int accumDx_ = 0;
    int accumDy_ = 0;
    int accumWheelDelta_ = 0;
};

} // namespace renderer
