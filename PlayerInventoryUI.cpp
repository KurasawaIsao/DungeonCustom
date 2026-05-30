#include "PlayerInventoryUI.h"
#include "player.h"
#include "manager.h"
#include "scene.h"
#include "MessageLog.h"
#include "UnitManager.h"
#include "Ally.h"
#include "MapManager.h"
#include "Camera.h"
#include "MiniMapRenderer.h"
#include "VisionMaskRenderer.h"
#include "KeyboardUI.h"
#include "RecruitManager.h"
#include "PotUI.h"
#include "Shrine.h"
#include "Item.h"
#include "Trap.h"
#include "ShopUI.h"
#include "Room.h"
#include "Input.h"
#include "Renderer.h"
#include "UIRenderer.h"
#include "UITextRenderer.h"
#include <algorithm>
#include <functional>
#include <string>
#include <cstring>

namespace
{
    void RestoreMainRenderTarget()
    {
        ID3D11RenderTargetView* rtv = Renderer::GetMainRenderTargetView();
        Renderer::GetDeviceContext()->OMSetRenderTargets(1, &rtv, nullptr);
    }

    D2D1_COLOR_F ToD2DColor(ImU32 color)
    {
        const float r = static_cast<float>(color & 0xff) / 255.0f;
        const float g = static_cast<float>((color >> 8) & 0xff) / 255.0f;
        const float b = static_cast<float>((color >> 16) & 0xff) / 255.0f;
        const float a = static_cast<float>((color >> 24) & 0xff) / 255.0f;
        return D2D1::ColorF(r, g, b, a);
    }

    void DrawMenuChoice(const std::string& text, bool selected, float x, float y, float width, const D2D1_COLOR_F& color)
    {
        std::string line = selected ? u8"・ " : "  ";
        line += text;
        UITextRenderer::DrawOutlinedTextUtf8(
            line,
            x,
            y,
            width,
            28.0f,
            20.0f,
            selected ? D2D1::ColorF(1.0f, 0.92f, 0.45f, 1.0f) : color);
    }

    void DrawRecruitChoice(const char* text, bool selected, float x, float y, float width)
    {
        DrawMenuChoice(text, selected, x, y, width, D2D1::ColorF(D2D1::ColorF::White));
    }

    void DrawStatusGauge(const UIRect& rect, float rate, const XMFLOAT4& baseColor, const XMFLOAT4& fillColor)
    {
        rate = std::clamp(rate, 0.0f, 1.0f);
        UIRenderer::DrawSolidRect(rect, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.82f));
        UIRenderer::DrawSolidRect(UIRect{ rect.x + 1.0f, rect.y + 1.0f, rect.w - 2.0f, rect.h - 2.0f }, baseColor);
        UIRenderer::DrawSolidRect(UIRect{ rect.x + 2.0f, rect.y + 2.0f, (rect.w - 4.0f) * rate, rect.h - 4.0f }, fillColor);
    }
}
void PlayerInventoryUI::DrawGameUI()
{
    DrawPlayerUI();
}


//ターン消費の際にも呼び出す
void PlayerInventoryUI::OpenShrineConfirm()
{
    m_ShrineConfirmCursor = 0;
    m_BlockShrineConfirm = true;
    m_State = UIState::ShrineConfirm;
}

void PlayerInventoryUI::CloseAllMenus()
{
    ClearAllyCameraFocus(true);
    m_State = UIState::Normal; // ステートをリセット
    m_ItemCommandTarget = -1;
    m_RenameTargetIndex = -1;
    m_OpenPotIndex = -1;
    m_MainMenuCursor = 0;
    m_BlockMainMenuCancel = false;
    m_InventoryCursor = 0;
    m_ItemCommandCursor = 0;
    m_AllyCursor = 0;
    m_AllyCommandCursor = 0;
    m_GroundCursor = 0;
    m_ShrineConfirmCursor = 0;
    m_PotSide = 0;
    m_PotItemCursor = 0;
    m_PotInventoryCursor = 0;
    m_RecruitTarget = nullptr;
    m_RecruitMessageShown = false;
    m_RecruitChoiceIndex = 0;
    m_BlockRecruitConfirm = false;
    m_BlockShrineConfirm = false;
    m_RecruitDismissCursor = 0;
    m_BlockRecruitDismiss = false;
    m_SelectedInventoryIdx.clear();
    m_SelectedPotItemIdx.clear();
}

void PlayerInventoryUI::RevealAllyOnMiniMap(Ally* ally)
{
    if (!ally) return;

    Scene* scene = Manager::GetScene();
    if (!scene) return;

    MiniMapRenderer* miniMap = scene->GetGameObject<MiniMapRenderer>();
    if (!miniMap) return;

    int viewDistance = 2;
    UnitManager* unitManager = UnitManager::Instance();
    Player* player = unitManager ? unitManager->GetPlayer() : nullptr;
    if (player)
    {
        viewDistance = player->GetViewDistance();
    }

    miniMap->RevealFromPosition(ally->GetGridPos(), viewDistance);
}

void PlayerInventoryUI::UpdateAllyCameraFocus(Ally* ally)
{
    if (!ally) return;

    Scene* scene = Manager::GetScene();
    if (!scene) return;

    Camera* camera = scene->GetGameObject<Camera>();
    if (camera)
    {
        camera->SetTarget(ally->GetPosition());
        camera->SetPlayerEnable(false);
        camera->SetOtherObjEnable(true);
    }

    VisionMaskRenderer* visionMask = scene->GetGameObject<VisionMaskRenderer>();
    if (visionMask)
    {
        visionMask->SetFocusOverride(ally->GetGridPos(), ally->GetWorldPosition());
    }

    if (m_FocusedAlly != ally)
    {
        RevealAllyOnMiniMap(ally);
        m_FocusedAlly = ally;
    }
    m_AllyCameraFocusActive = true;
}

void PlayerInventoryUI::ClearAllyCameraFocus(bool revealSelected)
{
    if (!m_AllyCameraFocusActive && !m_FocusedAlly) return;

    if (revealSelected)
    {
        RevealAllyOnMiniMap(m_FocusedAlly);
    }

    Scene* scene = Manager::GetScene();
    Camera* camera = scene ? scene->GetGameObject<Camera>() : nullptr;
    if (camera)
    {
        camera->SetOtherObjEnable(false);
        camera->SetPlayerEnable(true);
    }

    VisionMaskRenderer* visionMask = scene ? scene->GetGameObject<VisionMaskRenderer>() : nullptr;
    if (visionMask)
    {
        visionMask->ClearFocusOverride();
    }

    m_FocusedAlly = nullptr;
    m_AllyCameraFocusActive = false;
}
void PlayerInventoryUI::DrawPlayerUI()
{
    Player* player = Manager::GetScene()->GetGameObject<Player>();
    if (!player) return;

    // ステータス（HP/FOOD）は常に表示
    DrawPlayerStatus(player);

    // m_Stateごとに必要なウィンドウだけを描画する。複数画面を重ねる場合もここで順序を固定する。
    switch (m_State)
    {
    case UIState::MainMenu:
        DrawMainMenu(player);
        break;

    case UIState::ItemMenu:
        DrawItemMenu(player);
        break;

    case UIState::ItemCommand:
        DrawItemMenu(player);
        DrawItemCommandMenu(player);
        break;

    case UIState::AllyMenu:
        DrawAllyMenu();
        break;

    case UIState::GroundMenu:
        DrawGroundMenu(player);
        break;

    case UIState::PotMenu:
        DrawPotUI(player);
        break;

    case UIState::RecruitConfirm:
        DrawRecruitUI();
        break;

    case UIState::RecruitDismiss:
        DrawRecruitDismissUI();
        break;

    case UIState::Rename:
        DrawRenameUI(player);
        break;
    case UIState::ShrineConfirm:
        DrawShrineConfirmUI();
        break;
    default:
        break;
    }
}

