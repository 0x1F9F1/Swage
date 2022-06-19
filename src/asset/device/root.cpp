#include "root.h"

#include "../stream.h"

namespace Swage
{
    RootFileDevice::~RootFileDevice()
    {
        while (!mounts_.empty())
        {
            mounts_.pop_back();
        }
    }

    Rc<Stream> RootFileDevice::Open(StringView path, bool read_only)
    {
        Rc<Stream> result;

        VisitMountedDevices(
            path, read_only, [&result, read_only](const Rc<FileDevice>& device, StringView sub_path) -> bool {
                result = device->Open(sub_path, read_only);

                return result != nullptr;
            });

        return result;
    }

    Rc<Stream> RootFileDevice::Create(StringView path, bool write_only, bool truncate)
    {
        Rc<Stream> result;

        VisitMountedDevices(
            path, false, [&result, write_only, truncate](const Rc<FileDevice>& device, StringView sub_path) -> bool {
                result = device->Create(sub_path, write_only, truncate);

                return result != nullptr;
            });

        return result;
    }

    bool RootFileDevice::Exists(StringView path)
    {
        return VisitMountedDevices(path, true,
            [](const Rc<FileDevice>& device, StringView sub_path) -> bool { return device->Exists(sub_path); });
    }

    void RootFileDevice::AddMount(StringView prefix, bool read_only, Rc<FileDevice> device)
    {
        auto find = std::find_if(
            mounts_.begin(), mounts_.end(), [&prefix](const Mount& mount) { return mount.Prefix == prefix; });

        if (find == mounts_.end())
        {
            find = mounts_.emplace(mounts_.end(), Mount {String(prefix)});
        }

        find->Devices.push_back({read_only, std::move(device)});
    } // namespace Swage

    template <typename T>
    inline bool RootFileDevice::VisitMountedDevices(StringView path, bool read_only, T callback)
    {
        for (auto it1 = mounts_.rbegin(); it1 != mounts_.rend(); ++it1)
        {
            if (StartsWith(path, it1->Prefix))
            {
                StringView rel_path = path.substr(it1->Prefix.size());

                for (auto it2 = it1->Devices.rbegin(); it2 != it1->Devices.rend(); ++it2)
                {
                    if (!it2->ReadOnly || read_only)
                        if (callback(it2->Device, rel_path))
                            return true;
                }
            }
        }

        return false;
    }
} // namespace Swage
