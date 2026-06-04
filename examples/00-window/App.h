#pragma once

#include "../shared/BaseApp.h"

using namespace library;

class App : public BaseApp
{
    public:
        App();
        ~App() = default;

        void update(float deltaTime) override;
        void draw() override;
        void drawUI() override;
        void onResize(int width, int height) override;

    private:
};
