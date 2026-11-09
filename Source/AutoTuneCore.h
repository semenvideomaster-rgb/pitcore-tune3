#pragma once
// Автотюн в реальном времени без зависимостей от JUCE: YIN-детектор высоты + двухточечный сдвиг тона.
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

struct AutoTuneCore
{
    int   key = 0;            // 0..11 (C..B)
    int   scale = 1;          // 0 хроматика, 1 мажор, 2 минор
    float strength = 0.9f;    // 0..1, сила подтяжки
    float speedMs = 20.0f;    // время подстройки, мс (меньше = жёстче)

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        D = std::max (1, (int) std::lround (sr / 16000.0));
        fs = sr / D;
        wMin = (float) (0.02 * sr); wMax = (float) (0.06 * sr);
        int n = 1; while (n < (int) (2 * wMax) + 16) n <<= 1;
        buf.assign ((size_t) n, 0.f); mask = n - 1; wr = 0;
        ring.assign ((size_t) RN, 0.f); rpos = 0; acc = 0; accN = 0; hop = 0;
        phi = 0; ratio = targetRatio = 1.f; wCur = (float) (0.04 * sr); // окно фиксировано: его изменение на ходу сдвигает тон
        unvoiced = 0; hist.fill (0.f); histN = 0;
    }
    int latency() const { return (int) (0.02 * sr); }

    float process (float x)
    {
        buf[(size_t) wr] = x;
        acc += x;
        if (++accN >= D)
        {
            ring[(size_t) rpos] = acc / (float) D; rpos = (rpos + 1) & (RN - 1); acc = 0; accN = 0;
            if (++hop >= 128) { hop = 0; analyse(); }
        }
        const float tau = speedMs * 0.001f * (float) sr;
        const float a = tau < 1.f ? 1.f : 1.f - std::exp (-1.f / tau);
        ratio += (targetRatio - ratio) * a;
        phi += (1.f - ratio) / wCur;
        phi -= std::floor (phi);
        float p2 = phi + 0.5f; p2 -= std::floor (p2);
        const float y = tap (phi) * (0.5f - 0.5f * std::cos (6.2831853f * phi))
                      + tap (p2)  * (0.5f - 0.5f * std::cos (6.2831853f * p2));
        wr = (wr + 1) & mask;
        return y;
    }

private:
    static constexpr int RN = 2048;
    double sr = 44100, fs = 16000; int D = 3;
    std::vector<float> buf, ring; int mask = 0, wr = 0, rpos = 0, accN = 0, hop = 0, unvoiced = 0, histN = 0;
    float acc = 0, phi = 0, ratio = 1, targetRatio = 1, wCur = 1764, wMin = 882, wMax = 2646;
    std::array<float, 5> hist {};

    float tap (float p) const
    {
        const float d = p * wCur + 1.f; const int i = (int) d; const float f = d - (float) i;
        const float a = buf[(size_t) ((wr - i) & mask)], b = buf[(size_t) ((wr - i - 1) & mask)];
        return a + (b - a) * f;
    }

    float snap (float m) const
    {
        static const int sets[3][12] = { {1,1,1,1,1,1,1,1,1,1,1,1}, {1,0,1,0,1,1,0,1,0,1,0,1}, {1,0,1,1,0,1,0,1,1,0,1,0} };
        float best = std::round (m), bd = 99.f;
        for (int k = (int) std::floor (m) - 2; k <= (int) std::ceil (m) + 2; ++k)
        {
            const int pc = (((k - key) % 12) + 12) % 12;
            if (sets[scale][pc] && std::fabs ((float) k - m) < bd) { bd = std::fabs ((float) k - m); best = (float) k; }
        }
        return best;
    }

    void analyse()
    {
        constexpr int N = 1024, Wy = 512;
        float x[N]; for (int i = 0; i < N; ++i) x[i] = ring[(size_t) ((rpos - N + i) & (RN - 1))];
        const int tauMax = std::min (400, (int) (fs / 70.0)), tauMin = std::max (2, (int) (fs / 800.0)), off = N - Wy - tauMax - 1;
        float e = 0; for (int j = 0; j < Wy; ++j) e += x[off + j] * x[off + j];
        bool voiced = false; float f0 = 0;
        if (e / (float) Wy > 1e-6f)
        {
            std::array<float, 402> d {}; float run = 0; d[0] = 1;
            for (int t = 1; t <= tauMax + 1; ++t)
            {
                float s = 0; for (int j = 0; j < Wy; ++j) { const float df = x[off + j] - x[off + j + t]; s += df * df; }
                run += s; d[(size_t) t] = run > 0 ? s * (float) t / run : 1.f;
            }
            for (int t = tauMin; t < tauMax; ++t)
                if (d[(size_t) t] < 0.15f)
                {
                    while (t + 1 < tauMax && d[(size_t) t + 1] < d[(size_t) t]) ++t;
                    const float a = d[(size_t) t - 1], b = d[(size_t) t], c = d[(size_t) t + 1], den = a - 2 * b + c;
                    f0 = (float) fs / ((float) t + (den != 0 ? 0.5f * (a - c) / den : 0.f)); voiced = true; break;
                }
        }
        if (!voiced) { if (++unvoiced >= 3) targetRatio = 1.f; return; }
        unvoiced = 0;
        const float m = 69.f + 12.f * std::log2 (f0 / 440.f);
        for (size_t i = hist.size() - 1; i > 0; --i) hist[i] = hist[i - 1];
        hist[0] = m; histN = std::min (histN + 1, 5);
        std::array<float, 5> s = hist; std::sort (s.begin(), s.begin() + histN);
        const float mm = s[(size_t) (histN / 2)];
        targetRatio = std::pow (2.f, (snap (mm) - mm) * strength / 12.f);
    }
};
