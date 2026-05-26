#pragma once
#include "EffectBase.h"
#include "ItemDataBase.h"
#include "ItemInstance.h"
#include "MessageLog.h"
#include "Player.h"

class MudTrapEffect : public EffectBase
{
public:
    void Apply(const EffectContext& ctx) override
    {
        Player* player = dynamic_cast<Player*>(ctx.target);
        if (!player) return;

        const ItemData* rottenBread = ItemDatabase::Get("Food_RottenBread");
        if (!rottenBread) return;

        int convertedCount = 0;
        for (InventoryItem& slot : player->GetItems())
        {
            convertedCount += ConvertItem(slot.instance, rottenBread, slot.count);
            slot.count = slot.instance.GetStackCount();
        }

        if (convertedCount > 0)
        {
            MessageLog::Instance().AddMessage(u8"泥でパンがくさってしまった！");
        }
        else
        {
            MessageLog::Instance().AddMessage(u8"しかしくさるパンはなかった。");
        }
    }

private:
    int ConvertItem(ItemInstance& item, const ItemData* rottenBread, int stackCount)
    {
        int convertedCount = 0;

        // 泥の罠は手持ちに出ているパンだけを腐らせる。
        // 壺の中身は泥が直接かからないので、ここで対象外にする。
        if (item.IsPot())
        {
            return convertedCount;
        }

        const ItemData* data = item.GetData();
        if (!data || data->type != ItemType::Food || data->id == rottenBread->id)
        {
            return convertedCount;
        }

        const bool wasUnpaid = item.IsUnpaidShopItem();
        const int unpaidPrice = item.GetUnpaidShopPrice();
        const BlessState bless = item.GetBless();

        item = ItemInstance(rottenBread);
        item.SetStackCount(stackCount);
        item.SetBless(bless);
        if (wasUnpaid) item.SetUnpaidShopItem(true, unpaidPrice);

        return convertedCount + stackCount;
    }
};
