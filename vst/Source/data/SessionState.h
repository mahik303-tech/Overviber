#pragma once
#include <juce_core/juce_core.h>
#include "data/SynthModel.h"

// Versioned self-contained session: part presets plus the actual current wave
// frames, so edited/imported waves survive even when external files move.
class SessionState {
public:
    static juce::String encode(SynthModel& model) {
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("format", "Overviber"); root->setProperty("version", 1);
        root->setProperty("customRouting", model.usesCustomRouting());
        juce::Array<juce::var> parts;
        for (int i = 0; i < 16; ++i) {
            const auto& slot = model.getAfxKit().getSlot(i);
            auto part = std::make_unique<juce::DynamicObject>();
            part->setProperty("name", juce::String(slot.name));
            part->setProperty("preset", juce::String(model.getPresetManager().serializePresetToString(slot.preset)));
            juce::Array<juce::var> continuous;
            for (auto value : slot.preset.continuousParams) continuous.add(static_cast<int>(value));
            part->setProperty("continuous", juce::var(continuous));
            const auto& route = model.getPartRoute(i);
            part->setProperty("enabled", route.enabled != 0); part->setProperty("channel", route.channel);
            part->setProperty("low", route.low); part->setProperty("high", route.high);
            juce::MemoryOutputStream samples;
            for (int w = 0; w < abxCount; ++w)
                for (int n = 0; n < WTOSC_SAMPLE_COUNT; ++n)
                    samples.writeShort(static_cast<short>(slot.waveManager.getWaveData(static_cast<abx_t>(w))[n]));
            part->setProperty("waves", samples.getMemoryBlock().toBase64Encoding());
            parts.add(juce::var(part.release()));
        }
        root->setProperty("parts", juce::var(parts));
        juce::Array<juce::var> map, faders, pans, panCustomized, pattern, degrees;
        for (int n = 0; n < 128; ++n) map.add(model.getAfxKit().getSlotForNote(static_cast<uint8_t>(n)));
        for (int v = 0; v < SYNTH_VOICE_COUNT; ++v) {
            faders.add(model.getVoiceFader(v));
            pans.add(model.getStoredVoicePan(v));
            panCustomized.add(model.isVoicePanCustomized(v));
        }
        for (int s = 0; s < 16; ++s) {
            pattern.add(model.getArpSequence().getStepPattern(s));
            degrees.add(model.getArpSequence().getStepDegree(s));
        }
        root->setProperty("noteMap", juce::var(map)); root->setProperty("faders", juce::var(faders));
        root->setProperty("pans", juce::var(pans)); root->setProperty("panCustomized", juce::var(panCustomized));
        root->setProperty("pattern", juce::var(pattern)); root->setProperty("degrees", juce::var(degrees));
        root->setProperty("transpose", model.getArpSequence().transpose);
        root->setProperty("masterMute", model.isMasterMuted());
        return juce::JSON::toString(juce::var(root.release()), true);
    }

    // Decode into a temporary model. The caller commits it only on success.
    static bool decode(const juce::String& text, SynthModel& model) try {
        if (text.getNumBytesAsUTF8() > 4 * 1024 * 1024) return false;
        if (!text.trimStart().startsWithChar('{')) return false;
        auto root = juce::JSON::parse(text);
        if (root["format"].toString() != "Overviber" || static_cast<int>(root["version"]) != 1) return false;
        auto* parts = root["parts"].getArray();
        auto* map = root["noteMap"].getArray(); auto* faders = root["faders"].getArray();
        auto* pans = root["pans"].getArray(); auto* panCustomized = root["panCustomized"].getArray();
        auto* pattern = root["pattern"].getArray(); auto* degrees = root["degrees"].getArray();
        if (!parts || parts->size() != 16 || !map || map->size() != 128 || !faders || faders->size() != 6
            || !pattern || pattern->size() != 16 || !degrees || degrees->size() != 16) return false;
        for (int i = 0; i < 16; ++i) {
            const auto& item = parts->getReference(i);
            if (!item.isObject()) return false;
            auto& slot = model.getAfxKit().getSlot(i);
            if (!model.getPresetManager().parsePresetString(item["preset"].toString().toStdString(), slot.preset)) return false;
            const auto* continuous = item["continuous"].getArray();
            if (!continuous || continuous->size() != cpCount) return false;
            for (int cp = 0; cp < cpCount; ++cp) {
                const int value = continuous->getReference(cp);
                if (value < 0 || value > 65535) return false;
                slot.preset.continuousParams[cp] = static_cast<uint16_t>(value);
            }
            const int ch = item["channel"], lo = item["low"], hi = item["high"];
            if (ch < 0 || ch > 16 || lo < 0 || hi > 127 || lo > hi) return false;
            slot.name = item["name"].toString().toStdString(); slot.isCustomized = true;
            model.getPartRoute(i) = {static_cast<uint8_t>(static_cast<bool>(item["enabled"])),
                static_cast<uint8_t>(ch), static_cast<uint8_t>(lo), static_cast<uint8_t>(hi)};
            juce::MemoryBlock samples;
            if (!samples.fromBase64Encoding(item["waves"].toString()) || samples.getSize() != abxCount * WTOSC_SAMPLE_COUNT * 2) return false;
            juce::MemoryInputStream stream(samples, false);
            for (int w = 0; w < abxCount; ++w)
                for (int n = 0; n < WTOSC_SAMPLE_COUNT; ++n)
                    slot.waveManager.getMutableWaveData(static_cast<abx_t>(w))[n] = static_cast<uint16_t>(stream.readShort());
        }
        for (int n = 0; n < 128; ++n) {
            const int slot = map->getReference(n); if (slot < 0 || slot >= 16) return false;
            model.getAfxKit().setNoteMapping(static_cast<uint8_t>(n), static_cast<uint8_t>(slot));
        }
        for (int v = 0; v < 6; ++v) {
            const float f = static_cast<float>(faders->getReference(v));
            if (!std::isfinite(f) || f < 0 || f > 4) return false;   // up to +12 dB
            model.setVoiceFader(v, f);
        }
        if ((pans != nullptr) != (panCustomized != nullptr)) return false;
        if (pans) {
            if (pans->size() != 6 || panCustomized->size() != 6) return false;
            for (int v = 0; v < 6; ++v) {
                const float pan = static_cast<float>(pans->getReference(v));
                if (!std::isfinite(pan) || pan < -1.0f || pan > 1.0f) return false;
                model.setVoicePan(v, pan, static_cast<bool>(panCustomized->getReference(v)));
            }
        }
        model.setMasterMute(static_cast<bool>(root["masterMute"]));
        for (int s = 0; s < 16; ++s) {
            const int p = pattern->getReference(s), d = degrees->getReference(s);
            if (p < 0 || p > 3 || d < 0 || d > 11) return false;
            model.getArpSequence().setStepPattern(s, static_cast<uint8_t>(p));
            model.getArpSequence().setStepDegree(s, static_cast<uint8_t>(d));
        }
        const int transpose = root["transpose"];
        if (transpose < -48 || transpose > 48) return false;
        model.getArpSequence().transpose = static_cast<int8_t>(transpose);
        model.setCustomRouting(static_cast<bool>(root["customRouting"]));
        return true;
    } catch (const std::exception&) {
        return false;
    }
};
