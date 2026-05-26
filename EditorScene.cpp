#include "EditorScene.h"
#include "imgui.h"
#include "camera.h"
#include "RayUtil.h"
#include "MapEditor.h"
#include "player.h"
#include "MapRenderer.h"
#include "audio.h"

void EditorScene::Init()
{
    Camera* cam = AddGameObject<Camera>(0);
    cam->SetPlayerEnable(false);

    cam->SetPivot(Vector3(0.0f, 0.0f, 0.0f));

    AddGameObject<MapRenderer>(1)->SetEditor(true);
    AddGameObject<MapEditor>(0);
    m_BGM = new Audio;
    m_BGM->Load("Asset\\Sound\\docchigaiikana.wav");
    m_BGM->Play(true);
}

void EditorScene::Uninit()
{
    Scene::Uninit();
    m_BGM->Stop();
    delete m_BGM;
    m_BGM = nullptr;
}

void EditorScene::Update()
{
    Scene::Update();

}

void EditorScene::Draw()
{
    Scene::Draw();

    // デバッグ表示などあればここに
}