void PlayerInventoryUI::DrawMainMenu(Player* player)
{
    (void)player;

    const std::vector<std::string> commands = {
        u8"道具",
        u8"仲間",
        u8"足元",
        u8"閉じる"
    };

    if (m_MainMenuCursor < 0) m_MainMenuCursor = 0;
    if (m_MainMenuCursor >= static_cast<int>(commands.size())) m_MainMenuCursor = static_cast<int>(commands.size()) - 1;

    const bool cancelTriggered = Input::GetKeyTrigger('X') && !m_BlockMainMenuCancel;
    m_BlockMainMenuCancel = false;

    if (Input::GetKeyTrigger(VK_UP)) m_MainMenuCursor = (m_MainMenuCursor + static_cast<int>(commands.size()) - 1) % static_cast<int>(commands.size());
    if (Input::GetKeyTrigger(VK_DOWN)) m_MainMenuCursor = (m_MainMenuCursor + 1) % static_cast<int>(commands.size());
    if (cancelTriggered) m_MainMenuCursor = static_cast<int>(commands.size()) - 1;

    if (Input::GetKeyTrigger('Z') || cancelTriggered)
    {
        switch (m_MainMenuCursor)
        {
        case 0:
            m_InventoryCursor = 0;
            m_ItemCommandTarget = -1;
            m_ItemCommandCursor = 0;
            m_State = UIState::ItemMenu;
            break;
        case 1:
            m_State = UIState::AllyMenu;
            break;
        case 2:
            m_State = UIState::GroundMenu;
            break;
        default:
            CloseAllMenus();
            break;
        }
        return;
    }

    UIRect rect{ 20.0f, 20.0f, 190.0f, 178.0f };
    UIRenderer::DrawWindow(rect);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(u8"メニュー", rect.x + 24.0f, rect.y + 18.0f, rect.w - 48.0f, 28.0f, 22.0f, D2D1::ColorF(D2D1::ColorF::White));

    float y = rect.y + 54.0f;
    for (int i = 0; i < static_cast<int>(commands.size()); ++i)
    {
        DrawMenuChoice(commands[i], i == m_MainMenuCursor, rect.x + 30.0f, y, rect.w - 60.0f, D2D1::ColorF(D2D1::ColorF::White));
        y += 27.0f;
    }

    UITextRenderer::End();
    RestoreMainRenderTarget();
}

