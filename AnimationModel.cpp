#include "main.h"
#include "renderer.h"
#include "animationModel.h"
#include <fstream>
#include <algorithm>

namespace
{
    // 読み込み前にファイルの存在を確認し、Assimpへ不正なパスを渡さないようにする。
    bool AnimationFileExists(const char* fileName)
    {
        if (!fileName || fileName[0] == '\0') return false;
        std::ifstream file(fileName, std::ios::binary);
        return file.good();
    }
}

void AnimationModel::Draw()
{
    // 読み込みが完了していないモデルは描画できないため、GPUへ命令を送らず終了する。
    if (!m_AiScene || !m_VertexBuffer || !m_IndexBuffer) return;

    // 頂点シェーダーのスロット7へ、更新済みのボーン行列を設定する。
    Renderer::GetDeviceContext()->VSSetConstantBuffers(7, 1, &m_BoneConstantBuffer);
    // このモデルは三角形リスト前提。描画前に入力アセンブラの状態を明示しておく。
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // マテリアル情報が欠けている場合にも描画できるよう、白色を初期値にする。
    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = false;
    Renderer::SetMaterial(material);

    for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
    {
        aiMesh* mesh = m_AiScene->mMeshes[m];

        // Assimpのマテリアルから、メッシュに対応する色・透明度・テクスチャを取得する。
        aiString texture;
        aiColor3D diffuse;
        float opacity = 1.0f;

        aiMaterial* aimaterial = m_AiScene->mMaterials[mesh->mMaterialIndex];
        aimaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);
        aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
        aimaterial->Get(AI_MATKEY_OPACITY, opacity);

        if (texture == aiString(""))
        {
            material.TextureEnable = false;
        }
        else
        {
            Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture[texture.data]);
            material.TextureEnable = true;
        }

        material.Diffuse = XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, opacity);
        material.Ambient = material.Diffuse;
        Renderer::SetMaterial(material);

        // メッシュに対応するバッファを設定し、三角形インデックスをまとめて描画する。
        UINT stride = sizeof(VERTEX_3D);
        UINT offset = 0;
        Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer[m], &stride, &offset);
        Renderer::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);
        Renderer::GetDeviceContext()->DrawIndexed(mesh->mNumFaces * 3, 0, 0);
    }
}

