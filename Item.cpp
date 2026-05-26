#include "Item.h"
#include "player.h"
#include "Manager.h"
#include "MapManager.h"
#include "MessageLog.h"
#include "LightManager.h"
#include "ModelRenderer.h"
#include "ShopUI.h"

void Item::Init()
{
    m_Model = new ModelRenderer();
    Renderer::CreateVertexShader(&m_VS, &m_Layout, "shader\\unlitTextureVS.cso");
    Renderer::CreatePixelShader(&m_PS, "shader\\unlitTexturePS.cso");

    m_Scale = { 1.0f, 1.0f, 1.0f };
}
void Item::SetupFromInstance()
{
    if (!m_Instance.GetData()) return;

    switch (m_Instance.GetData()->type)
    {
    case ItemType::Herb:
        m_Model->Load("Asset\\Model\\Items\\Herb.obj");
        break;

    case ItemType::Pot:
        m_Model->Load("Asset\\Model\\Items\\Pot.obj");
        break;
    case ItemType::Food:
        m_Model->Load("Asset\\Model\\Items\\Food.obj");
        break;
    case ItemType::Staff:
        m_Model->Load("Asset\\Model\\Items\\Staff.obj"); 
        break;
    case ItemType::Arrow:
        m_Model->Load("Asset\\Model\\Items\\Arrow.obj");
        break;
    case ItemType::Stone:
        m_Model->Load("Asset\\Model\\Items\\Stone.obj");
        break;
    case ItemType::Weapon:
        m_Model->Load("Asset\\Model\\Items\\Weapon.obj");
        break;
    case ItemType::Shield:
        m_Model->Load("Asset\\Model\\Items\\Shield.obj");
        break;
    }
}

void Item::StartThrow(Player* thrower, const Vector3& startPos, const Vector2Int& startGrid, const Vector2Int& direction)
{
    m_Thrower = thrower;
    m_StartPos = startPos;

    // 向きが0（入力なし）の場合は、とりあえず足元に落として終了
    if (direction.x == 0 && direction.y == 0) {
        m_TargetGrid = startGrid;
        m_TargetPos = startPos;
        m_IsFlying = false;
        OnArrived();
        return;
    }

    MapData* map = MapManager::Instance()->GetCurrentMap();

    // 出発点はプレイヤーがいる位置
    Vector2Int current = startGrid;
    const int maxRange = (m_Instance.GetData() && m_Instance.GetData()->type == ItemType::Stone) ? 2 : 10;

    bool hitWall = false;

    for (int i = 0; i < maxRange; i++)
    {
        Vector2Int next = current + direction;

        if (!map->IsInside(next))
        {
            hitWall = true;
            break;
        }

        // 壁だけ判定する関数を使う
        if (map->IsWall(next))
        {
            hitWall = true;
            break;
        }

        current = next;

        Unit* target = map->GetUnitAt(current.x, current.y);
        if (target && target != thrower)
            break;
    }

    m_TargetGrid = current;
    m_TargetPos = Vector3(m_TargetGrid.x * TILE_DISTANCE, 0.01f, m_TargetGrid.y * TILE_DISTANCE);
    m_HitWall = hitWall;

   

    // 演出用変数リセット
    m_FlyT = 0.0f;
    m_IsFlying = true;

}
void Item::Update()
{
    if (!m_IsFlying) return;

    m_FlyT += 0.016f / m_FlyDuration;
    if (m_FlyT >= 1.0f)
    {
        m_FlyT = 1.0f;
        m_IsFlying = false;
        OnArrived(); // 着弾！
        return;
    }

    // 水平移動 Lerp
    m_Position.x = m_StartPos.x + (m_TargetPos.x - m_StartPos.x) * m_FlyT;
    m_Position.z = m_StartPos.z + (m_TargetPos.z - m_StartPos.z) * m_FlyT;

    // 放物線の高さ（Y）
    float height = 1.2f;
    m_Position.y = -4.0f * height * m_FlyT * (m_FlyT - 1.0f) + m_StartPos.y;
}

void Item::Draw()
{
	if (m_Model == nullptr) return;
    Renderer::SetLight(LightManager::Instance().GetLight());
    Renderer::SetCommonShader();

    XMMATRIX scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    XMMATRIX trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    XMMATRIX world = scale * rot * trans;
    Renderer::SetWorldMatrix(world);

    m_Model->Draw();
}

void Item::Uninit()
{
    delete m_Model;
}

void Item::OnStepped(Player* player)
{
    // 店の商品は踏んだ時点では拾わず、価格つきの確認を ShopUI に任せる。
    if (m_IsShopItem)
    {
        auto* ui = Manager::GetScene()->GetGameObject<ShopUI>();
        if (ui) ui->OpenShopBuyMenu(this);
        return;
    }

    // インベントリの現在の個数を取得
    auto& items = player->GetItems();

    if (items.size() >= 20)
    {
        MessageLog::Instance().AddMessage(
            u8"持ち物がいっぱいで" + m_Instance.GetDisplayName() + u8"を拾えない。"
        );
        return; 
    }

    const std::string name = m_Instance.GetDisplayName();
    player->AddItem(std::move(m_Instance));
    SetDestroy();

    MessageLog::Instance().AddMessage(
        (name + u8"を拾った").c_str()
    );
    MapManager::Instance()->GetCurrentMap()->RemoveMapObject(this);
}

void Item::OnHitWall()
{
    MapData* map = MapManager::Instance()->GetCurrentMap();

    // 着弾座標を確定させる
    m_GridPos = m_TargetGrid;

    if (m_Instance.IsPot())
    {
        MessageLog::Instance().AddMessage(m_Instance.GetDisplayName() + u8"は割れた。");
        m_Instance.OnBreak(m_GridPos);

        SetDestroy();  
        return;
    }

    m_Instance.Throw(nullptr, nullptr, m_GridPos);
    if (m_Instance.GetData() && m_Instance.GetData()->type == ItemType::Stone)
    {
        MessageLog::Instance().AddMessage(m_Instance.GetDisplayName() + u8"は砕け散った。");
        SetDestroy();
        return;
    }

    m_Position = m_TargetPos;
    map->AddMapObject(this,GetGridPos().x, GetGridPos().y);
    MessageLog::Instance().AddMessage(m_Instance.GetDisplayName() + u8"は地面に落ちた。");
}
void Item::OnArrived()
{
    if (m_HitWall)
    {
        OnHitWall();
        return;
    }
    MapData* map = MapManager::Instance()->GetCurrentMap();
    Unit* hitUnit = map->GetUnitAt(m_TargetGrid.x,m_TargetGrid.y);

    // 投げた本人以外に当たったか？
    if (hitUnit && hitUnit != m_Thrower) {
        m_Instance.Throw(m_Thrower, hitUnit, m_TargetGrid);
        if (m_Instance.IsPot()) m_Instance.OnBreak(m_TargetGrid);
        SetDestroy();
    }
    else {
        // 地面に落ちる
        m_GridPos = m_TargetGrid;
        m_Instance.Throw(m_Thrower, nullptr, m_GridPos);
        if (m_Instance.GetData() && m_Instance.GetData()->type == ItemType::Stone)
        {
            MessageLog::Instance().AddMessage(m_Instance.GetDisplayName() + u8"は砕け散った。");
            SetDestroy();
            return;
        }

        m_Position = m_TargetPos;
        map->AddMapObject(this, GetGridPos().x, GetGridPos().y);
        MessageLog::Instance().AddMessage(m_Instance.GetDisplayName() + u8"は地面に落ちた。");
    }
}





