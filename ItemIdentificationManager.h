// ItemIdentificationManager.h
#pragma once
#include <map>
#include <string>
#include <vector>
#include "DungeonData.h"
#include "ItemData.h"

struct IdentificationInfo {
    bool isIdentified = false;
    std::string unidentifiedName; // 未識別名
    std::string customName;       // プレイヤーが付けた名前
};

class ItemIdentificationManager {
public:
    static ItemIdentificationManager& Instance() {
        static ItemIdentificationManager instance;
        return instance;
    }

    // ゲーム開始時に未識別名をシャッフルして割り当てる
    void Init(const std::vector<ItemData>& database, UnidentifiedMode unidentifiedMode);

	// アイテムの実際の名前を指定して、識別情報を取得
    IdentificationInfo& GetInfo(const std::string& realName) {
        return m_Table[realName];
    }

    void SetIdentified(const std::string& realName, bool identified) {
        m_Table[realName].isIdentified = identified;
    }
    void SetKindIdentified(const std::string& realName) {
        if (m_Table.count(realName)) {
            m_Table[realName].isIdentified = true;
        }
    }

private:
    std::map<std::string, IdentificationInfo> m_Table;
};