#pragma once

#include <string>
#include <wrl/client.h>
#include <vector>

#pragma warning(push)
#pragma warning(disable:4005)

#include <d2d1.h>						// Direct2D
#include <dwrite_3.h>					// DirectWrite(外部フォント使用のため
#include "CommonTypes.h"			// 数学系ライブラリ

#pragma warning(pop)

#pragma comment(lib,"d2d1.lib")			// Direct2D用
#pragma comment(lib,"Dwrite.lib")		// DirectWrite用

using Microsoft::WRL::ComPtr;

class CustomFontCollectionLoader;

//使用するフォント名をここに追加する
namespace FontList
{
	enum class FontName
	{
		MelodyLine,
		Google,
	};
}

//＝＝＝＝＝＝＝＝＝＝＝＝＝
//		フォントデータ
//＝＝＝＝＝＝＝＝＝＝＝＝＝
struct FontData
{
	FontList::FontName font;							// フォント名
	DWRITE_FONT_WEIGHT fontWeight;						// フォントの太さ
	DWRITE_FONT_STYLE fontStyle;						// フォントスタイル
	DWRITE_FONT_STRETCH fontStretch;					// フォントの幅
	FLOAT fontSize;										// フォントサイズ
	std::wstring localeName;							// ロケール名
	DWRITE_TEXT_ALIGNMENT textAlignment;				// テキストの配置
	D2D1_COLOR_F Color;									// フォントの色

	D2D1_COLOR_F shadowColor;							// 影の色
	D2D1_POINT_2F shadowOffset;							// 影のオフセット

	// デフォルト設定（コンストラクタ）
	FontData()
	{
		font = FontList::FontName::Google;
		fontWeight = DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_NORMAL;
		fontStyle = DWRITE_FONT_STYLE::DWRITE_FONT_STYLE_NORMAL;
		fontStretch = DWRITE_FONT_STRETCH::DWRITE_FONT_STRETCH_NORMAL;
//		fontSize = 40;
		fontSize = 10;
		localeName = L"ja-jp";
		textAlignment = DWRITE_TEXT_ALIGNMENT::DWRITE_TEXT_ALIGNMENT_LEADING;
		Color = D2D1::ColorF(D2D1::ColorF::White);

		shadowColor = D2D1::ColorF(D2D1::ColorF::Black);
		shadowOffset = D2D1::Point2F(2.0f, -2.0f);
	}
};

//＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
//		DirectWriteクラス
//＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
class DirectWrite
{
private:
	ComPtr <ID2D1Factory>			m_cpD2DFactory = nullptr;		// Direct2Dリソース
	ComPtr <ID2D1RenderTarget>		m_cpRenderTarget = nullptr;		// Direct2Dレンダーターゲット
	ComPtr <ID2D1SolidColorBrush>	m_cpBrush = nullptr;			// Direct2Dブラシ設定
	ComPtr <ID2D1SolidColorBrush>	m_cpShadowBrush = nullptr;		// Direct2Dブラシ設定（影）
	ComPtr <IDWriteFactory5>		m_cpDWriteFactory;				// DirectWriteリソース
	ComPtr <IDWriteTextFormat>		m_cpTextFormat = nullptr;		// DirectWriteテキスト形式
	ComPtr <IDWriteTextLayout>		m_cpTextLayout = nullptr;		// DirectWriteテキスト書式
	ComPtr <IDXGISurface>			m_cpBackBuffer = nullptr;		// サーフェス情報

	static ComPtr <CustomFontCollectionLoader> pFontCollectionLoader;

	std::wstring m_CurrentText;										//現在描画している文字を保存する変数

	// フォントファイルリスト
	std::vector<ComPtr<IDWriteFontFile>> m_cpFontFileList;

	// フォントデータ
	FontData  m_Setting;

	// フォント名リスト
	std::vector<std::wstring> m_FontNamesList;

	// フォントのファイル名を取得する
	std::wstring GetFontFileNameWithoutExtension(const std::wstring& _filePath);

	// stringをwstringへ変換する
	std::wstring StringToWString(std::string _oString);

	//文字表示する時間の保持する変数
	float m_TotalTime = 0.0f;

public:
	// デフォルトコンストラクタを制限
	DirectWrite()
	{

	}

	// コンストラクタ
	// 第1引数：フォント設定
	DirectWrite(FontData* _set) :m_Setting(*_set) {};

	// 初期化関数
	HRESULT Init(IDXGISwapChain* _swapChain);

	/// <summary>
	/// フォントデータ構造体
	/// </summary>
	void SetFont(FontData _set);

