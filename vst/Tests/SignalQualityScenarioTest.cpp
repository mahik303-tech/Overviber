// ==============================================================================
// SignalQualityScenarioTest - frequency response and aliasing of the audio path
// ==============================================================================
// Measures every stage of a voice and the console bus with steady test tones,
// run as the engine runs them: at the voices' rate (2x the output rate below
// 100 kHz, SynthEngine::prepare), fed through the half-band interpolator and
// read after the decimator (Halfband2x.h).
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
#include "dsp/MasterBus.h"
#include "dsp/audible/ShelvesFilter.h"
#include "dsp/Halfband2x.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kFftSize = 1 << 14;
constexpr int kBaseBin = 37;          // odd: kFftSize is no multiple of it
constexpr int kSettle = 1 << 14;      // samples before the analysed block
constexpr double kPi = 3.14159265358979323846;

// As SynthEngine::prepare().
int oversamplingFor(float sampleRate) { return sampleRate < 100000.0f ? 2 : 1; }

// One sample in, one out, at the voices' rate.
using Processor = std::function<double(double)>;

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

// Runs a processor as the engine does: interpolated to the voices' rate,
// decimated back.
std::vector<double> run(const Processor& process, const std::vector<double>& in, int oversampling) {
    std::vector<double> out(in.size());
    halfband::Upsampler up;
    halfband::Downsampler down;
    for (size_t t = 0; t < in.size(); ++t) {
        if (oversampling == 1) { out[t] = process(in[t]); continue; }
        float a = 0, b = 0;
        up.process(static_cast<float>(in[t]), a, b);
        const float first = static_cast<float>(process(a));
        out[t] = down.process(first, static_cast<float>(process(b)));
    }
    return out;
}

double offGridDb(const std::vector<double>& power) {
    double onGrid = 0.0, offGrid = 0.0;
    for (int i = 1; i <= kFftSize / 2; ++i) (i % kBaseBin == 0 ? onGrid : offGrid) += power[i];
    return 10.0 * std::log10(std::max(offGrid, 1e-30) / std::max(onGrid, 1e-30));
}

int binFor(double hz, double sampleRate) {
    const int grid = static_cast<int>(std::lround(hz * kFftSize / sampleRate / kBaseBin));
    return std::max(1, grid) * kBaseBin;
}

// A stage at the voices' rate, built fresh for every measurement.
using Factory = std::function<Processor(float voiceRate)>;

// A filter as the voice runs it: 12 dB pad before, makeup after.
template <typename Filter>
Processor filterProcessor(std::shared_ptr<Filter> filter) {
    return [filter](double x) {
        return Voice::kFilterMakeup * filter->processSample(static_cast<float>(x) * Voice::kFilterInputPad);
    };
}

// The master bus with every voice on its own tone, centre pan (the voice
// buffers are interpolated per voice, as the voices run at the bus rate).
std::vector<double> busOutput(const std::vector<std::vector<double>>& voices, float sampleRate) {
    const int oversampling = oversamplingFor(sampleRate);
    MasterBus bus;
    bus.prepare(sampleRate, oversampling);
    PresetData main;
    main.setDefaults();
    bus.setParameters(main);
    std::vector<halfband::Upsampler> up(voices.size());
    std::vector<double> out(voices[0].size());
    for (size_t t = 0; t < out.size(); ++t) {
        float sub[16][2];
        for (size_t v = 0; v < voices.size(); ++v) {
            if (oversampling == 2) up[v].process(static_cast<float>(voices[v][t]), sub[v][0], sub[v][1]);
            else sub[v][0] = static_cast<float>(voices[v][t]);
        }
        for (int k = 0; k < oversampling; ++k) {
            for (size_t v = 0; v < voices.size(); ++v) bus.addVoice(sub[v][k], 0.5f, 0.5f);
            bus.endSubsample();
        }
        float left = 0, right = 0;
        bus.process(left, right);
        out[t] = left;
    }
    return out;
}

// Limits against regressions, with margin to the measured figures: the
// response at 15 kHz (fully open) and the aliasing at mix 1.0.
struct Row {
    std::string name;
    Factory make;
    double min15k;
    double maxAlias;
};

int failures = 0;

