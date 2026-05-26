#pragma once
#include <string>
class DataReference
{
public:
    static std::string NextDungeonId;
    static bool IsEditorTestPlay;
    static bool RandomizeEditorTestPlaySeed;
    static int EditorTestPlaySeedSalt;
};
