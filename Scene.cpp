#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "camera.h"
#include"scene.h"
#include <algorithm>

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
	const bool hasCamera = (camera != nullptr);
	Vector3 camposition;
	Vector3 camForward;
	if (hasCamera)
	{
		camposition = camera->GetPosition();
		camForward = camera->GetForward();
	}

	for (int i = 0; i < 3; i++)
	{
		if (i == 1 && hasCamera)
		{
			std::vector<GameObject*> sortTargets;

			for (auto gameobject : m_GameObject[i])
			{
				// マップなどの土台描画は先に固定順で描き、ユニットやエフェクトだけ距離順にする。
				if (!gameobject->UsesCameraSort())
				{
					gameobject->Draw();
					continue;
				}
				sortTargets.push_back(gameobject);
			}

			std::sort(sortTargets.begin(), sortTargets.end(), [&](GameObject* obj1, GameObject* obj2)
				{
					return obj1->GetZ(camposition, camForward) > obj2->GetZ(camposition, camForward);
				});

			for (auto gameobject : sortTargets)
			{
				gameobject->Draw();
			}
			continue;
		}

		for (auto gameobject : m_GameObject[i])
		{
			gameobject->Draw();
		}
	}
}
