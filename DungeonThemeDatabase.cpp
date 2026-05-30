#include "DungeonThemeDatabase.h"
#include "JsonIO.h"
#include "external/json.hpp"
#include <algorithm>
#include <filesystem>

using json = nlohmann::json;

namespace
{
    constexpr const char* kThemeDirectoryPath = "DungeonData\\DungeonThemeSettings\\Theme";
    constexpr const char* kBgmDirectoryPath = "DungeonData\\DungeonThemeSettings\\BGM";
    constexpr const char* kMapModelDirectoryPath = "DungeonData\\DungeonThemeSettings\\MapModel";
    constexpr const char* kLegacyThemeConfigPath = "DungeonData\\TileThemeSettings.json";
    constexpr const char* kDefaultThemeId = "default";

    std::vector<DungeonTileTypeSetting> g_TileTypes;
    std::vector<DungeonThemeData> g_Themes;
    bool g_Loaded = false;

    void EnsureThemeDirectories()
    {
        std::filesystem::create_directories(kThemeDirectoryPath);
        std::filesystem::create_directories(kBgmDirectoryPath);
        std::filesystem::create_directories(kMapModelDirectoryPath);
    }

    DungeonThemeTileModels MakeDefaultModels()
    {
        DungeonThemeTileModels models;
        models.floor = "Asset\\Model\\Floor.obj";
        models.wall = "Asset\\Model\\Wall.obj";
        return models;
    }

    DungeonThemeData MakeDefaultTheme()
    {
        DungeonThemeData theme;
        theme.id = kDefaultThemeId;
        theme.displayName = "Default";
        theme.bgmPath = "Asset\\Sound\\tsumetaidaichi.wav";
        theme.models = MakeDefaultModels();
        return theme;
    }

    std::vector<DungeonTileTypeSetting> MakeDefaultTileTypes()
    {
        return
        {
            { "Floor", "Floor", "floor" },
            { "Wall", "Wall", "wall" },
            { "Stair", "Stair", "stair" },
            { "Corridor", "Corridor", "corridor" },
            { "EditorCorridor", "Editor Corridor", "editorCorridor" },
            { "ShopFloor", "Shop Floor", "shopFloor" },
        };
    }

    std::string ReadString(const json& source, const char* key, const std::string& fallback)
    {
        if (!source.contains(key) || !source[key].is_string())
            return fallback;
        return source[key].get<std::string>();
    }

    void FillMissingThemeValues(DungeonThemeData& theme)
    {
        const DungeonThemeData defaults = MakeDefaultTheme();

        if (theme.id.empty()) theme.id = defaults.id;
        if (theme.displayName.empty()) theme.displayName = theme.id;
        if (theme.bgmPath.empty()) theme.bgmPath = defaults.bgmPath;

        // テーマJSONに一部モデルだけを書いた場合も、足りない分は既存モデルで補う。
        if (theme.models.floor.empty()) theme.models.floor = defaults.models.floor;
        if (theme.models.wall.empty()) theme.models.wall = defaults.models.wall;
    }

    void LoadTileTypesFromJson(const json& root)
    {
        if (!root.contains("tileTypes") || !root["tileTypes"].is_array())
            return;

        g_TileTypes.clear();
        for (const auto& tileJson : root["tileTypes"])
        {
            DungeonTileTypeSetting tile;
            tile.id = tileJson.value("id", "");
            tile.label = tileJson.value("label", tile.id);
            tile.modelKey = tileJson.value("modelKey", "");
            if (!tile.id.empty() && !tile.modelKey.empty())
                g_TileTypes.push_back(tile);
        }
    }

    bool ParseThemeJson(const json& themeJson, DungeonThemeData& outTheme)
    {
        outTheme = DungeonThemeData();
        outTheme.id = themeJson.value("id", "");
        outTheme.displayName = themeJson.value("name", outTheme.id);
        outTheme.bgmPath = themeJson.value("bgm", "");

        const json emptyModels = json::object();
        const json& models = themeJson.contains("models") ? themeJson["models"] : emptyModels;
        outTheme.models.floor = ReadString(models, "floor", "");
        outTheme.models.wall = ReadString(models, "wall", "");

        if (outTheme.id.empty())
            return false;

        FillMissingThemeValues(outTheme);
        return true;
    }

    void AddOrReplaceTheme(const DungeonThemeData& theme)
    {
        auto found = std::find_if(g_Themes.begin(), g_Themes.end(),
            [&](const DungeonThemeData& existing) { return existing.id == theme.id; });
        if (found != g_Themes.end())
            *found = theme;
        else
            g_Themes.push_back(theme);
    }

    bool LoadThemeJsonFile(const std::string& path)
    {
        json root;
        if (!JsonIO::LoadJson(path, root))
            return false;

        LoadTileTypesFromJson(root);

        if (root.contains("themes") && root["themes"].is_array())
        {
            for (const auto& themeJson : root["themes"])
            {
                DungeonThemeData theme;
                if (ParseThemeJson(themeJson, theme))
                    AddOrReplaceTheme(theme);
            }
            return true;
        }

        DungeonThemeData theme;
        if (!ParseThemeJson(root, theme))
            return false;

        AddOrReplaceTheme(theme);
        return true;
    }

