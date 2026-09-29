// ==============================================================================
// SignalQualityScenarioTest - frequency response and aliasing of the audio path
// ==============================================================================
// Measures every stage of a voice and the console bus with steady test tones:
//
//   response  gain of a quiet sine (-40 dBFS) relative to 1 kHz, filters fully
//             open (cutoff CV 65535, resonance 0): the treble loss of a stage
//   alias     share of energy off the tones' harmonic / intermodulation grid.
//             All tones sit on multiples of an odd base bin b of an N = 2^14
//             point FFT, so every product of the tones lands on a multiple of
//             b, while a product folded at Nyquist lands between them (N is
//             not a multiple of b). Rectangular window, bin-exact tones: no
//             leakage. The value is off-grid energy relative to the signal,
//             in dB (lower is cleaner).
//
// Levels follow the voice: the oscillator mix at full scale (1.0) enters a
// filter 12 dB down (Voice::kFilterInputPad) and leaves with the makeup gain.
// ==============================================================================
#include "dsp/Ssi2144Filter.h"
#include "dsp/SstLadderFilter.h"
#include "dsp/SemFilter.h"
#include "dsp/Lm13700Vca.h"
#include "dsp/Voice.h"
#include "dsp/ConsoleXProcessor.h"
#include "dsp/audible/ShelvesFilter.h"
#include "dsp/Halfband2x.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

constexpr int kFftSize = 1 << 14;
constexpr int kBaseBin = 37;          // odd: kFftSize is no multiple of it
constexpr int kSettle = 1 << 14;      // samples before the analysed block
constexpr double kPi = 3.14159265358979323846;

using Stage = std::function<void(const std::vector<double>& in, std::vector<double>& out)>;

void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double angle = -2.0 * kPi / static_cast<double>(len);
        const std::complex<double> wLen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wLen;
            }
        }
    }
}

// Power per bin (0 .. N/2) of the last kFftSize samples.
std::vector<double> spectrum(const std::vector<double>& signal) {
    std::vector<std::complex<double>> a(kFftSize);
    const size_t start = signal.size() - kFftSize;
    for (int i = 0; i < kFftSize; ++i) a[i] = signal[start + i];
    fft(a);
    std::vector<double> power(kFftSize / 2 + 1);
    for (int i = 0; i <= kFftSize / 2; ++i) power[i] = std::norm(a[i]);
    return power;
}

// Sum of sines on the given bins, long enough to settle and analyse.
std::vector<double> tones(const std::vector<int>& bins, double amplitude) {
    std::vector<double> x(kSettle + kFftSize, 0.0);
    for (size_t t = 0; t < x.size(); ++t)
        for (size_t k = 0; k < bins.size(); ++k)
            x[t] += amplitude * std::sin(2.0 * kPi * bins[k] * static_cast<double>(t) / kFftSize + 0.7 * k);
    return x;
}

std::vector<double> run(const Stage& stage, const std::vector<double>& in) {
    std::vector<double> out(in.size());
    stage(in, out);
    return out;
}

// Gain in dB of a quiet sine on `bin`.
double gainDb(const Stage& stage, int bin) {
    const double amplitude = 0.01;
    const auto power = spectrum(run(stage, tones({ bin }, amplitude)));
    const double measured = 2.0 * std::sqrt(power[bin]) / kFftSize;
    return 20.0 * std::log10(std::max(measured, 1e-12) / amplitude);
}

// Off-grid energy relative to the signal, dB.
double aliasDb(const Stage& stage, const std::vector<int>& bins, double amplitude) {
    const auto power = spectrum(run(stage, tones(bins, amplitude)));
    double onGrid = 0.0, offGrid = 0.0;
    for (int i = 1; i <= kFftSize / 2; ++i) (i % kBaseBin == 0 ? onGrid : offGrid) += power[i];
    return 10.0 * std::log10(std::max(offGrid, 1e-30) / std::max(onGrid, 1e-30));
}

int binFor(double hz, double sampleRate) {
    const int grid = static_cast<int>(std::lround(hz * kFftSize / sampleRate / kBaseBin));
    return std::max(1, grid) * kBaseBin;
}

