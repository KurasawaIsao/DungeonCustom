#include "GameRandom.h"
#include "EnemyDatabase.h"
#include "EnemyTableDatabase.h"
#include "WarpEffect.h"
#include "HealEffect.h"
#include "StatusEffect.h"
#include "EnemyModelManager.h"
#include "ItemDataBase.h"
#include "JsonIO.h"
#include "MessageLog.h"
#include <algorithm>
#include <utility>

std::unordered_map<std::string, EnemyData>
EnemyDatabase::m_Data;


namespace
{
    // Skillはshared_ptrを受け取るが、ここではデータベース共通の効果インスタンスを参照だけ渡す。
    StatusEffect g_Confuse(Status::Confusion, 5);
    StatusEffect g_Paralysis(Status::Paralysis, -1);
    StatusEffect g_Sleep(Status::Sleep, 3);
    WarpEffect g_Warp;

    void LogDataWarning(const std::string& message)
    {
        MessageLog::Instance().AddMessage("[Data] " + message);
    }
}


static void SetCombatAnimationNotifies(
    EnemyData& data,
    float attackHit,
    float skillEffect)
{
    // 現在使用する攻撃と特技だけを登録し、未使用アニメーションには通知を持たせない。
    data.visual.animationNotifies = {
        { "Attack", attackHit, "AttackHit" },
        { "Skill", skillEffect, "SkillEffect" }
    };
}

void EnemyDatabase::Register(EnemyData data)
{
    // 登録キーはEnemyData自身のIDから取得し、呼び出し側での二重指定をなくす。
    if (data.id.empty())
    {
        LogDataWarning("Enemy has empty id.");
        return;
    }

    const std::string id = data.id;
    const auto result = m_Data.emplace(id, std::move(data));
    if (!result.second)
    {
        LogDataWarning("Duplicate enemy id: " + id);
    }
}

