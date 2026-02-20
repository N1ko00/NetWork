#include "DirectWrite.h"
#include "../main.h"
#include <assert.h>

//＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
//	Windows11では.otfの方を推奨
//	.ttfとは相性が悪い
//＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝


//ComPtr <IDWriteFactory5>	DirectWrite::m_cpDWriteFactory;
ComPtr <CustomFontCollectionLoader> DirectWrite::pFontCollectionLoader;

std::wstring DirectWrite::GetFontFileNameWithoutExtension(const std::wstring& _filePath)
{
	// 末尾から検索してファイル名と拡張子の位置を取得
	size_t start = _filePath.find_last_of(L"/\\") + 1;
	size_t end = _filePath.find_last_of(L'.');

	// ファイル名を取得して返す
	return _filePath.substr(start, end - start);
}

std::wstring DirectWrite::StringToWString(std::string _oString)
{
	// 必要なバッファサイズを計算して、そのままwstringに変換
	int iBufferSize = MultiByteToWideChar(CP_ACP, 0, _oString.c_str(), -1, nullptr, 0);

	// バッファを確保し、直接wstringに変換
	std::wstring oRet(iBufferSize - 1, L'\0');
	MultiByteToWideChar(CP_ACP, 0, _oString.c_str(), -1, &oRet[0], iBufferSize);

	// 変換結果を返す
	return oRet;
}

HRESULT DirectWrite::Init(IDXGISwapChain* _swapChain)
{
	// バックバッファのリセット
	m_cpBackBuffer.Reset();

	// バックバッファの取得
	HRESULT result = _swapChain->GetBuffer(0, IID_PPV_ARGS(&m_cpBackBuffer));
	assert(SUCCEEDED(result) && "バックバッファの取得失敗");

	// Direct2Dファクトリ情報の初期化
	result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_cpD2DFactory.GetAddressOf());
	assert(SUCCEEDED(result) && "Direct2Dファクトリ情報の初期化失敗");

	// スクリーンのサイズを取得
	FLOAT dpiX = (FLOAT)SCREEN_WIDTH;
	FLOAT dpiY = (FLOAT)SCREEN_HEIGHT;

	// レンダーターゲットの作成
	D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)
	);

	// サーフェスに描画するレンダーターゲットを作成
	result = m_cpD2DFactory->CreateDxgiSurfaceRenderTarget(m_cpBackBuffer.Get(), &props,
		m_cpRenderTarget.GetAddressOf());
	assert(SUCCEEDED(result) && "サーフェスに描画するレンダーターゲットを作成失敗");

	// アンチエイリアシングモードの設定
	m_cpRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

	// IDWriteFactoryの作成
	result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(m_cpDWriteFactory.GetAddressOf()));
	assert(SUCCEEDED(result) && "IDWriteFactoryの作成失敗");

	// カスタムフォントコレクションローダーのインスタンス作成
	pFontCollectionLoader = ComPtr<CustomFontCollectionLoader>(new (std::nothrow) CustomFontCollectionLoader());
	if (!pFontCollectionLoader) {
		// インスタンス作成失敗
		return E_FAIL;
	}

	// カスタムフォントコレクションローダーを登録
	result = m_cpDWriteFactory->RegisterFontCollectionLoader(pFontCollectionLoader.Get());
	assert(SUCCEEDED(result) && "カスタムフォントコレクションローダーを登録失敗");

	// フォントファイルの読み込み
	result = FontLoader();
	assert(SUCCEEDED(result));

	// フォントを設定
	SetFont(m_Setting);

	return result;
}


