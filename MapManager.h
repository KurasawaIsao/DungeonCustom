#pragma once
#include "GameRandom.h"
#include "MapGenerator.h"
#include "GameObject.h"
#include <memory>
#include "DungeonData.h"
#include "MapLoader.h"
#include <algorithm>
#include <cstdlib>

namespace MapManagerDetail
{
    inline int ClampMapSizeForGeneration(int value)
    {
        if (value < 10) return 10;
        if (value > 120) return 120;
        return value;
    }
}

class Scene;
class Player;

//マップのデータを保持したりダンジョンのデータを保持したり、
//ダンジョンの生成リクエストや取得を行うクラス。
//結構よく使います。
class MapManager : public GameObject
{
private:
    static MapManager* m_Instance;
    // 現在階層の盤面データ。描画・Unit配置・足元イベントはこの MapData を参照する。
    std::unique_ptr<MapData> m_CurrentMap;

    // 階段演出中に二重で階層遷移リクエストが入るのを防ぐ。
    bool m_IsFloorChanging = false;

    FloorData m_DefaultFloor;
    DungeonData m_DungeonData;
    int m_CurrentFloor = 0;
    // 前フレームまで店内にいたか。店から出た瞬間だけ会計/泥棒判定を走らせる。
    bool m_PlayerWasInShop = false;

public:
    static MapManager* Instance()
    {
        return m_Instance;
    }
    MapManager() {
        m_DefaultFloor.width = 50;
        m_DefaultFloor.height = 50;
        m_DefaultFloor.itemTableId = "early";
        m_DefaultFloor.enemyTableId = "early";
        m_DefaultFloor.shopItemTableId = "early";
        m_DefaultFloor.maxEnemyCount = 10;
        m_DefaultFloor.maxItemCount = 10;
        m_Instance = this; }  

    void StartDungeon();
    void GenerateNewMap(FloorData floor, Scene* scene)
    {
        auto generateAutoMap = [&]()
        {
            int width = floor.width;
            int height = floor.height;
            if (floor.useFullyRandomMapSize)
            {
                width = GameRandom::Range(40, 90);
                height = GameRandom::Range(40, 90);
            }
            else if (floor.useRandomMapSize)
            {
                const int minW = (std::min)(floor.minWidth, floor.maxWidth);
                const int maxW = (std::max)(floor.minWidth, floor.maxWidth);
                const int minH = (std::min)(floor.minHeight, floor.maxHeight);
                const int maxH = (std::max)(floor.minHeight, floor.maxHeight);
                width = GameRandom::Range(minW, maxW);
                height = GameRandom::Range(minH, maxH);
            }
            width = MapManagerDetail::ClampMapSizeForGeneration(width);
            height = MapManagerDetail::ClampMapSizeForGeneration(height);
            m_CurrentMap = MapGenerator::Generate(width, height, scene);
        };

        if (floor.mapSource == MapSourceType::FixedMap && !floor.mapFilePath.empty())
        {
            m_CurrentMap = MapLoader::LoadFromFile(floor.mapFilePath, scene);
            if (!m_CurrentMap)
                generateAutoMap();
        }
        else
        {
            generateAutoMap();
        }


        if (m_CurrentMap)
        {
            // 非表示扱いのマップ外マスを、境界から10マスぶん壁として描画・判定できるようにする。
            m_CurrentMap->ExpandActiveAreaWithWall(10);
        }
        MapGenerator::SpawnPlayerInRoom();
    }

    MapData* GetCurrentMap() { return m_CurrentMap.get(); }
    const MapData* GetCurrentMap() const { return m_CurrentMap.get(); }

    void SetDungeonData(const DungeonData& data)
    {
 
        m_DungeonData = data;
        m_CurrentFloor = 0;
    }
    const DungeonData& GetDungeonData() const { return m_DungeonData; }

    const FloorData& GetCurrentFloorData() const;


    bool HasMap() const { return (bool)m_CurrentMap; }

    void ClearFloor();
    void ChangeFloor();

    void Init() override {}
    void Update() override;
    void Draw() override {}
    void Uninit() override { if (m_Instance == this) m_Instance = nullptr; }
    void RequestNextFloor();
    void RequestBackFloor(int count);
    void AfterUnitMoved(Unit* unit, bool suppressObjectStep = false);

};

