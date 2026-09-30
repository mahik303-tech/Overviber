// ==============================================================================
// TimingScenarioTest - timing of notes, arp, glide and controllers
// ==============================================================================
// Plays the engine as the plugin does (PluginProcessor::processBlock): the
// host transport at each block start, MIDI events splitting the block at
// their sample offsets. Measures
//
//   onset     samples from a note-on to the amp's rise (VCA gain), for notes
//             landing on every phase of the 4 kHz control grid: the latency
//             and its spread (jitter)
//   arp       generated note-ons against the host's grid (ppq x sample rate)
//             for several block sizes: missing, doubled and misplaced steps
//             (events land on their nearest sample),
//             the first step at transport start; a looping host; a stop and
//             a restart elsewhere; keys played while the host is stopped
//   swing     off-beat position within a pair of steps, as a fraction
//   gate      note length against step length x gate
//   clock     the internal clock's position after two minutes
//   glide     pitch steps of a legato octave glide, in cents
//   wheel     largest jump of the mod wheel as the matrix reads it, for a
//             7-bit CC sweep
//
// Before the timing refactoring (see REFACTORING.md) the same program found:
// onset jitter of one control period (0.23 ms), a host-synced arp that at
// 120 BPM stayed silent with blocks of 100, 250, 500 or 1000 samples and lost
// 15 of 64 steps with 64-sample blocks, no step at transport start, swing of
// at most 58 % (set: 75 %), gates in whole ticks (10 % played as 17 %), the
// internal clock 14 samples off after two minutes, glide in 12.5-cent steps
// and mod-wheel jumps of 1/127.
// ==============================================================================
#include "dsp/SynthEngine.h"
#include "dsp/MidiDispatcher.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name.c_str());
    failures += !ok;
}

struct Event {
    int64_t sample;
    std::array<uint8_t, 3> bytes;
};

Event noteOn(int64_t sample, uint8_t note, uint8_t velocity = 100) { return { sample, { 0x90, note, velocity } }; }
Event noteOff(int64_t sample, uint8_t note) { return { sample, { 0x80, note, 0 } }; }
Event cc(int64_t sample, uint8_t controller, uint8_t value) { return { sample, { 0xB0, controller, value } }; }

struct Host {
    double sampleRate = 48000.0;
    double bpm = 120.0;
    bool transport = true;     // a host transport, playing from ppq 0
    int64_t startAt = 0;       // stopped at ppq 0 until this sample (a block start)
    double loopQuarters = 0.0; // > 0: the host loops from ppq 0 over this length
    int block = 256;
    double ppqAt(int64_t sample) const {
        if (sample < startAt) return 0.0;
        const double ppq = static_cast<double>(sample - startAt) / sampleRate * bpm / 60.0;
        return loopQuarters > 0.0 ? std::fmod(ppq, loopQuarters) : ppq;
    }
};

struct Played {
    std::vector<int64_t> noteOns, noteOffs;   // absolute samples of the generated MIDI
};

// Plays `total` samples like PluginProcessor::processBlock. `perSample`
// (optional) runs after every sample, which then renders one at a time.
template <typename PerSample>
Played play(SynthEngine& engine, const Host& host, int64_t total, std::vector<Event> events, PerSample&& perSample,
            bool sampleWise) {
    std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.sample < b.sample; });
    Played played;
    std::vector<float> left(static_cast<size_t>(host.block)), right(static_cast<size_t>(host.block));
    size_t next = 0;
    for (int64_t pos = 0; pos < total; pos += host.block) {
        const int n = static_cast<int>(std::min<int64_t>(host.block, total - pos));
        engine.setEventOffset(0);
        engine.clearPendingMidiOut();
        if (host.transport) {
            engine.setHostBpm(static_cast<float>(host.bpm));
            engine.setHostTransport(host.ppqAt(pos), pos >= host.startAt);
        }
        int cursor = 0;
        auto renderTo = [&](int end) {
            if (!sampleWise) {
                if (end > cursor) engine.renderBlock(left.data() + cursor, right.data() + cursor, end - cursor, cursor);
                cursor = end;
                return;
            }
            for (; cursor < end; ++cursor) {
                engine.renderBlock(left.data() + cursor, right.data() + cursor, 1, cursor);
                perSample(pos + cursor);
            }
        };
        while (next < events.size() && events[next].sample < pos + n) {
            const int at = static_cast<int>(std::max<int64_t>(0, events[next].sample - pos));
            renderTo(at);
            engine.setEventOffset(at);
            mididispatch::dispatch(engine, events[next].bytes.data(), 3);
            ++next;
        }
        renderTo(n);
        for (const auto& ev : engine.getPendingMidiOut())
            (ev.isNoteOn ? played.noteOns : played.noteOffs).push_back(pos + ev.sampleOffset);
    }
    return played;
}

