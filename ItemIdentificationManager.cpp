#include "ItemIdentificationManager.h"
#include "GameRandom.h"
#include <algorithm>

void ItemIdentificationManager::Init(const std::vector<ItemData>& database, UnidentifiedMode unidentifiedMode) {
    m_Table.clear();

    // 未識別名のリソース定義
    std::vector<std::string> herbs = {
          u8"あかいくさ", u8"あおいくさ", u8"きいろいくさ", u8"くろいくさ",
          u8"たまむしいろのくさ", u8"みずいろのくさ", u8"ピンクいろのくさ",
          u8"にじいろのくさ", u8"グレーのくさ", u8"ベージュいろのくさ", u8"はだいろのくさ"
    };
    std::vector<std::string> pots = {
        u8"かたい壺", u8"あさい壺", u8"みかづき形の壺", u8"たかそうな壺"
    };
    std::vector<std::string> staves = {
		u8"カシの杖", u8"ナシの杖", u8"金の杖", u8"スギの杖", u8"ヒノキの杖", u8"クルミの杖",u8"サクラの杖", u8"マツの杖", u8"ヒバの杖", u8"クリの杖"
    };

    // シャッフル処理
    std::mt19937& g = GameRandom::Engine();

    std::shuffle(herbs.begin(), herbs.end(), g);
    std::shuffle(pots.begin(), pots.end(), g);
    std::shuffle(staves.begin(), staves.end(), g);

    int herbIdx = 0, potIdx = 0, staffIdx = 0;

    std::vector<std::string> unidentifiedTargets;
    for (const auto& data : database) {
		// 名前のみ最初から判明して呪い祝福のみを伏せる場合ここでブロック
        if (data.type == ItemType::Weapon|| data.type == ItemType::Shield) {
            continue;
        }

        if (data.identifiable) {
            unidentifiedTargets.push_back(data.name);
        }
    }
    std::shuffle(unidentifiedTargets.begin(), unidentifiedTargets.end(), g);

    std::map<std::string, bool> shouldAssignUnidentified;
    if (unidentifiedMode == UnidentifiedMode::All) {
        for (const auto& name : unidentifiedTargets) {
            shouldAssignUnidentified[name] = true;
        }
    }
    else if (unidentifiedMode == UnidentifiedMode::Half) {
        const int assignCount = (int)(unidentifiedTargets.size() + 1) / 2;
        for (int i = 0; i < assignCount; ++i) {
            shouldAssignUnidentified[unidentifiedTargets[i]] = true;
        }
    }

    for (const auto& data : database) {
        IdentificationInfo info;

        if (!data.identifiable || !shouldAssignUnidentified[data.name]) {
            // 割り当てない設定なら、強制的に「識別済み」とする
            info.isIdentified = true;
            info.unidentifiedName = ""; 
        }
        else {
            // 割り当てる設定なら、本来のデータ定義（identifiable）に従う
            info.isIdentified = !data.identifiable;

            // タイプに応じた名前の割り当て
            if (data.type == ItemType::Herb && herbIdx < herbs.size()) {
                info.unidentifiedName = herbs[herbIdx++];
            }
            else if (data.type == ItemType::Pot && potIdx < pots.size()) {
                info.unidentifiedName = pots[potIdx++];
            }
            else if (data.type == ItemType::Staff && staffIdx < staves.size()) {
                info.unidentifiedName = staves[staffIdx++];
            }
            else {

            }
        }

        m_Table[data.name] = info;
    }
}