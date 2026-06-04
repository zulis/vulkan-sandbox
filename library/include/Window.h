#pragma once

#include <functional>

struct SDL_Window;
union SDL_Event;

namespace library
{

enum class Key
{
    Unknown = 0,
    A = 4, B = 5, C = 6, D = 7, E = 8, F = 9, G = 10,
    H = 11, I = 12, J = 13, K = 14, L = 15, M = 16,
    N = 17, O = 18, P = 19, Q = 20, R = 21, S = 22,
    T = 23, U = 24, V = 25, W = 26, X = 27, Y = 28, Z = 29,
    Number1 = 30, Number2 = 31, Number3 = 32, Number4 = 33,
    Number5 = 34, Number6 = 35, Number7 = 36, Number8 = 37,
    Number9 = 38, Number0 = 39,
    Return = 40, Escape = 41, Backspace = 42, Tab = 43, Space = 44,
    Minus = 45, Equals = 46, LeftBracket = 47, RightBracket = 48,
    Backslash = 49, Semicolon = 51, Apostrophe = 52, Grave = 53,
    Comma = 54, Period = 55, Slash = 56, CapsLock = 57,
    F1 = 58, F2 = 59, F3 = 60, F4 = 61, F5 = 62, F6 = 63,
    F7 = 64, F8 = 65, F9 = 66, F10 = 67, F11 = 68, F12 = 69,
    PrintScreen = 70, ScrollLock = 71, Pause = 72, Insert = 73,
    Home = 74, PageUp = 75, Delete = 76, End = 77, PageDown = 78,
    Right = 79, Left = 80, Down = 81, Up = 82,
    LeftCtrl = 224, LeftShift = 225, LeftAlt = 226, LeftGui = 227,
    RightCtrl = 228, RightShift = 229, RightAlt = 230, RightGui = 231,
};

enum class Button
{
    Left = 0,
    Right = 1,
    Middle = 2
};

class Window
{
public:
    Window();
    ~Window();

    // --- Window management ---
    void setTitle(const char* title);
    bool pollEvents();

    int getWidth() const;
    int getHeight() const;

    // --- Platform interop ---
    SDL_Window* getSDLWindow() const;

    // --- Callbacks ---
    std::function<void()>                       closeEvent;
    std::function<void(int, int)>               resizeEvent;
    std::function<void(const SDL_Event*)>       eventCallback;

    // --- Keyboard ---
    bool isKeyDown(Key key) const;
    bool isKeyPressed(Key key) const;
    bool isKeyShiftDown() const;
    bool isKeyCtrlDown() const;
    bool isKeyAltDown() const;
    bool isKeySuperDown() const;
    const char* getTextInput() const;

    // --- Mouse ---
    int  getMouseX() const;
    int  getMouseY() const;
    int  getMouseDeltaX() const;
    int  getMouseDeltaY() const;
    int  getMouseWheel() const;
    bool isMouseButtonDown(Button button) const;

    void showMouse(bool show = true);

private:
    void initSDL();
    void cleanupSDL();

    SDL_Window* m_sdlWindow = nullptr;
    int m_width = 1280;
    int m_height = 720;
    bool m_minimized = false;

    // Keyboard state
    static constexpr int kMaxKeys = 512;
    static constexpr int kMaxTextInput = 32;
    bool m_keysDown[kMaxKeys]{};
    bool m_keysUp[kMaxKeys]{};
    char m_textInput[kMaxTextInput]{};
    bool m_keyShift = false;
    bool m_keyCtrl = false;
    bool m_keyAlt = false;
    bool m_keySuper = false;

    // Mouse state
    static constexpr int kMaxMouseButtons = 3;
    bool m_mouseButtonsDown[kMaxMouseButtons]{};
    int  m_mouseX = 0;
    int  m_mouseY = 0;
    int  m_mouseDeltaX = 0;
    int  m_mouseDeltaY = 0;
    int  m_mouseWheel = 0;
};

} // namespace library
