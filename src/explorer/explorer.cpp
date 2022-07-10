#include <SDL.h>

#include "swage.h"

#include "asset/assetmanager.h"
#include "asset/device/local.h"
#include "asset/device/zip.h"
#include "asset/glob.h"
#include "asset/stream.h"
#include "asset/stream/buffered.h"
#include "asset/stream/null.h"
#include "asset/stream/resource.h"
#include "asset/stream/winapi.h"

#include "games/angel/ares.h"
#include "games/angel/dave.h"

#include "games/bohemia/pak.h"
#include "games/bohemia/pbo.h"

#include "games/rage/rpf0.h"
#include "games/rage/rpf2.h"
#include "games/rage/rpf7.h"
#include "games/rage/rpf8.h"
#include "games/rage/rsc.h"

#include "crypto/secret.h"

#include "core/parallel.h"
#include "gui.h"
#include "resource.h"

#include <yaml-cpp/yaml.h>

#include <lua.hpp>

using namespace Swage;

void SearchForKeys();

Rc<FileDevice> LoadArchive(StringView name, Rc<Stream> handle)
{
    if (u32 magic = 0; handle->TryReadBulk(&magic, sizeof(magic), 0))
    {
        switch (magic)
        {
            case 0x30465052: return Rage::LoadRPF0(std::move(handle));
            case 0x32465052: return Rage::LoadRPF2(std::move(handle));

            case 0x52504637:
            case 0x37465052: return Rage::LoadRPF7(std::move(handle), name);

            case 0x52504638: return Rage::LoadRPF8(std::move(handle));

            case 0x53455241: return Angel::LoadARES(std::move(handle));

            case 0x45564144:
            case 0x65766144: return Angel::LoadDave(std::move(handle));

            case 0x02014B50:
            case 0x04034B50: return LoadZip(std::move(handle));

            case 0x4D524F46: return Bohemia::LoadPAK(std::move(handle));
        }
    }

    if (EndsWithI(name, ".zip") || EndsWithI(name, ".jar"))
        return LoadZip(std::move(handle));

    if (EndsWithI(name, ".pbo"))
        return Bohemia::LoadPBO(std::move(handle));

    return nullptr;
}

#include <stack>

struct MountedDevice
{
    Rc<FileDevice> Device;
    String Path;
    String Filter;
};

static Vec<MountedDevice> g_Devices;

static bool g_NeedsReload = true;

static Rc<FileDevice> g_CurrentDevice;
static String g_CurrentPath;
static String g_CurrentFilter;

static void UpdateFromCurrentMount()
{
    auto [device, path, filter] = g_Devices.back();
    g_CurrentDevice = device;
    g_CurrentPath = path;
    g_CurrentFilter = filter;

    g_NeedsReload = true;
}

static String g_CurrentDirectory;
static Vec<FolderEntry> g_CurrentEntries;
static usize g_CurrentFiles = 0;
static usize g_CurrentFolders = 0;

static String g_ExtractPath;
static String g_CurrentScriptName;

static bool g_AddResourceFileHeader = true;
static bool g_CreateShipList = false;

static lua_State* g_LuaState {};

static String SanitizeName(StringView name, bool full = false)
{
    String result;
    result.reserve(name.size());

    for (unsigned char c : name)
    {
        if (c <= 0x1F || c >= 0x7F || (full && std::strchr("<>:\"\\|?*", c)))
        {
            fmt::format_to(std::back_inserter(result), "${:02X}", c);
        }
        else
        {
            result += c;
        }
    }

    return result;
}

