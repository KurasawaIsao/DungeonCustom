#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "assimp-vc143-mt.lib")

#include "component.h"

#define MAX_BONES (256)
//変形後頂点構造体
struct DEFORM_VERTEX {
	aiVector3D Position;
	aiVector3D Normal;
	int        BoneNum;
	int        BoneIndices[4]; // ボーンインデックス
	float      BoneWeight[4];
};
//ボーン構造体
struct BONE
{
	aiMatrix4x4 Matrix;
	aiMatrix4x4 AnimationMatrix;
	aiMatrix4x4 OffsetMatrix;
	aiMatrix4x4 DefaultMatrix;
};

class AnimationModel : public Component
{
private:
	const aiScene* m_AiScene = nullptr;
	bool m_OwnsImportedScenes = true;
	std::unordered_map<std::string, const aiScene*> m_Animation;

	ID3D11Buffer**	m_VertexBuffer;
	ID3D11Buffer**	m_IndexBuffer;
	ID3D11Buffer* m_BoneConstantBuffer = nullptr;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Texture;
	std::vector<DEFORM_VERTEX>* m_DeformVertex;//変形後頂点データ
	std::unordered_map<std::string, BONE> m_Bone;//ボーンデータ（名前で参照）
	std::vector<std::string> m_BoneNames;// ボーン名の一覧（インデックスで何番目かを参照用）

	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Matrix);
	XMMATRIX AiMatrixToXMMatrix(aiMatrix4x4 m);

	float NowFrame;

public:
	using Component::Component;

	void Load( const char *FileName );
	void LoadAnimation( const char *FileName, const char *Name );
	void Uninit() override;
	void Update(const char *AnimationName1, int Frame1, const char* AnimationName2, int Frame2,float BlendRate);
	void Update(const char* AnimationName1, int Frame1);
	void Draw() override;

	ID3D11ShaderResourceView* GetTexture() {
		if (m_Texture.empty()) return nullptr;
		return m_Texture.begin()->second;
	}

	void CreateClone(const AnimationModel& src);
	int GetBoneIndex(const std::string& name);
	double GetAnimationTime(aiAnimation* anim, int frame);
	int GetAnimationFrameCount(const std::string& name);
	aiVector3D CalcInterpolatedPosition(double animTime, aiNodeAnim* nodeAnim);
	aiQuaternion CalcInterpolatedRotation(double animTime, aiNodeAnim* nodeAnim);
	aiVector3D CalcInterpolatedScaling(double animTime, aiNodeAnim* nodeAnim);
};