#include "main.h"
#include "renderer.h"
#include "animationModel.h"
#include "DebugTrace.h"
#include <fstream>

namespace
{
    bool AnimationFileExists(const char* fileName)
    {
        if (!fileName || fileName[0] == '\0') return false;
        std::ifstream file(fileName, std::ios::binary);
        return file.good();
    }
}

void AnimationModel::Draw()
{
    if (!m_AiScene || !m_VertexBuffer || !m_IndexBuffer) return;

    Renderer::GetDeviceContext()->VSSetConstantBuffers(7, 1, &m_BoneConstantBuffer);
    // このモデルは三角形リスト前提。描画前に入力アセンブラの状態を明示しておく。
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    MATERIAL material;
    ZeroMemory(&material, sizeof(material));
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = false;
    Renderer::SetMaterial(material);

    for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
    {
        aiMesh* mesh = m_AiScene->mMeshes[m];

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

        UINT stride = sizeof(VERTEX_3D);
        UINT offset = 0;
        Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer[m], &stride, &offset);
        Renderer::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);
        Renderer::GetDeviceContext()->DrawIndexed(mesh->mNumFaces * 3, 0, 0);
    }
}

void AnimationModel::Load(const char* FileName)
{
    if (!FileName || FileName[0] == '\0') {
        DebugTrace::Log("MODEL_LOAD_REJECT_EMPTY file=", FileName ? FileName : "null");
        return;
    }
    if (!AnimationFileExists(FileName)) {
        DebugTrace::Log("MODEL_LOAD_REJECT_MISSING file=", FileName);
        return;
    }

    m_AiScene = aiImportFile(FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);
    if (!m_AiScene)
    {
        DebugTrace::Log("MODEL_LOAD_FAILED file=", FileName,
            " error=", aiGetErrorString() ? aiGetErrorString() : "null");
        return;
    }

    m_VertexBuffer = new ID3D11Buffer * [m_AiScene->mNumMeshes];
    m_IndexBuffer = new ID3D11Buffer * [m_AiScene->mNumMeshes];
    m_DeformVertex = new std::vector<DEFORM_VERTEX>[m_AiScene->mNumMeshes];

    // ノード階層からボーン辞書を先に作り、アニメーション更新時に名前で引けるようにする。
    CreateBone(m_AiScene->mRootNode);

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
    const char* animName = (Name && Name[0] != '\0') ? Name : "<null>";
    if (!FileName || FileName[0] == '\0' || !Name || Name[0] == '\0') {
        DebugTrace::Log("ANIMATION_LOAD_REJECT_EMPTY file=", FileName ? FileName : "null",
            " name=", animName);
        if (Name) m_Animation.erase(Name);
        return;
    }

    if (!AnimationFileExists(FileName)) {
        DebugTrace::Log("ANIMATION_LOAD_REJECT_MISSING file=", FileName, " name=", animName);
        m_Animation.erase(Name);
        return;
    }

    const aiScene* scene = aiImportFile(FileName, aiProcess_ConvertToLeftHanded);


    if (!scene->HasAnimations()) {
        DebugTrace::Log("ANIMATION_LOAD_NO_ANIMATION file=", FileName, " name=", animName);
        aiReleaseImport(scene);
        m_Animation.erase(Name);
        return;
    }

    auto old = m_Animation.find(Name);
    if (old != m_Animation.end() && old->second && old->second != scene && m_OwnsImportedScenes) {
        aiReleaseImport(old->second);
    }
    m_Animation[Name] = scene;
}

void AnimationModel::CreateBone(aiNode* node)
{
    BONE bone{};
    aiMatrix4x4 identity(aiVector3D(1.0f, 1.0f, 1.0f), aiQuaternion(1.0f, 0.0f, 0.0f, 0.0f), aiVector3D(0.0f, 0.0f, 0.0f));
    bone.Matrix = identity;
    bone.AnimationMatrix = identity;
    bone.OffsetMatrix = identity;
    bone.DefaultMatrix = identity;
    m_Bone[node->mName.C_Str()] = bone;

    for (unsigned int n = 0; n < node->mNumChildren; n++)
    {
        CreateBone(node->mChildren[n]);
    }
}

void AnimationModel::CreateClone(const AnimationModel& src)
{
    if (!src.m_AiScene || !src.m_VertexBuffer || !src.m_IndexBuffer)
    {
        DebugTrace::Log("CREATE_CLONE_REJECT_EMPTY src=", static_cast<const void*>(&src));
        return;
    }

    m_AiScene = src.m_AiScene;
    m_OwnsImportedScenes = false;
    m_Animation = src.m_Animation;
    m_Texture = src.m_Texture;
    for (auto& texture : m_Texture)
    {
        if (texture.second) texture.second->AddRef();
    }
    m_Bone = src.m_Bone;
    m_BoneNames = src.m_BoneNames;

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

    D3D11_BUFFER_DESC cbd;
    ZeroMemory(&cbd, sizeof(cbd));
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(CONSTANT_BUFFER_BONE);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;
    Renderer::GetDevice()->CreateBuffer(&cbd, nullptr, &m_BoneConstantBuffer);

    NowFrame = 0.0f;
}

int AnimationModel::GetBoneIndex(const std::string& name)
{
    auto it = std::find(m_BoneNames.begin(), m_BoneNames.end(), name);
    if (it != m_BoneNames.end())
    {
        return (int)std::distance(m_BoneNames.begin(), it);
    }

    int index = (int)m_BoneNames.size();
    m_BoneNames.push_back(name);
    return index;
}