void AnimationModel::Load(const char* FileName)
{
    // 空のパスや存在しないファイルは読み込み対象にしない。
    if (!FileName || FileName[0] == '\0') {
        return;
    }
    if (!AnimationFileExists(FileName)) {
        return;
    }

    m_AiScene = aiImportFile(FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);
    if (!m_AiScene)
    {
        return;
    }

    // メッシュ数に合わせて、GPUバッファとCPU側の変形頂点配列を確保する。
    m_VertexBuffer = new ID3D11Buffer * [m_AiScene->mNumMeshes];
    m_IndexBuffer = new ID3D11Buffer * [m_AiScene->mNumMeshes];
    m_DeformVertex = new std::vector<DEFORM_VERTEX>[m_AiScene->mNumMeshes];

    // ノード階層からボーン辞書を先に作り、アニメーション更新時に名前で引けるようにする。
    CreateBone(m_AiScene->mRootNode);

    // 全ボーンの最終行列を頂点シェーダーへ渡す定数バッファを作成する。
    D3D11_BUFFER_DESC cbd;
    ZeroMemory(&cbd, sizeof(cbd));
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(CONSTANT_BUFFER_BONE);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;
    Renderer::GetDevice()->CreateBuffer(&cbd, nullptr, &m_BoneConstantBuffer);

    for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
    {
        aiMesh* mesh = m_AiScene->mMeshes[m];

        // 元頂点をCPU側へ複製し、後からボーン番号とウェイトを登録できる形にする。
        for (unsigned int v = 0; v < mesh->mNumVertices; v++)
        {
            DEFORM_VERTEX deformVertex;
            deformVertex.Position = mesh->mVertices[v];
            deformVertex.Normal = mesh->mNormals[v];
            deformVertex.BoneNum = 0;
            for (unsigned int b = 0; b < 4; b++)
            {
                deformVertex.BoneIndices[b] = 0;
                deformVertex.BoneWeight[b] = 0.0f;
            }
            m_DeformVertex[m].push_back(deformVertex);
        }

        // CPU側の確認・クローン用に、Assimpのボーン情報を名前と頂点ウェイトへ整理する。
        for (unsigned int b = 0; b < mesh->mNumBones; b++)
        {
            aiBone* bone = mesh->mBones[b];
            std::string boneName = bone->mName.C_Str();
            int boneIdx = GetBoneIndex(boneName);
            m_Bone[boneName].OffsetMatrix = bone->mOffsetMatrix;

            for (unsigned int w = 0; w < bone->mNumWeights; w++)
            {
                aiVertexWeight weight = bone->mWeights[w];
                int vIdx = weight.mVertexId;
                int slot = m_DeformVertex[m][vIdx].BoneNum;
                if (slot < 4)
                {
                    m_DeformVertex[m][vIdx].BoneIndices[slot] = boneIdx;
                    m_DeformVertex[m][vIdx].BoneWeight[slot] = weight.mWeight;
                    m_DeformVertex[m][vIdx].BoneNum++;
                }
            }
        }

        {
            // DirectXの頂点形式へ変換した配列から、メッシュ単位の頂点バッファを作成する。
            // 頂点には座標・法線・UVに加えて、GPUスキニング用のボーン番号と重みを詰める。
            VERTEX_3D* vertex = new VERTEX_3D[mesh->mNumVertices];
            std::vector<int> weightCount(mesh->mNumVertices, 0);

            for (unsigned int v = 0; v < mesh->mNumVertices; v++)
            {
                vertex[v].Position = XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
                vertex[v].Normal = XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
                vertex[v].TexCoord = mesh->HasTextureCoords(0) ? XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y) : XMFLOAT2(0.0f, 0.0f);
                vertex[v].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
                for (int i = 0; i < 4; i++)
                {
                    vertex[v].BoneIndices[i] = 0;
                    vertex[v].BoneWeights[i] = 0.0f;
                }
            }

            for (unsigned int b = 0; b < mesh->mNumBones; b++)
            {
                aiBone* bone = mesh->mBones[b];
                std::string boneName = bone->mName.C_Str();
                m_Bone[boneName].OffsetMatrix = bone->mOffsetMatrix;
                int boneIndex = GetBoneIndex(boneName);

                for (unsigned int w = 0; w < bone->mNumWeights; w++)
                {
                    aiVertexWeight weight = bone->mWeights[w];
                    int vId = weight.mVertexId;
                    int num = weightCount[vId];
                    if (num < 4)
                    {
                        vertex[vId].BoneIndices[num] = boneIndex;
                        vertex[vId].BoneWeights[num] = weight.mWeight;
                        weightCount[vId]++;
                    }
                }
            }

            // 4ボーン分に切ったあと、重みの合計が1になるよう正規化してからGPUへ送る。
            for (unsigned int v = 0; v < mesh->mNumVertices; v++)
            {
                float weightSum = 0.0f;
                for (int i = 0; i < 4; i++) weightSum += vertex[v].BoneWeights[i];
                if (weightSum > 0.001f)
                {
                    for (int i = 0; i < 4; i++) vertex[v].BoneWeights[i] /= weightSum;
                }
            }

            D3D11_BUFFER_DESC bd;
            ZeroMemory(&bd, sizeof(bd));
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(VERTEX_3D) * mesh->mNumVertices;
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA sd;
            ZeroMemory(&sd, sizeof(sd));
            sd.pSysMem = vertex;
            Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer[m]);
            delete[] vertex;
        }

        {
            // Assimpの面情報を三角形インデックスへ展開し、インデックスバッファを作成する。
            unsigned int* index = new unsigned int[mesh->mNumFaces * 3];
            for (unsigned int f = 0; f < mesh->mNumFaces; f++)
            {
                const aiFace* face = &mesh->mFaces[f];
                assert(face->mNumIndices == 3);
                index[f * 3 + 0] = face->mIndices[0];
                index[f * 3 + 1] = face->mIndices[1];
                index[f * 3 + 2] = face->mIndices[2];
            }

            D3D11_BUFFER_DESC bd;
            ZeroMemory(&bd, sizeof(bd));
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(unsigned int) * mesh->mNumFaces * 3;
            bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
            bd.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA sd;
            ZeroMemory(&sd, sizeof(sd));
            sd.pSysMem = index;
            Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_IndexBuffer[m]);
            delete[] index;
        }
    }

    // モデルファイルに埋め込まれたテクスチャをDirectXのSRVへ変換する。
    for (int i = 0; i < (int)m_AiScene->mNumTextures; i++)
    {
        aiTexture* aitexture = m_AiScene->mTextures[i];
        ID3D11ShaderResourceView* texture = nullptr;

        TexMetadata metadata;
        ScratchImage image;
        LoadFromWICMemory(aitexture->pcData, aitexture->mWidth, WIC_FLAGS_NONE, &metadata, image);
        CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
        assert(texture);
        m_Texture[aitexture->mFilename.data] = texture;
    }
}