void EnemyDatabase::Init()
{
    m_Data.clear();

    // =========================
    // EnemyData登録
    // =========================

    //順に
    //HP、ATK、DEF,ACC,EVD,EXP,視界範囲、勧誘修正値、アイテムドロップ率、ドロップするアイテム(ない場合アイテムテーブル)
	//仮眠率、仮眠タイプ、スキル、AIタイプ(1:〇回行動、2:〇倍速移動)、行動速度、移動速度、距離を保つタイプの敵の理想距離、ビジュアルデータ

    Register(
        EnemyData(
            "Kingyo", u8"にらみきんぎょ",
            9, 5, 2, 90, 5, 3, 8,
            10, 0.2f,"",1.0f,SleepType::WakeOnRoom,
            {
                Skill(u8"あやしい光", 0.1f, std::shared_ptr<EffectBase>(&g_Confuse, [](EffectBase*) {}), Skill::Condition::AdjacentToTarget, EffectTargetType::Single)
            },
            EnemyAIType::PatrolAndChase,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Normal, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\kingyo.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Attack.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Damaged.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Skill.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Sleep.fbx",
                {0.9f, 0.9f, 0.9f},
                1.0f
            }
        )
    );

    m_Data["Kingyo"].talkMessage[0] = u8"プクプク……。";
    m_Data["Kingyo"].talkMessage[1] = u8"がんばるよ！";
    m_Data["Kingyo"].talkMessage[2] = u8"";
    Register(
        EnemyData(
            "Obie", u8"おびえピラニア",
            10, 12, 4, 90, 5, 5, 10,
            1, 0.1f, "",0.0f, SleepType::WakeOnRoom,
            {
                
            },
            EnemyAIType::RunAway,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Fast, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\Obie.fbx",
                  "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Attack.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Damaged.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Skill.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Sleep.fbx",
                {0.9f, 0.9f, 0.9f},
                1.0f
            }
        )
    );
    m_Data["Obie"].talkMessage[0] = u8"うぅ...いやだよぉ...";
    m_Data["Obie"].talkMessage[1] = u8"戦いたくないよぉ...";
    m_Data["Obie"].talkMessage[2] = u8"";

    Register(
        EnemyData(
            "FishEye", u8"フィッシュアイ",
            13, 7, 4, 90, 5, 5, 10,
            2, 0.1f, "", 1.0f, SleepType::WakeOnRoom,
            {

            },
            EnemyAIType::WhimAlwaysAttack,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Normal, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\FishEye.fbx",
                  "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Attack.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Damaged.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Skill.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Sleep.fbx",
                {0.9f, 0.9f, 0.9f},
                1.0f
            }
        )
    );
    m_Data["FishEye"].talkMessage[0] = u8"間違ってオイラを食っちゃダメだよ。";
    m_Data["FishEye"].talkMessage[1] = u8"腹こわすからな！";
    m_Data["FishEye"].talkMessage[2] = u8"";

    Register(
        EnemyData(
            "Uonome", u8"うおのめかいぎょ",
            26, 10, 6, 90, 7, 8, 10,
            5, 0.1f, "", 0.2f, SleepType::WakeOnRoom,
            {
                Skill(u8"八方にらみ", 0.3f, std::shared_ptr<EffectBase>(&g_Confuse, [](EffectBase*) {}), Skill::Condition::Always, EffectTargetType::UserAround8)
            },
            EnemyAIType::PatrolAndChase,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Normal, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\Uonome.fbx",
              "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Attack.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Damaged.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Skill.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Sleep.fbx",
                {0.9f, 0.9f, 0.9f},
                1.0f
            }
        )
    );
    m_Data["Uonome"].talkMessage[0] = u8"オイラなんでか知らないけど";
    m_Data["Uonome"].talkMessage[1] = u8"足あるやつみんな嫌いなんだよなぁ";
    m_Data["Uonome"].talkMessage[2] = u8"";

    Register(
        EnemyData(
            "PypeDevil", u8"パイプデビル",
            29, 14, 8, 90, 8, 12, 10,
            0, 0.1f, "", 0.6f, SleepType::WakeOnAdjacent,
            {
                Skill(u8"甘い息", 0.2f, std::shared_ptr<EffectBase>(&g_Sleep, [](EffectBase*) {}),
                    Skill::Condition::AdjacentToTarget, EffectTargetType::Single, 1, true)
            },
            EnemyAIType::PatrolAndChase,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Normal, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\PypeDevil.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Idle.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Idle.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Attack.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Damage.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Skill.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Sleep.fbx",
                {0.9f, 0.9f, 0.9f},
                1.0f
            }
        )
    );
    m_Data["PypeDevil"].talkMessage[0] = u8"そなた、悪魔を従えるなど...";
    m_Data["PypeDevil"].talkMessage[1] = u8"やりおるな。";
    m_Data["PypeDevil"].talkMessage[2] = u8"";

    Register(
        EnemyData(
            "BlueJoker", u8"ブルージョーカー",
            36, 18, 10, 92, 10, 15, 12,
            -5, 0.1f, "", 0.1f, SleepType::WakeOnAdjacent,
            {
                Skill(u8"かなしばりの術", 0.3f, std::shared_ptr<EffectBase>(&g_Paralysis, [](EffectBase*) {}), Skill::Condition::HPBelowHalf,EffectTargetType::Single, 1, true)
            },
            EnemyAIType::PatrolAndChase,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Normal, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\BlueJoker.fbx",
                 "Asset\\Model\\EnemyModel\\tormasX_Idle.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Idle.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Attack.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Damage.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Skill.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Sleep.fbx",
                {0.9f, 0.9f, 0.9f},
                1.0f
            }
        )
    );
    m_Data["BlueJoker"].talkMessage[0] = u8"ワタクシの姿は仮の物。";
    m_Data["BlueJoker"].talkMessage[1] = u8"これも例外ではありませんよ！";
    m_Data["BlueJoker"].talkMessage[2] = u8"";

    Register(
        EnemyData(
            "Tormas", u8"トーマス・パクター",
            44, 22, 12, 92, 12, 20, 12,
            0, 0.0f, "", 0.0f, SleepType::WakeOnAdjacent,
            {
                Skill(u8"強風", 0.2f, std::shared_ptr<EffectBase>(&g_Warp, [](EffectBase*) {}), Skill::Condition::AdjacentToTarget, EffectTargetType::Single, 1, true),
                  Skill(u8"あやしい光", 0.15f, std::shared_ptr<EffectBase>(&g_Confuse, [](EffectBase*) {}), Skill::Condition::AdjacentToTarget, EffectTargetType::Single, 1, true),
            },
            EnemyAIType::PatrolAndChase,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Normal, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\tormasX.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Idle.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Idle.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Attack.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Damage.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Skill.fbx",
                "Asset\\Model\\EnemyModel\\tormasX_Sleep.fbx",
                {0.9f, 0.9f, 0.9f},
                1.0f
            }
        )
    );
    m_Data["Tormas"].talkMessage[0] = u8"嫌がらせがすぎるだと？";
    m_Data["Tormas"].talkMessage[1] = u8"何を言う。これも戦略であるぞ。";
    m_Data["Tormas"].talkMessage[2] = u8"";
    Register(
        EnemyData(
            "Blood", u8"返り血アーマー",
            13, 26, 10, 92, 12, 50, 12,
            -5, 0.0f, "", 0.3f, SleepType::WakeOnRoom,
            {},
            EnemyAIType::Berserk,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Fast, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\Blood.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Idle.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Walk.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Attack.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Damaged.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Attack.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Sleep.fbx",
                {0.5f, 0.5f, 0.5f},
                1.0f
            }
        )
    );
    m_Data["Blood"].talkMessage[0] = u8"お前敵か？ なに！ちがうのか！";
    m_Data["Blood"].talkMessage[1] = u8"なんでもいい、殴らせろ！";
    m_Data["Blood"].talkMessage[2] = u8"";
    Register(
        EnemyData(
            "Armor", u8"めだまのよろい",
            54, 16, 22, 92, 12, 34, 12,
            -5, 0.0f, "", 0.3f, SleepType::WakeOnRoom,
            {},
            EnemyAIType::KeepDistance,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Normal, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\Armor.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Idle.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Walk.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Attack.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Damaged.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Attack.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Sleep.fbx",
                {0.5f, 0.5f, 0.5f},
                1.0f
            }
        )
    );
    m_Data["Armor"].talkMessage[0] = u8"みんな勘違いしてるみたいだけどさ";
    m_Data["Armor"].talkMessage[1] = u8"本体はおなかの方だよ。";
    m_Data["Armor"].talkMessage[2] = u8"";

    Register(
        EnemyData(
            "Dark", u8"ダークゲイザー",
            74, 46, 16, 82, 12, 46, 12,
            -5, 0.1f, "", 0.3f, SleepType::WakeOnRoom,
            {},
            EnemyAIType::PatrolAndChase,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Normal, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\DarkGazer.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Idle.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Walk.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Attack.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Damaged.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Attack.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Sleep.fbx",
                {0.5f, 0.5f, 0.5f},
                1.0f
            }
        )
    );
    m_Data["Dark"].talkMessage[0] = u8"汝ノ敵ハ罪人ナリ。";
    m_Data["Dark"].talkMessage[1] = u8"私ハタダ罪ヲ裁クノミ。";
    m_Data["Dark"].talkMessage[2] = u8"";



    Register(
        EnemyData(
            "CurseFish", u8"災厄回遊魚",
            58, 20, 14, 90, 10, 42, 12,
            75, 0.1f, "", 0.2f, SleepType::WakeOnRoom,
            {
                Skill::WithRoomUnitProbability(u8"災厄の瞳", 0.05f, std::shared_ptr<EffectBase>(&g_Confuse, [](EffectBase*) {}), Skill::Condition::Always, EffectTargetType::UserRoom, 2, 0.05f)
            },
            EnemyAIType::PatrolAndChase,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Normal, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\Curse.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Idle.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Attack.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Damaged.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Skill.fbx",
                "Asset\\Model\\EnemyModel\\kingyo_Sleep.fbx",
                {1.2f, 1.2f, 1.2f},
                1.0f
            }
        )
    );
    m_Data["CurseFish"].talkMessage[0] = u8"僕役に立てるかなぁ？";
    m_Data["CurseFish"].talkMessage[1] = u8"あんまりこっち見ないでね。";
    m_Data["CurseFish"].talkMessage[2] = u8"";

    Register(
        EnemyData(
            "Hitoride", u8"ひとりでハンマー",
            100, 35, 10, 85, 5, 35, 20,
            -5, 0.8f, "", 0.1f, SleepType::WakeOnDamage,
            {},
            EnemyAIType::PatrolAndChase,
            EnemyTurnSpeedType::Normal, EnemyTurnSpeedType::Normal, 3,
            EnemyVisualData{
                "Asset\\Model\\Hitoride.fbx",
                "Asset\\Model\\TIdle.fbx",
                "Asset\\Model\\TRun.fbx",
                "Asset\\Model\\TAttack.fbx",
                "Asset\\Model\\TDamaged.fbx",
                "Asset\\Model\\TIdle.fbx",
                "Asset\\Model\\TSleep.fbx",
                {0.9f, 0.9f, 0.9f},
                1.0f
            }
        )
    );
    m_Data["Hitoride"].talkMessage[0] = u8"ドガーン！ガラガラーー！！";
    m_Data["Hitoride"].talkMessage[1] = u8"ガッシャン！ガッシャン！";
    m_Data["Hitoride"].talkMessage[2] = u8"";
 

    Register(
        EnemyData(
            "ShopKeeper", u8"店番",
            999, 180, 120, 100, 50, 0, 30,
            -1000, 0.0f, "", 0.0f, SleepType::None,
            {},
            EnemyAIType::Passive,
            EnemyTurnSpeedType::Fast, EnemyTurnSpeedType::Fast, 3,
            EnemyVisualData{
                "Asset\\Model\\EnemyModel\\BlueArmor.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Idle.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Walk.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Attack.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Damaged.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Attack.fbx",
                "Asset\\Model\\EnemyModel\\Armor_Sleep.fbx",
                {0.55f, 0.55f, 0.55f},
                1.0f
            }
        )
    );

    // =========================
    // アニメーション通知設定
    // =========================
    // 値は各アニメーション全体に対する0.0～1.0の割合。
    // 同じFBXを使う敵でも、将来個別モデルへ差し替えやすいよう敵ID単位で保持する。

    SetCombatAnimationNotifies(m_Data["Kingyo"],     0.46f, 0.00f);
    SetCombatAnimationNotifies(m_Data["Obie"],       0.42f, 0.00f);
    SetCombatAnimationNotifies(m_Data["FishEye"],    0.48f, 0.00f);
    SetCombatAnimationNotifies(m_Data["Uonome"],     0.50f, 0.00f);
    SetCombatAnimationNotifies(m_Data["PypeDevil"],  0.40f, 0.58f);
    SetCombatAnimationNotifies(m_Data["BlueJoker"],  0.40f, 0.58f);
    SetCombatAnimationNotifies(m_Data["Tormas"],     0.40f, 0.58f);
    SetCombatAnimationNotifies(m_Data["Blood"],      0.58f, 0.58f);
    SetCombatAnimationNotifies(m_Data["Armor"],      0.40f, 0.00f);
    SetCombatAnimationNotifies(m_Data["Dark"],       0.40f, 0.00f);
    SetCombatAnimationNotifies(m_Data["CurseFish"],  0.55f, 0.00f);
    SetCombatAnimationNotifies(m_Data["Hitoride"],   0.44f, 0.00f);
    SetCombatAnimationNotifies(m_Data["ShopKeeper"], 0.40f, 0.500f);

    // =========================
    // SpawnTable読み込み
    // =========================
    EnemyTableDatabase::LoadAll(
        "DungeonData\\EnemyTables\\");
    // =========================
    // EnemyModel読み込み
    // =========================
    for (const auto& [id, data] : m_Data) {
        EnemyModelManager::Instance().LoadModel(data);
    }
}
const EnemyData* EnemyDatabase::Get(const std::string& id)
{
    auto it = m_Data.find(id);
    if (it == m_Data.end())
    {
        return nullptr;
    }
    return &it->second;
}
std::vector<std::string> EnemyDatabase::GetAllIds()
{
    std::vector<std::string> ids;
    ids.reserve(m_Data.size());
    for (const auto& pair : m_Data)
    {
        ids.push_back(pair.first);
    }
    return ids;
}

