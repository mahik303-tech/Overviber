// Reference renderer and bit-exact regression check for the audio engine.
//
//   AudioReferenceRender --out <new dir> [--smoke] [--data <dir>]
//       Renders every case, writes one float32 WAV per case, metrics.csv,
//       hashes.txt and manifest.txt. An existing directory is never touched.
//   AudioReferenceRender --compare <baseline dir> [--smoke] [--data <dir>]
//       Renders the same cases in memory and compares them with hashes.txt of
//       the baseline. Differences report the largest sample deviation.
//       Exit code 0 = identical, 1 = difference, 77 = no baseline (CTest skip).
//   AudioReferenceRender --bench [--data <dir>]
//       Measures render time of fixed six-voice cases (not part of CTest).
//
// Hashes are only comparable on the same compiler, platform and build type.
#include "TestData.h"
#include "dsp/audible/stmlib/utils/random.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
static constexpr int kSkipReturnCode = 77;

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
// Reads the samples of a WAV written by wav() above (fixed 44-byte header).
static std::vector<float> readWav(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    in.seekg(0, std::ios::end);
    const auto size = static_cast<size_t>(in.tellg());
    if (size < 44) return {};
    std::vector<float> audio((size - 44) / sizeof(float));
    in.seekg(44);
    in.read(reinterpret_cast<char*>(audio.data()), static_cast<std::streamsize>(audio.size() * sizeof(float)));
    return audio;
}
// FNV-1a over the IEEE bit patterns: any changed bit changes the hash.
static uint64_t hashAudio(const std::vector<float>& audio) {
    uint64_t hash = 1469598103934665603ull;
    for (float value : audio) {
        uint32_t bits; std::memcpy(&bits, &value, sizeof(bits));
        for (int i = 0; i < 4; ++i) { hash ^= (bits >> (8 * i)) & 255u; hash *= 1099511628211ull; }
    }
    return hash;
}
static std::string hex(uint64_t value) {
    std::ostringstream text; text << std::hex << std::setw(16) << std::setfill('0') << value;
    return text.str();
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

// A timeline event runs before the sample at `frame`. Blocks are split there,
// exactly as the plugin processor splits host blocks at MIDI positions.
struct Event {
    int frame;
    std::function<void(SynthEngine&)> apply;
};
static std::vector<float> renderTimeline(SynthEngine& engine, std::vector<Event> events, int frames,
                                         int blockSize, RenderDiagnostics* diagnostics) {
#ifdef OVERVIBER_DIAGNOSTICS
    engine.setDiagnostics(diagnostics);
#endif
    std::stable_sort(events.begin(), events.end(),
                     [](const Event& a, const Event& b) { return a.frame < b.frame; });
    std::vector<float> result(static_cast<size_t>(frames) * 2);
    std::vector<float> left(static_cast<size_t>(blockSize)), right(static_cast<size_t>(blockSize));
    size_t next = 0;
    for (int offset = 0; offset < frames;) {
        while (next < events.size() && events[next].frame <= offset) events[next++].apply(engine);
        int count = std::min(blockSize, frames - offset);
        if (next < events.size()) count = std::min(count, events[next].frame - offset);
        engine.renderBlock(left.data(), right.data(), count);
        for (int i = 0; i < count; ++i) {
            result[2 * (offset + i)] = left[i]; result[2 * (offset + i) + 1] = right[i];
        }
        offset += count;
    }
#ifdef OVERVIBER_DIAGNOSTICS
    engine.setDiagnostics(nullptr);
#endif
    return result;
}
// Chord held for the first half, released for the second half.
static std::vector<Event> chord(int polyphony, int frames, uint8_t channel = 1) {
    static const uint8_t notes[] = {60, 64, 67, 72, 76, 79};
    std::vector<Event> events;
    for (int v = 0; v < polyphony; ++v) {
        const uint8_t note = notes[v];
        events.push_back({0, [=](SynthEngine& e) { e.noteOn(note, 60000, channel); }});
        events.push_back({frames / 2, [=](SynthEngine& e) { e.noteOff(note, 0, channel); }});
    }
    return events;
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

struct Options {
    fs::path data = OVERVIBER_TEST_DATA_DIR;
    fs::path out, compare;
    bool smoke = false, bench = false;
};
static Options parse(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for " + arg);
            return argv[++i];
        };
        if (arg == "--data") options.data = value();
        else if (arg == "--out") options.out = value();
        else if (arg == "--compare") options.compare = value();
        else if (arg == "--smoke") options.smoke = true;
        else if (arg == "--bench") options.bench = true;
        else throw std::runtime_error("Unknown argument: " + arg);
    }
    if (options.bench + !options.out.empty() + !options.compare.empty() != 1)
        throw std::runtime_error("Use exactly one of --out <dir>, --compare <dir> or --bench");
    return options;
}