void AnimationModel::LoadAnimation(const char* FileName, const char* Name)
{
    // ファイルパスまたは登録名が不正な場合は、同名の古い登録も残さない。
    const char* animName = (Name && Name[0] != '\0') ? Name : "<null>";
    if (!FileName || FileName[0] == '\0' || !Name || Name[0] == '\0') {
        if (Name) m_Animation.erase(Name);
        return;
    }

    if (!AnimationFileExists(FileName)) {
        m_Animation.erase(Name);
        return;
    }

    // アニメーション用ファイルは座標系を左手系へ変換して読み込む。
    const aiScene* scene = aiImportFile(FileName, aiProcess_ConvertToLeftHanded);


    // アニメーションを含まないシーンは登録せず、その場で解放する。
    if (!scene->HasAnimations()) {
        aiReleaseImport(scene);
        m_Animation.erase(Name);
        return;
    }

    // 同名データを再読み込みした場合は、所有中の古いシーンを解放して差し替える。
    auto old = m_Animation.find(Name);
    if (old != m_Animation.end() && old->second && old->second != scene && m_OwnsImportedScenes) {
        aiReleaseImport(old->second);
    }
    m_Animation[Name] = scene;
}

void AnimationModel::CreateBone(aiNode* node)
{
    // すべてのノードを単位行列で初期化し、名前から参照できるボーン辞書へ登録する。
    BONE bone{};
    aiMatrix4x4 identity(aiVector3D(1.0f, 1.0f, 1.0f), aiQuaternion(1.0f, 0.0f, 0.0f, 0.0f), aiVector3D(0.0f, 0.0f, 0.0f));
    bone.Matrix = identity;
    bone.AnimationMatrix = identity;
    bone.OffsetMatrix = identity;
    bone.DefaultMatrix = identity;
    m_Bone[node->mName.C_Str()] = bone;

    // 子ノードも再帰的に登録して、モデル全体の階層を走査する。
    for (unsigned int n = 0; n < node->mNumChildren; n++)
    {
        CreateBone(node->mChildren[n]);
    }
}

void AnimationModel::CreateClone(const AnimationModel& src)
{
    // 読み込み未完了のモデルからは有効なクローンを作成できない。
    if (!src.m_AiScene || !src.m_VertexBuffer || !src.m_IndexBuffer)
    {
        return;
    }

    // Assimpシーンとアニメーションは読み込み元と共有し、クローン側では解放しない。
    m_AiScene = src.m_AiScene;
    m_OwnsImportedScenes = false;
    m_Animation = src.m_Animation;
    // COMリソースは参照カウントを増やし、読み込み元が残っている間も共有できるようにする。
    m_Texture = src.m_Texture;
    for (auto& texture : m_Texture)
    {
        if (texture.second) texture.second->AddRef();
    }
    m_Bone = src.m_Bone;
    m_BoneNames = src.m_BoneNames;

    // メッシュごとのGPUバッファもAddRefして共有する。
    int meshCount = src.m_AiScene->mNumMeshes;
    m_IndexBuffer = new ID3D11Buffer * [meshCount];
    m_VertexBuffer = new ID3D11Buffer * [meshCount];

    for (int i = 0; i < meshCount; i++)
    {
        m_VertexBuffer[i] = src.m_VertexBuffer[i];
        m_IndexBuffer[i] = src.m_IndexBuffer[i];
        m_VertexBuffer[i]->AddRef();
        m_IndexBuffer[i]->AddRef();
    }

    // クローンは読み込み済みモデルのバッファを共有し、個別のボーン状態だけを持つ。
    m_DeformVertex = new std::vector<DEFORM_VERTEX>[meshCount];
    for (int i = 0; i < meshCount; i++)
    {
        m_DeformVertex[i] = src.m_DeformVertex[i];
    }

    // ボーン姿勢は個体ごとに異なるため、定数バッファだけはクローン専用に作成する。
    D3D11_BUFFER_DESC cbd;
    ZeroMemory(&cbd, sizeof(cbd));
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(CONSTANT_BUFFER_BONE);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;
    Renderer::GetDevice()->CreateBuffer(&cbd, nullptr, &m_BoneConstantBuffer);

    // 読み込み済みモデルに登録された通知はクローンへ引き継ぎ、再生状態とコールバックは個体ごとに初期化する。
    m_AnimationNotifies = src.m_AnimationNotifies;
    m_CurrentAnimation = "Idle";
    m_NextAnimation.clear();
    m_CurrentFrame = 0.0f;
    m_PlaybackSpeed = 0.5f;
    m_BlendRate = 1.0f;
    m_IsLooping = true;
    m_NotifyPlaybackAnimation.clear();
    m_NotifyCallback = nullptr;
}

int AnimationModel::GetBoneIndex(const std::string& name)
{
    // 登録済みなら既存インデックスを返し、ボーン配列の並びを変えない。
    auto it = std::find(m_BoneNames.begin(), m_BoneNames.end(), name);
    if (it != m_BoneNames.end())
    {
        return (int)std::distance(m_BoneNames.begin(), it);
    }

    // 初めて参照されたボーンは末尾へ追加し、その位置をGPU用インデックスとする。
    int index = (int)m_BoneNames.size();
    m_BoneNames.push_back(name);
    return index;
}

