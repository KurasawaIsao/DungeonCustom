#include "texture.h"
#include <unordered_map>
#include <string>
#include <cassert>
#include <wrl/client.h>
#include <d3d11.h>
using namespace DirectX;

std::unordered_map<std::string, ID3D11ShaderResourceView*> Texture::m_TexturePool;

ID3D11ShaderResourceView* Texture::Load(const char* filename)
{
    // キャッシュから取得
    if (m_TexturePool.count(filename) > 0)
    {
        return m_TexturePool[filename];
    }

    // マルチバイト→ワイド文字変換
    wchar_t wFileName[512];
    MultiByteToWideChar(CP_ACP, 0, filename, -1, wFileName, 512);

    // テクスチャ読み込み
    TexMetadata metadata;
    ScratchImage image;
    HRESULT hr = LoadFromWICFile(wFileName, WIC_FLAGS_NONE, &metadata, image);
   
    // シェーダーリソースビュー作成
    ID3D11ShaderResourceView* texture = nullptr;
    hr = CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
   

    // キャッシュに登録して返す
    m_TexturePool[filename] = texture;
    return texture;
}
ID3D11ShaderResourceView* Texture::LoadDDS(const char* FileName)
{
    if (m_TexturePool.count(FileName) > 0)
    {
        return m_TexturePool[FileName];
    }

    wchar_t wFileName[512];
    mbstowcs(wFileName, FileName, strlen(FileName) + 1);

    TexMetadata metadata;
    ScratchImage image;
    ID3D11ShaderResourceView* texture;

    // 画像読み込み
    // LoadFromWICFile(wFileName, WIC_FLAGS_NONE, &metadata, image);
    LoadFromDDSFile(wFileName, DDS_FLAGS_NONE, &metadata, image);

    // テクスチャ生成
    CreateShaderResourceView(
        Renderer::GetDevice(),
        image.GetImages(),
        image.GetImageCount(),
        image.GetMetadata(),
        &texture
    );

    assert(texture);

    m_TexturePool[FileName] = texture;

    return texture;
}

