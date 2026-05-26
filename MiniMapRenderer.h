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
    bool IsShowFullMap() const { return showFullMap; }
    void SetShowDiscoveredMap(bool showDiscovered);
    bool IsShowDiscoveredMap() const { return showDiscoveredMap; }
    bool IsLookMode() const { return lookMode; }

private:
    static constexpr int DRAW_CELL_COUNT = 30;

    Polygon2D miniMapPoly;
    Polygon2D lookMapPoly;
    Polygon2D blackOverlayPoly;
    MapData* map = nullptr;

    int mapW = 0;
    int mapH = 0;
    int texW = DRAW_CELL_COUNT;
    int texH = DRAW_CELL_COUNT;

    int posX = 0;
    int posY = 0;
    int width = 0;
    int height = 0;

    std::vector<unsigned int> pixels;
    std::vector<bool> discoveredTiles;
    std::vector<bool> discoveredRooms;
    bool showFullMap = false;
    bool showDiscoveredMap = false;
    bool lookMode = false;
    Vector2Int lookCenter{ 0, 0 };
    float lookPanTimer = 0.0f;

    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;

    void CreateTexture();
    void ReleaseTexture();
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
    unsigned int GetTileColor(int x, int y) const;
    bool IsShopFloorTile(int x, int y) const;
};
