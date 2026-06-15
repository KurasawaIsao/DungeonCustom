#pragma once
#include <string>
class DataReference
{
public:
    static std::string m_NextDungeonId;
    static bool m_IsEditorTestPlay;
    static bool m_RandomizeEditorTestPlaySeed;
    static int m_EditorTestPlaySeedSalt;
};