// A filter as the voice runs it: 12 dB pad before, makeup after.
template <typename Filter>
Stage filterStage(Filter& filter) {
    return [&filter](const std::vector<double>& in, std::vector<double>& out) {
        for (size_t t = 0; t < in.size(); ++t)
            out[t] = Voice::kFilterMakeup * filter.processSample(static_cast<float>(in[t]) * Voice::kFilterInputPad);
    };
}

struct Row {
    std::string name;
    std::function<Stage(float sampleRate)> make;
};

void report(const Row& row, float sampleRate) {
    const double reference = gainDb(row.make(sampleRate), binFor(1000.0, sampleRate));
    std::printf("  %-22s", row.name.c_str());
    for (double hz : { 5000.0, 10000.0, 15000.0, 18000.0, 20000.0 }) {
        if (hz > sampleRate * 0.47) { std::printf("%9s", "-"); continue; }
        std::printf("%+9.2f", gainDb(row.make(sampleRate), binFor(hz, sampleRate)) - reference);
    }
    const int toneBin = binFor(4500.0, sampleRate);
    std::printf("%+10.1f%+10.1f\n", aliasDb(row.make(sampleRate), { toneBin }, 1.0),
                aliasDb(row.make(sampleRate), { toneBin }, 2.0));
}

// Wavetable oscillator: energy away from the harmonics of its measured
// pitch (4-term Blackman-Harris window, harmonics +-8 bins), relative to the
// signal. The pitch is not bin-exact, hence the window.
double oscillatorAliasDb(float sampleRate, bool saw, uint16_t pitchCv, double& f0) {
    std::vector<uint16_t> table(WTOSC_SAMPLE_COUNT);
    for (int i = 0; i < WTOSC_SAMPLE_COUNT; ++i) {
        const double phase = 2.0 * kPi * i / WTOSC_SAMPLE_COUNT;
        double v = 0.0;
        if (saw) for (int h = 1; h < WTOSC_SAMPLE_COUNT / 2; ++h) v += std::sin(h * phase) / h * (2.0 / kPi);
        else v = std::sin(phase);
        table[i] = static_cast<uint16_t>(std::clamp(32768.0 + 30000.0 * v, 0.0, 65535.0));
    }
    WtOsc osc;
    osc.setSampleRate(sampleRate);
    osc.setSampleData(table.data(), table.data());
    osc.setParameters(pitchCv, wmOff, 0);
    const auto tickStep = static_cast<uint32_t>(SYNTH_MASTER_CLOCK / sampleRate);
    std::vector<std::complex<double>> a(kFftSize);
    for (int t = 0; t < kSettle + kFftSize; ++t) {
        const double x = (osc.processSample(tickStep, osmNone, nullptr) - 32768.0) / 32768.0;
        if (t < kSettle) continue;
        const double w = 2.0 * kPi * (t - kSettle) / kFftSize;
        a[t - kSettle] = x * (0.35875 - 0.48829 * std::cos(w) + 0.14128 * std::cos(2 * w) - 0.01168 * std::cos(3 * w));
    }
    fft(a);
    std::vector<double> power(kFftSize / 2 + 1);
    for (int i = 0; i <= kFftSize / 2; ++i) power[i] = std::norm(a[i]);
    int peak = 16;
    for (int i = 16; i < kFftSize / 2; ++i) if (power[i] > power[peak]) peak = i;
    const double l = std::log(power[peak - 1]), c = std::log(power[peak]), r = std::log(power[peak + 1]);
    const double f0Bin = peak + 0.5 * (l - r) / (l - 2 * c + r);
    f0 = f0Bin * sampleRate / kFftSize;
    std::vector<bool> harmonic(kFftSize / 2 + 1, false);
    for (int i = 0; i <= 8; ++i) harmonic[i] = true;   // DC
    for (double h = f0Bin; h < kFftSize / 2; h += f0Bin)
        for (int i = std::max(0, (int)h - 8); i <= std::min(kFftSize / 2, (int)h + 8); ++i) harmonic[i] = true;
    double on = 0.0, off = 0.0;
    for (int i = 9; i <= kFftSize / 2; ++i) (harmonic[i] ? on : off) += power[i];
    return 10.0 * std::log10(std::max(off, 1e-30) / on);
}

