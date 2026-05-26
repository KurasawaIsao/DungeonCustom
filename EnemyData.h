#pragma once
#include <string>
#include <vector>
#include "Vector3.h"
#include "SkillData.h"

enum class EnemyAIType
{
    // 通常巡回のみ。プレイヤーを見つけても追跡状態へ移らない。
    Patrol,
    // 巡回中に敵対対象を見つけると ChaseAI へ切り替える基本敵AI。
    PatrolAndChase,
    // 一定距離を保つAI。近い時は逃げ、遠い時は追う。
    KeepDistance,
    // 勢力を問わず視界内の全ユニットを攻撃対象にする無差別AI。
    Berserk,
    // 敵対対象から離れるAI。逃走先がない時は巡回に近い動きになる。
    RunAway,
    // ランダム移動を混ぜるが、隣接時の攻撃は必ず行う。
    WhimAlwaysAttack,
    // ランダム移動を混ぜ、隣接時の攻撃も確率で行う。
    WhimRandomAttack,
    // 自分からは行動しない受け身AI。
    Passive
};

enum class EnemyTurnSpeedType
{
    Slow,
    Normal,
    Fast,
    Triple
};
struct EnemyVisualData
{
    std::string modelPath;

    std::string animIdle;
    std::string animRun;
    std::string animAttack;
    std::string animDamaged;
    std::string animSkill;
    std::string animSleep;

    Vector3 scale;
    float yOffset;
};
enum class SleepType
{
    None,           
    WakeOnRoom,     
    WakeOnAdjacent,
    WakeOnDamage    
};
struct EnemySpawnEntry
{
    std::string id;
    int weight;
};
struct EnemySpawnTable
{
    std::string tableId;
    std::vector<EnemySpawnEntry> entries;
};

struct EnemyData
{
    EnemyData() {}
    EnemyData(
        const std::string& id_, const std::string& name_,
        int hp_, int atk_, int def_, int acc_, int evd_, int exp_,
        int vision_, int recruitMod_,
        float dropRate_, const std::string& fixedItem_,
        float Sleep, SleepType type,
        const std::vector<Skill>& skills_,
        EnemyAIType ai_,
        EnemyTurnSpeedType actionSpeed_,
        EnemyTurnSpeedType moveSpeed_,
        int keepDistance_,
        EnemyVisualData vi_
    )
        : id(id_), name(name_), maxHP(hp_), atk(atk_), def(def_), acc(acc_), evd(evd_)
        , expReward(exp_), visionRange(vision_)
        , recruitmentModifier(recruitMod_)
        , dropRate(dropRate_)     
        , sleepRate(Sleep)
        , sleepType(type)
        , fixedItemId(fixedItem_)  
        , skills(skills_)
        , aiType(ai_)
        , actionSpeed(actionSpeed_)
        , moveSpeed(moveSpeed_)
        , keepDistance(keepDistance_)
        , visual(vi_)
    {}

    std::string id;
    std::string name;

    std::string talkMessage[3];
    int maxHP;
    int atk;
    int def;

    int acc;
    int evd;

    int expReward;

    int visionRange;

    int recruitmentModifier;
    std::vector<Skill> skills;


    float dropRate;
    std::string fixedItemId;

    float sleepRate;
    SleepType sleepType;

    EnemyAIType aiType;
    EnemyTurnSpeedType actionSpeed = EnemyTurnSpeedType::Normal;
    EnemyTurnSpeedType moveSpeed = EnemyTurnSpeedType::Normal;
    int keepDistance = 2;

    EnemyVisualData visual;
};
