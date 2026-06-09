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

    // 視界の中心を一時的に指定し、移動演出中などにマスクの中心が先走らないようにする。
    void SetFocusOverride(const Vector2Int& gridPos, const Vector3& worldPos);
    // 一時指定した視界中心を解除し、プレイヤー位置を基準に戻す。
    void ClearFocusOverride();

private:
    // 画面上の矩形範囲を表す。マスクの一部を透明化するときの範囲指定に使う。
    struct ScreenRect
    {
        float left;
        float top;
        float right;
        float bottom;
    };

    // 画面上の1点を表す。斜めに投影されたタイル範囲の計算に使う。
    struct ScreenPoint
    {
        float x;
        float y;
    };

    // 画面上に投影された四角形と、その外接矩形をまとめて持つ。
    struct ScreenQuad
    {
        ScreenPoint points[4];
        ScreenRect bounds;
    };

    // マスク用テクスチャは画面より低解像度にして、毎フレーム更新の負荷を抑える。
    static constexpr int MASK_WIDTH = 640;
    static constexpr int MASK_HEIGHT = 360;
    // 視界外に重ねる黒マスクの最大不透明度。
    static constexpr unsigned int MAX_ALPHA = 170;

    // 画面全体へ重ねる2Dポリゴン。
    Polygon2D maskPoly;
    // CPU側で作成するRGBAマスク画像。アルファだけを使って暗さを表現する。
    std::vector<unsigned int> pixels;
    // CPU側で作ったマスクをGPUへ渡すための動的テクスチャ。
    ID3D11Texture2D* tex = nullptr;
    // 描画時にmaskPolyへ設定するテクスチャビュー。
    ID3D11ShaderResourceView* srv = nullptr;
    // trueの間はプレイヤー位置ではなくfocusGridPosを視界中心に使う。
    bool hasFocusOverride = false;
    Vector2Int focusGridPos{ 0, 0 };
    Vector3 focusWorldPos{ 0.0f, 0.0f, 0.0f };

    // マスク描画に使うGPUテクスチャを生成する。
    void CreateTexture();
    // GPUテクスチャとビューを解放する。
    void ReleaseTexture();
    // 通路上で使う円形の視界マスクをCPU側ピクセルに作成する。
    void BuildMask(float centerX, float centerY, float radius);
    // 部屋や部屋扱いの領域をまとめて明るくする視界マスクを作成する。
    void BuildRoomAndViewMask(class MapData* map, const Vector2Int& centerPos, int viewDistance);
    // グリッド範囲を画面上の矩形へ変換する。
    bool GetGridAreaScreenRect(int left, int top, int right, int bottom, ScreenRect& outRect) const;
    // グリッド範囲をカメラ投影後の四角形へ変換する。
    bool GetGridAreaScreenQuad(int left, int top, int right, int bottom, ScreenQuad& outQuad) const;
    // 指定した画面矩形の内側を透明化し、端だけ少しぼかす。
    void ClearScreenRectSmooth(const ScreenRect& rect);
    // 指定した画面四角形の内側を透明化し、端だけ少しぼかす。
    void ClearScreenQuadSmooth(const ScreenQuad& quad);
    // CPU側のpixelsをGPUテクスチャへ転送する。
    void ApplyToGPU();
    // 現在の階層設定とプレイヤー状態から、マスクを描画するか判定する。
    bool ShouldDrawMask(int& outViewDistance) const;
    // 3Dワールド座標をスクリーン座標へ変換する。
    bool WorldToScreen(const Vector3& world, float& outX, float& outY) const;
    // 視界距離を画面上の円中心と半径に変換する。
    bool GetMaskCircle(const Vector3& centerWorld, int viewDistance, float& outCenterX, float& outCenterY, float& outRadius) const;
};
