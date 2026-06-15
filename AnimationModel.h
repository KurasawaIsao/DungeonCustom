#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "assimp-vc143-mt.lib")

#include "component.h"

#define MAX_BONES (256)

// CPU側で保持する、スキニングに必要な頂点情報。
struct DEFORM_VERTEX {
	aiVector3D Position;       // モデル空間上の頂点座標。
	aiVector3D Normal;         // モデル空間上の法線。
	int        BoneNum;        // この頂点に登録済みのボーン数。
	int        BoneIndices[4]; // 頂点へ影響するボーンのインデックス。
	float      BoneWeight[4];  // 各ボーンが頂点へ与える影響度。
};

// 1本のボーンについて、姿勢計算に使用する行列をまとめた構造体。
struct BONE
{
	aiMatrix4x4 Matrix;          // GPUへ渡す最終的なスキニング行列。
	aiMatrix4x4 AnimationMatrix; // 現在フレームから求めたローカル変換行列。
	aiMatrix4x4 OffsetMatrix;    // メッシュ空間からボーン空間へ変換する逆バインド行列。
	aiMatrix4x4 DefaultMatrix;   // アニメーションが無い場合に使用する基準姿勢。
};

// アニメーションの指定フレームで発火する名前付き通知。
struct AnimationNotify
{
	float Frame = 0.0f; // 通知を発火するフレーム。
	std::string Name;   // 利用側へ渡す通知名。
};

// Assimpで読み込んだモデルをGPUスキニングで描画し、アニメーション再生も管理するコンポーネント。
class AnimationModel : public Component
{
private:
	// モデル本体と、名前ごとに読み込んだアニメーションシーン。
	const aiScene* m_AiScene = nullptr;
	// クローンは読み込み元のaiSceneを共有するため、解放責任の有無を記録する。
	bool m_OwnsImportedScenes = true;
	std::unordered_map<std::string, const aiScene*> m_Animation;

	// メッシュごとの頂点・インデックスバッファと、全ボーン共通の定数バッファ。
	ID3D11Buffer**	m_VertexBuffer;
	ID3D11Buffer**	m_IndexBuffer;
	ID3D11Buffer* m_BoneConstantBuffer = nullptr;

	// 描画用テクスチャと、CPU側で参照する頂点・ボーン情報。
	std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Texture;
	std::vector<DEFORM_VERTEX>* m_DeformVertex; // メッシュごとの変形対象頂点データ。
	std::unordered_map<std::string, BONE> m_Bone; // ボーン名から姿勢情報を参照する辞書。
	std::vector<std::string> m_BoneNames; // GPUへ渡す配列上のインデックスとボーン名の対応表。

	// ノード階層からボーンを作成し、親から子へ最終行列を伝播する。
	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Matrix);
	// Assimpの行列をDirectXMathの行列へ変換する。
	XMMATRIX AiMatrixToXMMatrix(aiMatrix4x4 m);

	// 再生状態はモデル側へ集約し、利用側がフレームやブレンドを管理しなくてよいようにする。
	std::string m_CurrentAnimation = "Idle"; // 現在の基準となるアニメーション名。
	std::string m_NextAnimation; // ブレンド中の遷移先アニメーション名。
	std::string m_FallbackAnimation = "Idle"; // 単発再生終了後に戻るアニメーション名。
	std::string m_NotifyPlaybackAnimation; // 前回の通知判定で再生対象だったアニメーション名。
	float m_CurrentFrame = 0.0f; // 現在の再生フレーム。
	float m_PlaybackSpeed = 0.5f; // 1回の更新で進めるフレーム量。
	float m_BlendRate = 1.0f; // 遷移元から遷移先へ補間する割合。
	bool m_IsLooping = true; // 現在のアニメーションをループさせるか。
	// アニメーション名ごとの通知一覧と、通知を利用側へ渡すコールバック。
	std::unordered_map<std::string, std::vector<AnimationNotify>> m_AnimationNotifies;
	std::function<void(const std::string&, const std::string&)> m_NotifyCallback;

	// 前回から今回までに通過した通知を抽出し、フレーム順にコールバックへ渡す。
	void DispatchAnimationNotifies(
		const std::string& animationName,
		float previousFrame,
		float currentFrame,
		bool animationChanged);

