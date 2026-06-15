#pragma once
#include <string>
#include <memory>
#include "EffectBase.h"
struct Skill {
    std::string name;
    float useProbability; // 確率
    std::shared_ptr<EffectBase> effect; // エフェクト
    EffectTargetType targetType = EffectTargetType::Single;
    int targetRadius = 1;
    // 固定確率か、部屋内ユニット数で確率を変えるかを指定する。
    enum class ProbabilityType {
        Fixed,
        UserRoomUnitCount
    } probabilityType = ProbabilityType::Fixed;
    // UserRoomUnitCount時、部屋内ユニット1体につき加算する確率。0.05f = 5%。
    float roomUnitProbabilityStep = 0.05f;

    enum class Condition {
        Always,
        AdjacentToTarget, // ターゲットが隣接
        HPBelowHalf,      // 自身HPが半分以下
    } condition;

    // trueの場合はSkillEffect Notifyまで効果適用を待ち、falseの場合は宣言直後に適用する。
    bool waitForAnimationNotify = false;

    Skill(std::string n, float prob, std::shared_ptr<EffectBase> eff, Condition cond,
        EffectTargetType target = EffectTargetType::Single, int radius = 1, bool waitForNotify = false)
        : name(n), useProbability(prob), effect(eff), targetType(target), targetRadius(radius),
          condition(cond), waitForAnimationNotify(waitForNotify) {}

    static Skill WithRoomUnitProbability(std::string n, float baseProb, std::shared_ptr<EffectBase> eff, Condition cond,
        EffectTargetType target = EffectTargetType::UserRoom, int radius = 1, float step = 0.05f,
        bool waitForNotify = false)
    {
        // 部屋全体特技用。基本確率に、使用者の部屋にいる追加ユニット数ぶんの確率を足す。
        Skill skill(n, baseProb, eff, cond, target, radius, waitForNotify);
        skill.probabilityType = ProbabilityType::UserRoomUnitCount;
        skill.roomUnitProbabilityStep = step;
        return skill;
    }
};
