#pragma once
#include <string>
#include "ItemInstance.h"

class ItemIDConverter
{
public:

    static std::string BlessToString(BlessState b);
    static BlessState StringToBless(const std::string& s);

    static std::string IdentifyToString(IdentifyState s);
};