const EnemyData* EnemyDatabase::DrawFromTable(const std::string& tableId)
{
    // EnemyTableDatabaseは出現比率だけを持ち、最終的なステータス実体はEnemyDatabaseから引く。
    // 1. データベースから指定されたIDのテーブルを取得
    const EnemySpawnTable* table = EnemyTableDatabase::Get(tableId);

    // テーブルが存在しない、またはエントリーが空の場合はnullptrを返す
    if (!table || table->entries.empty())
    {
        return nullptr;
    }

    // 2. 重みの総和（Total Weight）を計算
    int totalWeight = 0;
    for (const auto& entry : table->entries)
    {
        // 念のため正の数であることを保証
        totalWeight += std::max(0, entry.weight);
    }

    if (totalWeight <= 0) return nullptr;

    // 3. 0 から totalWeight - 1 の範囲で乱数を生成
    int roll = GameRandom::Range(0, totalWeight - 1);

    // 4. 重みに基づいて当選個体を特定
    for (const auto& entry : table->entries)
    {
        int weight = std::max(0, entry.weight);
        if (roll < weight)
        {
            // 当選したEnemyIDに対応する詳細データをEnemyDatabaseから取得して返す
            return EnemyDatabase::Get(entry.id);
        }
        roll -= weight;
    }

    return nullptr;
}