class Renderer {
public:
    explicit Renderer(const Options& o) : options(o) {}

    std::unique_ptr<SynthEngine> makeEngine(int rate) const {
        stmlib::Random::Seed(33); std::srand(33);
        auto engine = std::make_unique<SynthEngine>();
        engine->getWaveManager().setBaseDirectory((options.data / "WAVEDATA").string());
        engine->getPresetManager().setBaseDirectory((options.data / "PRESETS").string());
        engine->prepare(static_cast<float>(rate));
        return engine;
    }

    // Calls `emit` once per case with its name, rate and rendered audio.
    using Sink = std::function<void(const std::string&, int, const std::vector<float>&, const RenderDiagnostics&)>;
    void run(const Sink& emit) const {
        const bool smoke = options.smoke;
        auto one = [&](const std::string& name, int rate, SynthEngine& engine,
                       std::vector<Event> events, int frames, int block = 64) {
            RenderDiagnostics d;
            emit(name, rate, renderTimeline(engine, std::move(events), frames, block, &d), d);
        };

        // Factory snapshots. Preset renders force main-preset routing.
        auto catalog = makeEngine(48000);
        const int presets = smoke ? std::min(1, catalog->getPresetManager().getPresetCount())
                                  : catalog->getPresetManager().getPresetCount();
        if (presets == 0) throw std::runtime_error("No presets");
        for (int p = 0; p < presets; ++p) for (int voices : {1, 6}) {
            auto engine = makeEngine(48000);
            engine->loadPreset(p); engine->setSteppedParam(spEngineMode, emMultiChannel);
            const std::string name = "preset_" + std::to_string(engine->getPresetManager().getPresetNumber(p))
                + "_48000_v" + std::to_string(voices);
            one(name, 48000, *engine, chord(voices, 24000), 24000);
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
            one(name, rate, *engine, chord(voices, rate / 4), rate / 4);
        }

        scenarios(one);
    }

private:
    template <typename One>
    void scenarios(One& one) const {
        const int rate = 48000, frames = 36000;
        auto pot = [](int value) { return static_cast<uint16_t>(scan_potTo16bits(value)); };

        // Custom routing: three parts with distinct kit presets on channels 1-3,
        // plus main-part automation while the other parts are sounding.
        {
            auto e = makeEngine(rate);
            e->setCustomRouting(true);
            for (int p = 0; p < 16; ++p) e->getPartRoute(p) = {static_cast<uint8_t>(p < 3), static_cast<uint8_t>(p + 1), 0, 127};
            std::vector<Event> ev;
            const uint8_t notes[] = {48, 60, 67};
            for (uint8_t ch = 1; ch <= 3; ++ch) {
                const uint8_t note = notes[ch - 1];
                ev.push_back({0, [=](SynthEngine& s) { s.noteOn(note, 55000, ch); }});
                ev.push_back({frames / 2 + ch * 101, [=](SynthEngine& s) { s.noteOff(note, 0, ch); }});
            }
            ev.push_back({5003, [=](SynthEngine& s) { s.setContinuousParam(cpCutoff, pot(300)); }});
            ev.push_back({7011, [=](SynthEngine& s) { s.setContinuousParam(cpAmpRel, pot(800)); }});
            ev.push_back({9001, [=](SynthEngine& s) { s.setSteppedParam(spFilterModel, fmSST); }});
            one("scenario_multipart_routing", rate, *e, std::move(ev), frames);
        }
        if (options.smoke) return;

        // AFX kit: sound per key across several slots.
        {
            auto e = makeEngine(rate);
            e->setSteppedParam(spEngineMode, emAFX);
            std::vector<Event> ev;
            const uint8_t notes[] = {4, 20, 36, 50, 70, 90};
            for (int i = 0; i < 6; ++i) {
                const uint8_t note = notes[i];
                ev.push_back({i * 1500, [=](SynthEngine& s) { s.noteOn(note, 50000); }});
                ev.push_back({i * 1500 + 9000, [=](SynthEngine& s) { s.noteOff(note, 0); }});
            }
            one("scenario_afx_kit", rate, *e, std::move(ev), frames);
        }

        // Mono legato with glide and a moving target.
        for (int glide : {300, 700}) {
            auto e = makeEngine(rate);
            e->setSteppedParam(spVoiceCount, 0);
            e->setContinuousParam(cpGlide, pot(glide));
            std::vector<Event> ev;
            ev.push_back({0, [](SynthEngine& s) { s.noteOn(48, 60000); }});
            ev.push_back({6000, [](SynthEngine& s) { s.noteOn(60, 60000); }});
            ev.push_back({8000, [](SynthEngine& s) { s.noteOff(48, 0); }});
            ev.push_back({14000, [](SynthEngine& s) { s.noteOn(55, 60000); }});
            ev.push_back({16000, [](SynthEngine& s) { s.noteOff(60, 0); }});
            ev.push_back({24000, [](SynthEngine& s) { s.noteOff(55, 0); }});
            one("scenario_glide_" + std::to_string(glide), rate, *e, std::move(ev), frames);
        }

        // Parameter automation during held notes, at unaligned positions,
        // including a filter model/mode walk and Shelves band changes.
        for (int block : {64, 37}) {
            auto e = makeEngine(rate);
            auto ev = chord(4, frames);
            int t = 997;
            auto at = [&](std::function<void(SynthEngine&)> f) { ev.push_back({t, std::move(f)}); t += 1231; };
            at([=](SynthEngine& s) { s.setContinuousParam(cpCutoff, pot(250)); });
            at([=](SynthEngine& s) { s.setContinuousParam(cpResonance, pot(700)); });
            at([=](SynthEngine& s) { s.setContinuousParam(cpFilAtt, pot(400)); s.setContinuousParam(cpFilDec, pot(200)); });
            at([=](SynthEngine& s) { s.setContinuousParam(cpAmpSus, pot(300)); });
            at([=](SynthEngine& s) { s.setContinuousParam(cpWModAtt, pot(100)); s.setContinuousParam(cpABaseWMod, pot(600)); });
            at([=](SynthEngine& s) { s.setSteppedParam(spFilterModel, fmLiquid); });
            at([=](SynthEngine& s) { s.setSteppedParam(spFilterMode, 1); });
            at([=](SynthEngine& s) { s.setSteppedParam(spFilterModel, fmEQ); });
            at([=](SynthEngine& s) { s.setContinuousParam(cpShelvesLsGain, pot(800)); s.setContinuousParam(cpShelvesP2Freq, pot(600)); });
            at([=](SynthEngine& s) { s.setContinuousParam(cpCutoff, pot(700)); });
            at([=](SynthEngine& s) { s.setSteppedParam(spFilterModel, fmSST); s.setSteppedParam(spFilterMode, 2); });
            at([=](SynthEngine& s) { s.setSteppedParam(spAmpEnvLin, 1); s.setSteppedParam(spFilEnvSlow, 1); });
            at([=](SynthEngine& s) { s.setContinuousParam(cpDetune, pot(620)); s.setContinuousParam(cpBVol, pot(900)); });
            at([=](SynthEngine& s) { s.setContinuousParam(cpNoiseVol, pot(300)); });
            one("scenario_automation_b" + std::to_string(block), rate, *e, std::move(ev), frames, block);
        }

        // LFOs, modulation matrix and controllers.
        {
            auto e = makeEngine(rate);
            e->setContinuousParam(cpLFOFreq, pot(600)); e->setContinuousParam(cpLFOAmt, pot(999));
            e->setContinuousParam(cpLFOFilAmt, pot(400)); e->setContinuousParam(cpLFOPitchAmt, pot(80));
            e->setSteppedParam(spLFOTargets, otBoth);
            e->setContinuousParam(cpLFO2Freq, pot(300)); e->setContinuousParam(cpLFO2Amt, pot(700));
            e->setContinuousParam(cpLFO2AmpAmt, pot(300));
            e->setMatrixSlot(0, modSrcLFO2, modDestCutoff, modSrcNone, 40);
            e->setMatrixSlot(1, modSrcModWheel, modDestWaveModAll, modSrcNone, 70);
            e->setMatrixSlot(2, modSrcAftertouch, modDestResonance, modSrcNone, 50);
            e->setMatrixSlot(3, modSrcVelocity, modDestAmpLevel, modSrcLFO1, -30);
            e->setMatrixSlot(4, modSrcKeyTrack, modDestDetune, modSrcNone, 25);
            e->setMatrixSlot(5, modSrcFilterEnv, modDestPitchOscB, modSrcNone, 10);
            e->setMatrixSlot(6, modSrcBreath, modDestVolOscA, modSrcExpression, 60);
            e->setMatrixSlot(7, modSrcTimbreSlide, modDestNoiseVol, modSrcNone, 30);
            e->setSteppedParam(spModwheelTarget, modFilter);
            e->setSteppedParam(spPressureTarget, modWaveMod);
            auto ev = chord(3, frames);
            ev.push_back({2000, [](SynthEngine& s) { s.modWheel(40000); }});
            ev.push_back({4000, [](SynthEngine& s) { s.pitchBend(3000); }});
            ev.push_back({6000, [](SynthEngine& s) { s.channelPressure(50000); }});
            ev.push_back({8000, [](SynthEngine& s) { s.polyAftertouch(64, 20000); }});
            ev.push_back({9000, [](SynthEngine& s) { s.breathController(30000); s.expressionController(45000); }});
            ev.push_back({10000, [](SynthEngine& s) { s.timbreSlide(60000); s.controlChange(74, 100); }});
            ev.push_back({12000, [](SynthEngine& s) { s.pitchBend(-6000); }});
            one("scenario_modulation", rate, *e, std::move(ev), frames);
        }

        // MPE lower zone with per-note bend, pressure and timbre.
        {
            auto e = makeEngine(rate);
            e->setSteppedParam(spMPEMode, 1);
            e->setSteppedParam(spPressureTarget, modFilter);
            e->setSteppedParam(spTimbreTarget, modWaveMod);
            std::vector<Event> ev;
            const uint8_t notes[] = {55, 62, 69};
            for (uint8_t i = 0; i < 3; ++i) {
                const uint8_t ch = static_cast<uint8_t>(2 + i), note = notes[i];
                ev.push_back({i * 700, [=](SynthEngine& s) { s.noteOn(note, 50000, ch); }});
                ev.push_back({4000 + i * 900, [=](SynthEngine& s) { s.pitchBend(static_cast<int16_t>(1500 * (i + 1)), ch); }});
                ev.push_back({7000 + i * 500, [=](SynthEngine& s) { s.channelPressure(static_cast<uint16_t>(20000 * (i + 1)), ch); }});
                ev.push_back({9000 + i * 300, [=](SynthEngine& s) { s.timbreSlide(static_cast<uint16_t>(65535 - 15000 * i), ch); }});
                ev.push_back({20000 + i * 1000, [=](SynthEngine& s) { s.noteOff(note, 30000, ch); }});
            }
            one("scenario_mpe", rate, *e, std::move(ev), frames);
        }

        // Internal-clock arpeggiator.
        {
            auto e = makeEngine(rate);
            e->setSteppedParam(spArpSync, 0);
            e->setContinuousParam(cpArpBpm, pot(500));
            e->setSteppedParam(spArpMode, amUpDown);
            e->setSteppedParam(spArpOctaves, 1);
            e->setContinuousParam(cpArpGate, pot(600));
            e->setContinuousParam(cpArpSwing, pot(650));
            one("scenario_arp", rate, *e, chord(3, 72000), 72000);
        }

        // Hybrid oscillator and unison stack.
        {
            auto e = makeEngine(rate);
            e->setSteppedParam(spOscEngine, oeHybrid);
            e->setContinuousParam(cpElementsStrike, pot(700));
            one("scenario_hybrid", rate, *e, chord(3, frames), frames);
        }
        {
            auto e = makeEngine(rate);
            e->setSteppedParam(spUnison, 1);
            e->setContinuousParam(cpUnisonDetune, pot(400));
            std::vector<Event> ev{{0, [](SynthEngine& s) { s.noteOn(57, 60000); }},
                                  {frames / 2, [](SynthEngine& s) { s.noteOff(57, 0); }}};
            one("scenario_unison", rate, *e, std::move(ev), frames);
        }

        // Master bus fully engaged: resonance, console drive and Mackity send.
        {
            auto e = makeEngine(rate);
            e->setContinuousParam(cpResonance, pot(800));
            e->setContinuousParam(cpConsoleDrive, pot(600));
            e->setContinuousParam(cpMackitySend, pot(700));
            e->setContinuousParam(cpMackityDrive, pot(600));
            one("scenario_master_bus", rate, *e, chord(6, frames), frames);
        }

        // A prepared state (as sent by the editor) arriving while notes sound:
        // changed part controls reconfigure the affected voices.
        {
            auto e = makeEngine(rate);
            e->setCustomRouting(true);
            for (int p = 0; p < 16; ++p) e->getPartRoute(p) = {static_cast<uint8_t>(p < 2), static_cast<uint8_t>(p + 1), 0, 127};
            auto editor = makeEngine(rate);
            editor->setCustomRouting(true);
            for (int p = 0; p < 16; ++p) editor->getPartRoute(p) = e->getPartRoute(p);
            editor->getAfxKit().getSlot(1).preset.continuousParams[cpCutoff] = pot(200);
            editor->getAfxKit().getSlot(1).preset.continuousParams[cpAmpRel] = pot(700);
            editor->getAfxKit().getSlot(1).preset.continuousParams[cpShelvesLsGain] = pot(900);
            editor->setContinuousParam(cpFilDec, pot(700));
            auto state = std::make_shared<PreparedState>();
            editor->capturePreparedState(*state);
            state->panicGeneration = 0;
            std::vector<Event> ev{
                {0, [](SynthEngine& s) { s.noteOn(50, 60000, 1); s.noteOn(62, 60000, 2); }},
                {8117, [state](SynthEngine& s) { s.applyPreparedState(*state); }},
                {20000, [](SynthEngine& s) { s.noteOff(50, 0, 1); s.noteOff(62, 0, 2); }}};
            one("scenario_prepared_state", rate, *e, std::move(ev), frames);
        }
    }