// Half-band resampler at a 44.1 kHz base rate: passband gain of a tone
// through down- or upsampling, and the level of what folds into the band.
bool checkHalfband() {
    const double base = 44100.0, twice = 88200.0;
    auto level = [&](const std::vector<double>& s, double hz, double rate) {
        double re = 0, im = 0, sum = 0;
        for (size_t t = 0; t < s.size(); ++t) {
            const double w = 0.5 - 0.5 * std::cos(2 * kPi * t / s.size());
            sum += w;
            re += w * s[t] * std::cos(2 * kPi * hz * t / rate);
            im -= w * s[t] * std::sin(2 * kPi * hz * t / rate);
        }
        return 20.0 * std::log10(std::max(1e-15, 2.0 * std::sqrt(re * re + im * im) / sum));
    };
    double passband = 0.0, folded = -300.0, image = -300.0;
    for (double hz = 500.0; hz < 40000.0; hz += 1250.0) {
        halfband::Downsampler down;
        halfband::Upsampler up;
        std::vector<double> decimated, interpolated;
        for (int n = 0; n < 24000; ++n) {
            const float a = static_cast<float>(std::sin(2 * kPi * hz * (2 * n) / twice));
            const float b = static_cast<float>(std::sin(2 * kPi * hz * (2 * n + 1) / twice));
            const float d = down.process(a, b);
            float u0 = 0, u1 = 0;
            up.process(static_cast<float>(std::sin(2 * kPi * hz * n / base)), u0, u1);
            if (n < 4000) continue;
            decimated.push_back(d);
            interpolated.push_back(u0);
            interpolated.push_back(u1);
        }
        if (hz <= 20000.0) {
            passband = std::max({ passband, std::abs(level(decimated, hz, base)), std::abs(level(interpolated, hz, twice)) });
            image = std::max(image, level(interpolated, base - hz, twice));
        } else if (hz >= base - 20000.0) {
            folded = std::max(folded, level(decimated, base - hz, base));
        }
    }
    std::printf("Half-band 2x: passband to 20 kHz within %.4f dB, folded into it %.1f dB, images %.1f dB\n",
                passband, folded, image);
    return passband < 0.01 && folded < -100.0 && image < -100.0;
}

} // namespace

