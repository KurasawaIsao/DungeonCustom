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
    // マップは3D描画の土台なので、ユニットやエフェクトの距離ソートに混ぜない。
    bool UsesCameraSort() const override { return false; }
    void Uninit() override;
    void UpdateTile(int x, int y, TileType type);
    void Clear();
    void SetEditor(bool editor) { m_IsEditor = editor; }
    static void SetTheme(const std::string& themeId);
private:
    void BuildOuterWallTiles();
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
    // マップ範囲外に敷く外周壁のグリッド座標を保持
    std::vector<Vector2Int> m_OuterWallTiles;
    // 各マスの現在のタイプを保持（Draw時の振り分け用）
    std::vector<TileType> m_TileTypes;
    std::vector<bool> m_ActiveTiles;
    std::vector<XMMATRIX> m_ShopMatrices;
    std::vector<VERTEX_3D> m_GridVertices;
    ID3D11Buffer* m_GridVertexBuffer = nullptr;
    int m_GridVertexBufferCapacity = 0;

    static ModelRenderer* m_FloorModel;
    static ModelRenderer* m_WallModel;
    static ModelRenderer* m_StairModel;
    static ModelRenderer* m_CorridorModel;
    static ModelRenderer* m_EditorCorridorModel;
    static ModelRenderer* m_ShopFloorModel;
    static std::string m_CurrentThemeId;
    static ID3D11VertexShader* m_GridVertexShader;
    static ID3D11PixelShader* m_GridPixelShader;
    static ID3D11InputLayout* m_GridVertexLayout;
};
