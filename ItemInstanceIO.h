#pragma once
#include "external/json.hpp"
#include "ItemInstance.h"

namespace ItemInstanceIO
{
    nlohmann::json SerializeItemInstance(const ItemInstance& inst);
    ItemInstance DeserializeItemInstance(const nlohmann::json& j);
}