bool ExtractFile(
    const Rc<FileDevice>& device, StringView directory, StringView name, StringView output_name, bool default_to_bin)
{
    String input_path = Concat(directory, name);

    try
    {
        Rc<Stream> handle = device->Open(input_path, true);

        if (handle == nullptr)
            return false;

        Rc<Stream> output;

        if (PathCompareEqual(g_ExtractPath, "/dev/null"))
        {
            output = MakeRc<NullStream>();
        }
        else
        {
            String output_path = SanitizeName(output_name, true);
            output_path.insert(0, g_ExtractPath);

            output = LocalFiles()->Create(output_path, true, true);

            if (output == nullptr && default_to_bin)
                output = LocalFiles()->Create(g_ExtractPath + "output.bin", true, true);
        }

        if (output == nullptr)
            return false;

        if (g_AddResourceFileHeader)
        {
            Rage::datResourceFileHeader rsc_header;

            if (device->Extension(input_path, &rsc_header))
            {
                output->Write(&rsc_header, sizeof(rsc_header));
            }
        }

        handle->Rewind();

        if (i64 copied = handle->CopyTo(*output); copied != handle->Size())
            throw std::runtime_error("Could not completely copy file");
    }
    catch (const std::exception& ex)
    {
        SwLogError("Failed to extract file '{}' to '{}': {}", input_path, output_name, ex.what());

        return false;
    }

    return true;
}

bool ExtractFile(const Rc<FileDevice>& device, StringView directory, StringView name, bool default_to_bin)
{
    return ExtractFile(device, directory, name, name, default_to_bin);
}

bool PushMount(Rc<Stream> handle, StringView name)
{
    try
    {
        if (Rc<FileDevice> archive = LoadArchive(name, std::move(handle)))
        {
            g_Devices.push_back({std::move(archive), "", ""});
            UpdateFromCurrentMount();
            return true;
        }
        else
        {
            SwLogError("{}: Unrecognized archive type", name);
        }
    }
    catch (const std::exception& ex)
    {
        SwLogError("{}: {}", name, ex.what());
    }

    return false;
}

static bool PopMount()
{
    if (g_Devices.size() > 1)
    {
        g_Devices.pop_back();
        UpdateFromCurrentMount();
        return true;
    }

    return false;
}

static bool PopDirectory()
{
    if (!g_CurrentFilter.empty())
    {
        g_CurrentFilter.clear();
        g_NeedsReload = true;
        return true;
    }

    if (!g_CurrentPath.empty())
    {
        usize split = StringView(g_CurrentPath).substr(0, g_CurrentPath.size() - 1).find_last_of("\\/");
        g_CurrentPath.resize(split + 1);
        g_NeedsReload = true;
        return true;
    }

    return PopMount();
}

String GetBaseExtractPath()
{
    return Concat(AssetManager::PrefPath(), "Extracted/");
}

StringView lua_checkstringv(lua_State* L, int idx)
{
    size_t len = 0;

    if (const char* result = luaL_checklstring(L, idx, &len))
        return StringView {result, len};

    return {};
}

Option<StringView> lua_optstringv(lua_State* L, int idx)
{
    size_t len = 0;

    if (const char* result = lua_tolstring(L, idx, &len))
        return StringView {result, len};

    return None;
}

void lua_pushstringv(lua_State* L, StringView str)
{
    lua_pushlstring(L, str.data(), str.size());
}

static int explorer_log(lua_State* L)
{
    String output;

    int n = lua_gettop(L); /* number of arguments */
    int i;
    lua_getglobal(L, "tostring");
    for (i = 1; i <= n; i++)
    {
        const char* s;
        size_t l;
        lua_pushvalue(L, -1); /* function to be called */
        lua_pushvalue(L, i);  /* value to print */
        lua_call(L, 1, 1);
        s = lua_tolstring(L, -1, &l); /* get result */
        if (s == NULL)
            return luaL_error(L, "'tostring' must return a string to 'print'");
        if (i > 1)
            output += "\t";
        output.append(s, l);
        lua_pop(L, 1); /* pop result */
    }

    SwLogInfo("{}", output);

    return 0;
}

static int explorer_mount(lua_State* L)
{
    bool success = false;

    auto name = lua_checkstringv(L, 1);

    auto& [device, directory, _] = g_Devices.back();

    if (Rc<Stream> handle = device->Open(Concat(directory, name), true))
    {
        success = PushMount(std::move(handle), name);

        if (!success)
        {
            SwLogError("Failed to mount {} - Not an archive", name);
        }
    }
    else
    {
        SwLogError("Failed to mount {} - Does not exist", name);
    }

    lua_pushboolean(L, success);

    return 1;
}

