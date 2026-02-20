#pragma once

#include	<d3d11.h>
#include	<string>
#include	<wrl/client.h>							// ComPtrの定義を含むヘッダファイル
#include	<filesystem>
#include	"NonCopyable.h"

using Microsoft::WRL::ComPtr;

// テクスチャを扱うクラス
class CTexture : NonCopyable
{
	std::string m_texname{};						// ファイル名
	ComPtr<ID3D11ShaderResourceView> m_srv{};		// シェーダーリソースビュー

	int m_width{};									// 幅
	int m_height{};									// 高さ
	int m_bpp{};									// BPP

	std::vector<std::uint8_t>	m_pixels{};			// RGBAピクセル 20260113 add by suzuki
public:
	bool Load(const std::string& filename);
	bool LoadFromFemory(const unsigned char* data, int len);

	bool Load(const std::u8string& filename);		// 20250711

	bool Load(const std::filesystem::path filepath);		// 20260106

	bool LoadMipmap(const std::filesystem::path filepath, int mipmaplevel);

	void SetGPU();

	void Reset() noexcept;			// add by suzuki 20260106

	std::string GetTexName() const {
		return m_texname;
	}

	// 20260110 add by suzuki
	bool CreateFromRGBA(const std::uint8_t* rgba, int width, int height, bool genMips);
	std::vector<std::uint8_t>& Getpixels() { return m_pixels; }	// RGBAピクセル 20260113 add by suzuki
	int GetWidth() { return m_width; }
	int GetHeight() { return m_height; }
};