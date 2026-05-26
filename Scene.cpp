#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "camera.h"
#include"scene.h"

void Scene::Init()
{

}


void Scene::Uninit()
{
	// Scene が AddGameObject したものは Scene が破棄する。
	// 各 GameObject 側の Uninit で、モデル・エフェクト・登録済み管理情報を解放する。
	for (int i = 0; i < 3; i++)
	{
		for (auto gameobject : m_GameObject[i])
		{
			gameobject->Uninit();
			delete gameobject;
		}
		m_GameObject[i].clear();
	}


}

void Scene::Update()
{
	// layer 順に全 GameObject を更新する。
	// DungeonScene ではこの後に TurnManager が呼ばれ、ターン制の行動だけ追加で進む。
	for (int i = 0; i < 3; i++)
	{
		for (auto gameobject : m_GameObject[i])
		{
			gameobject->Update();
		}
	}
	for (int i = 0; i < 3; i++)
	{
		// Destroy フラグが立ったものはフレーム末に Scene のリストから外す。
		// 実体の delete は各 Clear/Uninit の流れで行う設計なので、ここではリスト整理だけ行う。
		m_GameObject[i].remove_if([](GameObject* gameobject) {
			return gameobject->Destroy();//削除関数実行(bool)
			});
	}

}

void Scene::Draw()
{

	Camera* camera = GetGameObject<Camera>();
	if (camera != nullptr)
	{
		// 半透明やビルボードを含む 3D オブジェクトは、カメラから遠い順に描く。
		// layer 1 だけを並べ替え、背景や UI の描画順は固定する。
		Vector3 camposition = camera->GetPosition();
		Vector3 camForward = camera->GetForward();


		m_GameObject[1].sort([&](GameObject* obj1, GameObject* obj2)
			{
				return obj1->GetZ(camposition,camForward) > obj2->GetZ(camposition, camForward);
			})
			;
	}
	


	for (int i = 0; i < 3; i++)
	{
		for (auto gameobject : m_GameObject[i])
		{
			gameobject->Draw();
		}
	}
}