static int explorer_unmount(lua_State* L)
{
    bool success = PopMount();

    lua_pushboolean(L, success);

    return 1;
}

static int explorer_outdir(lua_State* L)
{
    String result = g_ExtractPath;

    if (auto name = lua_optstringv(L, 1))
        g_ExtractPath = *name;

    lua_pushstringv(L, result);

    return 1;
}

static int explorer_curdir(lua_State* L)
{
    auto& [_, directory, _filter] = g_Devices.back();

    if (auto name = lua_optstringv(L, 1))
    {
        directory = *name;
    }

    lua_pushstringv(L, directory);

    return 1;
}

static int explorer_basedir(lua_State* L)
{
    lua_pushstringv(L, GetBaseExtractPath());

    return 1;
}

static int explorer_glob(lua_State* L)
{
    auto filter = lua_checkstringv(L, 1);

    auto& [device, directory, _] = g_Devices.back();

    Ptr<FileEnumerator> find = MakeUnique<FileGlobber>(device, directory, String(filter));

    FolderEntry entry;

    lua_newtable(L);

    for (int i = 1; find->Next(entry); ++i)
    {
        lua_pushstringv(L, entry.Name);
        lua_rawseti(L, -2, i);
    }

    return 1;
}

static int explorer_extract(lua_State* L)
{
    auto input = lua_checkstringv(L, 1);
    auto output = lua_checkstringv(L, 2);

    auto& [device, directory, _] = g_Devices.back();

    bool success = ExtractFile(device, directory, input, output, false);

    lua_pushboolean(L, success);

    return 1;
}

static int explorer_multiextract(lua_State* L)
{
    auto input = lua_checkstringv(L, 1);
    auto output = lua_checkstringv(L, 2);

    if (lua_type(L, 3) != LUA_TTABLE)
        return 0;

    lua_pushnil(L);

    Vec<String> files;

    while (lua_next(L, 3) != 0)
    {
        files.push_back(String(lua_checkstringv(L, -1)));
        lua_pop(L, 1);
    }

    auto device = g_Devices.back().Device;

    parallel_for_each(files.begin(), files.end(), [&device, &input, &output](const auto& file) {
        ExtractFile(device, input, file, Concat(output, file), false);
        return true;
    });

    return 0;
}

static const luaL_Reg explorer_lib[] = {
    {"mount", explorer_mount},
    {"unmount", explorer_unmount},

    {"outdir", explorer_outdir},
    {"curdir", explorer_curdir},
    {"basedir", explorer_basedir},

    {"glob", explorer_glob},

    {"extract", explorer_extract},
    {"multiextract", explorer_multiextract},

    {NULL, NULL},
};

int luaopen_explorer(lua_State* L)
{
    luaL_newlib(L, explorer_lib);

    lua_pushcfunction(L, explorer_log);
    lua_setglobal(L, "print");

    return 1;
}

static int PathInputCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
    {
        if (g_CurrentEntries.size() >= 1)
        {
            if (auto& entry = g_CurrentEntries[0]; entry.IsFolder)
            {
                String path = Concat(g_CurrentDirectory, entry.Name, "/");
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, path.c_str());
                data->ClearSelection();
            }
        }
    }
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
    {
        if (data->EventChar == '\\')
            data->EventChar = '/';
    }

    return 0;
};

