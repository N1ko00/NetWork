#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include "../system/stb_image.h"

using Microsoft::WRL::ComPtr;

struct DxTextureSRV
{
    ComPtr<ID3D11Texture2D>            tex;
    ComPtr<ID3D11ShaderResourceView>   srv;
    int                                width = 0;
    int                                height = 0;
};

HRESULT CreateSRVFromSTBImage(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::filesystem::path& filename,
    DxTextureSRV& out,
    bool flipY = false,
    bool autoGenerateMips = true,
    bool forceSRGB = false
);