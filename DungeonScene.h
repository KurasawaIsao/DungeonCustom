#pragma once
#include "scene.h"
#include "MiniMapRenderer.h"
#include "DungeonData.h"
class DungeonScene : public Scene
{
private:
    std::string m_DungeonId;
public:
    void InitDungeonData();
    void BuildDefaultDungeonData(DungeonData& dungeon);
    void Init() override;
    void Update() override;
    void Uninit() override;

    void SetDungeonId(const std::string& id)
    {
        m_DungeonId = id;
    }
};
