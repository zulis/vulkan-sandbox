#pragma once

#include <functional>

struct SDL_Window;

namespace library
{

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
    std::function<void()>             closeEvent;
    std::function<void(int, int)>     resizeEvent;

private:
    void initSDL();
    void cleanupSDL();

    SDL_Window* m_sdlWindow = nullptr;
    int m_width = 1280;
    int m_height = 720;
};

} // namespace library