void expect(bool ok, const std::string& what) {
    if (ok) return;
    std::printf("  [FAIL] %s\n", what.c_str());
    ++failures;
}

void report(const Row& row, float sampleRate) {
    const int oversampling = oversamplingFor(sampleRate);
    const float voiceRate = sampleRate * static_cast<float>(oversampling);
    auto gainDb = [&](int bin) {
        const double amplitude = 0.01;
        const auto power = spectrum(run(row.make(voiceRate), tones({ bin }, amplitude), oversampling));
        return 20.0 * std::log10(std::max(2.0 * std::sqrt(power[bin]) / kFftSize, 1e-12) / amplitude);
    };
    const double reference = gainDb(binFor(1000.0, sampleRate));
    std::printf("  %-22s", row.name.c_str());
    double at15k = 0.0;
    for (double hz : { 5000.0, 10000.0, 15000.0, 18000.0, 20000.0 }) {
        if (hz > sampleRate * 0.47) { std::printf("%9s", "-"); continue; }
        const double gain = gainDb(binFor(hz, sampleRate)) - reference;
        if (hz == 15000.0) at15k = gain;
        std::printf("%+9.2f", gain);
    }
    const int toneBin = binFor(4500.0, sampleRate);
    double alias = 0.0;
    for (double amplitude : { 1.0, 2.0 }) {
        const double a = offGridDb(spectrum(run(row.make(voiceRate), tones({ toneBin }, amplitude), oversampling)));
        if (amplitude == 1.0) alias = a;
        std::printf("%+10.1f", a);
    }
    std::printf("\n");
    expect(at15k >= row.min15k, row.name + ": response at 15 kHz");
    expect(alias <= row.maxAlias, row.name + ": aliasing");
}

