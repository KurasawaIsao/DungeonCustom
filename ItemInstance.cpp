#include "GameRandom.h"
#include "ItemInstance.h"
#include "player.h"
#include "Unit.h"
#include "ItemInstance.h"
#include "MessageLog.h"
#include "MapData.h"
#include "MapManager.h"
#include "manager.h"
#include "scene.h"
#include "Item.h"
#include "FlyingObject.h"
#include "PlayerInventoryUI.h"
#include "ShopUI.h"
#include "Enemy.h"
#include "EffectTargeting.h"

void ItemInstance::Use(Player* player, int inventoryIndex)
{
    // 壺なら UI を開くだけ（消費しない）
    if (IsPot())
    {
        PlayerInventoryUI* ui = Manager::GetScene()->GetGameObject<PlayerInventoryUI>();
        if (ui)
        {
            // 描画関数を呼ぶのではなく、"開くフラグと対象"をセットする
            ui->OpenPotMenu(inventoryIndex);
        }
        return;
    }

    switch (m_Data->type)
    {

    case ItemType::Herb:
        
        if (IsCursed())
        {
            MessageLog::Instance().AddMessage(player->GetName() + u8"は" + GetDisplayName() + u8"を飲もうとした！");
            MessageLog::Instance().AddMessage(GetDisplayName() +u8"は呪われている！");
            RevealOption();
        }
        else
        {
            TryIdentify(ItemCommandType::Use);
            MessageLog::Instance().AddMessage(player->GetName() + u8"は" + GetDisplayName() + u8"を飲んだ。");
            player->AddFullness(5);
        }

        break;
    case ItemType::Food:
        MessageLog::Instance().AddMessage(player->GetName() + u8"は" + GetDisplayName() + u8"を食べた。");
        break;
    case ItemType::Staff:
        MessageLog::Instance().AddMessage(player->GetName() + u8"は" + GetDisplayName() + u8"を振った！");
        if (IsCursed())MessageLog::Instance().AddMessage(GetDisplayName() + u8"は呪われている！");
        RevealOption();
    
        break;
    case ItemType::Weapon:
        if (player->GetEquippedWeaponIndex() == inventoryIndex) {
            player->UnequipWeapon(); // すでに装備中なら外す
            MessageLog::Instance().AddMessage(GetDisplayName() + u8"を外した。");
        }
        else {
            player->EquipItem(inventoryIndex); // 装備する
            MessageLog::Instance().AddMessage(GetDisplayName() + u8"を装備した！");
            if (IsCursed()) MessageLog::Instance().AddMessage(GetDisplayName() + u8"は呪われている！");
            RevealOption();
        }
        break;
    }
    // 呪い
    if (IsCursed())
    {
        return;
    }

    if (m_Data->type == ItemType::Staff) {
        if (m_Identify == IdentifyState::Unidentified) {
            m_UnidentifiedUsageCount--;
        }
        if (m_Charges <= 0) {
            MessageLog::Instance().AddMessage(u8"しかし何も起こらなかった。");
            return;
        }
		//祝福時は消費なし、呪い時はそもそもここに来ないので、通常アイテムと同様に祝福かどうかで分ける必要はない
        if (!IsBlessed()) m_Charges--;
        
        if (IsBlessed())
        {
            if(GameRandom::Range(0.0f,1.0f) < 0.6f) {
                MessageLog::Instance().AddMessage(GetDisplayName() + u8"は祝福状態から元に戻った。");
				SetBless(BlessState::Normal);
			}
        }

        // FlyingObject を生成して発射
        FlyingObject* bullet = Manager::GetScene()->AddGameObject<FlyingObject>(1);
        Vector2Int dir = player->GetFacingDir();


        bullet->Fire(
            "Asset\\Model\\MagicBullet.obj", 
            this,                             
            player->GetPosition(),          
            player->GetGridPos(),           
            dir,                             
            0.02f,                            // 速度
            15,                              // 射程距離
            player
        );
        return;
    }

    EffectContext ctx;
    ctx.source = EffectSourceType::Item;
    ctx.user = player;
    ctx.target = player;
    ctx.pos = player->GetGridPos();

    //効果の方に祝福かどうかの状態を入れる
    if (IsBlessed())      ctx.rank = EffectRank::Blessed;
    else if (IsCursed())  ctx.rank = EffectRank::Cursed;
    else                  ctx.rank = EffectRank::Normal;

    if (m_Data->effect) {
        if (ctx.targetType == EffectTargetType::Single) m_Data->effect->Apply(ctx);
        else EffectTargeting::ApplyToCollectedTargets(*m_Data->effect, ctx);
    }

 
 
}

