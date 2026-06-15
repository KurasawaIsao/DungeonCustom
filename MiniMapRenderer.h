#pragma once
#include <vector>
#include <d3d11.h>
#include "GameObject.h"
#include "Polygon.h"
#include "Vector2Int.h"

class MapData;
class Player;

class MiniMapRenderer : public GameObject
{
public:
    void Init(MapData* data, int drawX, int drawY, int drawW, int drawH);
    void Init() override {}
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void ResetMap(MapData* newData);
    void RevealFromPlayer();
    void RevealFromPosition(const Vector2Int& center, int viewDistance);
    void SetShowFullMap(bool showFull);
    bool IsShowFullMap() const { return m_ShowFullMap; }
    void SetShowDiscoveredMap(bool showDiscovered);
    bool IsShowDiscoveredMap() const { return m_ShowDiscoveredMap; }
    bool IsLookMode() const { return m_LookMode; }

private:
    static constexpr int DRAW_CELL_COUNT = 30;

    Polygon2D m_MiniMapPoly;
    Polygon2D m_LookMapPoly;
    Polygon2D m_BlackOverlayPoly;
    MapData* m_Map = nullptr;

    int m_MapWidth = 0;
    int m_MapHeight = 0;
    int m_TextureWidth = DRAW_CELL_COUNT;
    int m_TextureHeight = DRAW_CELL_COUNT;

    int m_PosX = 0;
    int m_PosY = 0;
    int m_Width = 0;
    int m_Height = 0;

    std::vector<unsigned int> m_Pixels;
    std::vector<bool> m_DiscoveredTiles;
    std::vector<bool> m_DiscoveredRooms;
    bool m_ShowFullMap = false;
    bool m_ShowDiscoveredMap = false;
    bool m_LookMode = false;
    Vector2Int m_LookCenter{ 0, 0 };
    float m_LookPanTimer = 0.0f;

    ID3D11Texture2D* m_Texture = nullptr;
    ID3D11ShaderResourceView* m_ShaderResourceView = nullptr;
    ID3D11SamplerState* m_MapSampler = nullptr;

    void CreateTexture();
    void ReleaseTexture();
    void CreateMapSampler();
    void ReleaseMapSampler();
    void ResizeTextureForCurrentMode();
    void SetLookMode(bool enabled);
    void UpdateLookModeInput();
    void ClampLookCenterToDiscoveredBounds();
    void BuildStaticLayer();
    void BuildDynamicLayer();
    void ApplyToGPU();

    Vector2Int GetViewportTopLeft() const;
    bool WorldToViewport(const Vector2Int& worldPos, int& outX, int& outY) const;
    bool GetDiscoveredBounds(int& outLeft, int& outTop, int& outRight, int& outBottom) const;
    void MarkDiscovered(int x, int y);
    bool IsDiscovered(int x, int y) const;
    void RevealViewArea(const Vector2Int& center, int viewDistance);
    void RevealRoom(int roomIndex, int viewDistance);
    void RevealConnectedCorridors(const Vector2Int& center, int viewDistance);
    bool GetPlayerRoomBounds(int& outLeft, int& outTop, int& outRight, int& outBottom) const;
    unsigned int GetTileColor(int x, int y) const;
    bool IsShopFloorTile(int x, int y) const;
};