void DirectWrite::SetFont(FontData _set)
{
	HRESULT result = S_OK;

	// 設定をコピー
	m_Setting = _set;

	// 前回作成したブラシを解放
	m_cpBrush.Reset();
	m_cpShadowBrush.Reset();

	// フォーマットを解放
	m_cpTextFormat.Reset();

	// フォント名を取得する
	std::wstring name = L"";
	int num = (int)m_Setting.font;
	ComPtr <IDWriteFontCollection> fc = fontCollection;
	if (num >= (int)m_FontNamesList.size() || m_FontNamesList.empty()) {
		// numがfontの種類より大きい or フォント名のリストが空だったらとりあえず先頭のfontを採用
		if (num >= (int)FontPath->size()) {

			name = m_FontNamesList[0];
		}
		else {
			name = FontPath[num];
			fc = nullptr;
		}
	}
	else
	{
		name = m_FontNamesList[num];
	}


	//関数CreateTextFormat()
	//第1引数：フォント名（L"メイリオ", L"Arial", L"Meiryo UI"等）
	//第2引数：フォントコレクション（nullptr）
	//第3引数：フォントの太さ（DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_WEIGHT_BOLD等）
	//第4引数：フォントスタイル（DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STYLE_ITALIC）
	//第5引数：フォントの幅（DWRITE_FONT_STRETCH_NORMAL,DWRITE_FONT_STRETCH_EXTRA_EXPANDED等）
	//第6引数：フォントサイズ（20, 30等）
	//第7引数：ロケール名（L""）
	//第8引数：テキストフォーマット（&g_pTextFormat）
	result = m_cpDWriteFactory->CreateTextFormat(
		name.c_str(),
		fc.Get(),
		m_Setting.fontWeight,
		m_Setting.fontStyle,
		m_Setting.fontStretch,
		m_Setting.fontSize,
		m_Setting.localeName.c_str(),
		m_cpTextFormat.GetAddressOf()
	);
	assert(SUCCEEDED(result) && "テキストフォーマット作成失敗");

	//関数SetTextAlignment()
	//第1引数：テキストの配置（DWRITE_TEXT_ALIGNMENT_LEADING：前, DWRITE_TEXT_ALIGNMENT_TRAILING：後, DWRITE_TEXT_ALIGNMENT_CENTER：中央,
	//                         DWRITE_TEXT_ALIGNMENT_JUSTIFIED：行いっぱい）
	result = m_cpTextFormat->SetTextAlignment(m_Setting.textAlignment);
	assert(SUCCEEDED(result) && "テキストの配置失敗");

	//関数CreateSolidColorBrush()
	//第1引数：フォント色（D2D1::ColorF(D2D1::ColorF::Black)：黒, D2D1::ColorF(D2D1::ColorF(0.0f, 0.2f, 0.9f, 1.0f))：RGBA指定）
	result = m_cpRenderTarget->CreateSolidColorBrush(m_Setting.Color, m_cpBrush.GetAddressOf());
	assert(SUCCEEDED(result) && "テキストの色失敗");

	// 影用のブラシを作成
	result = m_cpRenderTarget->CreateSolidColorBrush(m_Setting.shadowColor, m_cpShadowBrush.GetAddressOf());
	assert(SUCCEEDED(result) && "影用のブラシを作成失敗");

	//return result;
}

void DirectWrite::SetFonts(FontList::FontName _fontname, DWRITE_FONT_WEIGHT _fontWeight,
	DWRITE_FONT_STYLE _fontStyle, DWRITE_FONT_STRETCH _fontStretch, FLOAT _fontSize,
	std::wstring _localeName, DWRITE_TEXT_ALIGNMENT _textAlignment, D2D1_COLOR_F _Color,
	D2D1_COLOR_F _shadowColor, D2D1_POINT_2F _shadowOffset)
{
	FontData f;
	f.font = _fontname;
	f.fontWeight = _fontWeight;
	f.fontStyle = _fontStyle;
	f.fontStretch = _fontStretch;
	f.fontSize = _fontSize;
	f.localeName = _localeName;
	f.textAlignment = _textAlignment;
	f.Color = _Color;
	f.shadowColor = _shadowColor;
	f.shadowOffset = _shadowOffset;

	SetFont(f);
}

HRESULT DirectWrite::DrawString(std::wstring _str, DirectX::SimpleMath::Vector2 _pos,
	D2D1_DRAW_TEXT_OPTIONS _options, bool _shadow)
{
	HRESULT result = S_OK;

	// 文字列の変換
	//std::wstring wstr = StringToWString(_str);
	std::wstring wstr = _str;

	// 指定した文字を改行に置き換えた文字列を作成
	for (size_t pos = wstr.find(L'ワ'); pos != std::wstring::npos; pos = wstr.find(L'ワ', pos + 1))
	{
		wstr.replace(pos, 1, L"\n");
	}

	// ターゲットサイズの取得
	D2D1_SIZE_F targetSize = m_cpRenderTarget->GetSize();

	// テキストレイアウトを作成
	if (!m_cpTextLayout || m_CurrentText != wstr)
	{
		m_CurrentText = wstr; // 現在のテキストを更新

		// 以前のテキストレイアウトを解放
		m_cpTextLayout.Reset();

		result = m_cpDWriteFactory->CreateTextLayout(wstr.c_str(),
			static_cast<UINT32>(wstr.size()), m_cpTextFormat.Get(), targetSize.width,
			targetSize.height, m_cpTextLayout.GetAddressOf());
		assert(SUCCEEDED(result) && "テキストレイアウト作成失敗");
	}

	// 描画位置の確定
	D2D1_POINT_2F points;
	points.x = _pos.x;
	points.y = _pos.y;

	// 描画の開始
	m_cpRenderTarget->BeginDraw();

	// 影を描画する場合
	if (_shadow)
	{
		// 影の描画
		m_cpRenderTarget->DrawTextLayout(D2D1::Point2F(points.x - m_Setting.shadowOffset.x,
			points.y - m_Setting.shadowOffset.y),
			m_cpTextLayout.Get(),
			m_cpShadowBrush.Get(),
			_options);
	}

	// 描画処理
	m_cpRenderTarget->DrawTextLayout(points, m_cpTextLayout.Get(), m_cpBrush.Get(), _options);

	// 描画の終了
	result = m_cpRenderTarget->EndDraw();
	assert(SUCCEEDED(result) && "描画の終了処理失敗");

	return S_OK;
}

