#pragma once

#include <imgui.h>
#include <Window.h>

class BaseApp
{
public:
    BaseApp(const char* title);
    virtual ~BaseApp();

    void run();
    void quit();

    virtual void update(float deltaTime) = 0;
    virtual void draw() = 0;
    virtual void onResize(int width, int height) {}
    virtual void drawUI() = 0;

    library::Window* window = nullptr;

private:
    bool m_running = false;
};
