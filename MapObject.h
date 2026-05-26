#pragma once
#include "GameObject.h"
#include "Vector2Int.h"

// マスに配置されるオブジェクトの共通インターフェース
class MapObject : public GameObject
{
protected:
    Vector2Int m_GridPos;
    bool m_IsVisible;

public:
    virtual ~MapObject() {}

    void SetGridPos(Vector2Int pos) {
        m_GridPos = pos;
        m_Position = { pos.x * 2.0f, 0.01f, pos.y * 2.0f };
    }
    Vector2Int GetGridPos() const { return m_GridPos; }

    // プレイヤーが上に乗った時の処理
    virtual void OnStepped(class Player* player) {}

    // 攻撃された時の処理
    virtual void OnAttacked() {}

    void SetVisible(bool vis) { m_IsVisible = vis; }
    bool IsVisible() const { return m_IsVisible; }
};