void PlayerInventoryUI::DrawPlayerStatus(Player* player)
{
    const int hp = player->GetHP();
    const int maxHp = player->GetMaxHP();
    const float hpRate = (maxHp > 0) ? static_cast<float>(hp) / static_cast<float>(maxHp) : 0.0f;

    const int food = player->GetFullness();
    const int maxFood = player->GetMaxFullness();
    const float foodRate = (maxFood > 0) ? static_cast<float>(food) / static_cast<float>(maxFood) : 0.0f;

    const float panelW = 456.0f;
    const float panelH = 108.0f;
    const float panelX = static_cast<float>(SCREEN_WIDTH) * 0.5f - panelW * 0.5f;
    const float panelY = 16.0f;
    const UIRect panel{ panelX, panelY, panelW, panelH };

    UIRenderer::DrawSolidRect(panel, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.48f));
    UIRenderer::DrawSolidRect(UIRect{ panel.x + 2.0f, panel.y + 2.0f, panel.w - 4.0f, panel.h - 4.0f }, XMFLOAT4(0.03f, 0.03f, 0.03f, 0.58f));

    const UIRect hpGauge{ panel.x + 92.0f, panel.y + 14.0f, 330.0f, 14.0f };
    const UIRect foodGauge{ panel.x + 92.0f, panel.y + 40.0f, 330.0f, 14.0f };
    DrawStatusGauge(hpGauge, hpRate, XMFLOAT4(0.32f, 0.08f, 0.08f, 0.92f), XMFLOAT4(0.18f, 0.78f, 0.25f, 0.96f));
    DrawStatusGauge(foodGauge, foodRate, XMFLOAT4(0.18f, 0.16f, 0.10f, 0.92f), XMFLOAT4(0.94f, 0.78f, 0.22f, 0.96f));

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8("HP", panel.x + 22.0f, panel.y + 8.0f, 58.0f, 24.0f, 19.0f, D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8("FOOD", panel.x + 22.0f, panel.y + 34.0f, 58.0f, 24.0f, 18.0f, D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8(u8"ちから", panel.x + 22.0f, panel.y + 58.0f, 64.0f, 24.0f, 17.0f, D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8Aligned(
        std::to_string(hp) + " / " + std::to_string(maxHp),
        hpGauge.x,
        hpGauge.y - 5.0f,
        hpGauge.w - 8.0f,
        24.0f,
        16.0f,
        D2D1::ColorF(D2D1::ColorF::White),
        DWRITE_TEXT_ALIGNMENT_TRAILING);
    UITextRenderer::DrawOutlinedTextUtf8Aligned(
        std::to_string(food) + " / " + std::to_string(maxFood),
        foodGauge.x,
        foodGauge.y - 5.0f,
        foodGauge.w - 8.0f,
        24.0f,
        16.0f,
        D2D1::ColorF(D2D1::ColorF::White),
        DWRITE_TEXT_ALIGNMENT_TRAILING);
    UITextRenderer::DrawOutlinedTextUtf8Aligned(
        std::to_string(player->GetStrength()) + " / " + std::to_string(player->GetMaxStrength()),
        panel.x + 92.0f,
        panel.y + 56.0f,
        330.0f,
        24.0f,
        17.0f,
        D2D1::ColorF(0.75f, 0.95f, 1.0f, 1.0f),
        DWRITE_TEXT_ALIGNMENT_TRAILING);
    UITextRenderer::DrawOutlinedTextUtf8(
        "GOLD " + std::to_string(player->GetGold()),
        panel.x + 22.0f,
        panel.y + 82.0f,
        panel.w - 44.0f,
        22.0f,
        17.0f,
        D2D1::ColorF(1.0f, 0.90f, 0.30f, 1.0f));
    UITextRenderer::End();
    RestoreMainRenderTarget();
}
void PlayerInventoryUI::DrawItemMenu(Player* player)
{
    auto& items = player->GetItems();
    const bool activeList = (m_State == UIState::ItemMenu);

    if (items.empty())
    {
        if (Input::GetKeyTrigger('X')) m_State = UIState::MainMenu;

        UIRect rect{ 20.0f, 20.0f, 290.0f, 115.0f };
        UIRenderer::DrawWindow(rect);
        UITextRenderer::Begin();
        UITextRenderer::DrawOutlinedTextUtf8(u8"持ち物", rect.x + 22.0f, rect.y + 18.0f, rect.w - 44.0f, 28.0f, 22.0f, D2D1::ColorF(D2D1::ColorF::White));
        UITextRenderer::DrawOutlinedTextUtf8(u8"持ち物はない。", rect.x + 30.0f, rect.y + 58.0f, rect.w - 60.0f, 28.0f, 20.0f, D2D1::ColorF(D2D1::ColorF::White));
        UITextRenderer::End();
        RestoreMainRenderTarget();
        return;
    }

    if (m_InventoryCursor < 0) m_InventoryCursor = 0;
    if (m_InventoryCursor >= static_cast<int>(items.size())) m_InventoryCursor = static_cast<int>(items.size()) - 1;

    if (activeList)
    {
        if (Input::GetKeyTrigger(VK_UP)) m_InventoryCursor = (m_InventoryCursor + static_cast<int>(items.size()) - 1) % static_cast<int>(items.size());
        if (Input::GetKeyTrigger(VK_DOWN)) m_InventoryCursor = (m_InventoryCursor + 1) % static_cast<int>(items.size());
        if (Input::GetKeyTrigger('Z'))
        {
            m_ItemCommandTarget = m_InventoryCursor;
            m_ItemCommandCursor = 0;
            m_State = UIState::ItemCommand;
        }
        if (Input::GetKeyTrigger('X')) m_State = UIState::MainMenu;
    }

    UIRect rect{ 20.0f, 20.0f, 310.0f, 398.0f };
    UIRenderer::DrawWindow(rect);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(u8"持ち物", rect.x + 22.0f, rect.y + 17.0f, rect.w - 44.0f, 28.0f, 22.0f, D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8(u8"[Space]で整頓", rect.x + 188.0f, rect.y + 21.0f, 105.0f, 24.0f, 14.0f, D2D1::ColorF(0.7f, 0.7f, 0.7f, 1.0f));

    const int visibleCount = 12;
    int start = 0;
    if (m_InventoryCursor >= visibleCount) start = m_InventoryCursor - visibleCount + 1;
    const int end = (std::min)(start + visibleCount, static_cast<int>(items.size()));

    float y = rect.y + 52.0f;
    for (int i = start; i < end; ++i)
    {
        ItemInstance& inst = items[i].instance;
        inst.SetStackCount(items[i].count);
        std::string label = inst.GetDisplayName();
        if (items[i].isEquipped) label = "[E] " + label;
        if (IsPlayerInShop(player)) label += "  " + std::to_string(ShopUI::GetItemSellPrice(inst)) + "G";

        const bool selected = (i == m_InventoryCursor);
        DrawMenuChoice(label, selected, rect.x + 26.0f, y, rect.w - 52.0f, ToD2DColor(inst.GetNameColor()));
        y += 27.0f;
    }

    if (static_cast<int>(items.size()) > visibleCount)
    {
        std::string page = std::to_string(m_InventoryCursor + 1) + " / " + std::to_string(items.size());
        UITextRenderer::DrawOutlinedTextUtf8(page, rect.x + rect.w - 82.0f, rect.y + rect.h - 30.0f, 70.0f, 24.0f, 16.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f, 1.0f));
    }

    UITextRenderer::End();
    RestoreMainRenderTarget();
}
void PlayerInventoryUI::DrawItemCommandMenu(Player* player)
{
    if (m_ItemCommandTarget < 0)
    {
        m_State = UIState::ItemMenu;
        return;
    }

    auto& inventory = player->GetItems();
    if (m_ItemCommandTarget >= static_cast<int>(inventory.size()))
    {
        m_ItemCommandTarget = -1;
        m_ItemCommandCursor = 0;
        m_State = UIState::ItemMenu;
        return;
    }

    m_InventoryCursor = m_ItemCommandTarget;

    auto& itemInst = inventory[m_ItemCommandTarget].instance;
    const ItemData* data = itemInst.GetData();

    struct CommandEntry
    {
        std::string label;
        std::function<void()> action;
    };

    std::vector<CommandEntry> commands;

    if (data->type == ItemType::Weapon || data->type == ItemType::Shield || data->type == ItemType::Arrow)
    {
        const bool isEquipped = inventory[m_ItemCommandTarget].isEquipped;
        commands.push_back({ isEquipped ? u8"外す" : u8"装備する", [this, player]() {
            player->EquipItem(m_ItemCommandTarget);
            CloseAllMenus();
        } });
    }

    if (data->type == ItemType::Arrow)
    {
        commands.push_back({ u8"発射", [this, player]() {
            player->ShootArrow(m_ItemCommandTarget);
            CloseAllMenus();
        } });
    }
    else if (data->type == ItemType::Staff || data->type == ItemType::Herb || data->type == ItemType::Food)
    {
        const char* label = (data->type == ItemType::Staff) ? u8"振る" :
            (data->type == ItemType::Herb) ? u8"飲む" : u8"食べる";
        commands.push_back({ label, [this, player]() {
            player->UseItem(m_ItemCommandTarget);
            CloseAllMenus();
        } });
    }
    else if (data->type == ItemType::Pot)
    {
        commands.push_back({ u8"入れる・中を見る", [this]() {
            m_OpenPotIndex = m_ItemCommandTarget;
            m_State = UIState::PotMenu;
            m_ItemCommandTarget = -1;
            m_ItemCommandCursor = 0;
        } });
    }

    if (IsPlayerInShop(player))
    {
        commands.push_back({ u8"売る", [this, player]() {
            ShopUI::SellInventoryItem(player, m_ItemCommandTarget);
            CloseAllMenus();
        } });
    }

    commands.push_back({ u8"投げる", [this, player]() {
        player->ThrowItem(m_ItemCommandTarget);
        CloseAllMenus();
    } });

    commands.push_back({ u8"足元に置く", [this, player]() {
        DropItemAtFeet(player, m_ItemCommandTarget);
        CloseAllMenus();
    } });

    commands.push_back({ u8"交換", [this, player]() {
        ExchangeItemWithGroundItem(player, m_ItemCommandTarget);
        CloseAllMenus();
    } });

    if (itemInst.GetData()->canRename && !itemInst.IsOptionRevealed() && itemInst.GetIdentifiedState() == IdentifyState::Unidentified && !itemInst.IsOptionRevealed())
    {
        commands.push_back({ u8"名前を付ける", [this, &itemInst]() {
            m_RenameTargetIndex = m_ItemCommandTarget;
            memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
            strncpy_s(m_RenameBuffer, itemInst.GetDisplayName().c_str(), _TRUNCATE);
            m_State = UIState::Rename;
            m_ItemCommandTarget = -1;
            m_ItemCommandCursor = 0;
        } });
    }

    commands.push_back({ u8"キャンセル", [this]() {
        m_ItemCommandTarget = -1;
        m_ItemCommandCursor = 0;
        m_State = UIState::ItemMenu;
    } });

    if (commands.empty())
    {
        m_ItemCommandTarget = -1;
        m_ItemCommandCursor = 0;
        m_State = UIState::ItemMenu;
        return;
    }

    if (m_ItemCommandCursor < 0) m_ItemCommandCursor = 0;
    if (m_ItemCommandCursor >= static_cast<int>(commands.size())) m_ItemCommandCursor = static_cast<int>(commands.size()) - 1;

    if (Input::GetKeyTrigger(VK_UP)) m_ItemCommandCursor = (m_ItemCommandCursor + static_cast<int>(commands.size()) - 1) % static_cast<int>(commands.size());
    if (Input::GetKeyTrigger(VK_DOWN)) m_ItemCommandCursor = (m_ItemCommandCursor + 1) % static_cast<int>(commands.size());
    if (Input::GetKeyTrigger('X')) m_ItemCommandCursor = static_cast<int>(commands.size()) - 1;

    if (Input::GetKeyTrigger('Z') || Input::GetKeyTrigger('X'))
    {
        commands[m_ItemCommandCursor].action();
        return;
    }

    const float commandHeight = 52.0f + static_cast<float>(commands.size()) * 27.0f;
    UIRect rect{ 345.0f, 20.0f, 220.0f, commandHeight };
    UIRenderer::DrawWindow(rect);

    itemInst.SetStackCount(inventory[m_ItemCommandTarget].count);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(u8"操作選択", rect.x + 22.0f, rect.y + 15.0f, rect.w - 44.0f, 26.0f, 21.0f, D2D1::ColorF(D2D1::ColorF::White));

    float y = rect.y + 44.0f;
    for (int i = 0; i < static_cast<int>(commands.size()); ++i)
    {
        DrawMenuChoice(commands[i].label, i == m_ItemCommandCursor, rect.x + 26.0f, y, rect.w - 52.0f, D2D1::ColorF(D2D1::ColorF::White));
        y += 27.0f;
    }

    UITextRenderer::End();
    RestoreMainRenderTarget();
}
void PlayerInventoryUI::DrawAllyMenu()
{
    const auto& allies = UnitManager::Instance()->GetAllies();

    if (Input::GetKeyTrigger('X'))
    {
        ClearAllyCameraFocus(true);
        m_State = UIState::MainMenu;
        return;
    }

    UIRect rect{ 20.0f, 20.0f, 430.0f, 252.0f };
    UIRenderer::DrawWindow(rect);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(u8"仲間", rect.x + 22.0f, rect.y + 16.0f, rect.w - 44.0f, 28.0f, 22.0f, D2D1::ColorF(D2D1::ColorF::White));

    if (allies.empty())
    {
        ClearAllyCameraFocus(false);
        UITextRenderer::DrawOutlinedTextUtf8(u8"仲間はいない。", rect.x + 30.0f, rect.y + 58.0f, rect.w - 60.0f, 28.0f, 20.0f, D2D1::ColorF(D2D1::ColorF::White));
        UITextRenderer::End();
        RestoreMainRenderTarget();
        return;
    }

    if (m_AllyCursor < 0) m_AllyCursor = 0;
    if (m_AllyCursor >= static_cast<int>(allies.size())) m_AllyCursor = static_cast<int>(allies.size()) - 1;

    struct AllyCommand
    {
        const char* label;
        AllyAIMode mode;
    };
    const std::vector<AllyCommand> commands = {
        { u8"追従", AllyAIMode::Follow },
        { u8"応戦", AllyAIMode::Counter },
        { u8"待機", AllyAIMode::Wait },
        { u8"巡回", AllyAIMode::Patrol },
        { u8"特技禁止", AllyAIMode::NoSkill },
        { u8"撤退", AllyAIMode::Retreat }
    };

    if (m_AllyCommandCursor < 0) m_AllyCommandCursor = 0;
    if (m_AllyCommandCursor >= static_cast<int>(commands.size())) m_AllyCommandCursor = static_cast<int>(commands.size()) - 1;

    if (Input::GetKeyTrigger(VK_UP)) m_AllyCursor = (m_AllyCursor + static_cast<int>(allies.size()) - 1) % static_cast<int>(allies.size());
    if (Input::GetKeyTrigger(VK_DOWN)) m_AllyCursor = (m_AllyCursor + 1) % static_cast<int>(allies.size());
    if (Input::GetKeyTrigger(VK_LEFT)) m_AllyCommandCursor = (m_AllyCommandCursor + static_cast<int>(commands.size()) - 1) % static_cast<int>(commands.size());
    if (Input::GetKeyTrigger(VK_RIGHT)) m_AllyCommandCursor = (m_AllyCommandCursor + 1) % static_cast<int>(commands.size());
    if (Input::GetKeyTrigger('Z'))
    {
        if (Ally* ally = allies[m_AllyCursor])
        {
            ally->SetAIMode(commands[m_AllyCommandCursor].mode);
            RevealAllyOnMiniMap(ally);
        }
    }

    Ally* ally = allies[m_AllyCursor];
    UpdateAllyCameraFocus(ally);
    const std::string name = ally ? ally->GetName() : u8"不明";
    DrawMenuChoice(name, true, rect.x + 26.0f, rect.y + 52.0f, rect.w - 52.0f, D2D1::ColorF(D2D1::ColorF::White));

    if (ally)
    {
        std::string status = "Lv " + std::to_string(ally->GetLevel()) + "  HP " + std::to_string(ally->GetHP()) + " / " + std::to_string(ally->GetMaxHP());
        UITextRenderer::DrawOutlinedTextUtf8(status, rect.x + 34.0f, rect.y + 84.0f, rect.w - 68.0f, 24.0f, 17.0f, D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f));

        std::string params = "ATK " + std::to_string(ally->GetATK()) + "  DEF " + std::to_string(ally->GetDEF()) + "  ACC " + std::to_string(ally->GetACC()) + "  EVD " + std::to_string(ally->GetEVD());
        UITextRenderer::DrawOutlinedTextUtf8(params, rect.x + 34.0f, rect.y + 108.0f, rect.w - 68.0f, 24.0f, 17.0f, D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f));

        std::string ai = std::string("AI: ") + ally->GetAIModeName();
        UITextRenderer::DrawOutlinedTextUtf8(ai, rect.x + 34.0f, rect.y + 132.0f, rect.w - 68.0f, 24.0f, 17.0f, D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f));
    }

    float x = rect.x + 30.0f;
    float y = rect.y + 164.0f;
    for (int i = 0; i < static_cast<int>(commands.size()); ++i)
    {
        DrawMenuChoice(commands[i].label, i == m_AllyCommandCursor, x, y, 122.0f, D2D1::ColorF(D2D1::ColorF::White));
        x += 124.0f;
        if ((i % 3) == 2)
        {
            x = rect.x + 30.0f;
            y += 27.0f;
        }
    }

    UITextRenderer::End();
    RestoreMainRenderTarget();
}

