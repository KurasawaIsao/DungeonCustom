#pragma once
#include "Vector3.h"
#include <string>
#include "Unit.h"

class AnimationModel;

class TitleCharacter : public Unit
{
public:
    enum class AppearType
    {
        WalkFromBack,   // 奥から歩いてくる
        Warp,           // エフェクト出現
        WaterJump       // 水から跳ねる
    };

    void Init(
        const std::string& modelName,
        AppearType type,
        const Vector3& targetPos, const std::string& tag);
    void Init()override {};
    void Update() override;
    void Draw() override;
    void Uninit() override;
    void UpdateUnit()override {}; 
    void OnDeath(Unit* attacker = nullptr)override {};
    void AddAnimation(
        const std::string& name,
        const std::string& fbxPath);
    void SetEnable(bool enable) { m_Enable = enable; }
    void Start();
    void Attack()override {};

private:
    void UpdateWalk();
    void UpdateWarp();
    void UpdateWater();
    


    bool m_ReachedTarget = false;
    bool m_Enable;



private:

    AppearType m_Type;
    float m_Timer = 0.0f;


    Vector3 m_StartPos;
    Vector3 m_TargetPos;

    bool m_EffectPlayed = false;
    bool m_SplashPlayed = false;


};