    const Options& options;
};

static std::map<std::string, std::string> readHashes(const fs::path& file) {
    std::map<std::string, std::string> result;
    std::ifstream in(file);
    std::string name, hash, frames;
    while (in >> name >> hash >> frames) result[name] = hash;
    return result;
}

static int writeBaseline(const Options& options, const Renderer& renderer) {
    if (fs::exists(options.out)) throw std::runtime_error("Output exists; choose a new directory to preserve the baseline");
    fs::create_directories(options.out);
    std::ofstream csv(options.out / "metrics.csv"), hashes(options.out / "hashes.txt");
    csv << "case,stage,peak,rms,samples,non_finite,over_unity\n";
    int cases = 0, invalid = 0;
    renderer.run([&](const std::string& name, int rate, const std::vector<float>& audio, const RenderDiagnostics& d) {
        report(csv, name, d);
        wav(options.out / (name + ".wav"), audio, rate);
        hashes << name << ' ' << hex(hashAudio(audio)) << ' ' << audio.size() / 2 << '\n';
        if (d.outputLeft.nonFinite || d.outputRight.nonFinite) ++invalid;
        if (++cases % 32 == 0) std::cout << cases << " cases rendered\n";
    });
    std::ofstream manifest(options.out / "manifest.txt");
    manifest << "Overviber reference format 2\nIEEE float32 stereo WAV; no normalization\n"
             << "Seed=33 per case; block=64 unless named; velocity=60000; notes=60,64,67,72,76,79\n"
             << "Data=" << fs::absolute(options.data).string() << "\n"
             << "Mode=" << (options.smoke ? "smoke" : "full") << "\n"
             << "Preset renders force main-preset routing to measure the loaded preset.\n"
             << "Voice statistics aggregate active voice samples; output statistics include silence.\n"
             << "Cases=" << cases << "\nNonFiniteCases=" << invalid << "\n";
    if (!csv || !hashes || !manifest) throw std::runtime_error("Cannot write report");
    std::cout << cases << " cases, " << invalid << " non-finite cases. Output: " << options.out << '\n';
    return invalid ? 1 : 0;
}

