#include    "../system/stb_perlin.h"

struct TerrainNoiseParams {
    float baseFrequency = 1.0f / 64.0f; // 地形の荒さ（=スケールの逆）
    float heightScale = 20.0f;          // 高さの最大スケール（メートル想定）
    int   octaves = 5;                  // オクターブ数（層の数）
    float lacunarity = 2.0f;            // 周波数の増加倍率
    float gain = 0.5f;                  // 振幅の減衰
    unsigned int seed = 1337;           // シード（ワールドを変える）
    // 追加オプション
    float warpStrength = 0.0f;          // ドメインワープ量（0でOFF）
    float wrapMeters = 0.0f;            // シームレス・タイル長（0で非タイル）
};

// [-1, +1] → そのまま合成
static inline float perlin3(float x, float y, float z, unsigned seed,
    int x_wrap = 0, int y_wrap = 0, int z_wrap = 0)
{
    return stb_perlin_noise3_seed(x, y, z, x_wrap, y_wrap, z_wrap, seed);
}

// シームレスにしたい場合：ワールド長 wrapMeters をノイズ空間の wrap に対応づける
// 例：wrapMeters = 512 の時、ノイズ引数に (x * freq * wrapUnits / wrapMeters) を渡し、x_wrap=wrapUnits にする
// wrapUnits は 256 や 512 などの2冪が扱いやすい
static inline void calcWrapParams(float wrapMeters, float freq, int& x_wrap_out, float& freq_out)
{
    if (wrapMeters <= 0.0f) { x_wrap_out = 0; freq_out = freq; return; }
    const int wrapUnits = 256;            // 任意（2^Nが無難）
    freq_out = freq * (float)wrapUnits / wrapMeters;
    x_wrap_out = wrapUnits;
}

// フラクタル・ノイズ（必要ならドメインワープ）
float terrainHeight(float wx, float wz, const TerrainNoiseParams& P)
{
    // ドメインワープ（座標ずらし）で複雑さアップ（任意）
    float x = wx, z = wz;
    if (P.warpStrength > 0.0f) {
        float f0 = P.baseFrequency * 0.75f;
        int wrapX0 = 0, wrapZ0 = 0; float f0x = f0, f0z = f0;
        if (P.wrapMeters > 0.0f) {
            calcWrapParams(P.wrapMeters, f0, wrapX0, f0x);
            calcWrapParams(P.wrapMeters, f0, wrapZ0, f0z);
        }
        float wx0 = perlin3(x * f0x, 0.0f, z * f0z, P.seed + 123, wrapX0, 0, wrapZ0);
        float wz0 = perlin3(x * f0x, 100.0f, z * f0z, P.seed + 456, wrapX0, 0, wrapZ0);
        x += wx0 * P.warpStrength;
        z += wz0 * P.warpStrength;
    }

    float sum = 0.0f;
    float amp = 1.0f;
    float freq = P.baseFrequency;

    int x_wrap = 0, z_wrap = 0; float fx = freq, fz = freq;
    if (P.wrapMeters > 0.0f) {
        calcWrapParams(P.wrapMeters, freq, x_wrap, fx);
        calcWrapParams(P.wrapMeters, freq, z_wrap, fz);
    }

    for (int o = 0; o < P.octaves; ++o) {
        float v = perlin3(x * fx, 0.0f, z * fz, P.seed + o, x_wrap, 0, z_wrap);
        sum += v * amp;
        freq *= P.lacunarity;
        amp *= P.gain;

        if (P.wrapMeters > 0.0f) {
            calcWrapParams(P.wrapMeters, freq, x_wrap, fx);
            calcWrapParams(P.wrapMeters, freq, z_wrap, fz);
        }
        else {
            fx = fz = freq;
            x_wrap = z_wrap = 0;
        }
    }
    // sum は概ね [-1, +1] 範囲に収まるイメージ
    return sum * P.heightScale;
}