int main() {
    std::printf("=== Signal quality of the audio path ===\n");
    if (!checkHalfband()) {
        std::printf("[FAIL] half-band resampler\n");
        return 1;
    }
    std::printf("[PASS] half-band resampler\n");

    // Stages are rebuilt per measurement, so every tone starts from reset.
    std::vector<std::unique_ptr<Ssi2144Filter>> ssi;
    std::vector<std::unique_ptr<SstLadderFilter>> sst;
    std::vector<std::unique_ptr<SemFilter>> sem;
    std::vector<std::unique_ptr<ShelvesFilter>> shelves;
    std::vector<std::unique_ptr<Lm13700Vca>> vcas;
    std::vector<std::unique_ptr<ConsoleXProcessor>> consoles;

    auto semRow = [&](const char* name, uint8_t variant) {
        return Row{ name, [&, variant](float sr) {
            sem.push_back(std::make_unique<SemFilter>());
            auto& f = *sem.back();
            f.setSampleRate(sr); f.setVariant(variant); f.setMode(0); f.setCV(65535, 0);
            return filterStage(f);
        } };
    };
    const std::vector<Row> rows = {
        { "SSI2144 LP24", [&](float sr) {
            ssi.push_back(std::make_unique<Ssi2144Filter>());
            ssi.back()->setSampleRate(sr); ssi.back()->setCV(65535, 0);
            return filterStage(*ssi.back());
        } },
        { "SST ladder LP24", [&](float sr) {
            sst.push_back(std::make_unique<SstLadderFilter>());
            sst.back()->setSampleRate(sr); sst.back()->setMode(0); sst.back()->setCV(65535, 0);
            return filterStage(*sst.back());
        } },
        semRow("SEM OB-Xd LP", SemFilter::ObXd),
        semRow("SEM Oberheim LP", SemFilter::Oberheim),
        semRow("SEM Vult LP", SemFilter::Vult),
        semRow("SEM Cytomic LP", SemFilter::Cytomic),
        semRow("Liquid LP4", SemFilter::Liquid),
        { "Shelves EQ flat", [&](float sr) {
            shelves.push_back(std::make_unique<ShelvesFilter>());
            shelves.back()->setSampleRate(sr); shelves.back()->setMode(0); shelves.back()->setCV(65535, 0);
            return filterStage(*shelves.back());
        } },
        { "LM13700 VCA", [&](float sr) {
            vcas.push_back(std::make_unique<Lm13700Vca>());
            auto& vca = *vcas.back();
            vca.setSampleRate(sr); vca.setCV(65535);
            return Stage([&vca](const std::vector<double>& in, std::vector<double>& out) {
                for (size_t t = 0; t < in.size(); ++t) out[t] = vca.processSample(static_cast<float>(in[t]));
            });
        } },
        { "Console, one voice", [&](float sr) {
            consoles.push_back(std::make_unique<ConsoleXProcessor>());
            auto& console = *consoles.back();
            console.setSampleRate(sr); console.setParameters(0.1f, 1.0f, 17.0f / 999.0f);
            return Stage([&console](const std::vector<double>& in, std::vector<double>& out) {
                for (size_t t = 0; t < in.size(); ++t) {
                    // MasterBus: 6 dB in, centre pan (0.5 per side), 0.9 after.
                    float l = 0, r = 0, outL = 0, outR = 0;
                    const float x = static_cast<float>(in[t]) * 0.5f * 0.5f;
                    console.encodeVoice(x, x, l, r);
                    console.decodeMaster(l, r, outL, outR);
                    out[t] = outL * 0.9 * 2.0;   // back to the voice's scale
                }
            });
        } },
    };

    for (float sampleRate : { 44100.0f, 48000.0f, 96000.0f }) {
        std::printf("\n%.0f Hz          response dB re 1 kHz (-40 dBFS)             alias dB (4.5 kHz sine)\n", sampleRate);
        std::printf("  %-22s%9s%9s%9s%9s%9s%10s%10s\n", "stage", "5k", "10k", "15k", "18k", "20k", "mix 1.0", "mix 2.0");
        for (const auto& row : rows) report(row, sampleRate);

        // The console bus with six voices on separate tones: intermodulation
        // stays on the grid, folded products do not.
        for (double voiceLevel : { 0.5, 1.0 }) {
            ConsoleXProcessor console;
            console.setSampleRate(sampleRate);
            console.setParameters(0.1f, 1.0f, 17.0f / 999.0f);
            const std::vector<int> bins = { 23, 29, 37, 43, 53, 61 };
            std::vector<std::vector<double>> voices;
            for (int k : bins) voices.push_back(tones({ k * kBaseBin }, voiceLevel));
            std::vector<double> out(voices[0].size());
            for (size_t t = 0; t < out.size(); ++t) {
                float sumL = 0, sumR = 0;
                for (const auto& v : voices) {
                    float l = 0, r = 0;
                    const float x = static_cast<float>(v[t]) * 0.5f * 0.5f;
                    console.encodeVoice(x, x, l, r);
                    sumL += l; sumR += r;
                }
                float outL = 0, outR = 0;
                console.decodeMaster(sumL, sumR, outL, outR);
                out[t] = outL;
            }
            const auto power = spectrum(out);
            double onGrid = 0.0, offGrid = 0.0;
            for (int i = 1; i <= kFftSize / 2; ++i) (i % kBaseBin == 0 ? onGrid : offGrid) += power[i];
            std::printf("  Console, six voices at %.1f: alias %+.1f dB\n", voiceLevel,
                        10.0 * std::log10(std::max(offGrid, 1e-30) / onGrid));
        }
        for (bool saw : { false, true })
            for (uint16_t cv : { 256 * 45, 256 * 69, 256 * 81 }) {
                double f0 = 0.0;
                const double alias = oscillatorAliasDb(sampleRate, saw, cv, f0);
                std::printf("  Oscillator %s %6.0f Hz: alias %+.1f dB\n", saw ? "saw " : "sine", f0, alias);
            }
    }
    return 0;
}