HRESULT DirectWrite::DrawString(std::string _str, D2D1_RECT_F _rect, D2D1_DRAW_TEXT_OPTIONS _options, bool _shadow)
{
	HRESULT result = S_OK;

	// 文字列の変換
	std::wstring wstr = StringToWString(_str.c_str());

	// 描画の開始
	m_cpRenderTarget->BeginDraw();

	if (_shadow)
	{
		// 影の描画
		m_cpRenderTarget->DrawText(wstr.c_str(),
			(UINT32)wstr.size(),
			m_cpTextFormat.Get(),
			D2D1::RectF(_rect.left - m_Setting.shadowOffset.x,
				_rect.top - m_Setting.shadowOffset.y,
				_rect.right - m_Setting.shadowOffset.x,
				_rect.bottom - m_Setting.shadowOffset.y),
			m_cpShadowBrush.Get(), _options);
	}

	// 描画処理
	m_cpRenderTarget->DrawText(wstr.c_str(), (UINT32)wstr.size(), m_cpTextFormat.Get(),
		_rect, m_cpBrush.Get(), _options);

	// 描画の終了
	result = m_cpRenderTarget->EndDraw();
	assert(SUCCEEDED(result) && "描画の終了処理失敗");


	return S_OK;
}

HRESULT DirectWrite::FontLoader()
{
	HRESULT result = S_OK;

	// 配列からstd::vectorへ変換
	std::vector<std::wstring> fontPaths(std::begin(FontPath), std::end(FontPath));

	result = LoadFontFiles(fontPaths);

	// カスタムフォントコレクションの作成
	result = m_cpDWriteFactory->CreateCustomFontCollection(
		pFontCollectionLoader.Get(),
		m_cpFontFileList.data(),
		static_cast<UINT32>(m_cpFontFileList.size()),
		&fontCollection
	);
	if (FAILED(result)) { return result; }

	// フォント名を取得
	result = GetFontFamilyName(fontCollection.Get(), L"en-us"); // ロケールは適切な値に設定してください
	if (FAILED(result)) { return result; }

	return S_OK;
}


std::wstring DirectWrite::GetFontName(int _num)
{
	// フォント名のリストが空だった場合
	if (m_FontNamesList.empty())
	{
		return std::wstring();
	}

	// リストのサイズを超えていた場合
	if (_num >= static_cast<int>(m_FontNamesList.size()))
	{
		return m_FontNamesList[0];
	}

	return m_FontNamesList[_num];
}

int DirectWrite::GetFontNameNum()
{
	return (int)m_FontNamesList.size();
}

