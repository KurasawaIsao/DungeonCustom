#include "TitleCharacter.h"
#include "animationModel.h"
#include "renderer.h"
#include "WarpParticle.h"
#include "SplashParticle.h"
#include "manager.h"
#include "scene.h"

void TitleCharacter::Init(
    const std::string& modelName,
    AppearType type,
    const Vector3& targetPos, const std::string& tag)
{
    m_AnimationModel = new AnimationModel();
    m_AnimationModel->Load(modelName.c_str());
    m_AnimNow = tag;
    InitToonShader();

    m_Type = type;
    m_TargetPos = targetPos;

    switch (m_Type)
    {
    case AppearType::WalkFromBack:
        m_StartPos = targetPos + Vector3(0, 2.0f, -20);

        break;

    case AppearType::Warp:
        m_StartPos = targetPos;
        break;

    case AppearType::WaterJump:
        m_StartPos = targetPos + Vector3(0, -12.0f, 0);
        break;
    }
    m_AnimSpeed = 0.5f;
    SetPosition(m_StartPos);
}
void TitleCharacter::Start()
{
    SetEnable(true);

    SetPosition(m_StartPos);
    m_Timer = 0.0f;          // タイマーを0に戻して演出を最初からにする
    m_EffectPlayed = false;  // パーティクル発生フラグを戻す
    m_SplashPlayed = false;  // 水しぶきフラグを戻す
    m_ReachedTarget = false;
}
void TitleCharacter::Update()
{
    float dt = 1.0f / 60.0f;

    m_Timer += dt;
    UpdateAnimation();
    switch (m_Type)
    {
    case AppearType::WalkFromBack:
        UpdateWalk();
        break;

    case AppearType::Warp:
        UpdateWarp();
        break;

    case AppearType::WaterJump:
        UpdateWater();
        break;
    }
}
void TitleCharacter::UpdateWalk()
{
    float t = Vector3::Clamp(m_Timer / 2.0f, 0.0f, 1.0f);
    SetPosition(Vector3::Lerp(m_StartPos, m_TargetPos, t));

    if (t >= 1.0f && !m_ReachedTarget)
    {
        PlayAnimation("Idle",0.5f);
        m_ReachedTarget = true;
    }
}

void TitleCharacter::UpdateWarp()
{
    if (!m_EffectPlayed && m_Enable)
    {
        WarpParticle* warp = Manager::GetScene()->AddGameObject<WarpParticle>(1);

        warp->Center = m_TargetPos;

        for (int i = 0; i < 60; i++)
        {
            warp->Spawn(
                m_TargetPos,
                Vector4(0.6f, 0.8f, 1.0f, 1.0f)
            );
        }

        SetPosition(m_TargetPos);
        m_EffectPlayed = true;
    }

    if (IsAnimationFinished())
    {
        PlayAnimation("Idle",1.0f);
    }
}

void TitleCharacter::UpdateWater()
{
   
    if (m_Timer < 0.5f)
    {
        float t = m_Timer / 0.5f;
        SetPosition(Vector3::Lerp(m_StartPos, m_TargetPos, t));
    }
    else
    {
        if (!m_SplashPlayed && m_Enable)
        {
            auto* splash = Manager::GetScene()->AddGameObject<SplashParticle>(1);
            Vector3 p = m_TargetPos;
            Vector3 offset = { 0.0f,-3.0f,0.0f };
            for (int i = 0; i < 40; i++)
            {
                splash->Spawn(m_TargetPos + offset);
            }

            m_SplashPlayed = true;
            PlayAnimation("Idle", 0.5f);
            
        }
       
        Vector3 pos = m_TargetPos;
        pos.y += sinf((m_Timer - 0.5f) * 2.0f) * 0.15f;
        SetPosition(pos);
    }
}

void TitleCharacter::Draw()
{
    if (!m_AnimationModel) return;
    if (!m_Enable) return;

    XMMATRIX world, trans, rot, scale;
    scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    trans = XMMatrixTranslation(m_Position.x, m_Position.y + 1.3f, m_Position.z);
    world = scale * rot * trans;

    DrawToonModel(world);
}
void TitleCharacter::Uninit()
{
    if (m_AnimationModel)
    {
        m_AnimationModel->Uninit();
        delete m_AnimationModel;
        ReleaseToonShader();

    }
}
void TitleCharacter::AddAnimation(
    const std::string& name,
    const std::string& fbxPath)
{
    if (!m_AnimationModel) return;
    m_AnimationModel->LoadAnimation(fbxPath.c_str(), name.c_str());
}

