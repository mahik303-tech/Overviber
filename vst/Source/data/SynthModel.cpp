#include "SynthModel.h"
#include <algorithm>

SynthModel::SynthModel()
    : currentPreset(afxKit.getSlot(0).preset), waveManager(afxKit.getSlot(0).waveManager) {
    currentPreset.setDefaults();
    for (int part = 0; part < 16; ++part) routes[part].channel = static_cast<uint8_t>(part + 1);
}

void SynthModel::setContinuousParam(continuousParameter_t cp, uint16_t value) {
    if (cp < 0 || cp >= cpCount) return;
    currentPreset.continuousParams[cp] = value;
}

void SynthModel::setSteppedParam(steppedParameter_t sp, uint8_t value) {
    if (sp < 0 || sp >= spCount) return;
    if (sp == spEngineMode && value >= emCount) value = emMultiChannel;
    currentPreset.steppedParams[sp] = value;
}

void SynthModel::setMatrixSlot(int slotIndex, modSource_t src, modDest_t dest, modSource_t via, int16_t depth, bool enabled) {
    if (slotIndex < 0 || slotIndex >= MOD_MATRIX_SLOT_COUNT) return;
    auto& slot = currentPreset.modMatrix[slotIndex];
    slot.source = (uint8_t)src;
    slot.dest = (uint8_t)dest;
    slot.viaSource = (uint8_t)via;
    slot.depth = (int16_t)std::clamp((int)depth, -100, 100);
    slot.enabled = enabled;
}

void SynthModel::loadPreset(int presetIndex) {
    PresetData preset;
    if (!presetManager.loadPreset(presetIndex, preset)) return;
    // Presets replace wave and DSP state as one unit; the new panic
    // generation retires the previous preset's voices in the audio engine.
    panic();
    currentPreset = preset;
    // A new preset starts with every voice fader at 0 dB (the fader's
    // middle); a restored session keeps its faders (SessionState::decode).
    std::fill(std::begin(voiceFader), std::end(voiceFader), 1.0f);
    applyPreset();
}

void SynthModel::applyPreset() {
    waveManager.loadWave(abxAMain, currentPreset.oscBank[abxAMain], currentPreset.oscWave[abxAMain]);
    waveManager.loadWave(abxBMain, currentPreset.oscBank[abxBMain], currentPreset.oscWave[abxBMain]);
    waveManager.loadWave(abxACrossover, currentPreset.oscBank[abxACrossover], currentPreset.oscWave[abxACrossover]);
    waveManager.loadWave(abxBCrossover, currentPreset.oscBank[abxBCrossover], currentPreset.oscWave[abxBCrossover]);
}

float SynthModel::getInternalBpm() const {
    const float bpm = 20.0f + ((float)scan_potFrom16bits(currentPreset.continuousParams[cpArpBpm]) / 999.0f) * 280.0f;
    return std::clamp(bpm, 20.0f, 300.0f);
}

void SynthModel::setVoiceFader(int voice, float value) {
    if (voice >= 0 && voice < SYNTH_VOICE_COUNT) voiceFader[voice] = std::clamp(value, 0.0f, 4.0f);   // up to +12 dB
}

float SynthModel::getVoiceFader(int voice) const {
    return voice >= 0 && voice < SYNTH_VOICE_COUNT ? voiceFader[voice] : 1.0f;
}

void SynthModel::setVoicePan(int voice, float pan, bool customized) {
    if (voice < 0 || voice >= SYNTH_VOICE_COUNT) return;
    voicePan[voice] = std::clamp(pan, -1.0f, 1.0f);
    voicePanCustomized[voice] = customized;
}

float SynthModel::getVoicePan(int voice) const {
    if (voice < 0 || voice >= SYNTH_VOICE_COUNT) return 0.0f;
    if (!voicePanCustomized[voice] && presetUsesSingleVoice()) return 0.0f;
    return voicePan[voice];
}

float SynthModel::getStoredVoicePan(int voice) const {
    return voice >= 0 && voice < SYNTH_VOICE_COUNT ? voicePan[voice] : 0.0f;
}

bool SynthModel::isVoicePanCustomized(int voice) const {
    return voice >= 0 && voice < SYNTH_VOICE_COUNT && voicePanCustomized[voice];
}

bool SynthModel::presetUsesSingleVoice() const {
    if (currentPreset.steppedParams[spVoiceCount] == 0) return true;
    if (currentPreset.steppedParams[spUnison] == 0) return false;
    int patternNotes = 0;
    while (patternNotes < SYNTH_VOICE_COUNT
           && currentPreset.voicePattern[patternNotes] != ASSIGNER_NO_NOTE) ++patternNotes;
    return patternNotes == 1;
}

void SynthModel::capturePreparedState(PreparedState& state) const {
    for (int part = 0; part < 16; ++part) {
        const auto& slot = afxKit.getSlot(part);
        auto& target = state.parts[part];
        std::copy_n(slot.preset.continuousParams, cpCount, target.continuous);
        std::copy_n(slot.preset.steppedParams, spCount, target.stepped);
        std::copy_n(slot.preset.modMatrix, MOD_MATRIX_SLOT_COUNT, target.matrix);
        std::copy_n(slot.preset.voicePattern, SYNTH_VOICE_COUNT, target.pattern);
        // A reused state keeps unchanged waves; only new revisions are copied.
        const uint32_t revision = slot.waveManager.getRevision();
        if (target.waveRevision != revision) {
            for (int wave = 0; wave < abxCount; ++wave)
                std::copy_n(slot.waveManager.getWaveData(static_cast<abx_t>(wave)), WTOSC_SAMPLE_COUNT, target.waves[wave]);
            target.waveRevision = revision;
        }
        target.route = routes[part];
    }
    std::copy_n(voiceFader, SYNTH_VOICE_COUNT, state.faders);
    state.masterMute = masterMute;
    std::copy_n(voicePan, SYNTH_VOICE_COUNT, state.pans);
    for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) state.panCustomized[v] = voicePanCustomized[v] ? 1 : 0;
    for (int n = 0; n < 128; ++n) state.noteMap[n] = afxKit.getSlotForNote(static_cast<uint8_t>(n));
    for (int s = 0; s < 16; ++s) {
        state.arpPattern[s] = arpSequence.pattern[s];
        state.arpDegrees[s] = arpSequence.degrees[s];
    }
    state.transpose = arpSequence.transpose;
    state.customRouting = customRouting;
    state.panicGeneration = panicGeneration;
}