HRESULT DirectWrite::GetFontFamilyName(IDWriteFontCollection* _customFontCollection, std::wstring _locale)
{
	HRESULT result = S_OK;

	// null チェック
	if (!_customFontCollection)
	{
		return E_POINTER; // 無効なポインタの場合はエラーを返す
	}

	// フォントファミリー名一覧をリセット
	m_FontNamesList.clear();

	// フォントの数を取得
	UINT32 familyCount = _customFontCollection->GetFontFamilyCount();
	if (familyCount == 0)
	{
		return S_FALSE; // フォントファミリーが存在しない場合、エラーではなく警告として扱う
	}

	for (UINT32 i = 0; i < familyCount; ++i)
	{
		// フォントファミリーの取得
		ComPtr<IDWriteFontFamily> fontFamily = nullptr;
		result = _customFontCollection->GetFontFamily(i, fontFamily.GetAddressOf());
		assert(SUCCEEDED(result) && "フォントファミリーの取得失敗");

		// フォントファミリー名の一覧を取得
		ComPtr<IDWriteLocalizedStrings> familyNames = nullptr;
		result = fontFamily->GetFamilyNames(familyNames.GetAddressOf());
		assert(SUCCEEDED(result) && "フォントファミリー名の一覧取得失敗");

		// 指定されたロケールに対応するインデックスを検索
		UINT32 index = 0;
		BOOL exists = FALSE;
		result = familyNames->FindLocaleName(_locale.c_str(), &index, &exists);
		assert(SUCCEEDED(result) && "指定されたロケールに対応するインデックスを検索失敗");

		// 指定されたロケールが見つからなかった場合、デフォルトのロケールを使用
		if (!exists)
		{
			result = familyNames->FindLocaleName(L"en-us", &index, &exists);
			assert(SUCCEEDED(result) && "デフォルトロケールに対応するインデックスを検索失敗");

			if (!exists)
			{
				continue; // デフォルトロケールでも見つからなかった場合はスキップ
			}
		}

		// フォントファミリー名の長さを取得
		UINT32 length = 0;
		result = familyNames->GetStringLength(index, &length);
		assert(SUCCEEDED(result) && "フォントファミリー名の長さを取得失敗");

		// フォントファミリー名の取得
		std::wstring fontName(length + 1, L'\0'); // 終端文字を含めたバッファを確保
		result = familyNames->GetString(index, &fontName[0], length + 1);
		assert(SUCCEEDED(result) && "フォントファミリー名の取得失敗");

		// フォントファミリー名を追加
		m_FontNamesList.push_back(fontName);
	}

	return result;
}


HRESULT DirectWrite::GetAllFontFamilyName(IDWriteFontCollection* _customFontCollection)
{
	HRESULT result = S_OK;

	// フォントファミリー名一覧をリセット
	m_FontNamesList.clear();

	// フォントファミリーの数を取得
	UINT32 familyCount = _customFontCollection->GetFontFamilyCount();

	for (UINT32 i = 0; i < familyCount; ++i)
	{
		// フォントファミリーの取得
		ComPtr<IDWriteFontFamily> fontFamily = nullptr;
		result = _customFontCollection->GetFontFamily(i, fontFamily.GetAddressOf());
		assert(SUCCEEDED(result) && "フォントファミリーの取得失敗");

		// フォントファミリー名の一覧を取得
		ComPtr<IDWriteLocalizedStrings> familyNames = nullptr;
		result = fontFamily->GetFamilyNames(familyNames.GetAddressOf());
		assert(SUCCEEDED(result) && "フォントファミリー名の一覧取得失敗");

		// フォントファミリー名の数を取得
		UINT32 nameCount = familyNames->GetCount();

		// フォントファミリー名の数だけ繰り返す
		for (UINT32 j = 0; j < nameCount; ++j)
		{
			// フォントファミリー名の長さを取得
			UINT32 length = 0;
			result = familyNames->GetStringLength(j, &length);
			assert(SUCCEEDED(result) && "フォントファミリー名の長さを取得失敗");

			// std::wstring を使用して文字列を格納
			std::wstring fontName(length + 1, L'\0');  // 終端文字を含めたバッファを確保
			result = familyNames->GetString(j, &fontName[0], length + 1);
			assert(SUCCEEDED(result) && "フォントファミリー名の取得失敗");

			// フォントファミリー名を追加
			m_FontNamesList.push_back(fontName);
		}
	}

	return result;
}

HRESULT DirectWrite::LoadFontFiles(const std::vector<std::wstring>& fontPaths)
{
	HRESULT result = S_OK;

	for (const auto& path : fontPaths)
	{
		ComPtr<IDWriteFontFile> fontFile;
		result = m_cpDWriteFactory->CreateFontFileReference(path.c_str(), nullptr, fontFile.GetAddressOf());
		if (FAILED(result)) {
			wprintf(L"Failed to load font file: %s\n", path.c_str());
			return result;
		}

		// フォントファイルをリストに追加
		m_cpFontFileList.push_back(fontFile);
	}

	return result;
}