void AnimationModel::Uninit()
{
    if (!m_AiScene) return;

    if (m_BoneConstantBuffer)
    {
        m_BoneConstantBuffer->Release();
        m_BoneConstantBuffer = nullptr;
    }

    for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
    {
        if (m_VertexBuffer && m_VertexBuffer[m]) m_VertexBuffer[m]->Release();
        if (m_IndexBuffer && m_IndexBuffer[m]) m_IndexBuffer[m]->Release();
    }

    delete[] m_VertexBuffer;
    delete[] m_IndexBuffer;
    delete[] m_DeformVertex;
    m_VertexBuffer = nullptr;
    m_IndexBuffer = nullptr;
    m_DeformVertex = nullptr;

    for (std::pair<const std::string, ID3D11ShaderResourceView*> pair : m_Texture)
    {
        pair.second->Release();
    }
    m_Texture.clear();

    if (m_OwnsImportedScenes)
    {
        aiReleaseImport(m_AiScene);
        for (std::pair<const std::string, const aiScene*> pair : m_Animation)
        {
            if (pair.second) aiReleaseImport(pair.second);
        }
    }
    m_AiScene = nullptr;
    m_Animation.clear();
}

void AnimationModel::Update(const char* AnimationName1, int Frame1, const char* AnimationName2, int Frame2, float BlendRate)
{
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

    for (auto pair : m_Bone)
    {
        BONE* bone = &m_Bone[pair.first];
        aiNodeAnim* nodeAnim1 = nullptr;
        aiNodeAnim* nodeAnim2 = nullptr;

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

    aiMatrix4x4 rootMatrix = aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f), aiQuaternion((float)AI_MATH_PI, 0.0f, 0.0f), aiVector3D(0.0f, 0.0f, 0.0f));
    UpdateBoneMatrix(m_AiScene->mRootNode, rootMatrix);

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
    auto it = m_Animation.find(AnimationName1);
    if (it == m_Animation.end() || !it->second || !it->second->HasAnimations()) return;

    aiAnimation* animation = it->second->mAnimations[0];
    if (!animation) return;

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
    BONE* bone = &m_Bone[node->mName.C_Str()];
    aiMatrix4x4 worldMatrix = matrix * bone->AnimationMatrix;
    bone->Matrix = worldMatrix * bone->OffsetMatrix;

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
    if (!nodeAnim || nodeAnim->mNumPositionKeys == 0) return aiVector3D(0.0f, 0.0f, 0.0f);
    if (nodeAnim->mNumPositionKeys == 1) return nodeAnim->mPositionKeys[0].mValue;

    for (unsigned int i = 0; i < nodeAnim->mNumPositionKeys - 1; i++)
    {
        if (animTime < nodeAnim->mPositionKeys[i + 1].mTime)
        {
            double dt = nodeAnim->mPositionKeys[i + 1].mTime - nodeAnim->mPositionKeys[i].mTime;
            double factor = (animTime - nodeAnim->mPositionKeys[i].mTime) / dt;
            return nodeAnim->mPositionKeys[i].mValue * (float)(1.0 - factor) + nodeAnim->mPositionKeys[i + 1].mValue * (float)factor;
        }
    }
    return nodeAnim->mPositionKeys[nodeAnim->mNumPositionKeys - 1].mValue;
}

aiQuaternion AnimationModel::CalcInterpolatedRotation(double animTime, aiNodeAnim* nodeAnim)
{
    if (!nodeAnim || nodeAnim->mNumRotationKeys == 0) return aiQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
    if (nodeAnim->mNumRotationKeys == 1) return nodeAnim->mRotationKeys[0].mValue;

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
    return nodeAnim->mRotationKeys[nodeAnim->mNumRotationKeys - 1].mValue;
}

aiVector3D AnimationModel::CalcInterpolatedScaling(double animTime, aiNodeAnim* nodeAnim)
{
    if (!nodeAnim || nodeAnim->mNumScalingKeys == 0) return aiVector3D(1.0f, 1.0f, 1.0f);
    if (nodeAnim->mNumScalingKeys == 1) return nodeAnim->mScalingKeys[0].mValue;

    for (unsigned int i = 0; i < nodeAnim->mNumScalingKeys - 1; i++)
    {
        if (animTime < nodeAnim->mScalingKeys[i + 1].mTime)
        {
            double dt = nodeAnim->mScalingKeys[i + 1].mTime - nodeAnim->mScalingKeys[i].mTime;
            double factor = (animTime - nodeAnim->mScalingKeys[i].mTime) / dt;
            return nodeAnim->mScalingKeys[i].mValue * (float)(1.0 - factor) + nodeAnim->mScalingKeys[i + 1].mValue * (float)factor;
        }
    }
    return nodeAnim->mScalingKeys[nodeAnim->mNumScalingKeys - 1].mValue;
}

int AnimationModel::GetAnimationFrameCount(const std::string& name)
{
    auto it = m_Animation.find(name);
    if (it == m_Animation.end()) return 0;

    const aiScene* scene = it->second;
    if (!scene || scene->mNumAnimations == 0) return 0;

    return static_cast<int>(scene->mAnimations[0]->mDuration);
}

XMMATRIX AnimationModel::AiMatrixToXMMatrix(aiMatrix4x4 m)
{
    return XMMATRIX(
        m.a1, m.a2, m.a3, m.a4,
        m.b1, m.b2, m.b3, m.b4,
        m.c1, m.c2, m.c3, m.c4,
        m.d1, m.d2, m.d3, m.d4);
}