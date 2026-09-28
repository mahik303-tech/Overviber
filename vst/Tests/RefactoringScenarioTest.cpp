#include "data/SessionState.h"
#include "dsp/audible/stmlib/utils/random.h"
#include <iostream>
#include <cstdlib>
#include <new>

static bool trackAllocations = false;
static size_t allocations = 0;
void* operator new(std::size_t count) {
    if (trackAllocations) ++allocations;
    if (auto* memory = std::malloc(count ? count : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t count) { return ::operator new(count); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* name) { std::cout << (ok ? "PASS " : "FAIL ") << name << '\n'; failures += !ok; };
    auto source = std::make_unique<SynthEngine>(); source->prepare(48000);
    source->setCustomRouting(true);
    source->getPartRoute(3) = {1, 5, 30, 72};
    source->getAfxKit().getSlot(3).name = "Custom bell";
    source->getAfxKit().getSlot(3).preset.continuousParams[cpCutoff] = 12345;
    source->getAfxKit().getSlot(3).waveManager.getMutableWaveData(abxAMain)[100] = 54321;
    source->getAfxKit().setNoteMapping(40, 3);
    source->setVoiceFader(2, 0.37f);
    source->setVoicePan(2, -0.42f);
    source->getArpeggiator().setStepPattern(5, 3);
    source->getArpeggiator().setStepDegree(5, 9);
    auto restored = std::make_unique<SynthEngine>(); restored->prepare(48000);
    const auto text = SessionState::encode(*source);
    check(SessionState::decode(text, *restored), "versioned session decodes");
    check(restored->usesCustomRouting() && restored->getPartRoute(3).channel == 5, "part routing restored");
    check(restored->getAfxKit().getSlot(3).waveManager.getWaveData(abxAMain)[100] == 54321, "edited wave embedded");
    check(restored->getAfxKit().getSlot(3).preset.continuousParams[cpCutoff] == 12345, "part parameter restored");
    check(restored->getAfxKit().getSlotForNote(40) == 3 && restored->getVoiceFader(2) == 0.37f
        && restored->getVoicePan(2) == -0.42f, "mapping, faders and pans restored");
    check(restored->getArpeggiator().getStepPattern(5) == 3 && restored->getArpeggiator().getStepDegree(5) == 9, "sequencer restored");
    check(!SessionState::decode(text.replace("\"version\": 1", "\"version\": 99"), *restored), "unknown schema rejected");
    check(!SessionState::decode("voicePattern0 = invalid", *restored), "malformed numeric preset rejected without throwing");

    {
        // Single engine mode becomes Multi-Channel; the Mackity send is off
        // and unpadded unless a preset sets it.
        PresetManager presets;
        PresetData legacy;
        const bool parsed = presets.parsePresetString(
            "presetName = Legacy\nspEngineMode = 2\ncpMackityInTrim = 400\nmatrixSlot2_dest = ElementsSpace\n", legacy);
        check(parsed && legacy.steppedParams[spEngineMode] == emMultiChannel
                  && legacy.continuousParams[cpMackitySend] == 0
                  && legacy.steppedParams[spMackityReturnPad] == 0
                  && scan_potFrom16bits(legacy.continuousParams[cpConsoleDrive]) == 400,
              "preset: Single -> Multi-Channel, Mackity send off");
        check(legacy.modMatrix[2].dest == modDestElementsSpace, "matrix destination parsed by name");
        const auto text = presets.serializePresetToString(legacy);
        check(text.find("spConsoleModel") == std::string::npos
                  && text.find("cpMackitySend = 0") != std::string::npos
                  && text.find("cpMackityDrive = 300") != std::string::npos
                  && text.find("spMackityReturnPad = 0") != std::string::npos
                  && text.find("matrixSlot2_dest = ElementsSpace") != std::string::npos,
              "serialized preset: Mackity send/drive/pad, destinations by name");
    }

    auto prepared = std::make_unique<PreparedState>(); source->capturePreparedState(*prepared);
    auto audio = std::make_unique<SynthEngine>(); audio->prepare(48000);
    float left[128]{}, right[128]{};
    trackAllocations = true;
    audio->applyPreparedState(*prepared);
    for (int mode = 0; mode <= 9; ++mode) {
        audio->setSteppedParam(spArpMode, static_cast<uint8_t>(mode));
        for (int n = 48; n < 60; ++n) audio->noteOn(static_cast<uint8_t>(n), 50000, 1);
        for (int b = 0; b < 200; ++b) audio->renderBlock(left, right, 128);
        audio->allNotesOff(); audio->clearPendingMidiOut();
    }
    trackAllocations = false;
    check(allocations == 0, "prepared state / all arp modes allocate no heap memory");
    audio->setContinuousParam(cpCutoff, 54321);
    audio->applyPreparedState(*prepared, true);
    check(audio->getCurrentPreset().continuousParams[cpCutoff] == 54321, "queued editor snapshot cannot roll back current host parameters");

    // Preset browsing must retire voices before replacing their waveform and
    // filter data.  Otherwise a release tail from one preset continues with
    // the next preset's DSP state and produces clicks/noise on the next notes.
    auto presetSource = std::make_unique<SynthEngine>(); presetSource->prepare(48000);
    presetSource->getPresetManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/PRESETS");
    presetSource->getWaveManager().setBaseDirectory(std::string(OVERVIBER_TEST_DATA_DIR) + "/WAVEDATA");
    int preset17 = -1, preset18 = -1;
    for (int i = 0; i < presetSource->getPresetManager().getPresetCount(); ++i) {
        if (presetSource->getPresetManager().getPresetNumber(i) == 17) preset17 = i;
        if (presetSource->getPresetManager().getPresetNumber(i) == 18) preset18 = i;
    }
    check(preset17 >= 0 && preset18 >= 0, "presets 17 and 18 available for switch regression");
    auto switchState = std::make_unique<PreparedState>();
    auto switchAudio = std::make_unique<SynthEngine>(); switchAudio->prepare(48000);
    if (preset17 >= 0 && preset18 >= 0) {
        presetSource->loadPreset(preset17); presetSource->capturePreparedState(*switchState);
        switchAudio->applyPreparedState(*switchState);
        switchAudio->noteOn(60, 60000, 1);
        for (int b = 0; b < 16; ++b) switchAudio->renderBlock(left, right, 128);

        auto switchIsDeclickedAndSilent = [&](int presetIndex) {
            const float beforeLeft = left[127], beforeRight = right[127];
            presetSource->loadPreset(presetIndex); presetSource->capturePreparedState(*switchState);
            switchAudio->applyPreparedState(*switchState);
            switchAudio->renderBlock(left, right, 128);
            const float boundaryJump = std::max(std::abs(left[0] - beforeLeft), std::abs(right[0] - beforeRight));
            switchAudio->renderBlock(left, right, 128);
            const float settledPeak = std::max(std::abs(left[127]), std::abs(right[127]));
            return boundaryJump < 0.000001f && settledPeak < 0.000001f;
        };
        check(switchIsDeclickedAndSilent(preset18), "preset 17 to 18 de-clicks and retires old voices");
        switchAudio->noteOn(64, 60000, 1);
        for (int b = 0; b < 8; ++b) switchAudio->renderBlock(left, right, 128);
        check(switchIsDeclickedAndSilent(preset17), "preset 18 to 17 de-clicks and retires old voices");
    }

    int preset43 = -1;
    for (int i = 0; i < presetSource->getPresetManager().getPresetCount(); ++i)
        if (presetSource->getPresetManager().getPresetNumber(i) == 43) preset43 = i;
    check(preset43 >= 0, "preset 43 available for pad release regression");
    if (preset43 >= 0) {
        presetSource->loadPreset(preset43);
        const auto& pad = presetSource->getCurrentPreset();
        check(pad.steppedParams[spAmpEnvSlow] != 0 && pad.steppedParams[spFilEnvSlow] != 0,
              "preset 43 uses slow amp and filter envelope ranges");
        presetSource->noteOn(60, 60000, 1);
        for (int b = 0; b < 375; ++b) presetSource->renderBlock(left, right, 128); // 1 second
        presetSource->noteOff(60, 0, 1);
        for (int b = 0; b < 750; ++b) presetSource->renderBlock(left, right, 128); // 2 seconds into release
        check(presetSource->getVoiceAmpLevel(0) > 0, "preset 43 remains audible two seconds after note-off");
        for (int b = 0; b < 750; ++b) presetSource->renderBlock(left, right, 128); // release has completed
        check(presetSource->getVoiceAmpLevel(0) == 0, "preset 43 release eventually retires the voice");
    }

    VoiceAssigner assigner; int releases[6]{};
    assigner.setCallback([&](uint8_t, int8_t gate, int8_t voice, uint16_t, uint8_t) { if (!gate) ++releases[voice]; });
    assigner.assignNote(60, 1, 50000, 1, 1, 2, 0);
    assigner.assignNote(60, 1, 50000, 1, 2, 3, 0);
    const int otherVoice = assigner.getVoiceByChannel(3);
    assigner.assignNote(60, 0, 0, 1, 3, 2);
    check(otherVoice >= 0 && releases[otherVoice] == 0, "same note on another channel survives note-off");
    auto layer = std::make_unique<SynthEngine>(); layer->prepare(48000); layer->setCustomRouting(true);
    // Use sustained envelopes for both layers; the default second part is a short percussion sound.
    layer->getAfxKit().getSlot(1).preset = layer->getCurrentPreset();
    for (int p = 0; p < 16; ++p) layer->getPartRoute(p).enabled = p < 2;
    layer->getPartRoute(0).channel = layer->getPartRoute(1).channel = 1;
    layer->noteOn(60, 60000, 1); layer->renderBlock(left, right, 128);
    int active = 0; for (int v = 0; v < 6; ++v) { const auto level = layer->getVoiceAmpLevel(v); std::cout << "Voice " << v << " level " << level << '\n'; active += level > 0; }
    check(active == 2, "overlapping routes allocate two voices from the shared pool");
    bool bounded = true;
    for (int i = 0; i < 128; ++i) bounded &= std::isfinite(left[i]) && std::abs(left[i]) <= 0.98f;
    check(bounded, "finite bounded output");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