// Wavetable oscillator: energy away from the harmonics of its measured
// pitch (4-term Blackman-Harris window, harmonics +-8 bins), relative to the
// signal. The pitch is not bin-exact, hence the window.
double oscillatorAliasDb(float sampleRate, bool saw, uint16_t pitchCv, double& f0) {
    const int oversampling = oversamplingFor(sampleRate);
    const float voiceRate = sampleRate * static_cast<float>(oversampling);
    std::vector<uint16_t> table(WTOSC_SAMPLE_COUNT);
    for (int i = 0; i < WTOSC_SAMPLE_COUNT; ++i) {
        const double phase = 2.0 * kPi * i / WTOSC_SAMPLE_COUNT;
        double v = 0.0;
        if (saw) for (int h = 1; h < WTOSC_SAMPLE_COUNT / 2; ++h) v += std::sin(h * phase) / h * (2.0 / kPi);
        else v = std::sin(phase);
        table[i] = static_cast<uint16_t>(std::clamp(32768.0 + 30000.0 * v, 0.0, 65535.0));
    }
    WtOsc osc;
    osc.setSampleRate(voiceRate);
    osc.setSampleData(table.data(), table.data());
    osc.setParameters(pitchCv, wmOff, 0);
    const auto tickStep = static_cast<uint32_t>(SYNTH_MASTER_CLOCK / voiceRate);
    auto next = [&] { return static_cast<float>((osc.processSample(tickStep, osmNone, nullptr) - 32768.0) / 32768.0); };
    halfband::Downsampler down;
    std::vector<std::complex<double>> a(kFftSize);
    for (int t = 0; t < kSettle + kFftSize; ++t) {
        double x = next();
        if (oversampling == 2) x = down.process(static_cast<float>(x), next());
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

    auto semRow = [](const char* name, uint8_t variant) {
        return Row{ name, [variant](float rate) {
            auto f = std::make_shared<SemFilter>();
            f->setSampleRate(rate); f->setVariant(variant); f->setMode(0); f->setCV(65535, 0);
            return filterProcessor(f);
        }, variant == SemFilter::Liquid ? -9.0 : -0.5, variant == SemFilter::Liquid ? -100.0 : -120.0 };
    };
    const std::vector<Row> rows = {
        { "SSI2144 LP24", [](float rate) {
            auto f = std::make_shared<Ssi2144Filter>();
            f->setSampleRate(rate); f->setCV(65535, 0);
            return filterProcessor(f);
        }, -6.5, -100.0 },
        { "SST ladder LP24", [](float rate) {
            auto f = std::make_shared<SstLadderFilter>();
            f->setSampleRate(rate); f->setMode(0); f->setCV(65535, 0);
            return filterProcessor(f);
        }, -2.0, -100.0 },
        semRow("SEM OB-Xd LP", SemFilter::ObXd),
        semRow("SEM Oberheim LP", SemFilter::Oberheim),
        semRow("SEM Vult LP", SemFilter::Vult),
        semRow("SEM Cytomic LP", SemFilter::Cytomic),
        semRow("Liquid LP4", SemFilter::Liquid),
        { "Shelves EQ flat", [](float rate) {
            auto f = std::make_shared<ShelvesFilter>();
            f->setSampleRate(rate); f->setMode(0); f->setCV(65535, 0);
            return filterProcessor(f);
        }, -0.5, -100.0 },
        { "LM13700 VCA", [](float rate) {
            auto vca = std::make_shared<Lm13700Vca>();
            vca->setSampleRate(rate); vca->setCV(65535);
            return Processor([vca](double x) { return vca->processSample(static_cast<float>(x)); });
        }, -0.1, -65.0 },
    };

    for (float sampleRate : { 44100.0f, 48000.0f, 96000.0f }) {
        std::printf("\n%.0f Hz (voices at %dx)  response dB re 1 kHz (-40 dBFS)   alias dB (4.5 kHz sine)\n",
                    sampleRate, oversamplingFor(sampleRate));
        std::printf("  %-22s%9s%9s%9s%9s%9s%10s%10s\n", "stage", "5k", "10k", "15k", "18k", "20k", "mix 1.0", "mix 2.0");
        for (const auto& row : rows) report(row, sampleRate);

        // The master bus (console, ceiling, decimator) with one voice: its
        // response; with six voices on separate tones: intermodulation stays
        // on the grid, folded products do not.
        const double reference = [&] {
            const int bin = binFor(1000.0, sampleRate);
            const auto power = spectrum(busOutput({ tones({ bin }, 0.01) }, sampleRate));
            return 20.0 * std::log10(2.0 * std::sqrt(power[bin]) / kFftSize);
        }();
        std::printf("  %-22s", "Master bus, one voice");
        for (double hz : { 5000.0, 10000.0, 15000.0, 18000.0, 20000.0 }) {
            if (hz > sampleRate * 0.47) { std::printf("%9s", "-"); continue; }
            const int bin = binFor(hz, sampleRate);
            const auto power = spectrum(busOutput({ tones({ bin }, 0.01) }, sampleRate));
            const double gain = 20.0 * std::log10(2.0 * std::sqrt(power[bin]) / kFftSize) - reference;
            std::printf("%+9.2f", gain);
            expect(std::abs(gain) < 0.1, "master bus: flat response");
        }
        std::printf("\n");
        for (double voiceLevel : { 0.5, 1.0 }) {
            std::vector<std::vector<double>> voices;
            for (int k : { 23, 29, 37, 43, 53, 61 }) voices.push_back(tones({ k * kBaseBin }, voiceLevel));
            const double alias = offGridDb(spectrum(busOutput(voices, sampleRate)));
            std::printf("  Master bus, six voices at %.1f: alias %+.1f dB\n", voiceLevel, alias);
            // At 1.0 the output ceiling works at the output rate (see MasterBus).
            if (voiceLevel == 0.5) expect(alias <= -70.0, "master bus: aliasing of six voices");
        }
        for (bool saw : { false, true })
            for (uint16_t cv : { 256 * 45, 256 * 69, 256 * 81 }) {
                double f0 = 0.0;
                const double alias = oscillatorAliasDb(sampleRate, saw, cv, f0);
                std::printf("  Oscillator %s %6.0f Hz: alias %+.1f dB\n", saw ? "saw " : "sine", f0, alias);
                // A bright saw above 32 kHz folds as on the hardware (64 kHz DAC).
                if (!saw || cv == 256 * 69) expect(alias <= (saw ? -50.0 : -85.0), "oscillator: aliasing");
            }
    }
    std::printf("%s signal quality (%d failures)\n", failures ? "[FAIL]" : "[PASS]", failures);
    return failures ? 1 : 0;
}