void PlayerInventoryUI::DrawGroundMenu(Player* player)
{
    MapData* map = MapManager::Instance()->GetCurrentMap();
    MapObject* obj = (map && player) ? map->GetObjectAt(player->GetGridPos()) : nullptr;

    struct GroundCommand
    {
        std::string label;
        std::function<void()> action;
    };

    std::string message;
    std::vector<std::string> details;
    std::vector<GroundCommand> commands;

    if (!obj)
    {
        message = u8"足元には何もない。";
    }
    else if (Item* item = dynamic_cast<Item*>(obj))
    {
        message = item->GetInstance().GetDisplayName();
        if (item->IsShopItem()) details.push_back(u8"価格: " + std::to_string(item->GetInstance().GetBuyPrice()) + "G");
        if (item->IsPlayerShopItem()) details.push_back(u8"売値: " + std::to_string(item->GetInstance().GetSellPrice()) + "G");
        commands.push_back({ u8"拾う", [this, player, item]() {
            const bool shopItem = item->IsShopItem();
            item->OnStepped(player);
            if (!shopItem) CloseAllMenus();
        } });
    }
    else if (Trap* trap = dynamic_cast<Trap*>(obj))
    {
        message = u8"罠がある。";
        commands.push_back({ u8"調べる", [this, trap]() {
            trap->SetVisible(true);
            MessageLog::Instance().AddMessage(u8"足元の罠を確認した。");
            CloseAllMenus();
        } });
        commands.push_back({ u8"踏む", [this, player, trap]() {
            trap->OnStepped(player);
            CloseAllMenus();
            player->EndTurn();
        } });
    }
    else if (Shrine* shrine = dynamic_cast<Shrine*>(obj))
    {
        message = u8"不思議な祠がある。";
        commands.push_back({ u8"調べる", [shrine, player]() { shrine->OnStepped(player); } });
    }
    else
    {
        message = u8"足元のものは操作できない。";
    }

    commands.push_back({ u8"戻る", [this]() { m_State = UIState::MainMenu; } });

    if (m_GroundCursor < 0) m_GroundCursor = 0;
    if (m_GroundCursor >= static_cast<int>(commands.size())) m_GroundCursor = static_cast<int>(commands.size()) - 1;

    if (Input::GetKeyTrigger(VK_UP)) m_GroundCursor = (m_GroundCursor + static_cast<int>(commands.size()) - 1) % static_cast<int>(commands.size());
    if (Input::GetKeyTrigger(VK_DOWN)) m_GroundCursor = (m_GroundCursor + 1) % static_cast<int>(commands.size());
    if (Input::GetKeyTrigger('X'))
    {
        m_State = UIState::MainMenu;
        return;
    }
    if (Input::GetKeyTrigger('Z'))
    {
        commands[m_GroundCursor].action();
        return;
    }

    const float height = 132.0f + static_cast<float>(details.size()) * 24.0f + static_cast<float>(commands.size()) * 27.0f;
    UIRect rect{ 20.0f, 20.0f, 330.0f, height };
    UIRenderer::DrawWindow(rect);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(u8"足元", rect.x + 22.0f, rect.y + 16.0f, rect.w - 44.0f, 28.0f, 22.0f, D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8(message, rect.x + 30.0f, rect.y + 54.0f, rect.w - 60.0f, 32.0f, 20.0f, D2D1::ColorF(D2D1::ColorF::White));

    float y = rect.y + 84.0f;
    for (const std::string& detail : details)
    {
        UITextRenderer::DrawOutlinedTextUtf8(detail, rect.x + 34.0f, y, rect.w - 68.0f, 24.0f, 17.0f, D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f));
        y += 24.0f;
    }

    y += 12.0f;
    for (int i = 0; i < static_cast<int>(commands.size()); ++i)
    {
        DrawMenuChoice(commands[i].label, i == m_GroundCursor, rect.x + 30.0f, y, rect.w - 60.0f, D2D1::ColorF(D2D1::ColorF::White));
        y += 27.0f;
    }

    UITextRenderer::End();
    RestoreMainRenderTarget();
}
void PlayerInventoryUI::DrawPotUI(Player* player) {
    auto& items = player->GetItems();
    if (m_OpenPotIndex < 0 || m_OpenPotIndex >= (int)items.size()) {
        m_State = UIState::ItemMenu;
        return;
    }

    PotData* pot = items[m_OpenPotIndex].instance.GetPot();
    if (!pot) { m_State = UIState::ItemMenu; return; }

    std::vector<int> inventoryIndices;
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
    {
        if (i != m_OpenPotIndex) inventoryIndices.push_back(i);
    }

    const int potCount = static_cast<int>(pot->items.size());
    const int invCount = static_cast<int>(inventoryIndices.size());
    const int potRows = potCount + 1;
    const int invRows = invCount + 1;

    if (m_PotSide < 0) m_PotSide = 0;
    if (m_PotSide > 1) m_PotSide = 1;
    if (m_PotItemCursor < 0) m_PotItemCursor = 0;
    if (m_PotItemCursor >= potRows) m_PotItemCursor = potRows - 1;
    if (m_PotInventoryCursor < 0) m_PotInventoryCursor = 0;
    if (m_PotInventoryCursor >= invRows) m_PotInventoryCursor = invRows - 1;

    if (Input::GetKeyTrigger(VK_LEFT) || Input::GetKeyTrigger(VK_RIGHT)) m_PotSide = 1 - m_PotSide;
    if (Input::GetKeyTrigger(VK_UP))
    {
        if (m_PotSide == 0) m_PotItemCursor = (m_PotItemCursor + potRows - 1) % potRows;
        else m_PotInventoryCursor = (m_PotInventoryCursor + invRows - 1) % invRows;
    }
    if (Input::GetKeyTrigger(VK_DOWN))
    {
        if (m_PotSide == 0) m_PotItemCursor = (m_PotItemCursor + 1) % potRows;
        else m_PotInventoryCursor = (m_PotInventoryCursor + 1) % invRows;
    }
    if (Input::GetKeyTrigger('X'))
    {
        m_SelectedInventoryIdx.clear();
        m_SelectedPotItemIdx.clear();
        m_State = UIState::ItemMenu;
        return;
    }
    if (Input::GetKeyTrigger('Z'))
    {
        if (m_PotSide == 0)
        {
            if (m_PotItemCursor < potCount)
            {
                if (m_SelectedPotItemIdx.count(m_PotItemCursor) > 0) m_SelectedPotItemIdx.erase(m_PotItemCursor);
                else m_SelectedPotItemIdx.insert(m_PotItemCursor);
            }
            else
            {
                ProcessBulkTakeOutFromPot(player);
            }
        }
        else
        {
            if (m_PotInventoryCursor < invCount)
            {
                const int itemIndex = inventoryIndices[m_PotInventoryCursor];
                if (m_SelectedInventoryIdx.count(itemIndex) > 0) m_SelectedInventoryIdx.erase(itemIndex);
                else m_SelectedInventoryIdx.insert(itemIndex);
            }
            else
            {
                ProcessBulkPutIntoPot(player);
            }
        }
        return;
    }

    UIRect rect{ 20.0f, 20.0f, 610.0f, 392.0f };
    UIRenderer::DrawWindow(rect);

    const UIRect potRect{ rect.x + 24.0f, rect.y + 58.0f, 268.0f, 292.0f };
    const UIRect invRect{ rect.x + 318.0f, rect.y + 58.0f, 268.0f, 292.0f };

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(u8"壺の操作", rect.x + 22.0f, rect.y + 16.0f, rect.w - 44.0f, 28.0f, 22.0f, D2D1::ColorF(D2D1::ColorF::White));

    std::string potTitle = u8"壺の中 " + std::to_string(potCount) + " / " + std::to_string(pot->capacity);
    UITextRenderer::DrawOutlinedTextUtf8(potTitle, potRect.x, rect.y + 42.0f, potRect.w, 24.0f, 17.0f, m_PotSide == 0 ? D2D1::ColorF(1.0f, 0.92f, 0.45f, 1.0f) : D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f));
    UITextRenderer::DrawOutlinedTextUtf8(u8"手持ち", invRect.x, rect.y + 42.0f, invRect.w, 24.0f, 17.0f, m_PotSide == 1 ? D2D1::ColorF(1.0f, 0.92f, 0.45f, 1.0f) : D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f));

    const int visibleCount = 8;
    int potStart = 0;
    if (m_PotItemCursor >= visibleCount) potStart = m_PotItemCursor - visibleCount + 1;
    int potEnd = (std::min)(potStart + visibleCount, potCount);

    float y = potRect.y;
    if (potCount == 0)
    {
        UITextRenderer::DrawOutlinedTextUtf8(u8"空っぽ。", potRect.x + 10.0f, y, potRect.w - 20.0f, 28.0f, 20.0f, D2D1::ColorF(0.75f, 0.75f, 0.75f, 1.0f));
        y += 27.0f;
    }
    for (int i = potStart; i < potEnd; ++i)
    {
        std::string label = (m_SelectedPotItemIdx.count(i) > 0 ? "[x] " : "[ ] ") + pot->items[i].GetDisplayName();
        DrawMenuChoice(label, m_PotSide == 0 && i == m_PotItemCursor, potRect.x, y, potRect.w, D2D1::ColorF(D2D1::ColorF::White));
        y += 27.0f;
    }
    const bool potActionSelected = (m_PotSide == 0 && m_PotItemCursor == potCount);
    DrawMenuChoice(u8"取り出す", potActionSelected, potRect.x, potRect.y + 244.0f, potRect.w, D2D1::ColorF(D2D1::ColorF::White));

    int invStart = 0;
    if (m_PotInventoryCursor >= visibleCount) invStart = m_PotInventoryCursor - visibleCount + 1;
    int invEnd = (std::min)(invStart + visibleCount, invCount);

    y = invRect.y;
    if (invCount == 0)
    {
        UITextRenderer::DrawOutlinedTextUtf8(u8"入れられる物がない。", invRect.x + 10.0f, y, invRect.w - 20.0f, 28.0f, 20.0f, D2D1::ColorF(0.75f, 0.75f, 0.75f, 1.0f));
        y += 27.0f;
    }
    for (int viewIndex = invStart; viewIndex < invEnd; ++viewIndex)
    {
        const int itemIndex = inventoryIndices[viewIndex];
        items[itemIndex].instance.SetStackCount(items[itemIndex].count);
        std::string label = (m_SelectedInventoryIdx.count(itemIndex) > 0 ? "[x] " : "[ ] ") + items[itemIndex].instance.GetDisplayName();
        DrawMenuChoice(label, m_PotSide == 1 && viewIndex == m_PotInventoryCursor, invRect.x, y, invRect.w, ToD2DColor(items[itemIndex].instance.GetNameColor()));
        y += 27.0f;
    }
    const bool invActionSelected = (m_PotSide == 1 && m_PotInventoryCursor == invCount);
    DrawMenuChoice(u8"入れる", invActionSelected, invRect.x, invRect.y + 244.0f, invRect.w, D2D1::ColorF(D2D1::ColorF::White));

    UITextRenderer::End();
    RestoreMainRenderTarget();
}
void PlayerInventoryUI::ProcessBulkPutIntoPot(Player* player) {
    PotUI::BulkPutInto(player, m_OpenPotIndex, m_SelectedInventoryIdx);
    m_SelectedInventoryIdx.clear();
    CloseAllMenus(); // UIを閉じる
}