public:
	using Component::Component;
	using NotifyCallback = std::function<void(const std::string&, const std::string&)>;

	// ループ再生と単発再生を同じ入口で扱い、単発終了時はfallbackAnimationへ戻す。
	void PlayAnimation(
		const std::string& animationName,
		float speed = 1.0f,
		bool loop = true,
		const std::string& fallbackAnimation = "Idle",
		bool useBlend = true);
	// 再生フレーム、通知、ブレンド、ボーン姿勢を1回分更新する。
	void UpdatePlayback();
	// 現在の再生位置がアニメーション終端へ到達したかを返す。
	bool IsAnimationFinished() const;
	// 単発アニメーションの再生中かを返す。
	bool IsOneShotPlaying() const { return !m_IsLooping; }

	// Notifyのタイミング判定は再生フレームを所有するAnimationModel自身が行う。
	bool AddAnimationNotify(const std::string& animationName, float frame, const std::string& notifyName);
	// 0.0～1.0の再生割合を実フレームへ変換して通知を登録する。
	bool AddAnimationNotifyNormalized(const std::string& animationName, float normalizedTime, const std::string& notifyName);
	// 指定アニメーションに同名の通知が登録されているかを返す。
	bool HasAnimationNotify(const std::string& animationName, const std::string& notifyName) const;
	// 全アニメーションの通知設定を削除する。
	void ClearAllAnimationNotifies();
	// 通知発火時に呼び出す処理を登録する。
	void SetNotifyCallback(NotifyCallback callback) { m_NotifyCallback = callback; }

	// モデル本体を読み込み、描画とスキニングに必要なGPUリソースを作成する。
	void Load( const char *FileName );
	// 別ファイルのアニメーションを指定名で読み込む。
	void LoadAnimation( const char *FileName, const char *Name );
	// 所有しているGPUリソースとインポート済みデータを解放する。
	void Uninit() override;
	// 2つのアニメーションを指定割合でブレンドしてボーン姿勢を更新する。
	void Update(const char *AnimationName1, int Frame1, const char* AnimationName2, int Frame2,float BlendRate);
	// 1つのアニメーションの指定フレームでボーン姿勢を更新する。
	void Update(const char* AnimationName1, int Frame1);
	// 現在のボーン行列とマテリアルを使用して全メッシュを描画する。
	void Draw() override;

	// 登録済みテクスチャの先頭を取得する。テクスチャが無い場合はnullptrを返す。
	ID3D11ShaderResourceView* GetTexture() {
		if (m_Texture.empty()) return nullptr;
		return m_Texture.begin()->second;
	}

	// 読み込み済みリソースを共有し、個別の再生状態を持つクローンを作成する。
	void CreateClone(const AnimationModel& src);
	// ボーン名に対応するGPU配列上のインデックスを取得し、未登録なら追加する。
	int GetBoneIndex(const std::string& name);
	// 指定フレームをアニメーション内のtick時間へ変換する。
	double GetAnimationTime(aiAnimation* anim, int frame);
	// 指定アニメーションの再生尺をフレーム数相当の整数値で返す。
	int GetAnimationFrameCount(const std::string& name);
	// 指定時刻を挟むキーから位置を線形補間する。
	aiVector3D CalcInterpolatedPosition(double animTime, aiNodeAnim* nodeAnim);
	// 指定時刻を挟むキーから回転を球面線形補間する。
	aiQuaternion CalcInterpolatedRotation(double animTime, aiNodeAnim* nodeAnim);
	// 指定時刻を挟むキーから拡大率を線形補間する。
	aiVector3D CalcInterpolatedScaling(double animTime, aiNodeAnim* nodeAnim);
};
