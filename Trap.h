#pragma once
#include "MapObject.h"
#include "TrapData.h"
#include "ModelRenderer.h"

class Player;
class Unit;
class Trap : public MapObject
{
private:
    ModelRenderer* m_Model = nullptr;

    const TrapData* m_Data = nullptr;

    bool m_SingleUse = false;             // Žg‚¢ŽÌ‚Ä
    int m_LastActivatedTurn = -1;

public:
    void Init() override;

    void Update() override;
    void Draw() override;

    void Uninit() override {
        delete m_Model;
    }

    virtual void Activate(Unit* target);
    void OnStepped(Player* player) override;
    void Setup(const TrapData* data) {
        m_Data = data;
        if (m_Model && m_Data) {
            m_Model->Load(m_Data->modelPath.c_str());
        }
    }

    void SetGridPos(Vector2Int p) { m_GridPos = p; }
};