void PlayerInventoryUI::ProcessBulkTakeOutFromPot(Player* player) {
    PotUI::BulkTakeOut(player, m_OpenPotIndex, m_SelectedPotItemIdx);
    m_SelectedPotItemIdx.clear();
    // 取り出した後はメニューを開いたままにする
}
void PlayerInventoryUI::PutItemIntoPot(Player* player, int inventoryIndex)
{
    // 引数に現在の開いている壺のインデックスを渡し、あとの処理は委譲
    if (PotUI::TryPut(player, m_OpenPotIndex, inventoryIndex)) {
        // 成功したら画面を閉じる
        CloseAllMenus();
    }
}
void PlayerInventoryUI::DrawRenameUI(Player* player)
{
    // KeyboardUI クラスへ処理を委譲
    int res = KeyboardUI::Draw(m_RenameBuffer, sizeof(m_RenameBuffer), m_RenameMode);

    if (res == 1) { // 決定
        if (m_RenameTargetIndex >= 0) {
            // アイテム命名
            std::string realName = player->GetItems()[m_RenameTargetIndex].instance.GetTrueName();
            ItemIdentificationManager::Instance().GetInfo(realName).customName = m_RenameBuffer;
            MessageLog::Instance().AddMessage(u8"アイテムを「" + std::string(m_RenameBuffer) + u8"」と名付けた。");
            m_State = UIState::ItemMenu; // アイテム一覧に戻る
        }
        else if (m_RenameTargetIndex == -2 && m_RecruitTarget) {
            // 勧誘成功処理を RecruitManager へ委譲
            const std::string recruitName = (strlen(m_RenameBuffer) > 0) ? std::string(m_RenameBuffer) : m_RecruitTarget->GetName();
            RecruitManager::ExecuteRecruit(m_RecruitTarget, recruitName);
            CloseAllMenus();
        }
        m_RenameTargetIndex = -1;
    }
    else if (res == -1) { // キャンセル
        m_State = (m_RenameTargetIndex == -2) ? UIState::RecruitConfirm : UIState::ItemMenu;
        m_RenameTargetIndex = -1;
    }
}
void PlayerInventoryUI::BeginRecruitNameInput()
{
    if (!m_RecruitTarget) { CloseAllMenus(); return; }

    m_RenameTargetIndex = -2;
    memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
    strncpy_s(m_RenameBuffer, m_RecruitTarget->GetName().c_str(), _TRUNCATE);
    m_State = UIState::Rename;
}