static int compareBaseline(const Options& options, const Renderer& renderer) {
    const auto reference = readHashes(options.compare / "hashes.txt");
    if (reference.empty()) {
        std::cout << "No baseline at " << options.compare << " - skipped. Create one with --out.\n";
        return kSkipReturnCode;
    }
    int cases = 0, differing = 0, missing = 0;
    std::map<std::string, bool> seen;
    renderer.run([&](const std::string& name, int, const std::vector<float>& audio, const RenderDiagnostics&) {
        ++cases; seen[name] = true;
        const auto found = reference.find(name);
        if (found == reference.end()) { ++missing; std::cout << "NEW      " << name << '\n'; return; }
        if (found->second == hex(hashAudio(audio))) return;
        ++differing;
        const auto old = readWav(options.compare / (name + ".wav"));
        float maxDiff = 0.0f; long first = -1;
        for (size_t i = 0; i < std::min(old.size(), audio.size()); ++i) {
            const float diff = std::abs(old[i] - audio[i]);
            if (diff > 0.0f && first < 0) first = static_cast<long>(i / 2);
            maxDiff = std::max(maxDiff, diff);
        }
        std::cout << "DIFFERS  " << name << "  max |diff| " << std::setprecision(6) << maxDiff
                  << "  first frame " << first << (old.size() != audio.size() ? "  (length differs)" : "") << '\n';
    });
    // Only full baselines are checked for removed cases; a smoke run is a subset.
    int removed = 0;
    if (!options.smoke)
        for (const auto& entry : reference)
            if (!seen.count(entry.first)) { ++removed; std::cout << "REMOVED  " << entry.first << '\n'; }
    std::cout << cases << " cases compared: " << cases - differing - missing << " identical, "
              << differing << " differ, " << missing << " new, " << removed << " removed\n";
    return differing || missing || removed ? 1 : 0;
}

