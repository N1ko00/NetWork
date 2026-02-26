#include "DebugUI.h"

// DebugUI に登録された「追加のデバッグ描画処理」を保持する静的配列。
// Render() のたびに中身を順番に呼び出す。
std::vector<std::function<void(void)>> DebugUI::m_debugfunction;

void DebugUI::Init(ID3D11Device* device, ID3D11DeviceContext* context)
{
    // ImGui のバージョン整合チェック（ヘッダとライブラリが合っているか）
    IMGUI_CHECKVERSION();

    // ImGui のグローバルコンテキストを作成（ImGuiを使うための前提）
    ImGui::CreateContext();

    // ImGui 全体設定（IO: Input/Output 設定オブジェクト）
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // キーボード・ゲームパッドでの操作を有効化（任意）

    // ─────────────────────────────────────────────
    // フォント設定（日本語表示に必須）
    // ImGui は「UTF-8文字列」だけでは日本語が表示できません。
    // その文字の“字形（グリフ）”を含むフォントをロードして、
    // フォントアトラスに焼き込む必要があります。
    // ─────────────────────────────────────────────
    io.Fonts->AddFontFromFileTTF(
        "assets\\font\\NotoSansJP-Black.otf", // 日本語対応フォント（パスは実行ファイル基準）
        18.0f,                                // フォントサイズ（px相当）
        nullptr,                              // 追加設定（フォント設定を細かくしたい場合に指定）
        io.Fonts->GetGlyphRangesJapanese()    // 日本語の文字範囲をロード（必要なグリフを入れる）
    );

    // 追加したフォントをデフォルトに設定（ImGui::Text 等がこのフォントで描画される）
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }
    // ─────────────────────────────────────────────
    // 見た目（スタイル）設定
    // ─────────────────────────────────────────────
    ImGui::StyleColorsDark();  // 暗色テーマ
    // ImGui::StyleColorsLight(); // 明色テーマ（必要ならこちら）

    // ─────────────────────────────────────────────
    // プラットフォーム（Win32）とレンダラー（DX11）バックエンド初期化
    // ImGui 本体だけでは入出力/描画ができないのでバックエンドを初期化する。
    // ─────────────────────────────────────────────
    ImGui_ImplWin32_Init(Application::GetWindow()); // Win32: 入力・ウィンドウ連携
    ImGui_ImplDX11_Init(device, context);           // DX11: 描画連携（SRVなど内部生成あり）
}

void DebugUI::DisposeUI()
{
    // ─────────────────────────────────────────────
    // 終了処理（初期化した逆順で破棄）
    // ─────────────────────────────────────────────
    ImGui_ImplDX11_Shutdown();  // DX11バックエンド破棄
    ImGui_ImplWin32_Shutdown(); // Win32バックエンド破棄
    ImGui::DestroyContext();    // ImGuiコンテキスト破棄
}

// デバッグ表示関数の登録
// 例：DebugUI::RedistDebugFunction([](){ ImGui::Text("Hello"); });
void DebugUI::RedistDebugFunction(std::function<void(void)> f)
{
    // std::move で function をムーブして無駄なコピーを避ける
    m_debugfunction.push_back(std::move(f));
}

void DebugUI::BeginFrame()
{
    // ─────────────────────────────────────────────
    // 1) 新しいフレームの開始（バックエンド → ImGui の順）
    // ─────────────────────────────────────────────
    ImGui_ImplDX11_NewFrame();  // DX11側：フレーム準備（描画関連の準備）
    ImGui_ImplWin32_NewFrame(); // Win32側：入力状態更新など
    ImGui::NewFrame();          // ImGui側：このフレームのUI構築開始
}

void DebugUI::EndFrame() {
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void DebugUI::Render(){
    ImGui::Begin("Debug Information");
    // フレームレートなどの統計情報を表示
    // 注意：ImGui::Text は printf 形式なので、文字列直渡しは避けるのが安全
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / io.Framerate, io.Framerate);

    ImGui::End(); // ウィンドウ終了

    // ─────────────────────────────────────────────
    // 3) 登録された追加デバッグUIを実行
    //    ※ f() の中で ImGui::Begin/End したり、Text したりできる
    // ─────────────────────────────────────────────
    for (auto& f : m_debugfunction)
    {
        f();
    }
}