void PlayerInventoryUI::DismissAllyForRecruit(Ally* ally)
{
    if (!ally) return;

    std::string allyName = ally->GetName();
    UnitManager::Instance()->RemoveAlly(ally);
    ally->StopLoopEffect();
    ally->SetDestroy();
    MessageLog::Instance().AddMessage(allyName + u8"と別れた。");

    BeginRecruitNameInput();
}

void PlayerInventoryUI::DrawRecruitDismissUI()
{
    if (!m_RecruitTarget) { CloseAllMenus(); return; }

    const auto& allies = UnitManager::Instance()->GetAllies();
    if (static_cast<int>(allies.size()) < 10)
    {
        BeginRecruitNameInput();
        return;
    }

    if (m_RecruitDismissCursor < 0) m_RecruitDismissCursor = 0;
    if (m_RecruitDismissCursor >= static_cast<int>(allies.size())) m_RecruitDismissCursor = static_cast<int>(allies.size()) - 1;

    const bool confirmTriggered = Input::GetKeyTrigger('Z') && !m_BlockRecruitDismiss;
    const bool cancelTriggered = Input::GetKeyTrigger('X') && !m_BlockRecruitDismiss;
    m_BlockRecruitDismiss = false;

    if (Input::GetKeyTrigger(VK_UP)) m_RecruitDismissCursor = (m_RecruitDismissCursor + static_cast<int>(allies.size()) - 1) % static_cast<int>(allies.size());
    if (Input::GetKeyTrigger(VK_DOWN)) m_RecruitDismissCursor = (m_RecruitDismissCursor + 1) % static_cast<int>(allies.size());

    if (cancelTriggered)
    {
        m_BlockRecruitConfirm = true;
        m_State = UIState::RecruitConfirm;
        return;
    }
    if (confirmTriggered)
    {
        DismissAllyForRecruit(allies[m_RecruitDismissCursor]);
        return;
    }

    UIRect rect{ SCREEN_WIDTH - 500.0f, SCREEN_HEIGHT - 455.0f, 440.0f, 365.0f };
    UIRenderer::DrawWindow(rect);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(u8"仲間がいっぱいです", rect.x + 24.0f, rect.y + 18.0f, rect.w - 48.0f, 28.0f, 22.0f, D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8(u8"誰と別れますか？", rect.x + 34.0f, rect.y + 52.0f, rect.w - 68.0f, 28.0f, 20.0f, D2D1::ColorF(D2D1::ColorF::White));

    float y = rect.y + 88.0f;
    for (int i = 0; i < static_cast<int>(allies.size()); ++i)
    {
        Ally* ally = allies[i];
        std::string label = ally ? ally->GetName() : u8"不明";
        if (ally)
        {
            label += "  Lv " + std::to_string(ally->GetLevel()) + "  HP " + std::to_string(ally->GetHP()) + "/" + std::to_string(ally->GetMaxHP());
        }
        DrawRecruitChoice(label.c_str(), i == m_RecruitDismissCursor, rect.x + 38.0f, y, rect.w - 76.0f);
        y += 27.0f;
    }

    UITextRenderer::DrawOutlinedTextUtf8(u8"X: 戻る", rect.x + 34.0f, rect.y + rect.h - 34.0f, rect.w - 68.0f, 24.0f, 16.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f, 1.0f));
    UITextRenderer::End();
    RestoreMainRenderTarget();
}
void PlayerInventoryUI::DrawRecruitUI()
{
    if (!m_RecruitTarget) { CloseAllMenus(); return; }

    if (!m_RecruitMessageShown)
    {
        MessageLog::Instance().AddMessage(u8"なんと" + m_RecruitTarget->GetName() + u8"が起き上がり");
        MessageLog::Instance().AddMessage(u8"仲間になりたそうに　こちらを見ている！");
        MessageLog::Instance().AddMessage(u8"仲間にしてあげますか？");
        m_RecruitMessageShown = true;
    }

    if (m_BlockRecruitConfirm)
    {
        if (!Input::GetKeyPress('Z'))
        {
            m_BlockRecruitConfirm = false;
        }
    }

    const bool confirmTriggered = Input::GetKeyTrigger('Z') && !m_BlockRecruitConfirm;
    const bool cancelTriggered = Input::GetKeyTrigger('X');

    if (Input::GetKeyTrigger(VK_UP) || Input::GetKeyTrigger(VK_DOWN)) m_RecruitChoiceIndex = 1 - m_RecruitChoiceIndex;
    if (cancelTriggered) m_RecruitChoiceIndex = 1;

    if (confirmTriggered || cancelTriggered)
    {
        if (m_RecruitChoiceIndex == 0 && confirmTriggered)
        {
            m_RenameTargetIndex = -2;
            memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
            strncpy_s(m_RenameBuffer, m_RecruitTarget->GetName().c_str(), _TRUNCATE);
            m_State = UIState::Rename;
            return;
        }

        RecruitManager::DeclineRecruit(m_RecruitTarget);
        CloseAllMenus();
        return;
    }

    UIRect rect{ SCREEN_WIDTH - 340.0f, SCREEN_HEIGHT - 330.0f, 280.0f, 170.0f };
    UIRenderer::DrawWindow(rect);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(u8"勧誘確認", rect.x + 24.0f, rect.y + 18.0f, rect.w - 48.0f, 28.0f, 22.0f, D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8(u8"仲間にしますか？", rect.x + 34.0f, rect.y + 54.0f, rect.w - 68.0f, 28.0f, 20.0f, D2D1::ColorF(D2D1::ColorF::White));
    DrawRecruitChoice(u8"はい", m_RecruitChoiceIndex == 0, rect.x + 48.0f, rect.y + 92.0f, rect.w - 96.0f);
    DrawRecruitChoice(u8"いいえ", m_RecruitChoiceIndex == 1, rect.x + 48.0f, rect.y + 124.0f, rect.w - 96.0f);
    UITextRenderer::End();
    RestoreMainRenderTarget();
}
void PlayerInventoryUI::DrawShrineConfirmUI()
{
    const std::vector<std::string> commands = {
        u8"1階戻る",
        u8"2階戻る",
        u8"3階戻る",
        u8"やめる"
    };

    if (m_BlockShrineConfirm)
    {
        if (!Input::GetKeyPress('Z'))
        {
            m_BlockShrineConfirm = false;
        }
    }

    if (Input::GetKeyTrigger(VK_UP)) m_ShrineConfirmCursor = (m_ShrineConfirmCursor + static_cast<int>(commands.size()) - 1) % static_cast<int>(commands.size());
    if (Input::GetKeyTrigger(VK_DOWN)) m_ShrineConfirmCursor = (m_ShrineConfirmCursor + 1) % static_cast<int>(commands.size());
    if (Input::GetKeyTrigger('X')) m_ShrineConfirmCursor = static_cast<int>(commands.size()) - 1;

    const bool confirmTriggered = Input::GetKeyTrigger('Z') && !m_BlockShrineConfirm;
    const bool cancelTriggered = Input::GetKeyTrigger('X');
    if (confirmTriggered || cancelTriggered)
    {
        if (m_ShrineConfirmCursor >= 0 && m_ShrineConfirmCursor <= 2 && confirmTriggered)
        {
            MapManager::Instance()->RequestBackFloor(m_ShrineConfirmCursor + 1);
            CloseAllMenus();
        }
        else
        {
            CloseAllMenus();
        }
        return;
    }

    UIRect rect{
        SCREEN_WIDTH * 0.5f - 170.0f,
        SCREEN_HEIGHT * 0.5f - 120.0f,
        340.0f,
        238.0f
    };
    UIRenderer::DrawWindow(rect);

    UITextRenderer::Begin();
    UITextRenderer::DrawOutlinedTextUtf8(
        u8"不思議な祠",
        rect.x + 28.0f,
        rect.y + 18.0f,
        rect.w - 56.0f,
        30.0f,
        24.0f,
        D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8(
        u8"祠が激しく震えている！",
        rect.x + 34.0f,
        rect.y + 58.0f,
        rect.w - 68.0f,
        28.0f,
        20.0f,
        D2D1::ColorF(D2D1::ColorF::White));
    UITextRenderer::DrawOutlinedTextUtf8(
        u8"過去の階層へ戻りますか？",
        rect.x + 34.0f,
        rect.y + 86.0f,
        rect.w - 68.0f,
        28.0f,
        20.0f,
        D2D1::ColorF(D2D1::ColorF::White));

    float y = rect.y + 124.0f;
    for (int i = 0; i < static_cast<int>(commands.size()); ++i)
    {
        const D2D1_COLOR_F color = (i == 3)
            ? D2D1::ColorF(1.0f, 0.58f, 0.58f, 1.0f)
            : D2D1::ColorF(D2D1::ColorF::White);
        DrawMenuChoice(commands[i], m_ShrineConfirmCursor == i, rect.x + 48.0f, y, rect.w - 96.0f, color);
        y += 27.0f;
    }
    UITextRenderer::End();
    RestoreMainRenderTarget();
}

void PlayerInventoryUI::DropItemAtFeet(Player* player, int inventoryIndex)
{
    if (!player) return;
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return;

    auto& items = player->GetItems();
    if (inventoryIndex < 0 || inventoryIndex >= (int)items.size()) return;

    Vector2Int pos = player->GetGridPos();
    if (map->GetObjectAt(pos)) {
        MessageLog::Instance().AddMessage(u8"足元にはすでに何かがある。");
        return;
    }

    items[inventoryIndex].instance.SetStackCount(items[inventoryIndex].count);
    std::string name = items[inventoryIndex].instance.GetDisplayName();
    const bool droppedInShop = ShopUI::IsPlayerOnShopTile(player);
    const bool returningShopItem = droppedInShop && items[inventoryIndex].instance.IsUnpaidShopItem();
    ItemInstance dropped = std::move(items[inventoryIndex].instance);
    player->RemoveItemAt(inventoryIndex);

    Item* item = Manager::GetScene()->AddGameObject<Item>(1);
    item->SetInstance(std::move(dropped));
    if (returningShopItem)
    {
        // 未払いの商品を店内に置いた場合は売却予定にせず、商品を棚へ戻した扱いにする。
        item->GetInstance().SetUnpaidShopItem(false);
        item->SetShopItem(true);
    }
    else if (droppedInShop)
    {
        // 自分の持ち物を店内に置いた時だけ、売却予定品として扱う。
        item->SetPlayerShopItem(true);
    }
    map->AddMapObject(item, pos.x, pos.y);

    if (returningShopItem)
        MessageLog::Instance().AddMessage(name + u8"を店の商品として戻した。");
    else if (droppedInShop)
        MessageLog::Instance().AddMessage(name + u8"を売り物として置いた。売値 " + std::to_string(item->GetInstance().GetSellPrice()) + u8"G。");
    else
        MessageLog::Instance().AddMessage(name + u8"を足元に置いた。");
    CloseAllMenus();
    player->EndTurn();
}

void PlayerInventoryUI::ExchangeItemWithGroundItem(Player* player, int inventoryIndex)
{
    if (!player) return;
    MapData* map = MapManager::Instance()->GetCurrentMap();
    if (!map) return;

    auto& items = player->GetItems();
    if (inventoryIndex < 0 || inventoryIndex >= (int)items.size()) return;

    // 店の商品は購入処理を通す必要があるため、通常の足元交換からは除外する。
    Item* groundItem = dynamic_cast<Item*>(map->GetObjectAt(player->GetGridPos()));
    if (!groundItem || groundItem->IsShopItem()) {
        MessageLog::Instance().AddMessage(u8"交換できる足元の道具がない。");
        return;
    }

    items[inventoryIndex].instance.SetStackCount(items[inventoryIndex].count);
    std::string handName = items[inventoryIndex].instance.GetDisplayName();
    std::string groundName = groundItem->GetInstance().GetDisplayName();
    ItemInstance handItem = std::move(items[inventoryIndex].instance);
    const int handCount = items[inventoryIndex].count;
    items[inventoryIndex].instance = std::move(groundItem->GetInstance());
    items[inventoryIndex].count = items[inventoryIndex].instance.GetStackCount();
    items[inventoryIndex].isEquipped = false;
    handItem.SetStackCount(handCount);
    groundItem->SetInstance(std::move(handItem));
    player->RefreshEquipIndices();

    MessageLog::Instance().AddMessage(handName + u8"と" + groundName + u8"を交換した。");
    CloseAllMenus();
    player->EndTurn();
}
bool PlayerInventoryUI::IsPlayerInShop(Player* player) const
{
    return ShopUI::IsPlayerOnShopTile(player);
}

