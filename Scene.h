#pragma once
#include<list>
#include<vector>
#include "GameObject.h"
class Scene :public GameObject
{
private:
	// Scene が所有する GameObject 群。
	// layer 0: 背景・管理系、layer 1: 3D モデル、layer 2: UI/前面表示、という使い分けを想定。
	// Scene ごとに中身を分けるため static にはしない。
	std::list<GameObject*> m_GameObject[3];


public:
	virtual void Init();
	virtual void Uninit();
	virtual void Update();
	virtual void Draw();

	template <typename T>
	T* AddGameObject(int layer)
	{
		// 生成直後に Init まで呼ぶ。
		// 以降の Update/Draw/Uninit は Scene が一括で面倒を見る。
		T* gameobject = new T();
		gameobject->Init();
		m_GameObject[layer].push_back(gameobject);
		return gameobject;
	};
	//既に生成したGameObjectを追加
	void AddGameObject(GameObject* obj,int layer)
	{
		m_GameObject[layer].push_back(obj);
	}


	template <typename T>
	T* GetGameObject()
	{
		// 最初に見つかった該当型を返す簡易検索。
		// 複数存在する型を扱う場合は GetGameObjects を使う。
		for (int i = 0; i < 3; i++)
		{
			for (auto gameobject : m_GameObject[i])
			{
				T* find = dynamic_cast<T*>(gameobject);
				if (find != nullptr)
					return find;
			}
		}

		return nullptr;
	};

	template <typename T>
	std::vector<T*> GetGameObjects()
	{
		// 敵・アイテム・ImGuiObject など、同じ型が複数あるものをまとめて取得する。
		std::vector<T*>  finds;
		for (int i = 0; i < 3; i++)
		{
			for (auto gameobject : m_GameObject[i])
			{
				T* find = dynamic_cast<T*>(gameobject);
				if (find != nullptr)
					finds.push_back(find);
			}
		}

		return finds;
	};
};