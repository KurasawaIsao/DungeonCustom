#pragma once
#include "Vector2Int.h"

class Unit;
class Player;
class ItemInstance;

enum class EffectSourceType {
    Item,
    Skill,
    Trap
};

enum class ItemCommandType
{
    Use,
    Throw
};

enum class EffectRank {
    Normal,
    Blessed,
    Cursed
};

enum class EffectTargetType {
    Single,
    RoomAndVision,
    TargetAndAround,
    TargetAndRadius,
    // 使用者がいる部屋全体を対象にする。主に敵の部屋全体特技で使う。
    UserRoom,
    // 使用者の周囲8マスだけを対象にする。中心にいる使用者自身は含めない。
    UserAround8
};

struct EffectContext {
    EffectSourceType source = EffectSourceType::Item;
    Unit* user = nullptr;
    Unit* target = nullptr;
    Vector2Int pos = { 0, 0 };
    Vector2Int direction = { 0, 0 };
    EffectRank rank = EffectRank::Normal;
    EffectTargetType targetType = EffectTargetType::Single;
    int targetRadius = 1;
};

class EffectBase {
public:
    virtual ~EffectBase() = default;
    virtual void Apply(const EffectContext& ctx) = 0;
};
