#include <Window.h>

#include <SDL3/SDL.h>

#include <cstring>
#include <iostream>

namespace library
{

// ===================================================================
// Window — SDL3 window only, no renderer knowledge
// ===================================================================
Window::Window()
{
    initSDL();
}

Window::~Window()
{
    cleanupSDL();
}

void Window::initSDL()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return;
    }

    m_sdlWindow = SDL_CreateWindow("Vulkan Sandbox",
        m_width, m_height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    if (!m_sdlWindow)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
    }
}

void Window::cleanupSDL()
{
    if (m_sdlWindow) { SDL_DestroyWindow(m_sdlWindow); m_sdlWindow = nullptr; }
    SDL_Quit();
}

void Window::setTitle(const char* title)
{
    if (m_sdlWindow)
        SDL_SetWindowTitle(m_sdlWindow, title);
}

bool Window::pollEvents()
{
    if (!m_sdlWindow) return false;

    // Reset per-frame state
    std::memset(m_textInput, 0, kMaxTextInput);
    m_mouseDeltaX = 0;
    m_mouseDeltaY = 0;
    m_mouseWheel = 0;
    for (int i = 0; i < kMaxKeys; ++i)
        m_keysUp[i] = false;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        // Let external consumers (e.g. ImGui) see every event
        if (eventCallback)
            eventCallback(&event);

        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            if (closeEvent) closeEvent();
            return false;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
            int w = event.window.data1;
            int h = event.window.data2;
            if (w == 0 || h == 0)
            {
                m_minimized = true;
                break;
            }
            m_minimized = false;
            m_width = w;
            m_height = h;
            if (resizeEvent)
                resizeEvent(w, h);
            break;
        }

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        {
            int sc = event.key.scancode;
            if (sc >= 0 && sc < kMaxKeys)
            {
                m_keysDown[sc] = event.key.down;
                m_keysUp[sc] = !event.key.down;
            }
            SDL_Keymod mods = SDL_GetModState();
            m_keyShift = (mods & SDL_KMOD_SHIFT) != 0;
            m_keyCtrl  = (mods & SDL_KMOD_CTRL)  != 0;
            m_keyAlt   = (mods & SDL_KMOD_ALT)   != 0;
            m_keySuper = (mods & SDL_KMOD_GUI)   != 0;
            break;
        }

        case SDL_EVENT_TEXT_INPUT:
            std::strncpy(m_textInput, event.text.text, kMaxTextInput - 1);
            m_textInput[kMaxTextInput - 1] = '\0';
            break;

        case SDL_EVENT_MOUSE_MOTION:
            m_mouseX = static_cast<int>(event.motion.x);
            m_mouseY = static_cast<int>(event.motion.y);
            m_mouseDeltaX = static_cast<int>(event.motion.xrel);
            m_mouseDeltaY = static_cast<int>(event.motion.yrel);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            switch (event.button.button)
            {
            case SDL_BUTTON_LEFT:
                m_mouseButtonsDown[static_cast<int>(Button::Left)] = event.button.down;
                break;
            case SDL_BUTTON_RIGHT:
                m_mouseButtonsDown[static_cast<int>(Button::Right)] = event.button.down;
                break;
            case SDL_BUTTON_MIDDLE:
                m_mouseButtonsDown[static_cast<int>(Button::Middle)] = event.button.down;
                break;
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            m_mouseWheel = (event.wheel.x != 0) ? static_cast<int>(event.wheel.x)
                                                : static_cast<int>(event.wheel.y);
            break;

        default:
            break;
        }
    }

    return true;
}

int Window::getWidth() const  { return m_width; }
int Window::getHeight() const { return m_height; }

SDL_Window* Window::getSDLWindow() const { return m_sdlWindow; }

// --- Keyboard ---
bool Window::isKeyDown(Key key) const
{
    int sc = static_cast<int>(key);
    return (sc >= 0 && sc < kMaxKeys) ? m_keysDown[sc] : false;
}

bool Window::isKeyPressed(Key key) const
{
    int sc = static_cast<int>(key);
    return (sc >= 0 && sc < kMaxKeys) ? m_keysUp[sc] : false;
}

bool Window::isKeyShiftDown() const { return m_keyShift; }
bool Window::isKeyCtrlDown()  const { return m_keyCtrl; }
bool Window::isKeyAltDown()   const { return m_keyAlt; }
bool Window::isKeySuperDown() const { return m_keySuper; }
const char* Window::getTextInput() const { return m_textInput; }

// --- Mouse ---
int Window::getMouseX() const       { return m_mouseX; }
int Window::getMouseY() const       { return m_mouseY; }
int Window::getMouseDeltaX() const  { return m_mouseDeltaX; }
int Window::getMouseDeltaY() const  { return m_mouseDeltaY; }
int Window::getMouseWheel() const   { return m_mouseWheel; }

bool Window::isMouseButtonDown(Button button) const
{
    int idx = static_cast<int>(button);
    return (idx >= 0 && idx < kMaxMouseButtons) ? m_mouseButtonsDown[idx] : false;
}

void Window::showMouse(bool show)
{
    if (!m_sdlWindow) return;
    if (show)
    {
        SDL_ShowCursor();
        SDL_SetWindowRelativeMouseMode(m_sdlWindow, false);
    }
    else
    {
        SDL_HideCursor();
        SDL_SetWindowRelativeMouseMode(m_sdlWindow, true);
    }
}

} // namespace library
