#pragma once
#include <vector>
#include <string>

// DungeonData は JSON から読み込むダンジョン設定のメモリ上表現。
// DungeonScene::InitDungeonData で読み込まれ、MapManager が現在階層の FloorData を参照して
// MapGenerator/EnemyTable/ItemTable へ生成条件を渡す。
enum class MapSourceType
{
    Auto,     
    FixedMap,   
};
enum class UnidentifiedMode
{
    None,
    Half,
    All,
};
enum class MapLayoutType {
    Random,
    CrossFive,
    SmallRoomChain,
    TreasureRoomChain,
    TShapeFour,
    SoilEight,
    LShape,
    LargeRoom,
};

struct LayoutWeight {
    MapLayoutType type;
    int weight; 
};
struct FixedRoomSetting {
    std::string path;
    int appearanceRate = 100; 
};
struct FloorData {
    // 1フロア分の生成設定。
    // マップサイズ、部屋数、敵/アイテムテーブル、視界、特殊部屋の出現率などをまとめて持つ。
    int width;
    int height;
    bool useRandomMapSize = false;
    bool useFullyRandomMapSize = false;
    int minWidth = 40;
    int maxWidth = 60;
    int minHeight = 40;
    int maxHeight = 60;
    MapSourceType mapSource = MapSourceType::Auto;
    std::string itemTableId;
    std::string enemyTableId;
    std::string trapTableId;
    std::string shopItemTableId;
    int viewDistance = 2;
    bool playerVisionClear = true;
    // 0以下なら無制限。正の値なら、そのフロアで指定ターン経過時に風でゲームオーバーにする。
    int windTurnLimit = 1500;

    int maxEnemyCount;
    int maxItemCount;
    int maxTrapCount = 10;

    int monsterHouseAppearanceRate = 0;
    int shopAppearanceRate = 0;
    int shopTrapDensity = 35;

    int minRoomCount = 3;
    int maxRoomCount = 6;
    int corridorComplexity = 45;
    int generationSeedOffset = 0;
    bool useFixedRoom = false;
    std::vector<FixedRoomSetting> fixedRoomPaths;
    bool spawnAllFixedRooms = true;
    std::vector<LayoutWeight> layoutWeights = { {MapLayoutType::Random, 100} };

    std::string mapFilePath;
};


class DungeonData
{
public:
    DungeonData()
        : m_DungeonId(""), m_Floors()
    {}

    void AddFloor(const FloorData& floor)
    {
        m_Floors.push_back(floor);
    }
    void RemoveFloor(int index)
    {
        if (index < 0 || index >= (int)m_Floors.size())
            return;

        m_Floors.erase(m_Floors.begin() + index);
    }

    FloorData& GetFloor(int floorIndex)
    {
        return m_Floors[floorIndex];
    }

    const FloorData& GetFloor(int floorIndex) const
    {
        return m_Floors[floorIndex];
    }

    void Clear()
    {
        m_Floors.clear();
    }
    int GetFloorCount() const { return (int)m_Floors.size(); }

    void SetDungeonId(const std::string& id) { m_DungeonId = id; }
    const std::string& GetDungeonId() const { return m_DungeonId; }

    void SetUseGenerationSeed(bool enabled) { m_UseGenerationSeed = enabled; }
    bool UseGenerationSeed() const { return m_UseGenerationSeed; }

    void SetGenerationSeed(int seed) { m_GenerationSeed = seed; }
    int GetGenerationSeed() const { return m_GenerationSeed; }

    void SetUnidentifiedMode(UnidentifiedMode mode) { m_UnidentifiedMode = mode; }
    UnidentifiedMode GetUnidentifiedMode() const { return m_UnidentifiedMode; }

    void SetBlessOrCurseEnabled(bool enabled) { m_BlessOrCurseEnabled = enabled; }
    bool IsBlessOrCurseEnabled() const { return m_BlessOrCurseEnabled; }

private:
    std::string m_DungeonId;
    bool m_UseGenerationSeed = false;
    int m_GenerationSeed = 0;
    UnidentifiedMode m_UnidentifiedMode = UnidentifiedMode::None;
    bool m_BlessOrCurseEnabled = true;
    std::vector<FloorData> m_Floors;
};