//投げられたアイテムが当たった時の処理
void ItemInstance::Throw(Player* player, Unit* hit, Vector2Int& pos)
{
    if (Enemy* shopkeeper = dynamic_cast<Enemy*>(hit))
    {
        ShopUI::AngerShopKeeper(shopkeeper);

    }
    // 呪い時の処理：共通効果を呼ばずに固定ダメージ（既存ロジック）
    if (IsCursed())
    {
        if (hit)
        {
            hit->ConstantDamage(2);
        }
        return;
    }

    EffectContext ctx;
    ctx.source = EffectSourceType::Item;
    ctx.user = player; // 投げた人
    ctx.target = hit;    // 当たった対象（いなければ nullptr）
    ctx.pos = pos;    // 着弾点
    if (IsBlessed())      ctx.rank = EffectRank::Blessed;
    else if (IsCursed())  ctx.rank = EffectRank::Cursed;
    else                  ctx.rank = EffectRank::Normal;

    if (m_Data->effect)
    {
        if (!hit)
        {
            if (m_Data->type == ItemType::Stone)
            {
                m_Data->effect->Apply(ctx);
            }
            return;
        }

        // 食べ物系は投げても効果が出ず、固定ダメージにする
        if (m_Data->type == ItemType::Food)
        {
            hit->ConstantDamage(2);
        }
        else
        {
            // 薬草などは投げ当てても効果（回復など）を発揮する
            if (ctx.targetType == EffectTargetType::Single) m_Data->effect->Apply(ctx);
        else EffectTargeting::ApplyToCollectedTargets(*m_Data->effect, ctx);
        }
    }
}

void ItemInstance::CreatePot(int cap, bool takeOut)
{
    m_Pot = std::make_unique<PotData>();
    m_Pot->capacity = cap;
    m_Pot->canTakeOut = takeOut;
}