void AnimationModel::Uninit()
{
    // モデル未読み込み時は解放対象が無いため終了する。
    if (!m_AiScene) return;

    // 個体ごとに所有するボーン定数バッファを解放する。
    if (m_BoneConstantBuffer)
    {
        m_BoneConstantBuffer->Release();
        m_BoneConstantBuffer = nullptr;
    }

    // 各メッシュの頂点・インデックスバッファの参照を解放する。
    for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
    {
        if (m_VertexBuffer && m_VertexBuffer[m]) m_VertexBuffer[m]->Release();
        if (m_IndexBuffer && m_IndexBuffer[m]) m_IndexBuffer[m]->Release();
    }

    // メッシュ数に合わせて確保したCPU側配列を破棄する。
    delete[] m_VertexBuffer;
    delete[] m_IndexBuffer;
    delete[] m_DeformVertex;
    m_VertexBuffer = nullptr;
    m_IndexBuffer = nullptr;
    m_DeformVertex = nullptr;

    // 読み込んだテクスチャの参照をすべて解放する。
    for (std::pair<const std::string, ID3D11ShaderResourceView*> pair : m_Texture)
    {
        pair.second->Release();
    }
    m_Texture.clear();

    // 元モデルだけがAssimpシーンを解放し、共有クローンからの二重解放を防ぐ。
    if (m_OwnsImportedScenes)
    {
        aiReleaseImport(m_AiScene);
        for (std::pair<const std::string, const aiScene*> pair : m_Animation)
        {
            if (pair.second) aiReleaseImport(pair.second);
        }
    }
    // 外部コールバックを含む登録情報を消去し、解放後に参照されない状態へ戻す。
    m_AiScene = nullptr;
    m_Animation.clear();
    m_AnimationNotifies.clear();
    m_NotifyCallback = nullptr;
}

void AnimationModel::PlayAnimation(
    const std::string& animationName,
    float speed,
    bool loop,
    const std::string& fallbackAnimation,
    bool useBlend)
{
    // 空の名前では再生状態を変更しない。
    if (animationName.empty()) return;

    // 再生速度、ループ設定、単発終了後の戻り先を更新する。
    m_PlaybackSpeed = speed;
    m_IsLooping = loop;
    if (!fallbackAnimation.empty()) m_FallbackAnimation = fallbackAnimation;

    // 確定済みの同じモーションを継続する場合は、再生位置を不用意に巻き戻さない。
    if (m_NextAnimation.empty() && m_CurrentAnimation == animationName) return;

    const bool isPendingSameAnimation = (m_NextAnimation == animationName);
    // 通常切替で同じ遷移先が設定済みなら、進行中のブレンドをそのまま継続する。
    if (useBlend && isPendingSameAnimation) return;

    // 即時切替では遷移元の姿勢を破棄し、指定モーションだけを次の更新から再生する。
    if (!useBlend)
    {
        m_CurrentAnimation = animationName;
        m_NextAnimation.clear();
        // 同じ遷移先を確定するだけなら、再生中のフレーム位置は維持する。
        if (!isPendingSameAnimation) m_CurrentFrame = 0.0f;
        m_BlendRate = 1.0f;
        return;
    }

    // 遷移先を設定し、先頭フレームからブレンドを開始する。
    m_NextAnimation = animationName;
    m_CurrentFrame = 0.0f;
    m_BlendRate = 0.0f;
}

void AnimationModel::UpdatePlayback()
{
    if (!m_AiScene) return;

    // ブレンド中は遷移先を実際の再生対象としてNotifyを判定する。
    const std::string playbackAnimation =
        m_NextAnimation.empty() ? m_CurrentAnimation : m_NextAnimation;
    const bool animationChanged = (m_NotifyPlaybackAnimation != playbackAnimation);
    const float previousFrame = m_CurrentFrame;

    m_CurrentFrame += m_PlaybackSpeed;
    m_NotifyPlaybackAnimation = playbackAnimation;

    // 現在のフレームと前のフレームを渡し、更新中に通過した通知を発火させる。
    DispatchAnimationNotifies(
        playbackAnimation,
        previousFrame,
        m_CurrentFrame,
        animationChanged);

    // ブレンド率を徐々に進め、完了時に遷移先を現在アニメーションとして確定する。
    m_BlendRate = (std::min)(1.0f, m_BlendRate + 0.1f);
    if (m_BlendRate >= 1.0f && !m_NextAnimation.empty())
    {
        m_CurrentAnimation = m_NextAnimation;
        m_NextAnimation.clear();
    }

    // 単発アニメーションの終端へ到達したら、指定された待機アニメーションへ戻す。
    if (!m_IsLooping)
    {
        const int maxFrame = GetAnimationFrameCount(playbackAnimation);
        if (maxFrame > 0 && m_CurrentFrame >= static_cast<float>(maxFrame - 1))
        {
            m_IsLooping = true;
            PlayAnimation(m_FallbackAnimation, 1.0f, true, m_FallbackAnimation);
        }
    }

    // 現在と遷移先の姿勢をブレンドし、GPUへ渡すボーン行列を更新する。
    Update(
        m_CurrentAnimation.c_str(),
        static_cast<int>(m_CurrentFrame),
        m_NextAnimation.empty() ? m_CurrentAnimation.c_str() : m_NextAnimation.c_str(),
        static_cast<int>(m_CurrentFrame),
        m_BlendRate);
}

