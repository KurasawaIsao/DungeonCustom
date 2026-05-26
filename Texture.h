#pragma once
#include<unordered_map>
#include<string.h>
#include "main.h"
#include "modelRenderer.h"
#include "renderer.h"
#include"manager.h"
class Texture {
public:
    static ID3D11ShaderResourceView* Load(const char* filename);
    static ID3D11ShaderResourceView* LoadDDS(const char* FileName);
private:
    static std::unordered_map<std::string, ID3D11ShaderResourceView*> m_TexturePool;

};