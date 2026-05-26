#pragma once
#include "GameObject.h"
#include "Vector2Int.h"
#include "ItemInstance.h"
#include "GameUIObject.h"
#include <set>
#include <string>
#include <vector>

class Player;
class Ally;

struct KanaLayout
{
    std::vector<std::vector<const char*>> table;
};
enum class UIState
{
    Normal,
    MainMenu,
    ItemMenu,
    ItemCommand,
    AllyMenu,
    GroundMenu,
    PotMenu,
    RecruitConfirm,
    RecruitDismiss,
    Rename,
    ShrineConfirm
};
class PlayerInventoryUI : public GameObject, public GameUIObject
{
private:


    UIState m_State = UIState::Normal;

    // 選択状態の管理
    int m_ItemCommandTarget = -1;
    int m_RenameTargetIndex = -1; // -2は勧誘用
    int m_OpenPotIndex = -1;
    char m_RenameBuffer[64] = {};
    int m_RenameMode = 0; // キーボードのひらがな/カタカナ切替用
    int m_MainMenuCursor = 0;
    bool m_BlockMainMenuCancel = false;
    int m_InventoryCursor = 0;
    int m_ItemCommandCursor = 0;
    int m_AllyCursor = 0;
    int m_AllyCommandCursor = 0;
    int m_GroundCursor = 0;
    int m_ShrineConfirmCursor = 0;
    int m_PotSide = 0;
    int m_PotItemCursor = 0;
    int m_PotInventoryCursor = 0;

    std::set<int> m_SelectedInventoryIdx;
    std::set<int> m_SelectedPotItemIdx;

    class Enemy* m_RecruitTarget = nullptr;
    bool m_RecruitMessageShown = false;
    int m_RecruitChoiceIndex = 0;
    bool m_BlockRecruitConfirm = false;
    bool m_BlockShrineConfirm = false;
    int m_RecruitDismissCursor = 0;
    bool m_BlockRecruitDismiss = false;
    Ally* m_FocusedAlly = nullptr;
    bool m_AllyCameraFocusActive = false;

public:
    void Init() override { { CloseAllMenus(); } }
    void Update() override {};
    void Draw() override {};
    void Uninit() override {};

    void DrawGameUI() override;

    // 外部からのトリガー
    void OpenItemMenu() { m_MainMenuCursor = 0; m_BlockMainMenuCancel = true; m_State = UIState::MainMenu; }
    void OpenRecruitMenu(class Enemy* target) {
        m_RecruitTarget = target;
        m_RecruitMessageShown = false;
        m_RecruitChoiceIndex = 0;
        m_BlockRecruitConfirm = true;
        m_State = UIState::RecruitConfirm;
    }
    void OpenPotMenu(int inventoryIndex) {
        m_OpenPotIndex = inventoryIndex; 
        m_PotSide = 0;
        m_PotItemCursor = 0;
        m_PotInventoryCursor = 0;
        m_State = UIState::PotMenu;      
    }
    void OpenShrineConfirm();
    void SetState(UIState state) { m_State = state; }
    void CloseAllMenus();

    bool IsAnyMenuOpen() const { return m_State != UIState::Normal; }

private:
    void DrawPlayerUI();
    void DrawPlayerStatus(Player* player);
    void DrawMainMenu(Player* player);
    void DrawItemMenu(Player* player);
    void DrawItemCommandMenu(Player* player);
    void DrawAllyMenu();
    void DrawGroundMenu(Player* player);
    void DrawRenameUI(Player* player);
    void DrawPotUI(Player* player);
    void DrawRecruitUI();
    void DrawRecruitDismissUI();
    void BeginRecruitNameInput();
    void UpdateAllyCameraFocus(Ally* ally);
    void ClearAllyCameraFocus(bool revealSelected);
    void RevealAllyOnMiniMap(Ally* ally);
    void DismissAllyForRecruit(class Ally* ally);
    void DrawShrineConfirmUI();
    bool IsPlayerInShop(Player* player) const;
    void DropItemAtFeet(Player* player, int inventoryIndex);
    void ExchangeItemWithGroundItem(Player* player, int inventoryIndex);

    void PutItemIntoPot(Player* player, int inventoryIndex);
    void ProcessBulkPutIntoPot(Player* player);
    void ProcessBulkTakeOutFromPot(Player* player);
};
