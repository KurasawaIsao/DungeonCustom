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
void Item::LoadModelForType(ModelRenderer* model, ItemType type)
{
    if (!model) return;

    // アイテムの表示モデルはここで一元管理し、利用側へパスを公開しない。
    switch (type)
    {
    case ItemType::Food:   model->Load("Asset\\Model\\Items\\Food.obj"); break;
    case ItemType::Pot:    model->Load("Asset\\Model\\Items\\Pot.obj"); break;
    case ItemType::Staff:  model->Load("Asset\\Model\\Items\\Staff.obj"); break;
    case ItemType::Arrow:  model->Load("Asset\\Model\\Items\\Arrow.obj"); break;
    case ItemType::Stone:  model->Load("Asset\\Model\\Items\\Stone.obj"); break;
    case ItemType::Weapon: model->Load("Asset\\Model\\Items\\Weapon.obj"); break;
    case ItemType::Shield: model->Load("Asset\\Model\\Items\\Shield.obj"); break;
    case ItemType::Herb:
    default:               model->Load("Asset\\Model\\Items\\Herb.obj"); break;
    }
}

void Item::SetupFromInstance()
{
    if (!m_Instance.GetData()) return;
    LoadModelForType(m_Model, m_Instance.GetData()->type);
}

void Item::Update()
{
    // 地面に配置されたアイテム自体には、毎フレーム更新する処理はない。
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

    MessageLog::Instance().AddMessage(
        (name + u8"を拾った").c_str()
    );
    MapManager::Instance()->GetCurrentMap()->RemoveMapObject(this);
    SetDestroy();

}