Played play(SynthEngine& engine, const Host& host, int64_t total, std::vector<Event> events) {
    return play(engine, host, total, std::move(events), [](int64_t) {}, false);
}

std::unique_ptr<SynthEngine> makeEngine(float sampleRate) {
    auto engine = std::make_unique<SynthEngine>();
    engine->prepare(sampleRate);
    return engine;
}

// ---- Onset --------------------------------------------------------------
struct Onset {
    int minStart = 1 << 30, maxStart = 0;   // first sample with VCA gain above -60 dB
    int minHalf = 1 << 30, maxHalf = 0;     // first sample at half the settled gain
};

Onset measureOnset(float sampleRate, int trials) {
    auto engine = makeEngine(sampleRate);
    engine->setContinuousParam(cpAmpAtt, 0);
    engine->setContinuousParam(cpAmpSus, UINT16_MAX);
    engine->setContinuousParam(cpAmpRel, 0);
    Host host;
    host.sampleRate = sampleRate;
    host.transport = false;
    host.block = 64;
    Onset result;
    const int64_t window = static_cast<int64_t>(sampleRate * 0.004);   // 4 ms after the note
    for (int trial = 0; trial < trials; ++trial) {
        // An idle stretch whose length is no multiple of the control period
        // puts each note on another phase of the control grid.
        const int64_t start = 1000 + trial * 7;
        std::vector<float> gain;
        play(*engine, host, start + window, { noteOn(start, 60) }, [&](int64_t sample) {
            if (sample < start) return;
            float g = 0.0f;
            for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) g = std::max(g, engine->getVoice(v).getVca().getCurrentGain());
            gain.push_back(g);
        }, true);
        const float settled = gain.back();
        int first = -1, half = -1;
        for (int i = 0; i < static_cast<int>(gain.size()); ++i) {
            if (first < 0 && gain[i] > 0.001f) first = i;
            if (half < 0 && gain[i] >= 0.5f * settled) half = i;
        }
        result.minStart = std::min(result.minStart, first);
        result.maxStart = std::max(result.maxStart, first);
        result.minHalf = std::min(result.minHalf, half);
        result.maxHalf = std::max(result.maxHalf, half);
        engine->panic();
    }
    return result;
}

// ---- Arp ----------------------------------------------------------------
struct ArpTiming {
    int notes = 0;
    int firstStep = -1;          // grid step of the first note
    int missing = 0, doubled = 0;
    double maxError = 0.0;       // samples off the grid
};

// Steps of `stepQuarter` quarter notes; odd steps late by `swing` (0.5: none).
ArpTiming arpAgainstGrid(const std::vector<int64_t>& noteOns, double samplesPerStep, double swing, int steps) {
    ArpTiming t;
    std::vector<int> count(static_cast<size_t>(steps), 0);
    const double pair = 2.0 * samplesPerStep;
    for (const int64_t at : noteOns) {
        const int k = static_cast<int>(std::floor(at / pair + 0.25)) * 2;   // the pair the note belongs to
        double best = 1e30;
        int bestStep = -1;
        for (int s = std::max(0, k - 2); s <= k + 3; ++s) {
            const double expected = (s / 2) * pair + (s % 2) * swing * pair;
            if (std::abs(at - expected) < std::abs(best)) { best = at - expected; bestStep = s; }
        }
        if (bestStep < 0 || bestStep >= steps) continue;
        ++t.notes;
        if (t.firstStep < 0) t.firstStep = bestStep;
        ++count[static_cast<size_t>(bestStep)];
        t.maxError = std::max(t.maxError, std::abs(best));
    }
    for (int s = std::max(0, t.firstStep); s < steps; ++s) {
        if (count[static_cast<size_t>(s)] == 0) ++t.missing;
        if (count[static_cast<size_t>(s)] > 1) t.doubled += count[static_cast<size_t>(s)] - 1;
    }
    return t;
}

void setArp(SynthEngine& engine, arpMode_t mode, uint8_t rate, int gatePot, int swingPot, bool sync) {
    engine.setSteppedParam(spArpMode, static_cast<uint8_t>(mode));
    engine.setSteppedParam(spArpRate, rate);
    engine.setSteppedParam(spArpSync, sync ? 1 : 0);
    engine.setContinuousParam(cpArpGate, static_cast<uint16_t>(scan_potTo16bits(gatePot)));
    engine.setContinuousParam(cpArpSwing, static_cast<uint16_t>(scan_potTo16bits(swingPot)));
}

