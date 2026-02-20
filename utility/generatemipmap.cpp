#include    "generatemipmap.h"

// ファイルを丸ごと読み込む（日本語パスでもOKになりやすい）
bool ReadAllBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& out)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;

    ifs.seekg(0, std::ios::end);
    const std::streamoff size = ifs.tellg();
    if (size <= 0) return false;
    ifs.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(size));
    ifs.read(reinterpret_cast<char*>(out.data()), size);
    return ifs.good();
}

// stb_image で読み込んで mipmap 対応 SRV を作る
// - autoGenerateMips: true なら GenerateMips を使う（推奨）
// - forceSRGB: true なら SRGB フォーマットにする（アルベド等に便利）
HRESULT CreateSRVFromSTBImage(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::filesystem::path& filename,
    DxTextureSRV& out,
    bool flipY,
    bool autoGenerateMips,
    bool forceSRGB
)
{
    if (!device || !context) return E_INVALIDARG;

    // --- load file bytes ---
    std::vector<std::uint8_t> fileBytes;
    if (!ReadAllBytes(filename, fileBytes))
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

    // --- stb decode (force RGBA8) ---
    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    int w = 0, h = 0, comp = 0;
    unsigned char* rgba = stbi_load_from_memory(
        fileBytes.data(),
        static_cast<int>(fileBytes.size()),
        &w, &h, &comp,
        STBI_rgb_alpha
    );
    if (!rgba) {
        return E_FAIL;
    }

    const DXGI_FORMAT fmt = forceSRGB
        ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
        : DXGI_FORMAT_R8G8B8A8_UNORM;

    // --- create texture (mips) ---
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(w);
    desc.Height = static_cast<UINT>(h);
    desc.MipLevels = autoGenerateMips ? 0u : 1u;   // 0 = full chain (autogen)
    desc.ArraySize = 1;
    desc.Format = fmt;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (autoGenerateMips) {
        // GenerateMips を使うために RenderTarget と MISC フラグが必要
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, tex.GetAddressOf());
    if (FAILED(hr)) {
        stbi_image_free(rgba);
        return hr;
    }

    // level 0 だけ書き込む（残り mip は GenerateMips が作る）
    const UINT rowPitch = static_cast<UINT>(w * 4); // RGBA8
    context->UpdateSubresource(tex.Get(), 0, nullptr, rgba, rowPitch, 0);

    stbi_image_free(rgba);

    // --- create SRV ---
    D3D11_SHADER_RESOURCE_VIEW_DESC sdesc{};
    sdesc.Format = desc.Format;
    sdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sdesc.Texture2D.MostDetailedMip = 0;
    sdesc.Texture2D.MipLevels = autoGenerateMips ? UINT(-1) : 1; // -1 = 全ミップ

    ComPtr<ID3D11ShaderResourceView> srv;
    hr = device->CreateShaderResourceView(tex.Get(), &sdesc, srv.GetAddressOf());
    if (FAILED(hr)) return hr;

    // --- generate mips ---
    if (autoGenerateMips) {
        context->GenerateMips(srv.Get());
    }

    out.tex = tex;
    out.srv = srv;
    out.width = w;
    out.height = h;
    return S_OK;
}