bool AnimationModel::IsAnimationFinished() const
{
    // ブレンド中は遷移先を、通常時は現在のアニメーションを判定対象にする。
    const std::string& playbackAnimation =
        m_NextAnimation.empty() ? m_CurrentAnimation : m_NextAnimation;
    auto it = m_Animation.find(playbackAnimation);
    if (it == m_Animation.end() || !it->second || !it->second->HasAnimations()) return false;

    // 再生位置が終端フレーム以上なら完了とみなす。
    const int maxFrame = static_cast<int>(it->second->mAnimations[0]->mDuration);
    return maxFrame > 0 && m_CurrentFrame >= static_cast<float>(maxFrame - 1);
}

bool AnimationModel::AddAnimationNotify(
    const std::string& animationName,
    float frame,
    const std::string& notifyName)
{
    // 名前やフレームが不正な通知は登録しない。
    if (animationName.empty() || notifyName.empty() || frame < 0.0f) return false;

    // 発火順が安定するよう、追加後にフレーム昇順へ並べる。
    std::vector<AnimationNotify>& notifies = m_AnimationNotifies[animationName];
    notifies.push_back({ frame, notifyName });
    std::stable_sort(
        notifies.begin(),
        notifies.end(),
        [](const AnimationNotify& lhs, const AnimationNotify& rhs)
        {
            return lhs.Frame < rhs.Frame;
        });
    return true;
}

bool AnimationModel::AddAnimationNotifyNormalized(
    const std::string& animationName,
    float normalizedTime,
    const std::string& notifyName)
{
    // 敵ごとに尺が異なる場合でも扱いやすいよう、0.0～1.0の割合指定をフレームへ変換する。
    if (normalizedTime < 0.0f || normalizedTime > 1.0f) return false;

    const int frameCount = GetAnimationFrameCount(animationName);
    if (frameCount <= 0) return false;
    const float frame = normalizedTime * static_cast<float>((std::max)(0, frameCount - 1));
    return AddAnimationNotify(animationName, frame, notifyName);
}

bool AnimationModel::HasAnimationNotify(
    const std::string& animationName,
    const std::string& notifyName) const
{
    // 指定アニメーションに対応する通知一覧だけを検索する。
    auto it = m_AnimationNotifies.find(animationName);
    if (it == m_AnimationNotifies.end()) return false;

    return std::any_of(
        it->second.begin(),
        it->second.end(),
        [&](const AnimationNotify& notify)
        {
            return notify.Name == notifyName;
        });
}

void AnimationModel::ClearAllAnimationNotifies()
{
    // コールバック自体は残し、登録済みの通知タイミングだけを消去する。
    m_AnimationNotifies.clear();
}

void AnimationModel::DispatchAnimationNotifies(
    const std::string& animationName,
    float previousFrame,
    float currentFrame,
    bool animationChanged)
{

    // 始めに、アニメーション名と対応した通知リストを引く。
    auto notifyIt = m_AnimationNotifies.find(animationName);

    // 通知リストが存在しない場合は発火対象がないため終了する。
    if (notifyIt == m_AnimationNotifies.end()) return;

    const int frameCount = GetAnimationFrameCount(animationName);
    if (frameCount <= 0) return;

    // 切替直後は0フレームの通知も発火対象へ含める。
    const float rangeStart = animationChanged ? -0.0001f : previousFrame;
    if (currentFrame < rangeStart) return;

    // 今回の更新範囲に含まれる通知を一旦集め、発火順を後から整える。
    std::vector<AnimationNotify> firedNotifies;
    for (const AnimationNotify& notify : notifyIt->second)
    {
        if (notify.Frame >= static_cast<float>(frameCount)) continue;

        if (!m_IsLooping)
        {
            if (rangeStart < notify.Frame && notify.Frame <= currentFrame)
                firedNotifies.push_back(notify);
            continue;
        }

        // ループ再生では複数周回を一度に跨いだ場合も、通過した回数だけ通知する。
        const int firstLoop = (std::max)(0, static_cast<int>(rangeStart / frameCount));
        const int lastLoop = static_cast<int>(currentFrame / frameCount);
        for (int loop = firstLoop; loop <= lastLoop; ++loop)
        {
            const float notifyFrame = notify.Frame + static_cast<float>(loop * frameCount);
            if (rangeStart < notifyFrame && notifyFrame <= currentFrame)
                firedNotifies.push_back({ notifyFrame, notify.Name });
        }
    }

    // 複数周回を跨いだ場合も、実際に通過した時刻の順で通知する。
    std::stable_sort(
        firedNotifies.begin(),
        firedNotifies.end(),
        [](const AnimationNotify& lhs, const AnimationNotify& rhs)
        {
            return lhs.Frame < rhs.Frame;
        });

    // コールバック内で通知設定が変更されても、抽出済み一覧を使って安全に発火する。
    if (!m_NotifyCallback) return;
    for (const AnimationNotify& notify : firedNotifies)
    {
        m_NotifyCallback(animationName, notify.Name);
    }
}

