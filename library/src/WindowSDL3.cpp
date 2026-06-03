#include <Window.h>

#include <SDL3/SDL.h>

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

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            if (closeEvent) closeEvent();
            return false;
        }
        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        {
            if (resizeEvent)
            {
                int w, h;
                SDL_GetWindowSizeInPixels(m_sdlWindow, &w, &h);
                resizeEvent(w, h);
            }
        }
    }

    return true;
}

int Window::getWidth() const  { return m_width; }
int Window::getHeight() const { return m_height; }

SDL_Window* Window::getSDLWindow() const { return m_sdlWindow; }

} // namespace library
