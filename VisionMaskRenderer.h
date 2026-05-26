#pragma once
#include <vector>
#include <d3d11.h>
#include "GameObject.h"
#include "Polygon.h"
#include "Vector2Int.h"
#include "Vector3.h"

class VisionMaskRenderer : public GameObject
{
public:
    void Init() override;
    void Uninit() override;
    void Update() override {}
    void Draw() override;
    void SetFocusOverride(const Vector2Int& gridPos, const Vector3& worldPos);
    void ClearFocusOverride();

private:
    struct ScreenRect
    {
        float left;
        float top;
        float right;
        float bottom;
    };

    struct ScreenPoint
    {
        float x;
        float y;
    };

    struct ScreenQuad
    {
        ScreenPoint points[4];
        ScreenRect bounds;
    };

    static constexpr int MASK_WIDTH = 640;
    static constexpr int MASK_HEIGHT = 360;
    static constexpr unsigned int MAX_ALPHA = 170;

    Polygon2D maskPoly;
    std::vector<unsigned int> pixels;
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    bool hasFocusOverride = false;
    Vector2Int focusGridPos{ 0, 0 };
    Vector3 focusWorldPos{ 0.0f, 0.0f, 0.0f };

    void CreateTexture();
    void ReleaseTexture();
    void BuildMask(float centerX, float centerY, float radius);
    void BuildRoomAndViewMask(class MapData* map, const Vector2Int& centerPos, int viewDistance);
    bool GetGridAreaScreenRect(int left, int top, int right, int bottom, ScreenRect& outRect) const;
    bool GetGridAreaScreenQuad(int left, int top, int right, int bottom, ScreenQuad& outQuad) const;
    void ClearScreenRectSmooth(const ScreenRect& rect);
    void ClearScreenQuadSmooth(const ScreenQuad& quad);
    void ApplyToGPU();
    bool ShouldDrawMask(int& outViewDistance) const;
    bool WorldToScreen(const Vector3& world, float& outX, float& outY) const;
    bool GetMaskCircle(const Vector3& centerWorld, int viewDistance, float& outCenterX, float& outCenterY, float& outRadius) const;
};