void ShowWindow()
{
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));

    if (!ImGui::Begin("File Explorer", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
    {
        ImGui::End();

        return;
    }

    if (g_NeedsReload)
    {
        g_NeedsReload = false;

        auto& device = g_Devices.back();
        g_CurrentDevice = device.Device;
        g_CurrentPath = device.Path;
        g_CurrentFilter = device.Filter;

        auto [dirname, basename] = SplitPath(g_CurrentPath);
        g_CurrentDirectory = dirname;
        g_CurrentEntries.clear();
        g_CurrentFiles = 0;
        g_CurrentFolders = 0;

        if (Ptr<FileEnumerator> find = !g_CurrentFilter.empty()
                ? MakeUnique<FileGlobber>(g_CurrentDevice, String(dirname), g_CurrentFilter)
                : g_CurrentDevice->Enumerate(dirname))
        {
            FolderEntry entry;

            while (g_CurrentEntries.size() < 100000 && find->Next(entry))
            {
                if (g_CurrentFilter.empty() && !StartsWithI(entry.Name, basename))
                    continue;

                ++(entry.IsFolder ? g_CurrentFolders : g_CurrentFiles);

                g_CurrentEntries.push_back(std::move(entry));
            }
        }

        std::sort(g_CurrentEntries.begin(), g_CurrentEntries.end(), [](const FolderEntry& lhs, const FolderEntry& rhs) {
            if (lhs.IsFolder != rhs.IsFolder)
                return lhs.IsFolder > rhs.IsFolder;

            return PathCompareLess(lhs.Name, rhs.Name);
        });
    }

    if (ImGui::Button("Settings"))
    {
        ImGui::OpenPopup("Settings");
    }

    ImGui::SameLine();

    if (ImGui::Button("Extract All"))
    {
        if (g_CreateShipList)
        {
            BufferedStream output(LocalFiles()->Create(Concat(g_ExtractPath, "shiplist.txt"), true, true));

            for (const auto& entry : g_CurrentEntries)
            {
                output.PutString(entry.Name);
                output.PutCh('\n');
            }
        }

        Atomic<usize> num_files = 0;

        parallel_for_each(g_CurrentEntries.begin(), g_CurrentEntries.end(), [&num_files](const auto& entry) {
            if (!entry.IsFolder)
            {
                auto& device = g_Devices.back();
                ExtractFile(device.Device, device.Path, entry.Name, false);
                ++num_files;
            }

            return true;
        });

        SwLogInfo("Extracted {} files", num_files);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##OutputPath", "Output Path", &g_ExtractPath);

    if (ImGui::IsItemDeactivatedAfterEdit() && (!g_ExtractPath.empty() && g_ExtractPath.back() != '/'))
        g_ExtractPath += '/';

    bool execute_script = ImGui::Button("Execute");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Script Path", "Script Path", &g_CurrentScriptName);

    ImGui::Separator();

    if (ImGui::ArrowButton("Parent Directory", ImGuiDir_Up))
        PopDirectory();

    ImGui::SameLine();

    g_NeedsReload |= ImGui::InputTextWithHint("##Path", "Path", &g_CurrentPath,
        ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackCharFilter |
            ImGuiInputTextFlags_NoUndoRedo,
        PathInputCallback);

    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        auto [_, basename] = SplitPath(g_CurrentPath);

        if (g_CurrentEntries.size() == 1 && g_CurrentEntries[0].IsFolder &&
            StringICompareEqual(basename, g_CurrentEntries[0].Name))
        {
            g_CurrentPath += '/';
            g_NeedsReload = true;
        }
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(-1);

    g_NeedsReload |= ImGui::InputTextWithHint("##Filter", "Filter", &g_CurrentFilter);

    ImGui::Text("%zu files, %zu folders (%zu total)", g_CurrentFiles, g_CurrentFolders, g_CurrentEntries.size());

    ImGui::BeginChild("Files");

    ImGui::BeginGroup();

    ImGui::Columns(2, "Files");

    ImGui::Text("Name");
    ImGui::NextColumn();

    ImGui::Text("Size");
    ImGui::NextColumn();

    ImGui::Separator();

    ImGuiListClipper clipper {};
    clipper.Begin((int) g_CurrentEntries.size());

    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const auto& entry = g_CurrentEntries[i];

            ImGui::PushID(i);

            ImGui::PushStyleColor(ImGuiCol_Text, entry.IsFolder ? Solarized::Orange.Value : Solarized::Green.Value);
            ImGui::Text(entry.IsFolder ? "[D]" : "[F]");
            ImGui::PopStyleColor();
            ImGui::SameLine();

            if (ImGui::Selectable(SanitizeName(entry.Name).c_str()))
            {
                if (entry.IsFolder)
                {
                    g_CurrentPath = Concat(g_CurrentDirectory, entry.Name, "/");
                    g_CurrentFilter.clear();
                    g_NeedsReload = true;
                }
                else if (ImGui::GetIO().KeyCtrl)
                {
                    if (ExtractFile(g_CurrentDevice, g_CurrentDirectory, entry.Name, true))
                        SwLogInfo("Extracted {}", entry.Name);
                }
                else if (Rc<Stream> handle = g_CurrentDevice->Open(Concat(g_CurrentDirectory, entry.Name), true))
                {
                    if (PushMount(handle, entry.Name))
                        SwLogInfo("Mounted {}", entry.Name);
                }
            }

            ImGui::NextColumn();

            if (!entry.IsFolder)
            {
                if (entry.Size <= (9999 * 1024))
                    ImGui::Text("%5zu KB", (entry.Size + 0x3FF) >> 10);
                else
                    ImGui::Text("%5zu MB", (entry.Size + 0xFFFFF) >> 20);
            }

            ImGui::NextColumn();

            ImGui::PopID();
        }
    }

    ImGui::Columns(1);

    ImGui::EndGroup();
    ImGui::EndChild();

    if (ImGui::BeginPopup("Settings"))
    {
        ImGui::Checkbox("RSC Header", &g_AddResourceFileHeader);
        ImGui::SameLine();

        ImGui::Checkbox("Ship List", &g_CreateShipList);
        ImGui::SameLine();

        if (ImGui::Button("Find Keys"))
            SearchForKeys();

        ImGui::EndPopup();
    }

    ImGui::End();

    {
        auto& device = g_Devices.back();
        device.Device = g_CurrentDevice;
        device.Path = g_CurrentPath;
        device.Filter = g_CurrentFilter;
    }

    if (execute_script)
    {
        String script_path = Concat("user:/scripts/", g_CurrentScriptName);

        if (Rc<Stream> s = AssetManager::Open(script_path))
        {
            String script_code = s->ReadAllText();
            s = nullptr;

            SwLogInfo("Executing Script {}", g_CurrentScriptName);

            bool error = luaL_loadbuffer(g_LuaState, script_code.data(), script_code.size(), script_code.data()) ||
                lua_pcall(g_LuaState, 0, 0, 0);

            if (error)
            {
                SwLogError("Executing Script {} Failed:\n{}", g_CurrentScriptName, lua_tostring(g_LuaState, -1));

                lua_pop(g_LuaState, 1); /* pop error message from the stack */
            }
        }
        else
        {
            SwLogInfo("Script {} not found", g_CurrentScriptName);
        }
    }
}