void AnimationModel::Update(const char* AnimationName1, int Frame1, const char* AnimationName2, int Frame2, float BlendRate)
{
    // 両方のアニメーションが有効な場合のみ、ブレンド姿勢を計算する。
    auto it1 = m_Animation.find(AnimationName1);
    auto it2 = m_Animation.find(AnimationName2);
    if (it1 == m_Animation.end() || !it1->second || !it1->second->HasAnimations()) return;
    if (it2 == m_Animation.end() || !it2->second || !it2->second->HasAnimations()) return;

    aiAnimation* animation1 = it1->second->mAnimations[0];
    aiAnimation* animation2 = it2->second->mAnimations[0];
    if (!animation1 || !animation2) return;

    // フレーム番号をAssimpのtick時間へ変換し、キー間を補間する。
    double animTime1 = GetAnimationTime(animation1, Frame1);
    double animTime2 = GetAnimationTime(animation2, Frame2);

    // 全ボーンについて、2つのアニメーションから同名チャンネルを探す。
    for (auto pair : m_Bone)
    {
        BONE* bone = &m_Bone[pair.first];
        aiNodeAnim* nodeAnim1 = nullptr;
        aiNodeAnim* nodeAnim2 = nullptr;

        // 1つ目と2つ目のアニメーションから、このボーンのキーフレーム列を取得する。
        for (unsigned int c = 0; c < animation1->mNumChannels; c++)
        {
            if (animation1->mChannels[c]->mNodeName == aiString(pair.first))
            {
                nodeAnim1 = animation1->mChannels[c];
                break;
            }
        }
        for (unsigned int c = 0; c < animation2->mNumChannels; c++)
        {
            if (animation2->mChannels[c]->mNodeName == aiString(pair.first))
            {
                nodeAnim2 = animation2->mChannels[c];
                break;
            }
        }

        // チャンネルが無い場合に備え、移動なし・回転なし・等倍を初期姿勢とする。
        aiQuaternion rot1(1.0f, 0.0f, 0.0f, 0.0f);
        aiVector3D pos1(0.0f, 0.0f, 0.0f);
        aiVector3D scale1(1.0f, 1.0f, 1.0f);
        aiQuaternion rot2(1.0f, 0.0f, 0.0f, 0.0f);
        aiVector3D pos2(0.0f, 0.0f, 0.0f);
        aiVector3D scale2(1.0f, 1.0f, 1.0f);

        // キーが存在するチャンネルだけAssimpの値で上書きし、無いチャンネルはデフォルト姿勢を使う。
        if (nodeAnim1)
        {
            pos1 = CalcInterpolatedPosition(animTime1, nodeAnim1);
            rot1 = CalcInterpolatedRotation(animTime1, nodeAnim1);
            if (nodeAnim1->mNumScalingKeys > 0) scale1 = CalcInterpolatedScaling(animTime1, nodeAnim1);
        }
        if (nodeAnim2)
        {
            pos2 = CalcInterpolatedPosition(animTime2, nodeAnim2);
            rot2 = CalcInterpolatedRotation(animTime2, nodeAnim2);
            if (nodeAnim2->mNumScalingKeys > 0) scale2 = CalcInterpolatedScaling(animTime2, nodeAnim2);
        }

        // 2つのアニメーションを同じボーン名でブレンドする。
        aiVector3D pos = pos1 * (1.0f - BlendRate) + pos2 * BlendRate;
        aiQuaternion rot;
        aiQuaternion::Interpolate(rot, rot1, rot2, BlendRate);
        aiVector3D scale = scale1 * (1.0f - BlendRate) + scale2 * BlendRate;

        bone->AnimationMatrix = aiMatrix4x4(scale, rot, pos);
    }

    // ルートから子へ姿勢を伝播し、各ボーンの最終行列を作成する。
    aiMatrix4x4 rootMatrix = aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f), aiQuaternion((float)AI_MATH_PI, 0.0f, 0.0f), aiVector3D(0.0f, 0.0f, 0.0f));
    UpdateBoneMatrix(m_AiScene->mRootNode, rootMatrix);

    // 未使用ボーンを単位行列で埋め、登録済みボーンだけを定数バッファへ設定する。
    CONSTANT_BUFFER_BONE cbBone{};
    for (int i = 0; i < MAX_BONES; i++) cbBone.BoneMatrices[i] = XMMatrixIdentity();
    for (int i = 0; i < (int)m_BoneNames.size(); i++)
    {
        if (i >= MAX_BONES) break;
        // HLSL側の行列の並びに合わせるため、転置してから渡す。
        cbBone.BoneMatrices[i] = XMMatrixTranspose(AiMatrixToXMMatrix(m_Bone[m_BoneNames[i]].Matrix));
    }
    Renderer::GetDeviceContext()->UpdateSubresource(m_BoneConstantBuffer, 0, nullptr, &cbBone, 0, 0);
}

