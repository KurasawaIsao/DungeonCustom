#pragma once
#include "MapObject.h"
#include "TrapData.h"
#include "ModelRenderer.h"

class Player;
class Unit;
class ItemInstance;
class Trap : public MapObject
{
private:
    ModelRenderer* m_Model = nullptr;

    const TrapData* m_Data = nullptr;

    bool m_SingleUse = false;             // 使い捨て
    int m_LastActivatedTurn = -1;
    float m_ItemActivationTimer = 0.0f;
    bool m_DestroyAfterItemActivation = false;

public:
    void Init() override;

    void Update() override;
    void Draw() override;

    void Uninit() override {
        delete m_Model;
    }

    virtual void Activate(Unit* target);
    // アイテムが着地した場合は、対象ユニットなしで罠を作動させる。
    bool ActivateByItem(ItemInstance* item = nullptr);
    static constexpr float GetItemActivationDuration() { return 0.35f; }
    void OnStepped(Player* player) override;
    void Setup(const TrapData* data) {
        m_Data = data;
        if (m_Model && m_Data) {
            m_Model->Load(m_Data->modelPath.c_str());
        }
    }

    void SetGridPos(Vector2Int p) { m_GridPos = p; }
};