static int bench(const Renderer& renderer) {
    const int rate = 44100, frames = rate * 10;
    const char* filters[] = {"SSI2144", "Liquid", "Shelves", "SST"};
    const char* engines[] = {"Wavetable", "Elements", "Hybrid"};
    std::cout << "Six voices, " << rate << " Hz, block 512, 10 s audio per case\n";
    for (int oscEngine = 0; oscEngine < 3; ++oscEngine) for (int filter = 0; filter < 4; ++filter) {
        double best = 1e30;
        for (int repeat = 0; repeat < 3; ++repeat) {
            auto e = renderer.makeEngine(rate);
            e->setSteppedParam(spOscEngine, static_cast<uint8_t>(oscEngine));
            e->setSteppedParam(spFilterModel, static_cast<uint8_t>(filter));
            e->setContinuousParam(cpAmpSus, 65535);
            const auto start = std::chrono::steady_clock::now();
            renderTimeline(*e, chord(6, frames * 2), frames, 512, nullptr);
            best = std::min(best, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        }
        std::cout << std::left << std::setw(10) << engines[oscEngine] << std::setw(9) << filters[filter]
                  << std::right << std::fixed << std::setprecision(1) << std::setw(9) << best << " ms  "
                  << std::setw(7) << (10000.0 / best) << "x realtime\n";
    }
    return 0;
}

int main(int argc, char* argv[]) {
    try {
        const Options options = parse(argc, argv);
        if (!fs::is_directory(options.data / "PRESETS") || !fs::is_directory(options.data / "WAVEDATA"))
            throw std::runtime_error("Missing data directory");
        const Renderer renderer(options);
        if (options.bench) return bench(renderer);
        if (!options.compare.empty()) return compareBaseline(options, renderer);
        return writeBaseline(options, renderer);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
