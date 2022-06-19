#pragma once

#include <SDL.h>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace Swage::Gui
{
    void Init(SDL_Window* window);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    bool ProcessEvent(const SDL_Event* event);
    void StyleColorsSolarized(bool dark);
} // namespace Swage::Gui

// https://ethanschoonover.com/solarized/
namespace Solarized
{
    const ImColor Base03 {0, 43, 54};
    const ImColor Base02 {7, 54, 66};
    const ImColor Base01 {88, 110, 117};
    const ImColor Base00 {101, 123, 131};
    const ImColor Base0 {131, 148, 150};
    const ImColor Base1 {147, 161, 161};
    const ImColor Base2 {238, 232, 213};
    const ImColor Base3 {253, 246, 227};

    const ImColor BaseDark[8] {Base03, Base02, Base01, Base00, Base0, Base1, Base2, Base3};
    const ImColor BaseLight[8] {Base3, Base2, Base1, Base0, Base00, Base01, Base02, Base03};

    const ImColor Yellow {181, 137, 0};
    const ImColor Orange {203, 75, 22};
    const ImColor Red {220, 50, 47};
    const ImColor Magenta {211, 54, 130};
    const ImColor Violet {108, 113, 196};
    const ImColor Blue {38, 139, 210};
    const ImColor Cyan {42, 161, 152};
    const ImColor Green {133, 153, 0};
} // namespace Solarized