void AnimationModel::Update(const char* AnimationName1, int Frame1)
{
    // 指定アニメーションが読み込まれていない場合は姿勢を変更しない。
    auto it = m_Animation.find(AnimationName1);
    if (it == m_Animation.end() || !it->second || !it->second->HasAnimations()) return;

    aiAnimation* animation = it->second->mAnimations[0];
    if (!animation) return;

    // 各ボーンに対応するチャンネルから、指定フレームの回転と位置を取得する。
    for (auto pair : m_Bone)
    {
        BONE* bone = &m_Bone[pair.first];
        aiNodeAnim* nodeAnim = nullptr;

        for (unsigned int c = 0; c < animation->mNumChannels; c++)
        {
            if (animation->mChannels[c]->mNodeName == aiString(pair.first))
            {
                nodeAnim = animation->mChannels[c];
                break;
            }
        }

        aiQuaternion rot(1.0f, 0.0f, 0.0f, 0.0f);
        aiVector3D pos(0.0f, 0.0f, 0.0f);

        // キー配列の範囲内へフレームを折り返し、該当キーをそのまま使用する。
        if (nodeAnim)
        {
            if (nodeAnim->mNumRotationKeys > 0) {
                int f = Frame1 % nodeAnim->mNumRotationKeys;
                rot = nodeAnim->mRotationKeys[f].mValue;
            }

            if (nodeAnim->mNumPositionKeys > 0) {
                int f = Frame1 % nodeAnim->mNumPositionKeys;
                pos = nodeAnim->mPositionKeys[f].mValue;
            }
        }

        bone->AnimationMatrix = aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f), rot, pos);
    }

    // 単体再生でも階層行列を更新し、GPU用の最終ボーン行列を作成する。
    aiMatrix4x4 rootMatrix = aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f), aiQuaternion((float)AI_MATH_PI, 0.0f, 0.0f), aiVector3D(0.0f, 0.0f, 0.0f));
    UpdateBoneMatrix(m_AiScene->mRootNode, rootMatrix);

    CONSTANT_BUFFER_BONE cbBone{};
    for (int i = 0; i < MAX_BONES; i++) cbBone.BoneMatrices[i] = XMMatrixIdentity();
    for (int i = 0; i < (int)m_BoneNames.size(); i++)
    {
        if (i >= MAX_BONES) break;
        cbBone.BoneMatrices[i] = XMMatrixTranspose(AiMatrixToXMMatrix(m_Bone[m_BoneNames[i]].Matrix));
    }
    Renderer::GetDeviceContext()->UpdateSubresource(m_BoneConstantBuffer, 0, nullptr, &cbBone, 0, 0);
}

void AnimationModel::UpdateBoneMatrix(aiNode* node, aiMatrix4x4 matrix)
{
    // 親の変換と現在ボーンのローカル姿勢を合成し、逆バインド行列を適用する。
    BONE* bone = &m_Bone[node->mName.C_Str()];
    aiMatrix4x4 worldMatrix = matrix * bone->AnimationMatrix;
    bone->Matrix = worldMatrix * bone->OffsetMatrix;

    // 合成済みの行列を親行列として、子ボーンへ再帰的に伝える。
    for (unsigned int n = 0; n < node->mNumChildren; n++)
    {
        UpdateBoneMatrix(node->mChildren[n], worldMatrix);
    }
}

double AnimationModel::GetAnimationTime(aiAnimation* anim, int frame)
{
    if (!anim || anim->mDuration <= 0.0) return 0.0;

    // 呼び出し側のFrameはtick相当で進んでいるため、durationでループさせる。
    return fmod((double)frame, anim->mDuration);
}

