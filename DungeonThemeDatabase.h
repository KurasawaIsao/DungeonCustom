#pragma once
#include <string>
#include <vector>

// エディタに表示するタイル種別と、テーマ側で対応するモデル項目名を保持する。
struct DungeonTileTypeSetting
{
    std::string id;
    std::string label;
    std::string modelKey;
};

// テーマごとに差し替えるマップモデル一式。
struct DungeonThemeTileModels
{
    std::string floor;
    std::string wall;
};

// 階層テーマ。モデル一式と、その階層で流すBGMをまとめて扱う。
struct DungeonThemeData
{
    std::string id;
    std::string displayName;
    std::string bgmPath;
    DungeonThemeTileModels models;
};

class DungeonThemeDatabase
{
public:
    static void Init();
    static void Reload();
    static bool LoadFromFile(const std::string& path);
    static bool SaveTheme(const DungeonThemeData& theme);
    static std::string GetThemeDirectory();
    static std::string GetBgmDirectory();
    static std::string GetMapModelDirectory();
    static bool Exists(const std::string& themeId);
    static const DungeonThemeData& GetThemeOrDefault(const std::string& themeId);
    static const std::vector<DungeonThemeData>& GetAllThemes();
    static const std::vector<DungeonTileTypeSetting>& GetTileTypes();
};
