#pragma once
#include "GameObject.h"
#include "MapData.h"
#include "renderer.h"
#include <string>
class ModelRenderer;

class MapRenderer : public GameObject
{
public:
    void Init() override;
    void Build(const MapData& map);
    void Update() override {};
    void Draw() override;
    void Uninit() override;
    void UpdateTile(int x, int y, TileType type);
    void Clear();
    void SetEditor(bool editor) { m_IsEditor = editor; }
    static void SetTheme(const std::string& themeId);
private:
    void BuildGridVertices();
    void CreateGridVertexBuffer();
    void DrawGrid();
    void AddGridLine(const XMFLOAT3& start, const XMFLOAT3& end);
    static void LoadCurrentThemeModels();

    int m_Width = 0;
    int m_Height = 0;
    bool m_IsEditor = false;
    // 全マスの行列を保持
    std::vector<XMMATRIX> m_AllMatrices;
    // 各マスの現在のタイプを保持（Draw時の振り分け用）
    std::vector<TileType> m_TileTypes;
    std::vector<bool> m_ActiveTiles;
    std::vector<XMMATRIX> m_ShopMatrices;
    std::vector<VERTEX_3D> m_GridVertices;
    ID3D11Buffer* m_GridVertexBuffer = nullptr;
    int m_GridVertexBufferCapacity = 0;

    static ModelRenderer* s_FloorModel;
    static ModelRenderer* s_WallModel;
    static ModelRenderer* s_StairModel;
    static ModelRenderer* s_CorridorModel;
    static ModelRenderer* s_EditorCorridorModel;
    static ModelRenderer* s_ShopFloorModel;
    static std::string s_CurrentThemeId;
    static ID3D11VertexShader* s_GridVertexShader;
    static ID3D11PixelShader* s_GridPixelShader;
    static ID3D11InputLayout* s_GridVertexLayout;
};