HRESULT DirectWrite::DrawStringPartial(std::wstring _str, DirectX::SimpleMath::Vector2 _pos,
	bool _shadow, bool _changeText, float _delaytime,float _elapsedTime,
	D2D1_DRAW_TEXT_OPTIONS _options)
{
	HRESULT result = S_OK;

	// 文字列の変換
	//std::wstring wstr = StringToWString(_str);
	std::wstring wstr = _str;

	// 経過時間に基づいて表示する文字数を計算
	m_TotalTime += _elapsedTime;

	// 一文字ごとに遅延させる
	size_t numCharsToDisplay = static_cast<size_t>(m_TotalTime / _delaytime);

	// 文字数が表示する文字列の長さを超えないように制限
	if (numCharsToDisplay > wstr.size()) {
		numCharsToDisplay = wstr.size();
	}

	// 表示する部分文字列を取得
	std::wstring partialStr = wstr.substr(0, numCharsToDisplay);

	// 指定した文字を改行に置き換えた文字列を作成
	for (size_t pos = partialStr.find(L'ワ'); pos != std::wstring::npos; pos = partialStr.find(L'ワ', pos + 1))
	{
		partialStr.replace(pos, 1, L"\n");
	}

	// テキストレイアウトを再生成する必要がある場合のみ生成
	if (!m_cpTextLayout || m_CurrentText != partialStr)
	{
		m_CurrentText = partialStr; // 現在のテキストを更新

		// 以前のテキストレイアウトを解放
		m_cpTextLayout.Reset();

		// 新しいテキストレイアウトを作成
		result = m_cpDWriteFactory->CreateTextLayout(partialStr.c_str(),
			static_cast<UINT32>(partialStr.size()), m_cpTextFormat.Get(), m_cpRenderTarget->GetSize().width,
			m_cpRenderTarget->GetSize().height, m_cpTextLayout.GetAddressOf());
		assert(SUCCEEDED(result) && "テキストレイアウト作成失敗");

		if (partialStr.size() >= 4 && _changeText)
		{
			ComPtr<ID2D1SolidColorBrush> colorBrush;
			result = ChangeTextFont(m_cpTextLayout.Get(),FontList::FontName::MelodyLine, 4, 6);
			assert(SUCCEEDED(result) && "フォントの設定失敗");
			m_cpRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow),
				colorBrush.GetAddressOf());
			m_cpTextLayout->SetDrawingEffect(colorBrush.Get(), {4,5});
		}
		
	}

	// 描画位置の確定
	D2D1_POINT_2F points = { _pos.x, _pos.y };

	// 描画の開始
	m_cpRenderTarget->BeginDraw();

	// 影を描画する場合
	if (_shadow)
	{
		// 影の描画
		m_cpRenderTarget->DrawTextLayout(D2D1::Point2F(points.x - m_Setting.shadowOffset.x,
			points.y - m_Setting.shadowOffset.y),
			m_cpTextLayout.Get(),
			m_cpShadowBrush.Get(),
			_options);
	}

	// テキストの描画
	m_cpRenderTarget->DrawTextLayout(points, m_cpTextLayout.Get(), m_cpBrush.Get(), _options);

	// 描画の終了
	result = m_cpRenderTarget->EndDraw();
	assert(SUCCEEDED(result) && "描画の終了処理失敗");

	return S_OK;
}

void DirectWrite::ResetText()
{
	m_cpTextLayout.Reset();
	m_CurrentText.clear();
	m_TotalTime = 0.0f;
}

//void DirectWrite::ReleaseFontCollectionLoader()
//{
//	m_cpDWriteFactory->UnregisterFontCollectionLoader(pFontCollectionLoader.Get());
//	pFontCollectionLoader.Reset();
//}

DirectWrite::~DirectWrite()
{
	// フォントコレクションローダーの登録解除
	if (m_cpDWriteFactory && pFontCollectionLoader)
	{
		m_cpDWriteFactory->UnregisterFontCollectionLoader(pFontCollectionLoader.Get());
	}

	// 静的メンバーの解放
	if (pFontCollectionLoader)
	{
		pFontCollectionLoader.Reset(); // COM ポインタをリセット
	}

	// 追加のクリーンアップ
	m_cpFontFileList.clear();

	// 各リソースの解放
	m_cpD2DFactory.Reset();
	m_cpRenderTarget.Reset();
	m_cpBrush.Reset();
	m_cpShadowBrush.Reset();
	m_cpDWriteFactory.Reset();
	m_cpTextFormat.Reset();
	m_cpTextLayout.Reset();
	m_cpBackBuffer.Reset();

	// フォントコレクションのリセット
	fontCollection.Reset();
}