#include "ItemIDConverter.h"

std::string ItemIDConverter::BlessToString(BlessState b)
{
    switch (b)
    {
    case BlessState::Blessed: return "Blessed";
    case BlessState::Cursed:  return "Cursed";
    default:                  return "Normal";
    }
}

BlessState ItemIDConverter::StringToBless(const std::string& s)
{
    if (s == "Blessed") return BlessState::Blessed;
    if (s == "Cursed")  return BlessState::Cursed;
    return BlessState::Normal;
}

std::string ItemIDConverter::IdentifyToString(IdentifyState s)
{
    return s == IdentifyState::Identified
        ? "Identified"
        : "Unidentified";
}

