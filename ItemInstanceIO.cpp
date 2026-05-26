#include "ItemInstanceIO.h"
#include "ItemIDConverter.h"
#include "ItemDataBase.h"

namespace ItemInstanceIO
{
    using json = nlohmann::json;

    json SerializeItemInstance(const ItemInstance& inst)
    {
        json j;
        const ItemData* data = inst.GetData();

        j["data"] = data->id;
        j["identify"] = ItemIDConverter::IdentifyToString(inst.GetIdentifiedState());
        j["bless"] = ItemIDConverter::BlessToString(inst.GetBless());
        j["charge"] = inst.GetCharge();
        j["stackCount"] = inst.GetStackCount();

        if (inst.IsPot())
        {
            PotData* pot = inst.GetPot();
            json jpot;
            jpot["capacity"] = pot->capacity;
            jpot["canTakeOut"] = pot->canTakeOut;

            json items = json::array();
            for (auto& i : pot->items)
            {
                items.push_back(SerializeItemInstance(i));
            }
            jpot["items"] = items;
            j["pot"] = jpot;
        }
        else
        {
            j["pot"] = nullptr;
        }

        return j;
    }

    ItemInstance DeserializeItemInstance(const json& j)
    {
        const ItemData* data =
            ItemDatabase::Get(j["data"].get<std::string>());

        ItemInstance inst(data);

        inst.SetBless(ItemIDConverter::StringToBless(j["bless"]));
        inst.SetCharge(j["charge"]);
        inst.SetStackCount(j.value("stackCount", 1));
        inst.TryIdentify(ItemCommandType::Use);

        if (j["identify"] == "Identified")
            inst.SetIdentified();

        if (!j["pot"].is_null())
        {
            const json& jp = j["pot"];
            inst.CreatePot(jp["capacity"], jp["canTakeOut"]);

            for (auto& ji : jp["items"])
            {
                inst.PutItem(DeserializeItemInstance(ji));
            }
        }

        return inst;
    }
}