void LoadKeys()
{
    Rage::RPF7::LoadKeys();
    Rage::RPF8::LoadKeys();
}

void LoadLegacyKeys()
{
    Vec<u8> data;

    const auto add_secrets = [&data](Stream& s, usize count, usize size) {
        data.resize(size);

        for (usize i = 0; i < count; ++i)
        {
            if (s.TryRead(data.data(), ByteSize(data)))
                Secrets.Add(data);
        }
    };

    if (Rc<Stream> s = AssetManager::Open("user:/gtav_pc_keys.bin"); s && s->Size() == 0x1CE40)
    {
        add_secrets(*s, 101, 0x140); // Keys
        add_secrets(*s, 84, 0x400);  // Tables
    }

    if (Rc<Stream> s = AssetManager::Open("user:/gtav_ps4_keys.bin"); s && s->Size() == 0xCA0)
        add_secrets(*s, 101, 0x20);

    if (Rc<Stream> s = AssetManager::Open("user:/gtav_ps3_keys.bin"); s && s->Size() == 0x20)
        add_secrets(*s, 1, 0x20);

    if (Rc<Stream> s = AssetManager::Open("user:/gtav_360_keys.bin"); s && s->Size() == 0x20)
        add_secrets(*s, 1, 0x20);

    if (Rc<Stream> s = AssetManager::Open("user:/launcher_keys.bin"); s && s->Size() == 0x20)
        add_secrets(*s, 1, 0x20);

    if (Rc<Stream> s = AssetManager::Open("user:/rage_default_keys.bin"); s && s->Size() == 0x20)
        add_secrets(*s, 1, 0x20);

    if (Rc<Stream> s = AssetManager::Open("user:/rdr2_android_keys.bin"); s && s->Size() == 0x21D10)
    {
        add_secrets(*s, 164, 0x140); // Keys
        add_secrets(*s, 84, 0x400);  // Tables
        add_secrets(*s, 1, 0x10);    // IV
    }

    if (Rc<Stream> s = AssetManager::Open("user:/rdr2_ps4_keys.bin"); s && s->Size() == 0x21D10)
    {
        add_secrets(*s, 164, 0x140); // Keys
        add_secrets(*s, 84, 0x400);  // Tables
        add_secrets(*s, 1, 0x10);    // IV
    }
}