aiVector3D AnimationModel::CalcInterpolatedPosition(double animTime, aiNodeAnim* nodeAnim)
{
    // キーが無い場合は移動なし、1つだけならその値を返す。
    if (!nodeAnim || nodeAnim->mNumPositionKeys == 0) return aiVector3D(0.0f, 0.0f, 0.0f);
    if (nodeAnim->mNumPositionKeys == 1) return nodeAnim->mPositionKeys[0].mValue;

    // 指定時刻を挟む2キーを探し、経過割合で線形補間する。
    for (unsigned int i = 0; i < nodeAnim->mNumPositionKeys - 1; i++)
    {
        if (animTime < nodeAnim->mPositionKeys[i + 1].mTime)
        {
            double dt = nodeAnim->mPositionKeys[i + 1].mTime - nodeAnim->mPositionKeys[i].mTime;
            double factor = (animTime - nodeAnim->mPositionKeys[i].mTime) / dt;
            return nodeAnim->mPositionKeys[i].mValue * (float)(1.0 - factor) + nodeAnim->mPositionKeys[i + 1].mValue * (float)factor;
        }
    }
    // 最終キー以降の時刻では、最後の位置を維持する。
    return nodeAnim->mPositionKeys[nodeAnim->mNumPositionKeys - 1].mValue;
}

aiQuaternion AnimationModel::CalcInterpolatedRotation(double animTime, aiNodeAnim* nodeAnim)
{
    // キーが無い場合は回転なし、1つだけならその値を返す。
    if (!nodeAnim || nodeAnim->mNumRotationKeys == 0) return aiQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
    if (nodeAnim->mNumRotationKeys == 1) return nodeAnim->mRotationKeys[0].mValue;

    // 指定時刻を挟む2キーを球面線形補間し、回転の長さを正規化する。
    for (unsigned int i = 0; i < nodeAnim->mNumRotationKeys - 1; i++)
    {
        if (animTime < nodeAnim->mRotationKeys[i + 1].mTime)
        {
            double dt = nodeAnim->mRotationKeys[i + 1].mTime - nodeAnim->mRotationKeys[i].mTime;
            double factor = (animTime - nodeAnim->mRotationKeys[i].mTime) / dt;
            aiQuaternion out;
            aiQuaternion::Interpolate(out, nodeAnim->mRotationKeys[i].mValue, nodeAnim->mRotationKeys[i + 1].mValue, (float)factor);
            out.Normalize();
            return out;
        }
    }
    // 最終キー以降の時刻では、最後の回転を維持する。
    return nodeAnim->mRotationKeys[nodeAnim->mNumRotationKeys - 1].mValue;
}

aiVector3D AnimationModel::CalcInterpolatedScaling(double animTime, aiNodeAnim* nodeAnim)
{
    // キーが無い場合は等倍、1つだけならその値を返す。
    if (!nodeAnim || nodeAnim->mNumScalingKeys == 0) return aiVector3D(1.0f, 1.0f, 1.0f);
    if (nodeAnim->mNumScalingKeys == 1) return nodeAnim->mScalingKeys[0].mValue;

    // 指定時刻を挟む2キーを探し、経過割合で線形補間する。
    for (unsigned int i = 0; i < nodeAnim->mNumScalingKeys - 1; i++)
    {
        if (animTime < nodeAnim->mScalingKeys[i + 1].mTime)
        {
            double dt = nodeAnim->mScalingKeys[i + 1].mTime - nodeAnim->mScalingKeys[i].mTime;
            double factor = (animTime - nodeAnim->mScalingKeys[i].mTime) / dt;
            return nodeAnim->mScalingKeys[i].mValue * (float)(1.0 - factor) + nodeAnim->mScalingKeys[i + 1].mValue * (float)factor;
        }
    }
    // 最終キー以降の時刻では、最後の拡大率を維持する。
    return nodeAnim->mScalingKeys[nodeAnim->mNumScalingKeys - 1].mValue;
}

int AnimationModel::GetAnimationFrameCount(const std::string& name)
{
    // アニメーションの尺はAssimpのtick時間で管理されているため、フレーム数相当の整数値へ丸める。
    auto it = m_Animation.find(name);
    if (it == m_Animation.end()) return 0;

    const aiScene* scene = it->second;
    if (!scene || scene->mNumAnimations == 0) return 0;

    return static_cast<int>(scene->mAnimations[0]->mDuration);
}

XMMATRIX AnimationModel::AiMatrixToXMMatrix(aiMatrix4x4 m)
{
    // Assimp行列の各要素を同じ並びでDirectXMathの行列へ詰め替える。
    return XMMATRIX(
        m.a1, m.a2, m.a3, m.a4,
        m.b1, m.b2, m.b3, m.b4,
        m.c1, m.c2, m.c3, m.c4,
        m.d1, m.d2, m.d3, m.d4);
}
