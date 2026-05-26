#pragma once
#include "scene.h"
#include "MapData.h"

class EditorScene : public Scene
{
private:
    class Audio* m_BGM;
public:
    void Init() override;
    void Update() override;
    void Draw() override;
    void Uninit() override;
};
