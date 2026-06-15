#pragma once
#include "EffectBase.h"
#include "ItemDataBase.h"
#include "ItemInstance.h"
#include "MessageLog.h"
#include "Player.h"
#include "Enemy.h"
#include "Ally.h"
#include "Item.h"
#include "Manager.h"
#include "MapManager.h"
#include "Scene.h"
#include "Trap.h"

class MudTrapEffect : public EffectBase
{
public:
    void Apply(const EffectContext& ctx) override
    {
        const ItemData* rottenBread = ItemDatabase::Get("Food_RottenBread");
        if (!rottenBread) return;

        // アイテムで作動した泥罠の上に敵か仲間がいれば、死亡扱いにせずパンへ変化させる。
        if (dynamic_cast<Enemy*>(ctx.target) || dynamic_cast<Ally*>(ctx.target))
        {
            ConvertUnitToRottenBread(ctx.target, ctx.pos, rottenBread);
        }

        if (ctx.item)
        {
            // 落下アイテムは通常のパンと大きなパンだけを腐らせ、それ以外は元のまま残す。
            if (IsConvertibleBread(ctx.item->GetData()))
            {
                const std::string itemName = ctx.item->GetDisplayName();
                ReplaceWithRottenBread(*ctx.item, rottenBread, ctx.item->GetStackCount());
                MessageLog::Instance().AddMessage(
                    itemName + u8"はくさったパンになってしまった！");
            }
            return;
        }

        Player* player = dynamic_cast<Player*>(ctx.target);
        if (!player) return;

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
    bool IsConvertibleBread(const ItemData* data) const
    {
        if (!data) return false;
        return data->id == "Food_Bread" || data->id == "Food_BigBread";
    }

    bool FindClockwiseDropGrid(MapData* map, const Vector2Int& center, Vector2Int& dropGrid) const
    {
        if (!map) return false;

        // 投擲アイテムと同じく、北側から時計回りに半径1、半径2の外周を探索する。
        for (int radius = 1; radius <= 2; ++radius)
        {
            auto TryGrid = [&](int offsetX, int offsetY)
            {
                const Vector2Int candidate = center + Vector2Int(offsetX, offsetY);
                // IsTileFreeで罠、既存アイテム、ユニットがあるマスをまとめて除外する。
                if (!map->IsTileFree(candidate)) return false;

                dropGrid = candidate;
                return true;
            };

            if (TryGrid(0, -radius)) return true;
            for (int x = 1; x <= radius; ++x) if (TryGrid(x, -radius)) return true;
            for (int y = -radius + 1; y <= radius; ++y) if (TryGrid(radius, y)) return true;
            for (int x = radius - 1; x >= -radius; --x) if (TryGrid(x, radius)) return true;
            for (int y = radius - 1; y >= -radius; --y) if (TryGrid(-radius, y)) return true;
            for (int x = -radius + 1; x < 0; ++x) if (TryGrid(x, -radius)) return true;
        }

        return false;
    }

    void ConvertUnitToRottenBread(Unit* unit, const Vector2Int& pos, const ItemData* rottenBread)
    {
        if (!unit || !rottenBread) return;

        MapManager* mapManager = MapManager::Instance();
        MapData* map = mapManager ? mapManager->GetCurrentMap() : nullptr;
        Scene* scene = Manager::GetScene();
        if (!map || !scene) return;

        const std::string unitName = unit->GetName();

        // 経験値・通常ドロップ・死亡ログを発生させず、フレーム末の通常破棄へ渡す。
        unit->ConsumeAllActions();
        unit->ConsumeAllMoves();
        unit->SetActionPhaseChecked(true);
        unit->SetMovePhaseChecked(true);
        unit->SetVisible(false);
        unit->StopLoopEffect();
        unit->SetDestroy();

        Vector2Int dropGrid;
        if (FindClockwiseDropGrid(map, pos, dropGrid))
        {
            // 罠マスには置かず、探索で見つけた周囲の空きマスへくさったパンを落とす。
            Item* breadObject = scene->AddGameObject<Item>(1);
            ItemInstance bread(rottenBread);
            bread.SetIdentified();
            breadObject->SetInstance(std::move(bread));
            map->AddMapObject(breadObject, dropGrid.x, dropGrid.y);
        }
        else
        {
            MessageLog::Instance().AddMessage(u8"くさったパンは落ちる場所がなかった。");
        }

        MessageLog::Instance().AddMessage(
            unitName + u8"はくさったパンになってしまった！");
    }

    int ConvertItem(ItemInstance& item, const ItemData* rottenBread, int stackCount)
    {
        // 泥の罠は通常のパンと大きなパンだけを腐らせ、壺の中身や他の食料は対象外にする。
        if (item.IsPot() || !IsConvertibleBread(item.GetData()))
        {
            return 0;
        }

        ReplaceWithRottenBread(item, rottenBread, stackCount);
        return stackCount;
    }

    void ReplaceWithRottenBread(ItemInstance& item, const ItemData* rottenBread, int stackCount)
    {
        // 投げた店の商品状態を失わないよう、アイテム固有の状態を引き継ぐ。
        const bool wasUnpaid = item.IsUnpaidShopItem();
        const int unpaidPrice = item.GetUnpaidShopPrice();

        item = ItemInstance(rottenBread);
        item.SetStackCount(stackCount);
        if (wasUnpaid) item.SetUnpaidShopItem(true, unpaidPrice);
    }
};