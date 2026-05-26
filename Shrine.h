#pragma once
#include "MapObject.h"
#include "animationModel.h"

class Shrine : public MapObject
{
private:
    AnimationModel* m_Model = nullptr;
    Vector2Int m_GridPos;
    int m_RemainingUses = 1;

public:
    void Init() override;
    void Update() override {}
    void Draw() override;
    void Uninit() override;

    void Setup(Vector2Int pos) {
        m_GridPos = pos;
        m_Position = { pos.x * 2.0f, 0.01f, pos.y * 2.0f };
    }

    void SetGridPos(Vector2Int pos) { m_GridPos = pos; }
    Vector2Int GetGridPos() const { return m_GridPos; }

    void OnStepped(class Player* player) override;
    void OnAttacked() override;
};