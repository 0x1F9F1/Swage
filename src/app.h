#pragma once

namespace Swage
{
    class Application
    {
    public:
        Application();

        virtual ~Application() = 0;

        virtual i32 Init() = 0;
        virtual bool Update() = 0;
        virtual i32 Shutdown() = 0;

        static Application* Instance();

    private:
        static Application* instance_;
    };

    inline Application* Application::Instance()
    {
        return instance_;
    }
} // namespace Swage