void SearchForKeys()
{
    LoadLegacyKeys();

    if (Rc<Stream> s = AssetManager::Open("user:/extra_secrets.bin"))
        Secrets.Load(*s);

    String temp_path = Win32PathExpandEnvStrings("%TEMP%/");

    const auto open_proc = [&](StringView name, StringView wnd_class) -> Rc<Stream> {
        // Check for a dump in the user directory
        if (Rc<Stream> result = AssetManager::Open(Concat("user:/", name, ".dmp")))
            return result;

        // Check for a dump in TEMP
        if (Rc<Stream> result = Win32FileOpen(Concat(temp_path, name, ".dmp"), true))
            return result;

        // Check for a running process
        // TODO: Validate process exe name
        if (u32 proc_id = Win32ProcIdFromWindow(NULL, String(wnd_class).c_str()))
        {
            if (Rc<Stream> result = Win32ProcOpenStream(proc_id))
                return result;
        }

        return nullptr;
    };

    if (Rc<Stream> stream = open_proc("GTA5", "grcWindow"))
    {
        SwLogInfo("Searching for GTA5 PC Keys...");

        SecretFinder finder;
        Rage::RPF7::FindKeys_GTA5_PC(finder);

        if (auto found = finder.Search(*stream); found.size() == 101 + 84)
        {
            SwLogInfo("Found GTA5 PC keys");

            Secrets.Add(found);
        }
        else
        {
            SwLogError("Failed to find GTA5 PC keys");
        }
    }

    if (Rc<Stream> stream = open_proc("RDR2", "sgaWindow"))
    {
        SwLogInfo("Searching for RDR2 PC Keys...");

        SecretFinder finder;
        Rage::RPF8::FindKeys_RDR2_PC(finder);

        auto found = finder.Search(*stream);

        SwLogInfo("Found {}", found.size());

        if (found.size() >= 654)
        {
            SwLogInfo("Found RDR2 PC keys");

            Secrets.Add(found);
        }
        else
        {
            SwLogError("Failed to find RDR2 PC keys");
        }
    }

    if (Rc<Stream> stream = AssetManager::Open("user:/daemon.dmp"))
    {
        SwLogInfo("Searching for RDR2 PS4 Keys...");

        SecretFinder finder;
        Rage::RPF8::FindKeys_RDR2_PS4(finder);

        if (auto found = finder.Search(*stream); found.size() == 163 + 84 + 1)
        {
            SwLogInfo("Found RDR2 PS4 keys");

            Secrets.Add(found);
        }
        else
        {
            SwLogError("Failed to find RDR2 PS4 keys");
        }
    }

    LoadKeys();

    if (Rc<Stream> s = AssetManager::Create("user:/secrets.bin"))
        Secrets.Save(*s);
}

