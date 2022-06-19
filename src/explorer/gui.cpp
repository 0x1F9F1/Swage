#include "gui.h"

#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

namespace Swage::Gui
{
    static SDL_Window* g_Window;
    static SDL_Renderer* g_Renderer;

    void StyleColorsSolarized(bool dark);

    void Init(SDL_Window* window)
    {
        g_Window = window;
        g_Renderer = SDL_GetRenderer(window);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::GetIO().IniFilename = nullptr;

        StyleColorsSolarized(true);

        ImGui_ImplSDL2_InitForSDLRenderer(g_Window, g_Renderer);
        ImGui_ImplSDLRenderer_Init(g_Renderer);
    }

    void Shutdown()
    {
        ImGui_ImplSDLRenderer_Shutdown();
        ImGui_ImplSDL2_Shutdown();
    }

    void BeginFrame()
    {
        ImGui_ImplSDLRenderer_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void EndFrame()
    {
        ImGui::Render();

        SDL_SetRenderDrawColor(g_Renderer, 0, 0, 0, 0);
        SDL_RenderClear(g_Renderer);

        ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(g_Renderer);
    }

    bool ProcessEvent(const SDL_Event* event)
    {
        return ImGui_ImplSDL2_ProcessEvent(event);
    }

    void StyleColorsSolarized(bool dark)
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.Alpha = 1.0f;
        style.GrabMinSize = 30.0f;

        style.WindowRounding = style.ChildRounding = style.PopupRounding = style.FrameRounding =
            style.ScrollbarRounding = style.GrabRounding = style.TabRounding = 0.0f;

        style.ChildBorderSize = style.PopupBorderSize = style.FrameBorderSize = style.TabBorderSize = 1.0f;
        style.WindowBorderSize = 0.0f;

        const ImColor* base = dark ? Solarized::BaseDark : Solarized::BaseLight;

        style.Colors[ImGuiCol_Text] = base[7];

        ImVec4 text_bg = base[2];
        text_bg.w = 0.6f;

        style.Colors[ImGuiCol_TextSelectedBg] = text_bg;

        style.Colors[ImGuiCol_WindowBg] = base[0];

        style.Colors[ImGuiCol_Border] = base[3];

        style.Colors[ImGuiCol_TitleBg] = base[0];
        style.Colors[ImGuiCol_TitleBgCollapsed] = base[0];
        style.Colors[ImGuiCol_TitleBgActive] = base[1];

        style.Colors[ImGuiCol_Header] = base[1];

        style.Colors[ImGuiCol_HeaderHovered] = base[2];
        style.Colors[ImGuiCol_HeaderActive] = base[3];

        style.Colors[ImGuiCol_CheckMark] = base[5];
        style.Colors[ImGuiCol_CheckMark] = Solarized::Orange;

        style.Colors[ImGuiCol_FrameBg] = base[0];
        style.Colors[ImGuiCol_FrameBgActive] = base[0];
        style.Colors[ImGuiCol_FrameBgHovered] = base[1];

        style.Colors[ImGuiCol_Button] = base[1];
        style.Colors[ImGuiCol_ButtonHovered] = base[2];
        style.Colors[ImGuiCol_ButtonActive] = base[1];

        style.Colors[ImGuiCol_ScrollbarGrab] = base[1];
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = base[3];
        style.Colors[ImGuiCol_ScrollbarGrabActive] = base[2];

        style.Colors[ImGuiCol_ResizeGrip] = base[2];
        style.Colors[ImGuiCol_ResizeGripActive] = base[3];
        style.Colors[ImGuiCol_ResizeGripHovered] = base[4];

        style.Colors[ImGuiCol_PopupBg] = base[1];
    }
} // namespace Swage::Gui
