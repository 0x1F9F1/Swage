#include "assetmanager.h"

#include "swage.h"

#include "filedevice.h"
#include "stream.h"

#include "device/local.h"
#include "device/relative.h"
#include "device/root.h"

#include "stream/winapi.h"

namespace Swage
{
    static String s_BasePath;
    static String s_PrefPath;

    static Rc<RootFileDevice> s_RootDevice;

    void AssetManager::Init()
    {
        s_BasePath = Win32GetBasePath();
        s_PrefPath = Win32GetPrefPath(GetApplicationName());

        SwAssert(!s_BasePath.empty() && s_BasePath.back() == '/');
        SwAssert(!s_PrefPath.empty() && s_PrefPath.back() == '/');

        SwLogInfo("Base Path: {}", s_BasePath);
        SwLogInfo("Pref Path: {}", s_PrefPath);

        s_RootDevice = swref RootFileDevice();

        const auto mount_local = [](StringView prefix, bool read_only, StringView path) {
            Mount(String(prefix), read_only, swref RelativeFileDevice(String(path), LocalFiles()));
        };

#if defined(SWAGE_ASSET_DIR) && !defined(SWAGE_RELEASE)
        mount_local("", false, SWAGE_ASSET_DIR);
#endif

        mount_local("", true, s_BasePath);
        mount_local("user:/", false, s_PrefPath);

        std::atexit(AssetManager::Shutdown);
    }

    void AssetManager::Mount(String prefix, bool read_only, Rc<FileDevice> device)
    {
        s_RootDevice->AddMount(std::move(prefix), read_only, std::move(device));
    }

    void AssetManager::Shutdown()
    {
        s_RootDevice.reset();
    }

    StringView AssetManager::BasePath()
    {
        return s_BasePath;
    }

    StringView AssetManager::PrefPath()
    {
        return s_PrefPath;
    }

    Rc<Stream> AssetManager::Open(StringView path, bool read_only)
    {
        return s_RootDevice->Open(path, read_only);
    }

    Rc<Stream> AssetManager::Create(StringView path, bool write_only, bool truncate)
    {
        return s_RootDevice->Create(path, write_only, truncate);
    }

    bool AssetManager::Exists(StringView path)
    {
        return s_RootDevice->Exists(path);
    }
} // namespace Swage