class ArchiveExplorerApplication final : public Application
{
public:
    i32 Init() override;
    bool Update() override;
    i32 Shutdown() override;

private:
    SDL_Window* window_ {};
    SDL_Renderer* renderer_ {};
    i32 poll_frames_ {};
};

Ptr<Application> Swage::CreateApplication()
{
    return MakeUnique<ArchiveExplorerApplication>();
}

const char* Swage::GetApplicationName()
{
    return "ArchiveExplorer";
}

i32 ArchiveExplorerApplication::Init()
{
    String game_dir = "";

    if (Rc<Stream> s = AssetManager::Open("user:/config.yaml"))
    {
        YAML::Node config = YAML::Load(s->ReadAllText());

        game_dir = config["lastdir"].as<String>("");
        g_CurrentScriptName = config["lastscript"].as<String>("");
    }

    AssetManager::Mount("", true, LoadZip(MakeRc<ResourceStream>(nullptr, IDR_RESOURCES_ZIP, "ZIP")));

    if (Rc<Stream> s = AssetManager::Open("user:/secrets.bin"))
        Secrets.Load(*s);

    LoadKeys();

    if (Rc<Stream> s = AssetManager::Create("user:/secrets.bin"))
        Secrets.Save(*s);

    if (Rc<Stream> s = AssetManager::Open("user:/rdr2_files.txt"))
    {
        BufferedStream reader(s);

        Rage::RPF8::LoadFileList(reader);
    }

    if (Rc<Stream> s = AssetManager::Open("user:/rdr2_possible_files.txt"))
    {
        BufferedStream reader(s);

        Rage::RPF8::LoadPossibleFileList(reader);
    }

    window_ = SDL_CreateWindow("Archive Explorer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 960,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

    if (window_ == nullptr)
    {
        SwLogError("Failed to create window: %s", SDL_GetError());
        return -1;
    }

    // TODO: Disable vsync when using WaitEvent?
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_PRESENTVSYNC);

    if (renderer_ == nullptr)
    {
        SwLogError("Failed to create renderer: %s", SDL_GetError());
        return -1;
    }

    Gui::Init(window_);

    g_ExtractPath = GetBaseExtractPath();
    g_Devices.push_back({LocalFiles(), game_dir, ""});
    UpdateFromCurrentMount();

    g_LuaState = luaL_newstate();
    lua_atpanic(g_LuaState, [](lua_State* L) -> int { SwLogFatal("LUA PANIC: {}", lua_tostring(L, -1)); });

    luaL_openlibs(g_LuaState);
    luaL_requiref(g_LuaState, "explorer", luaopen_explorer, 1);

    return 0;
}

bool ArchiveExplorerApplication::Update()
{
    bool running = true;

    SDL_Event event;

    for (bool b = (poll_frames_ ? SDL_PollEvent : SDL_WaitEvent)(&event); b; b = SDL_PollEvent(&event))
    {
        Gui::ProcessEvent(&event);

        // Poll and render for at least 3 frames after any updates.
        poll_frames_ = 3;

        if (event.type == SDL_QUIT)
            running = false;
    }

    if (poll_frames_)
        --poll_frames_;

    if (running)
    {
        Gui::BeginFrame();
        ShowWindow();
        Gui::EndFrame();
    }

    return running;
}

i32 ArchiveExplorerApplication::Shutdown()
{
    if (g_LuaState)
    {
        lua_close(g_LuaState);
        g_LuaState = nullptr;
    }

    if (Rc<Stream> s = AssetManager::Create("user:/rdr2_files.txt"))
    {
        BufferedStream writer(s);

        Rage::RPF8::SaveFileList(writer);
    }

    if (Rc<Stream> s = AssetManager::Create("user:/config.yaml"))
    {
        YAML::Node config;

        config["lastdir"] = g_Devices.front().Path;
        config["lastscript"] = g_CurrentScriptName;

        YAML::Emitter emitter;
        emitter << config;
        s->Write(emitter.c_str(), emitter.size());
    }

    g_Devices.clear();

    Gui::Shutdown();

    if (renderer_)
    {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }

    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    return 0;
}