ArpTiming hostArp(double sampleRate, double bpm, int block, int steps, double swingPot = 500) {
    auto engine = makeEngine(static_cast<float>(sampleRate));
    setArp(*engine, amUp, 3, 500, static_cast<int>(swingPot), true);
    Host host;
    host.sampleRate = sampleRate;
    host.bpm = bpm;
    host.block = block;
    const double samplesPerStep = 60.0 / bpm * sampleRate / 4.0;
    const auto total = static_cast<int64_t>(samplesPerStep * steps);
    const auto played = play(*engine, host, total, { noteOn(0, 60), noteOn(0, 64), noteOn(0, 67) });
    const double swing = arpSwingFraction(static_cast<uint16_t>(scan_potTo16bits(static_cast<int>(swingPot))));
    return arpAgainstGrid(played.noteOns, samplesPerStep, swing, steps);
}

bool arpOnGrid(const ArpTiming& t, int steps) {
    return t.notes == steps && t.firstStep == 0 && t.missing == 0 && t.doubled == 0 && t.maxError <= 0.5 + 1e-6;
}

} // namespace

int main() {
    // ---- Onset
    for (const float rate : { 44100.0f, 48000.0f, 96000.0f }) {
        const auto onset = measureOnset(rate, 24);
        std::printf("onset %6.0f Hz: start %d..%d samples, half gain %d..%d (jitter %d samples = %.3f ms)\n",
                    rate, onset.minStart, onset.maxStart, onset.minHalf, onset.maxHalf,
                    onset.maxHalf - onset.minHalf, (onset.maxHalf - onset.minHalf) * 1000.0 / rate);
        // The first envelope step holds 0 for one control period (as the
        // firmware's DAC), then the amp rises: the same for every note.
        const int period = static_cast<int>(std::ceil(rate * 2.0f / DACSPI_UPDATE_HZ));
        check(onset.minStart == onset.maxStart && onset.minHalf == onset.maxHalf,
              "note start without jitter at " + std::to_string(static_cast<int>(rate)) + " Hz");
        check(onset.maxStart <= period / 2 + 2, "amp rises one control period after the note");
    }

    // ---- Arp against the host grid
    for (const double bpm : { 120.0, 97.3 }) {
        for (const int block : { 64, 100, 250, 441, 500, 512, 1000 }) {
            const auto t = hostArp(48000.0, bpm, block, 64);
            std::printf("arp host %5.1f bpm, block %4d: %2d notes, first step %d, missing %d, doubled %d, max error %.2f samples\n",
                        bpm, block, t.notes, t.firstStep, t.missing, t.doubled, t.maxError);
            check(arpOnGrid(t, 64), "host-synced arp on the grid, block " + std::to_string(block));
        }
    }

    // ---- A host looping one bar (192 blocks of 500 samples at 120 BPM):
    // every loop plays its 16 steps from the loop start, no note hangs over
    // the jump.
    {
        const auto engine = makeEngine(48000.0f);
        setArp(*engine, amUp, 3, 500, 500, true);
        Host host;
        host.block = 500;
        host.loopQuarters = 4.0;
        const auto played = play(*engine, host, 96000 * 3, { noteOn(0, 60), noteOn(0, 64), noteOn(0, 67) });
        std::vector<int64_t> inLoop;
        for (const int64_t at : played.noteOns) inLoop.push_back(at % 96000);
        const auto t = arpAgainstGrid(inLoop, 6000.0, 0.5, 16);
        bool closed = played.noteOffs.size() + 1 >= played.noteOns.size();
        for (size_t i = 0; i < played.noteOns.size() && i < played.noteOffs.size(); ++i)
            closed &= played.noteOffs[i] - played.noteOns[i] == 3003;   // gate 500/999 of 6000: 3003.003
        std::printf("arp host loop: %zu notes in 3 loops, missing %d, max error %.2f samples\n",
                    played.noteOns.size(), t.missing, t.maxError);
        check(played.noteOns.size() == 48 && t.firstStep == 0 && t.missing == 0 && t.maxError <= 0.5 + 1e-6,
              "looping host: 16 steps per loop");
        check(closed, "looping host: every note ends after its gate");
    }

    // ---- Stop for a while, then start at ppq 2 (a locate): the stop ends
    // the sounding note at once, the held keys play on at the host tempo
    // (the step at sample 6000), the step on the start position plays on
    // the first sample.
    {
        const auto engine = makeEngine(48000.0f);
        setArp(*engine, amUp, 3, 900, 500, true);
        std::vector<float> left(512), right(512);
        const uint8_t on[3][3] = { { 0x90, 60, 100 }, { 0x90, 64, 100 }, { 0x90, 67, 100 } };
        std::vector<std::pair<int64_t, bool>> events;
        for (int block = 0; block < 60; ++block) {
            // Stopped, a host reports the stop position.
            const bool playing = block < 10 || block >= 20;
            const double ppq = block < 20 ? std::min(block, 10) * 512 / 24000.0 : 2.0 + (block - 20) * 512 / 24000.0;
            engine->setEventOffset(0);
            engine->clearPendingMidiOut();
            engine->setHostBpm(120.0f);
            engine->setHostTransport(ppq, playing);
            if (block == 0) for (const auto& m : on) mididispatch::dispatch(*engine, m, 3);
            engine->renderBlock(left.data(), right.data(), 512, 0);
            for (const auto& ev : engine->getPendingMidiOut()) events.push_back({ block * 512 + ev.sampleOffset, ev.isNoteOn });
        }
        std::vector<int64_t> onsWhileStopped, offsWhileStopped;
        int64_t firstAfterRestart = -1;
        for (const auto& [at, isOn] : events) {
            if (at >= 10 * 512 && at < 20 * 512) (isOn ? onsWhileStopped : offsWhileStopped).push_back(at);
            if (isOn && at >= 20 * 512 && firstAfterRestart < 0) firstAfterRestart = at;
        }
        std::printf("arp stop/start: %zu note-offs and %zu note-ons while stopped (first off at %lld, first on at %lld), "
                    "first note after the restart at +%lld samples\n",
                    offsWhileStopped.size(), onsWhileStopped.size(),
                    static_cast<long long>(offsWhileStopped.empty() ? -1 : offsWhileStopped[0]),
                    static_cast<long long>(onsWhileStopped.empty() ? -1 : onsWhileStopped[0]),
                    static_cast<long long>(firstAfterRestart - 20 * 512));
        check(offsWhileStopped.size() == 1 && offsWhileStopped[0] == 10 * 512, "transport stop ends the step at once");
        check(onsWhileStopped.size() == 1 && onsWhileStopped[0] == 6000,
              "held keys play on at the host tempo while stopped");
        check(firstAfterRestart == 20 * 512, "restart at ppq 2 plays its step on the first sample");
    }

    // ---- Host stopped from the start (90 BPM: 8000 samples per 1/16): a key
    // starts the arp at once and it runs at the host tempo; the transport
    // start locks it to the song, the step at ppq 0 on the start's sample.
    {
        const auto engine = makeEngine(48000.0f);
        setArp(*engine, amUp, 3, 500, 500, true);
        Host host;
        host.bpm = 90.0;
        host.block = 500;
        host.startAt = 40000;
        const auto played = play(*engine, host, 64000, { noteOn(1234, 60), noteOn(1234, 64) });
        std::vector<int64_t> stopped, started;
        for (const int64_t at : played.noteOns) (at < host.startAt ? stopped : started).push_back(at);
        std::string list;
        for (const int64_t at : played.noteOns) list += " " + std::to_string(at);
        std::printf("arp keys while the host is stopped, start at %lld: note-ons at%s\n",
                    static_cast<long long>(host.startAt), list.c_str());
        check(stopped == std::vector<int64_t>{ 1234, 9234, 17234, 25234, 33234 },
              "host stopped: a key starts the arp at once, steps at the host tempo");
        check(started == std::vector<int64_t>{ 40000, 48000, 56000 }, "host start: the arp locks to the song grid");
    }

    // ---- Swing: off-beat position within the pair
    for (const int pot : { 500, 580, 660, 750 }) {
        const auto engine = makeEngine(48000.0f);
        setArp(*engine, amUp, 3, 300, pot, true);
        Host host;
        const auto played = play(*engine, host, 48000 * 4, { noteOn(0, 60), noteOn(0, 64) });
        const double pair = 2.0 * 6000.0;
        double sum = 0.0;
        int n = 0;
        for (const int64_t at : played.noteOns) {
            const double phase = std::fmod(static_cast<double>(at), pair) / pair;
            if (phase > 0.25) { sum += phase; ++n; }
        }
        const double swing = arpSwingFraction(static_cast<uint16_t>(scan_potTo16bits(pot)));
        std::printf("swing %3.0f %% set: off-beat at %.3f of the pair (%d notes)\n", swing * 100.0, n ? sum / n : 0.0, n);
        check(n > 0 && std::abs(sum / n - swing) < 0.001, "swing " + std::to_string(pot / 10) + " %");
    }

    // ---- Gate length at 1/32 (3000 samples at 120 BPM)
    for (const int pot : { 100, 300, 500, 833 }) {
        const auto engine = makeEngine(48000.0f);
        setArp(*engine, amUp, 5, pot, 500, true);
        Host host;
        const auto played = play(*engine, host, 48000 * 2, { noteOn(0, 60), noteOn(0, 64) });
        double sum = 0.0;
        int n = 0;
        for (const int64_t on : played.noteOns) {
            for (const int64_t off : played.noteOffs)
                if (off > on) { sum += static_cast<double>(off - on); ++n; break; }
        }
        const double gate = arpGateFraction(static_cast<uint16_t>(scan_potTo16bits(pot)));
        std::printf("gate %3.0f %% set: notes %.0f samples, expected %.0f\n", gate * 100.0, n ? sum / n : 0.0, 3000.0 * gate);
        check(n > 0 && std::abs(sum / n - 3000.0 * gate) < 1.5, "gate " + std::to_string(pot / 10) + " % of a 1/32 step");
    }

    // ---- Internal clock over two minutes at 44.1 kHz
    {
        const auto engine = makeEngine(44100.0f);
        setArp(*engine, amUp, 3, 500, 500, false);
        engine->setContinuousParam(cpArpBpm, static_cast<uint16_t>(scan_potTo16bits(357)));
        const double bpm = arpInternalBpm(static_cast<uint16_t>(scan_potTo16bits(357)));
        Host host;
        host.sampleRate = 44100.0;
        host.transport = false;
        host.block = 512;
        const int64_t total = 44100 * 120;
        const auto played = play(*engine, host, total, { noteOn(0, 60), noteOn(0, 64) });
        const double samplesPerStep = 60.0 / bpm * 44100.0 / 4.0;
        const int64_t last = played.noteOns.empty() ? 0 : played.noteOns.back();
        const double steps = std::round((last - played.noteOns.front()) / samplesPerStep);
        const double drift = (last - played.noteOns.front()) - steps * samplesPerStep;
        std::printf("internal clock %.2f bpm: %zu notes, last note %.2f samples off the grid of the first\n", bpm,
                    played.noteOns.size(), drift);
        check(!played.noteOns.empty() && played.noteOns.front() == 0, "internal clock: the first key plays at once");
        check(std::abs(drift) < 1.0, "internal clock without drift over two minutes");
    }

    // ---- Glide: pitch steps of a legato octave glide
    {
        const auto engine = makeEngine(48000.0f);
        engine->setSteppedParam(spVoiceCount, 0);
        engine->setContinuousParam(cpGlide, static_cast<uint16_t>(scan_potTo16bits(700)));
        Host host;
        host.transport = false;
        std::vector<uint16_t> cv;
        play(*engine, host, 48000, { noteOn(0, 48), noteOn(4800, 60) }, [&](int64_t sample) {
            if (sample >= 4800) cv.push_back(engine->getOscANoteCV(0));
        }, true);
        int changes = 0, maxStep = 0;
        for (size_t i = 1; i < cv.size(); ++i) {
            const int step = std::abs(static_cast<int>(cv[i]) - static_cast<int>(cv[i - 1]));
            changes += step != 0;
            maxStep = std::max(maxStep, step);
        }
        std::printf("glide octave: %d pitch steps, largest %.1f cents\n", changes, maxStep * 100.0 / WTOSC_CV_SEMITONE);
        check(maxStep * 100.0 / WTOSC_CV_SEMITONE < 2.0, "glide in steps below 2 cents");
    }

    // ---- Mod wheel: 7-bit CC sweep over half a second
    {
        const auto engine = makeEngine(48000.0f);
        Host host;
        host.transport = false;
        std::vector<Event> events{ noteOn(0, 60) };
        for (int value = 0; value <= 127; ++value) events.push_back(cc(1000 + value * 189, 1, static_cast<uint8_t>(value)));
        float previous = 0.0f, maxJump = 0.0f;
        play(*engine, host, 30000, events, [&](int64_t) {
            const float wheel = engine->evaluateModSource(0, modSrcModWheel);
            maxJump = std::max(maxJump, std::abs(wheel - previous));
            previous = wheel;
        }, true);
        std::printf("mod wheel sweep: largest jump per sample %.4f of full scale\n", maxJump);
        check(maxJump < 0.001f, "mod wheel CC steps smoothed");
    }

    std::printf("%s\n", failures ? "TimingScenarioTest FAILED" : "TimingScenarioTest passed");
    return failures ? 1 : 0;
}
