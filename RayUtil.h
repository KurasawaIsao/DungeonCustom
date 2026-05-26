#pragma once
#include "Vector3.h"
#include <DirectXMath.h>

using namespace DirectX;

class RayUtil
{
public:
    struct Ray
    {
        Vector3 origin;
        Vector3 dir;
    };

    // スクリーン → レイ
    static Ray ScreenPointToRay(
        int mouseX, int mouseY,
        int screenW, int screenH,
        XMMATRIX view,
        XMMATRIX proj)
    {
        // ① スクリーン座標 
        float ndcX = (2.0f * (float)mouseX / (float)screenW) - 1.0f;
        float ndcY = 1.0f - (2.0f * (float)mouseY / (float)screenH);

        // ② 逆行列の準備
        XMMATRIX invView = XMMatrixInverse(nullptr, view);
        XMMATRIX invProj = XMMatrixInverse(nullptr, proj);

        // ③ クリップ空間からワールド空間へ (W除算による透視投影補正)
        XMVECTOR rayClipNear = XMVectorSet(ndcX, ndcY, 0.0f, 1.0f);
        XMVECTOR rayClipFar = XMVectorSet(ndcX, ndcY, 1.0f, 1.0f);

        // プロジェクション逆変換
        XMVECTOR rayEyeNear = XMVector4Transform(rayClipNear, invProj);
        XMVECTOR rayEyeFar = XMVector4Transform(rayClipFar, invProj);

        // Wで除算してカメラ空間の座標を確定させる (これを忘れると高さによってズレる)
        rayEyeNear = XMVectorScale(rayEyeNear, 1.0f / XMVectorGetW(rayEyeNear));
        rayEyeFar = XMVectorScale(rayEyeFar, 1.0f / XMVectorGetW(rayEyeFar));

        // ビュー逆変換でワールド座標へ
        XMVECTOR rayWorldNear = XMVector4Transform(rayEyeNear, invView);
        XMVECTOR rayWorldFar = XMVector4Transform(rayEyeFar, invView);

        // ④ レイの構築
        Ray ray;
        ray.origin = GetCameraWorldPos(view); //

        // 方向ベクトル = (遠平面の点 - 近平面の点)
        XMVECTOR dirVec = XMVector3Normalize(XMVectorSubtract(rayWorldFar, rayWorldNear));

        ray.dir = {
            XMVectorGetX(dirVec),
            XMVectorGetY(dirVec),
            XMVectorGetZ(dirVec)
        };

        return ray;
    }

    // レイと Y=0 の平面との交差
    static bool RayPlaneIntersection(
        const Vector3& origin,
        const Vector3& dir,
        float& t,
        float planeY = 0.0f) 
    {
        Vector3 n = { 0, 1, 0 };
        float denom = Vector3::dot(n, dir);
        if (fabs(denom) < 1e-6f) return false;

        t = (planeY - origin.y) / denom;
        return (t >= 0);
    }

private:

    // === View 行列から Camera の world 座標を求める ===
    static Vector3 GetCameraWorldPos(const XMMATRIX& view)
    {
        XMMATRIX inv = XMMatrixInverse(nullptr, view);
        return Vector3(
            XMVectorGetX(inv.r[3]),
            XMVectorGetY(inv.r[3]),
            XMVectorGetZ(inv.r[3])
        );
    }
};