	/// <summary>
	/// // フォント設定
	// 第1引数：フォント名（L"メイリオ", L"Arial", L"Meiryo UI"等）
	// 第2引数：フォントの太さ（DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_WEIGHT_BOLD等）
	// 第3引数：フォントスタイル（DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STYLE_ITALIC）
	// 第4引数：フォントの幅（DWRITE_FONT_STRETCH_NORMAL,DWRITE_FONT_STRETCH_EXTRA_EXPANDED等）
	// 第5引数：フォントサイズ（20, 30等）
	// 第6引数：ロケール名（L"ja-jp"等）
	// 第7引数：テキストの配置（DWRITE_TEXT_ALIGNMENT_LEADING：前, 等）
	// 第8引数：フォントの色（D2D1::ColorF(D2D1::ColorF::Black)：黒, D2D1::ColorF(D2D1::ColorF(0.0f, 0.2f, 0.9f, 1.0f))：RGBA指定等）
	// 第9引数：影の色（D2D1::ColorF(D2D1::ColorF::Black)：黒, D2D1::ColorF(D2D1::ColorF(0.0f, 0.2f, 0.9f, 1.0f))：RGBA指定等）
	// 第10引数：影のオフセット（D2D1::Point2F(2.0f, 2.0f)：右下にポイントずらす）
	/// </summary>
	void SetFonts
	(
		FontList::FontName		_fontname,											// フォント名
		DWRITE_FONT_WEIGHT		_fontWeight = DWRITE_FONT_WEIGHT_NORMAL,			// フォントの太さ
		DWRITE_FONT_STYLE		_fontStyle = DWRITE_FONT_STYLE_NORMAL,				// フォントスタイル
		DWRITE_FONT_STRETCH		_fontStretch = DWRITE_FONT_STRETCH_NORMAL,			// フォントの幅
		FLOAT					_fontSize = 20,										// フォントサイズ
		std::wstring _localeName = L"ja-jp",										// ロケール名
		DWRITE_TEXT_ALIGNMENT	_textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING,		// テキストの配置
		D2D1_COLOR_F			_Color = D2D1::ColorF(D2D1::ColorF::White),			// フォントの色
		D2D1_COLOR_F			_shadowColor = D2D1::ColorF(D2D1::ColorF::Black),	// 影の色
		D2D1_POINT_2F			_shadowOffset = D2D1::Point2F(2.0f, -2.0f)			// 影のオフセット
	);

	/// <summary>
	/// 文字描画
	// string：文字列
	// pos：描画ポジション
	// options：テキストの整形
	/// </summary>
	HRESULT DrawString(std::wstring _str, DirectX::SimpleMath::Vector2 _pos,
		D2D1_DRAW_TEXT_OPTIONS _options = D2D1_DRAW_TEXT_OPTIONS_NONE, bool _shadow = false);


	/// <summary>
	/// 文字を一文字ずつ描画
	/// </summary>
	/// <param name="_num"></param>
	/// <returns></returns>
	HRESULT DrawStringPartial(std::wstring _str, DirectX::SimpleMath::Vector2 _pos,
		bool _shadow = false,bool _changeText = true ,float _delaytime = 0.2f,
		float _elapsedTime = 0.1f,
		D2D1_DRAW_TEXT_OPTIONS _options = D2D1_DRAW_TEXT_OPTIONS_NONE
		);

	// 文字描画
	// string：文字列
	// rect：領域指定
	// options：テキストの整形
	HRESULT DrawString(std::string _str, D2D1_RECT_F _rect,
		D2D1_DRAW_TEXT_OPTIONS _options, bool _shadow = false);

	// 指定されたパスのフォントを読み込む
	HRESULT FontLoader();

	// フォント名を取得する
	std::wstring GetFontName(int _num);

	// 読み込んだフォント名の数を取得する
	int GetFontNameNum();

	// フォント名を取得し直す
	HRESULT GetFontFamilyName(IDWriteFontCollection* _customFontCollection,
		std::wstring _locale = L"ja-jp");

	// 全てのフォント名を取得し直す
	HRESULT GetAllFontFamilyName(IDWriteFontCollection* _customFontCollection);

	/// <summary>
	/// フォントファイルリストに外部フォントを追加する
	/// </summary>
	HRESULT LoadFontFiles(const std::vector<std::wstring>& fontPaths);

	// カスタムフォントコレクション
	ComPtr <IDWriteFontCollection> fontCollection = nullptr;

	/// <summary>
	/// シーンチェンジするときに呼び出すと
	/// もう一度一文字ずつ表示する
	/// </summary>
	void ResetText();