    void LoadDefaultData()
    {
        g_TileTypes = MakeDefaultTileTypes();
        g_Themes.clear();
        g_Themes.push_back(MakeDefaultTheme());
        g_Loaded = true;
    }

    bool LoadThemeDirectory()
    {
        EnsureThemeDirectories();
        g_TileTypes = MakeDefaultTileTypes();
        g_Themes.clear();

        for (const auto& entry : std::filesystem::directory_iterator(kThemeDirectoryPath))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;

            LoadThemeJsonFile(entry.path().string());
        }

        if (g_Themes.empty() && std::filesystem::exists(kLegacyThemeConfigPath))
            LoadThemeJsonFile(kLegacyThemeConfigPath);

        if (g_Themes.empty())
            g_Themes.push_back(MakeDefaultTheme());

        g_Loaded = true;
        return true;
    }

    void EnsureLoaded()
    {
        if (g_Loaded)
            return;

        if (!LoadThemeDirectory())
            LoadDefaultData();
    }

    std::string SanitizeFileName(std::string name)
    {
        for (char& c : name)
        {
            const bool invalid = (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|');
            if (invalid || c == ' ')
                c = '_';
        }
        if (name.empty())
            name = kDefaultThemeId;
        return name;
    }
}
void DungeonThemeDatabase::Init()
{
    EnsureThemeDirectories();

    std::string defaultPath = GetThemeDirectory() + "\\" + std::string(kDefaultThemeId) + ".json";

    //  default.json が存在しない場合のみ自動生成して書き出す
    if (!std::filesystem::exists(defaultPath))
    {
        DungeonThemeData defaultTheme = MakeDefaultTheme();

        json root;
        root["id"] = defaultTheme.id;
        root["name"] = defaultTheme.displayName;
        root["bgm"] = defaultTheme.bgmPath;
        root["models"] = json::object();
        root["models"]["floor"] = defaultTheme.models.floor;
        root["models"]["wall"] = defaultTheme.models.wall;

        JsonIO::SaveJson(defaultPath, root);
    }

    Reload();
}

void DungeonThemeDatabase::Reload()
{
    g_Loaded = false;
    EnsureLoaded();
}

bool DungeonThemeDatabase::LoadFromFile(const std::string& path)
{
    g_TileTypes = MakeDefaultTileTypes();
    g_Themes.clear();

    if (!LoadThemeJsonFile(path))
    {
        LoadDefaultData();
        return false;
    }

    if (g_TileTypes.empty())
        g_TileTypes = MakeDefaultTileTypes();
    if (g_Themes.empty())
        g_Themes.push_back(MakeDefaultTheme());

    g_Loaded = true;
    return true;
}

bool DungeonThemeDatabase::SaveTheme(const DungeonThemeData& theme)
{
    if (theme.id.empty())
        return false;

    EnsureThemeDirectories();

    json root;
    root["id"] = theme.id;
    root["name"] = theme.displayName.empty() ? theme.id : theme.displayName;
    root["bgm"] = theme.bgmPath;
    root["models"] = json::object();
    root["models"]["floor"] = theme.models.floor;
    root["models"]["wall"] = theme.models.wall;

    const std::string path = std::string(kThemeDirectoryPath) + "\\" + SanitizeFileName(theme.id) + ".json";
    const bool saved = JsonIO::SaveJson(path, root);
    if (saved)
        Reload();
    return saved;
}

std::string DungeonThemeDatabase::GetThemeDirectory()
{
    EnsureThemeDirectories();
    return kThemeDirectoryPath;
}

std::string DungeonThemeDatabase::GetBgmDirectory()
{
    EnsureThemeDirectories();
    return kBgmDirectoryPath;
}

std::string DungeonThemeDatabase::GetMapModelDirectory()
{
    EnsureThemeDirectories();
    return kMapModelDirectoryPath;
}

bool DungeonThemeDatabase::Exists(const std::string& themeId)
{
    EnsureLoaded();
    if (themeId.empty())
        return true;

    return std::any_of(g_Themes.begin(), g_Themes.end(),
        [&](const DungeonThemeData& theme) { return theme.id == themeId; });
}

const DungeonThemeData& DungeonThemeDatabase::GetThemeOrDefault(const std::string& themeId)
{
    EnsureLoaded();

    auto found = std::find_if(g_Themes.begin(), g_Themes.end(),
        [&](const DungeonThemeData& theme) { return theme.id == themeId; });
    if (found != g_Themes.end())
        return *found;

    found = std::find_if(g_Themes.begin(), g_Themes.end(),
        [](const DungeonThemeData& theme) { return theme.id == kDefaultThemeId; });
    if (found != g_Themes.end())
        return *found;

    return g_Themes.front();
}

const std::vector<DungeonThemeData>& DungeonThemeDatabase::GetAllThemes()
{
    EnsureLoaded();
    return g_Themes;
}

const std::vector<DungeonTileTypeSetting>& DungeonThemeDatabase::GetTileTypes()
{
    EnsureLoaded();
    return g_TileTypes;
}
