#include "TestData.h"
#include "dsp/audible/stmlib/utils/random.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace fs = std::filesystem;
static void word(std::ostream& out, uint32_t value, int bytes = 4) {
    for (int i = 0; i < bytes; ++i) out.put(static_cast<char>((value >> (8 * i)) & 255));
}
static void wav(const fs::path& path, const std::vector<float>& audio, int rate) {
    std::ofstream out(path, std::ios::binary);
    const auto bytes = static_cast<uint32_t>(audio.size() * sizeof(float));
    out.write("RIFF", 4); word(out, 36 + bytes); out.write("WAVEfmt ", 8);
    word(out, 16); word(out, 3, 2); word(out, 2, 2); // IEEE float, stereo
    word(out, rate); word(out, rate * 8); word(out, 8, 2); word(out, 32, 2);
    out.write("data", 4); word(out, bytes);
    for (float value : audio) {
        uint32_t bits; std::memcpy(&bits, &value, sizeof(bits)); word(out, bits);
    }
    if (!out) throw std::runtime_error("Cannot write WAV: " + path.string());
}
static SignalStats combine(SignalStats a, const SignalStats& b) {
    a.peak = std::max(a.peak, b.peak); a.squares += b.squares;
    a.samples += b.samples; a.nonFinite += b.nonFinite; a.overUnity += b.overUnity;
    return a;
}
static void row(std::ostream& csv, const std::string& name, const std::string& stage,
                const SignalStats& stats) {
    csv << name << ',' << stage << ',' << std::setprecision(12) << stats.peak << ','
        << stats.rms() << ',' << stats.samples << ',' << stats.nonFinite << ',' << stats.overUnity << '\n';
}
static std::vector<float> render(SynthEngine& engine, int rate, int polyphony,
                                 RenderDiagnostics& diagnostics, int frames) {
    engine.setDiagnostics(&diagnostics);
    const uint8_t notes[] = {60, 64, 67, 72, 76, 79};
    for (int v = 0; v < polyphony; ++v) engine.noteOn(notes[v], 60000, 1);
    std::vector<float> result(static_cast<size_t>(frames) * 2);
    std::array<float, 64> left{}, right{};
    const int release = frames / 2;
    for (int offset = 0; offset < frames;) {
        if (offset == release)
            for (int v = 0; v < polyphony; ++v) engine.noteOff(notes[v], 0, 1);
        int count = std::min(64, frames - offset);
        if (offset < release) count = std::min(count, release - offset);
        engine.renderBlock(left.data(), right.data(), count);
        for (int i = 0; i < count; ++i) {
            result[2 * (offset + i)] = left[i]; result[2 * (offset + i) + 1] = right[i];
        }
        offset += count;
    }
    engine.setDiagnostics(nullptr);
    return result;
}
static void report(std::ostream& csv, const std::string& name, const RenderDiagnostics& d) {
    VoiceDiagnostics total;
    for (const auto& voice : d.voices) {
        total.oscillator = combine(total.oscillator, voice.oscillator);
        total.mixed = combine(total.mixed, voice.mixed);
        total.filtered = combine(total.filtered, voice.filtered);
        total.vca = combine(total.vca, voice.vca);
    }
    row(csv, name, "oscillator", total.oscillator); row(csv, name, "mixed", total.mixed);
    row(csv, name, "filter", total.filtered); row(csv, name, "vca", total.vca);
    row(csv, name, "bus", combine(d.busLeft, d.busRight));
    row(csv, name, "console", combine(d.consoleLeft, d.consoleRight));
    row(csv, name, "output", combine(d.outputLeft, d.outputRight));
}
int main(int argc, char* argv[]) {
    try {
        // argv[1]: fixture path, argv[2]: a NEW output directory, argv[3]: smoke.
        const fs::path data = argc > 1 ? argv[1] : OVERVIBER_TEST_DATA_DIR;
        const fs::path output = argc > 2 ? argv[2] : "audio-reference";
        const bool smoke = argc > 3 && std::string(argv[3]) == "smoke";
        if (fs::exists(output)) throw std::runtime_error("Output exists; choose a new directory to preserve the baseline");
        if (!fs::is_directory(data / "PRESETS") || !fs::is_directory(data / "WAVEDATA"))
            throw std::runtime_error("Missing data directory");
        fs::create_directories(output);
        std::ofstream csv(output / "metrics.csv");
        csv << "case,stage,peak,rms,samples,non_finite,over_unity\n";
        std::ofstream manifest(output / "manifest.txt");
        manifest << "Overviber reference format 1\nIEEE float32 stereo WAV; no normalization\n"
                 << "Seed=33 per case; block=64; velocity=60000; notes=60,64,67,72,76,79\n"
                 << "Data=" << fs::absolute(data).string() << "\n"
                 << "Preset renders force main-preset routing to measure the loaded preset.\n"
                 << "Voice statistics aggregate active voice samples; output statistics include silence.\n";
        int cases = 0, invalid = 0;
        auto makeEngine = [&](int rate) {
            stmlib::Random::Seed(33); std::srand(33);
            auto engine = std::make_unique<SynthEngine>();
            engine->getWaveManager().setBaseDirectory((data / "WAVEDATA").string());
            engine->getPresetManager().setBaseDirectory((data / "PRESETS").string());
            engine->prepare(static_cast<float>(rate));
            return engine;
        };
        auto saveCase = [&](SynthEngine& engine, const std::string& name, int rate, int voices,
                            bool writeAudio, int frames) {
            RenderDiagnostics d;
            auto audio = render(engine, rate, voices, d, frames);
            report(csv, name, d);
            if (writeAudio) wav(output / (name + ".wav"), audio, rate);
            if (d.outputLeft.nonFinite || d.outputRight.nonFinite) ++invalid;
            ++cases;
            if (cases % 32 == 0) std::cout << cases << " cases rendered\n";
        };
        // Factory snapshots, with exact textual settings alongside the audio.
        auto catalog = makeEngine(48000);
        const int presets = smoke ? std::min(1, catalog->getPresetManager().getPresetCount())
                                  : catalog->getPresetManager().getPresetCount();
        if (presets == 0) throw std::runtime_error("No presets");
        for (int p = 0; p < presets; ++p) for (int voices : {1, 6}) {
            auto engine = makeEngine(48000);
            engine->loadPreset(p); engine->setSteppedParam(spEngineMode, emMultiChannel);
            const std::string name = "preset_" + std::to_string(engine->getPresetManager().getPresetNumber(p))
                + "_48000_v" + std::to_string(voices);
            std::ofstream settings(output / (name + ".conf"));
            settings << engine->getPresetManager().serializePresetToString(engine->getCurrentPreset());
            saveCase(*engine, name, 48000, voices, true, 24000);
        }
        // Controlled Elements matrix: all models, filters, resonance and voice counts.
        for (int rate : {44100, 48000, 96000}) for (int model = 0; model < 4; ++model)
        for (int filter = 0; filter < (smoke ? 1 : 4); ++filter)
        for (int resonance : {0, 500, 999}) for (int voices : {1, 6}) {
            if (smoke && (rate != 48000 || resonance != 0 || voices != 1)) continue;
            auto engine = makeEngine(rate);
            engine->setSteppedParam(spEngineMode, emMultiChannel);
            engine->setSteppedParam(spOscEngine, oeElements);
            engine->setSteppedParam(spElementsModel, static_cast<uint8_t>(model));
            engine->setSteppedParam(spFilterModel, static_cast<uint8_t>(filter));
            engine->setContinuousParam(cpResonance, scan_potTo16bits(resonance));
            engine->setContinuousParam(cpElementsStrike, scan_potTo16bits(800));
            engine->setContinuousParam(cpElementsGeometry, scan_potTo16bits(400));
            engine->setContinuousParam(cpElementsBrightness, scan_potTo16bits(600));
            engine->setContinuousParam(cpElementsDamping, scan_potTo16bits(350));
            engine->setContinuousParam(cpAmpRel, scan_potTo16bits(600));
            const std::string name = "elements_" + std::to_string(model) + "_filter_" + std::to_string(filter)
                + "_res_" + std::to_string(resonance) + "_" + std::to_string(rate) + "_v" + std::to_string(voices);
            saveCase(*engine, name, rate, voices, rate == 48000 && filter == 0 && resonance == 0, rate / 4);
        }
        manifest << "Cases=" << cases << "\nNonFiniteCases=" << invalid << "\n";
        if (!csv || !manifest) throw std::runtime_error("Cannot write report");
        std::cout << cases << " cases, " << invalid << " non-finite cases. Output: " << output << '\n';
        return invalid ? 1 : 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