void ItemInstance::OnBreak(const Vector2Int& center)
{
    if (!IsPot() || !m_Pot) return;

    // 中身をローカルに移動（これで m_Pot->items は空になる）
    std::vector<ItemInstance> itemsToDrop = std::move(m_Pot->items);
    m_Pot->items.clear();

    MapData* map = MapManager::Instance()->GetCurrentMap();
    static const Vector2Int offsets[] = {
        {0,0},{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}
    };

    for (auto& inst : itemsToDrop)
    {
        Vector2Int dropPos = center;
        for (int i = 0; i < (int)std::size(offsets); i++)
        {
            Vector2Int p = center + offsets[i];
            if (map->IsTileFree(p)) {
                dropPos = p;
                break;
            }
        }

        Item* newItem = Manager::GetScene()->AddGameObject<Item>(1);
        if (newItem)
        {
            newItem->SetInstance(std::move(inst));
            newItem->SetupFromInstance();
            newItem->SetGridPos(dropPos);
            newItem->SetPosition(Vector3(dropPos.x * 2.0f, 0.05f, dropPos.y * 2.0f));
            map->AddMapObject(newItem, dropPos.x, dropPos.y);
        }
    }
}
void ItemInstance::InitIdentify(bool applyBlessOrCurse)
{
	// アイテムデータの識別状態を確認して、必要なら識別する
    auto& managerInfo = ItemIdentificationManager::Instance().GetInfo(m_Data->name);
    if (managerInfo.isIdentified) {
        SetIdentified();
    }

    if (m_Data && m_Data->BlessOrCurse && applyBlessOrCurse) { // アイテムデータが呪い対象なら
        int r = GameRandom::Range(0, 99);

        if (r < 10)
        {
            SetBless(BlessState::Cursed);     // 10%
        }
        else if (r < 20)
        {
            SetBless(BlessState::Blessed);    // 次の10%
        }
        else
        {
            SetBless(BlessState::Normal);
        }

        // 消耗品（草や食べ物）や未識別アイテムは、最初は状態を伏せておく
        m_IsOptionRevealed = !m_Data->identifiable;
        
    }
    else if (!applyBlessOrCurse) {
        SetBless(BlessState::Normal);
        m_IsOptionRevealed = true;
    }
    else if (m_Data && m_Data->type == ItemType::Pot) {
        // そもそも呪いがないアイテム（
        m_IsOptionRevealed = false;
    }
    else {

        m_IsOptionRevealed = true;
    }
 
}
std::string ItemInstance::GetDisplayName() const
{
    auto& info = ItemIdentificationManager::Instance().GetInfo(m_Data->name);
    std::string displayName;

    //  矢や石のスタック数
    if ((m_Data->type == ItemType::Arrow || m_Data->type == ItemType::Stone) && m_StackCount > 0) {
        displayName += std::to_string(m_StackCount) + u8"個の";
    }

    //  現在の状態に合わせた名前の表示
    if (m_Identify == IdentifyState::Identified || info.isIdentified) {
        displayName += m_Data->name;
    }
    else if (!info.customName.empty()) {
        displayName += info.customName + u8"（？" + info.unidentifiedName + u8"）";
    }
    else {
        displayName += info.unidentifiedName;
    }

    //  修正値
    if ((m_Identify == IdentifyState::Identified || info.isIdentified) &&
        (m_Data->type == ItemType::Weapon || m_Data->type == ItemType::Shield))
    {
        if (m_PlusValue >= 0) {
            displayName += "+" + std::to_string(m_PlusValue);
        }
		else  if (m_PlusValue < 0)
        {
            displayName += std::to_string(m_PlusValue); // 負の数は - が自動でつく
        }
    }

    // 壺の容量
    if (m_Data->type == ItemType::Pot && IsPot()) {
        displayName += "[" + std::to_string(m_Pot->items.size()) + "/" + std::to_string(m_Pot->capacity) + "]";
    }

    //  杖の回数
    if (m_Data->type == ItemType::Staff) {
        if (m_Identify == IdentifyState::Identified || info.isIdentified) {
            displayName += "[" + std::to_string(m_Charges) + "]";
        }
        else {
            displayName += "[" + std::to_string(m_UnidentifiedUsageCount) + "]";
        }
    }

    return displayName;
}
ImU32 ItemInstance::GetNameColor() const {
    auto& info = ItemIdentificationManager::Instance().GetInfo(m_Data->name);

    if (m_Identify == IdentifyState::Unidentified && !info.isIdentified) {
        return IM_COL32(255, 255, 0, 255); // 黄色
    }

    if (m_IsOptionRevealed || m_Identify == IdentifyState::Identified || info.isIdentified) {
        if (IsCursed()) {
            return IM_COL32(255, 100, 100, 255); // 赤色
        }
        if (IsBlessed()) {
            return IM_COL32(100, 255, 255, 255); // 水色
        }
    }

    // 通常状態
    return IM_COL32(255, 255, 255, 255); // 白色
}
bool ItemInstance::PutItem(ItemInstance&& item)
{
    if (!IsPot()) return false;
    if (m_Pot->items.size() >= m_Pot->capacity) return false;

    m_Pot->items.push_back(std::move(item));
    return true;
}
std::optional<ItemInstance> ItemInstance::TakeOutItem()
{
    if (!IsPot() || !m_Pot->canTakeOut || m_Pot->items.empty())
        return std::nullopt;

    ItemInstance item = std::move(m_Pot->items.back());
    m_Pot->items.pop_back();
    return item;
}


void ItemInstance::TryIdentify(ItemCommandType cmd)
{
    if (m_Identify == IdentifyState::Identified && !m_IsOptionRevealed)
        return;

    if (!m_Data->identifiable)
        return;
       auto& managerInfo = ItemIdentificationManager::Instance().GetInfo(m_Data->name);
       if(managerInfo.isIdentified)
		   return;
    switch (m_Data->useType)
    {
    case ItemUseType::Drink:
        SetIdentified();

        if (m_Data->identifiable) {
            std::string realName = GetTrueName();

            Player* player = Manager::GetScene()->GetGameObject<Player>();
            // マネージャー側の種類フラグを「識別済み」にする
            ItemIdentificationManager::Instance().SetKindIdentified(realName);

            // プレイヤーが持っている同じ種類のアイテムもすべて識別済みに更新する
            auto& inventory = player->GetItems();
            for (auto& slot : inventory) {
                if (slot.instance.GetData() && slot.instance.GetTrueName() == realName) {
                    slot.instance.SetIdentified();
                }
            }
        }
        MessageLog::Instance().AddMessage(
            GetUnidetifiedName() + u8"は" + GetTrueName() + u8"だった！"
        );
        break;

    case ItemUseType::Zap:

        break;

    case ItemUseType::Put:

        break;

    case ItemUseType::Equip:

        break;
    }
}
