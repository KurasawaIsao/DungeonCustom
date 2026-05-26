#pragma once
#include "main.h"
#include"Vector3.h"
class GameObject
{
protected:
	bool m_Destroy = false;

	Vector3 m_Position{ 0.0f,0.0f,0.0f };
	Vector3 m_Rotation{ 0.0f,0.0f,0.0f };
	Vector3 m_Scale{ 1.0f,1.0f,1.0f };

public:
	virtual ~GameObject() = default;

	virtual void Init() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
	virtual void Uninit() = 0;

	void SetDestroy() { m_Destroy = true; };
	bool Destroy()
	{
		if (m_Destroy)
		{
			Uninit();
			delete this;
			return true;
		}
		else
		{
			return false;
		}
	}

	Vector3 GetPosition() { return m_Position; };
	void SetPosition(Vector3 Pos) { m_Position = Pos; };
	Vector3 GetRotation() { return m_Rotation; };
	void SetRotation(Vector3 rot) { m_Rotation = rot; };
	Vector3 GetScale() { return m_Scale; };
	void SetScale(Vector3 scale) { m_Scale = scale; };


	Vector3 GetRight()
	{
		XMMATRIX matrix;
		matrix = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);

		Vector3 right;
		XMStoreFloat3((XMFLOAT3*)&right, matrix.r[0]);

		return right;
	}

	virtual Vector3 GetForward()//デフォルト。継承先にこれと同じものがあればそちらが優先的に呼ばれる
	{
		XMMATRIX matrix;
		matrix = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);

		Vector3 right;
		XMStoreFloat3((XMFLOAT3*)&right, matrix.r[2]);

		return right;
	}
	float GetZ(Vector3 position, Vector3 Forward)
	{
		Vector3 diff = m_Position - position;
		return diff.Length(); // ← OK：距離順
	}

	static float dot(const Vector3& a, const Vector3& b){
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}
};