	//一部の文字のフォントを変更
	HRESULT ChangeTextFont(IDWriteTextLayout* _textLayout,
		FontList::FontName _fontName,  UINT32 _startChar, UINT32 _endChar)
	{
		return _textLayout->SetFontFamilyName(GetFontName((int)_fontName).c_str(),
			{_startChar,_endChar});
	}

	//static void ReleaseFontCollectionLoader();

	~DirectWrite();
};

const std::wstring FontPath[] =
{
	L"Assets\\font\\MelodyLine-free.otf",			//メロディーライン
	L"Assets\\font\\NotoSansJP-Black.otf",			//Google日本語文字
};

//=============================================================================
//		カスタムファイルローダー
//=============================================================================
class CustomFontFileEnumerator : public IDWriteFontFileEnumerator
{
public:
	CustomFontFileEnumerator(ComPtr<IDWriteFactory> factory, const std::vector<std::wstring>& fontFilePaths)
		: factory_(factory), fontFilePaths_(fontFilePaths), currentFileIndex_(-1)
	{
	}

	~CustomFontFileEnumerator() = default;

	IFACEMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override
	{
		if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontFileEnumerator))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}

	IFACEMETHODIMP_(ULONG) AddRef() override
	{
		return InterlockedIncrement(&refCount_);
	}

	IFACEMETHODIMP_(ULONG) Release() override
	{
		ULONG newCount = InterlockedDecrement(&refCount_);
		if (newCount == 0)
		{
			delete this;
		}
		else if (newCount > static_cast<ULONG>(-1)) // オーバーフロー対策
		{
			__debugbreak(); // デバッグ用に停止
		}

		return newCount;
	}

	IFACEMETHODIMP MoveNext(OUT BOOL* hasCurrentFile) noexcept override
	{
		*hasCurrentFile = (++currentFileIndex_ < static_cast<int>(fontFilePaths_.size())) ? TRUE : FALSE;
		return S_OK;
	}

	IFACEMETHODIMP GetCurrentFontFile(OUT IDWriteFontFile** fontFile) noexcept override
	{
		if (currentFileIndex_ < 0 || currentFileIndex_ >= static_cast<int>(fontFilePaths_.size()))
		{
			*fontFile = nullptr;
			return E_FAIL;
		}

		try
		{
			return factory_->CreateFontFileReference(fontFilePaths_[currentFileIndex_].c_str(), nullptr, fontFile);
		}
		catch (const std::exception&)
		{
			*fontFile = nullptr;
			return E_FAIL;
		}
	}

private:
	ULONG refCount_ = 0;
	ComPtr<IDWriteFactory> factory_;   // ComPtr を使用して自動的に管理
	std::vector<std::wstring> fontFilePaths_;
	int currentFileIndex_ = -1;
};


//=============================================================================
//		カスタムフォントコレクションローダー
//=============================================================================
class CustomFontCollectionLoader : public IDWriteFontCollectionLoader
{
public:
	// コンストラクタ
	CustomFontCollectionLoader() : refCount_(0) {}

	// IUnknown メソッド
	IFACEMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override
	{
		if (ppvObject == nullptr)
			return E_POINTER;

		if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontCollectionLoader))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		else
		{
			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}
	}

	IFACEMETHODIMP_(ULONG) AddRef() override
	{
		return InterlockedIncrement(&refCount_);
	}

	IFACEMETHODIMP_(ULONG) Release() override
	{
		ULONG newCount = InterlockedDecrement(&refCount_);
		if (newCount == 0)
		{
			delete this;
		}
		else if (newCount > static_cast<ULONG>(-1)) // オーバーフロー対策
		{
			__debugbreak(); // デバッグ用に停止
		}

		return newCount;
	}

	// IDWriteFontCollectionLoader メソッド
	IFACEMETHODIMP CreateEnumeratorFromKey
	(
		IDWriteFactory* factory,
		void const* collectionKey,
		UINT32 collectionKeySize,
		OUT IDWriteFontFileEnumerator** fontFileEnumerator
	) noexcept override
	{
		if (!factory || !fontFileEnumerator || !collectionKey || collectionKeySize == 0)
		{
			return E_POINTER;
		}

		std::vector<std::wstring> fontFilePaths(std::begin(FontPath), std::end(FontPath));

		// new ではなく参照カウント管理
		ComPtr<CustomFontFileEnumerator> enumerator =
			new (std::nothrow) CustomFontFileEnumerator(factory, fontFilePaths);

		if (!enumerator)
		{
			return E_OUTOFMEMORY;
		}

		*fontFileEnumerator = enumerator.Detach(); // ComPtr から所有権を移動
		return S_OK;
	}


private:
	ULONG refCount_; // 参照カウント
};