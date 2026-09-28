const fs = require('fs');
const path = require('path');

// =============================================================================
// Helper: Variable-Length Quantity for MIDI
// =============================================================================
function writeVarLen(value) {
    const buffer = [];
    let v = value;
    buffer.push(v & 0x7F);
    while ((v >>= 7) > 0) {
        buffer.push((v & 0x7F) | 0x80);
    }
    buffer.reverse();
    return Buffer.from(buffer);
}

class MidiTrack {
    constructor() {
        this.events = [];
    }

    addEvent(absoluteTick, bytes) {
        this.events.push({
            tick: Math.round(absoluteTick),
            bytes: Buffer.from(bytes)
        });
    }

    buildTrackBuffer() {
        this.events.sort((a, b) => a.tick - b.tick);
        const chunks = [];
        let lastTick = 0;

        for (const ev of this.events) {
            const delta = Math.max(0, ev.tick - lastTick);
            chunks.push(writeVarLen(delta));
            chunks.push(ev.bytes);
            lastTick = ev.tick;
        }

        chunks.push(writeVarLen(0));
        chunks.push(Buffer.from([0xFF, 0x2F, 0x00]));

        const trackData = Buffer.concat(chunks);
        const header = Buffer.alloc(8);
        header.write('MTrk', 0, 4, 'ascii');
        header.writeUInt32BE(trackData.length, 4);
        return Buffer.concat([header, trackData]);
    }
}

function createMidiFile(track, ticksPerQuarter = 480) {
    const trackBuf = track.buildTrackBuffer();
    const header = Buffer.alloc(14);
    header.write('MThd', 0, 4, 'ascii');
    header.writeUInt32BE(6, 4);
    header.writeUInt16BE(0, 8);
    header.writeUInt16BE(1, 10);
    header.writeUInt16BE(ticksPerQuarter, 12);
    return Buffer.concat([header, trackBuf]);
}

function bpmToMicroseconds(bpm) {
    return Math.round(60000000 / bpm);
}

// =============================================================================
// 12 Presets Definitions
// =============================================================================
const presets = [
    {
        num: 52,
        name: "52 Cyberpunk Blade Runner Lead",
        bpm: 110,
        midiName: "01_Cyberpunk_Blade_Runner_Lead_110BPM.mid",
        conf: `presetName = Cyberpunk Blade Runner Lead
bank0 = ProphetVS
wave0 = VS 102.wav
bank1 = AKWF_bw_sawbright
wave1 = AKWF_saw.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 999
cpABaseWMod = 150
cpBFreq = 0
cpBVol = 850
cpBBaseWMod = 0
cpDetune = 16
cpCutoff = 380
cpResonance = 220
cpFilEnvAmt = 420
cpFilKbdAmt = 480
cpWModAEnv = 180
cpFilAtt = 180
cpFilDec = 450
cpFilSus = 720
cpFilRel = 580
cpAmpAtt = 80
cpAmpDec = 300
cpAmpSus = 950
cpAmpRel = 620
cpLFOFreq = 220
cpLFOAmt = 0
cpLFOPitchAmt = 0
cpLFOWModAmt = 30
cpLFOFilAmt = 40
cpLFOAmpAmt = 0
cpLFO2Freq = 60
cpLFO2Amt = 0
cpModDelay = 100
cpGlide = 180
cpAmpVelocity = 320
cpFilVelocity = 450
cpMasterTune = 0
cpUnisonDetune = 12
cpNoiseVol = 10
cpLFO2PitchAmt = 8
cpLFO2WModAmt = 35
cpLFO2FilAmt = 45
cpLFO2AmpAmt = 0
cpLFOResAmt = 25
cpLFO2ResAmt = 15
cpWModAtt = 150
cpWModDec = 400
cpWModSus = 650
cpWModRel = 550
cpWModBEnv = 0
cpWModVelocity = 280
cpAmpLevel = 920
cpShelvesLsFreq = 180
cpShelvesLsGain = 30
cpShelvesP1Gain = 0
cpShelvesP2Freq = 650
cpShelvesP2Gain = 20
cpShelvesP2Q = 300
cpShelvesHsFreq = 800
cpShelvesHsGain = 40
cpMackityInTrim = 180
cpMackityOutPad = 950
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 2
spBWModType = 0
spLFOShape = 3
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 0
spAmpEnvSlow = 0
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 5
spPresetType = 1
spPresetStyle = 6
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 0
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 0
spFilterMode = 0
spMackity = 1
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 1
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 2
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 1
matrixSlot0_dest = 11
matrixSlot0_via = 0
matrixSlot0_depth = 70
matrixSlot0_curve = 1
matrixSlot0_en = 1
matrixSlot1_src = 3
matrixSlot1_dest = 18
matrixSlot1_via = 0
matrixSlot1_depth = 60
matrixSlot1_curve = 0
matrixSlot1_en = 1
matrixSlot2_src = 4
matrixSlot2_dest = 6
matrixSlot2_via = 0
matrixSlot2_depth = 50
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 5
matrixSlot3_dest = 11
matrixSlot3_via = 0
matrixSlot3_depth = 45
matrixSlot3_curve = 1
matrixSlot3_en = 1
matrixSlot4_src = 9
matrixSlot4_dest = 15
matrixSlot4_via = 0
matrixSlot4_depth = 35
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 6
matrixSlot5_dest = 23
matrixSlot5_via = 0
matrixSlot5_depth = 40
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 7
matrixSlot6_dest = 11
matrixSlot6_via = 0
matrixSlot6_depth = 50
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 16
matrixSlot7_dest = 4
matrixSlot7_via = 0
matrixSlot7_depth = 20
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Melodic expressive lead line with pitch bend and aftertouch swells
            const notes = [
                { bar: 0.0, len: 1.5, note: 62 }, // D4
                { bar: 1.5, len: 0.5, note: 65 }, // F4
                { bar: 2.0, len: 2.0, note: 67 }, // G4
                { bar: 4.0, len: 1.0, note: 69 }, // A4
                { bar: 5.0, len: 1.0, note: 72 }, // C5
                { bar: 6.0, len: 2.0, note: 74 }  // D5
            ];
            notes.forEach(n => {
                const on = Math.round(n.bar * BAR);
                const off = Math.round((n.bar + n.len) * BAR - 40);
                track.addEvent(on, [0x90, n.note, 105]);
                track.addEvent(off, [0x80, n.note, 64]);
            });
            // Mod Wheel and Aftertouch swells
            for (let t = 0; t <= 8 * BAR; t += 30) {
                const phase = (t / (8 * BAR));
                const mw = Math.round(30 + Math.sin(phase * Math.PI) * 90);
                const at = Math.round(Math.max(0, Math.sin((t % (2 * BAR)) / (2 * BAR) * Math.PI) * 115));
                track.addEvent(t, [0xB0, 1, mw]);
                track.addEvent(t, [0xD0, at]);
                track.addEvent(t, [0xB0, 74, Math.round(40 + Math.sin(phase * 3 * Math.PI) * 70)]);
            }
        }
    },
    {
        num: 53,
        name: "53 Sub Terra Analog Acid Bass",
        bpm: 130,
        midiName: "02_Sub_Terra_Acid_Bass_130BPM.mid",
        conf: `presetName = Sub Terra Analog Acid Bass
bank0 = AKWF_c604
wave0 = AKWF_saw.wav
bank1 = AKWF_raw
wave1 = AKWF_saw.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 999
cpABaseWMod = 220
cpBFreq = 0
cpBVol = 650
cpBBaseWMod = 0
cpDetune = 6
cpCutoff = 220
cpResonance = 450
cpFilEnvAmt = 580
cpFilKbdAmt = 380
cpWModAEnv = 200
cpFilAtt = 0
cpFilDec = 320
cpFilSus = 120
cpFilRel = 180
cpAmpAtt = 0
cpAmpDec = 350
cpAmpSus = 280
cpAmpRel = 180
cpLFOFreq = 180
cpLFOAmt = 0
cpLFOPitchAmt = 0
cpLFOWModAmt = 0
cpLFOFilAmt = 0
cpLFOAmpAmt = 0
cpLFO2Freq = 80
cpLFO2Amt = 0
cpModDelay = 0
cpGlide = 80
cpAmpVelocity = 480
cpFilVelocity = 750
cpMasterTune = 0
cpUnisonDetune = 5
cpNoiseVol = 0
cpLFO2PitchAmt = 0
cpLFO2WModAmt = 0
cpLFO2FilAmt = 0
cpLFO2AmpAmt = 0
cpLFOResAmt = 0
cpLFO2ResAmt = 0
cpWModAtt = 0
cpWModDec = 280
cpWModSus = 150
cpWModRel = 180
cpWModBEnv = 0
cpWModVelocity = 400
cpAmpLevel = 950
cpShelvesLsFreq = 120
cpShelvesLsGain = 60
cpShelvesP1Gain = 0
cpShelvesP2Freq = 850
cpShelvesP2Gain = 40
cpShelvesP2Q = 350
cpShelvesHsFreq = 800
cpShelvesHsGain = -20
cpMackityInTrim = 350
cpMackityOutPad = 880
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 5
spBWModType = 0
spLFOShape = 1
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 0
spAmpEnvSlow = 0
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 0
spPresetType = 0
spPresetStyle = 2
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 0
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 1
spFilterMode = 0
spMackity = 1
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 0
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 0
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 5
matrixSlot0_dest = 11
matrixSlot0_via = 0
matrixSlot0_depth = 85
matrixSlot0_curve = 1
matrixSlot0_en = 1
matrixSlot1_src = 1
matrixSlot1_dest = 12
matrixSlot1_via = 0
matrixSlot1_depth = 75
matrixSlot1_curve = 0
matrixSlot1_en = 1
matrixSlot2_src = 4
matrixSlot2_dest = 16
matrixSlot2_via = 0
matrixSlot2_depth = 65
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 3
matrixSlot3_dest = 6
matrixSlot3_via = 0
matrixSlot3_depth = 55
matrixSlot3_curve = 0
matrixSlot3_en = 1
matrixSlot4_src = 7
matrixSlot4_dest = 22
matrixSlot4_via = 0
matrixSlot4_depth = -35
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 8
matrixSlot5_dest = 11
matrixSlot5_via = 0
matrixSlot5_depth = 50
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 13
matrixSlot6_dest = 11
matrixSlot6_via = 0
matrixSlot6_depth = 30
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 6
matrixSlot7_dest = 15
matrixSlot7_via = 0
matrixSlot7_depth = -25
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Driving 16th-note acid bassline with accents
            const baseNotes = [ 36, 36, 48, 36, 39, 36, 41, 42, 36, 36, 48, 46, 39, 41, 43, 34 ];
            for (let b = 0; b < 4; b++) {
                for (let i = 0; i < 16; i++) {
                    const tick = (b * BAR) + (i * (PPQ / 4));
                    const isAccent = (i % 4 === 0) || (i === 6) || (i === 11);
                    const vel = isAccent ? 124 : 75;
                    const pitch = baseNotes[i];
                    track.addEvent(tick, [0x90, pitch, vel]);
                    track.addEvent(tick + (PPQ / 4) - 20, [0x80, pitch, 64]);
                }
            }
            // Continuous Mod Wheel resonance sweep
            for (let t = 0; t <= 4 * BAR; t += 30) {
                const mw = Math.round(15 + Math.sin(t / (4 * BAR) * Math.PI) * 105);
                track.addEvent(t, [0xB0, 1, mw]);
                track.addEvent(t, [0xB0, 74, Math.round(30 + (t / (4 * BAR)) * 80)]);
            }
        }
    },
    {
        num: 54,
        name: "54 Shimmer Glass Ambient Keys",
        bpm: 85,
        midiName: "03_Shimmer_Glass_Keys_85BPM.mid",
        conf: `presetName = Shimmer Glass Ambient Keys
bank0 = AKWF_epiano
wave0 = AKWF_epiano.wav
bank1 = AKWF_sinharm
wave1 = AKWF_sinharm.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 950
cpABaseWMod = 120
cpBFreq = 0
cpBVol = 780
cpBBaseWMod = 60
cpDetune = 8
cpCutoff = 650
cpResonance = 150
cpFilEnvAmt = 280
cpFilKbdAmt = 550
cpWModAEnv = 180
cpFilAtt = 10
cpFilDec = 480
cpFilSus = 420
cpFilRel = 680
cpAmpAtt = 15
cpAmpDec = 520
cpAmpSus = 550
cpAmpRel = 720
cpLFOFreq = 140
cpLFOAmt = 0
cpLFOPitchAmt = 5
cpLFOWModAmt = 30
cpLFOFilAmt = 35
cpLFOAmpAmt = 0
cpLFO2Freq = 50
cpLFO2Amt = 0
cpModDelay = 80
cpGlide = 0
cpAmpVelocity = 650
cpFilVelocity = 580
cpMasterTune = 0
cpUnisonDetune = 10
cpNoiseVol = 15
cpLFO2PitchAmt = 8
cpLFO2WModAmt = 25
cpLFO2FilAmt = 35
cpLFO2AmpAmt = 0
cpLFOResAmt = 15
cpLFO2ResAmt = 10
cpWModAtt = 20
cpWModDec = 450
cpWModSus = 350
cpWModRel = 600
cpWModBEnv = 120
cpWModVelocity = 450
cpAmpLevel = 900
cpShelvesLsFreq = 180
cpShelvesLsGain = 15
cpShelvesP1Gain = 0
cpShelvesP2Freq = 750
cpShelvesP2Gain = 45
cpShelvesP2Q = 280
cpShelvesHsFreq = 850
cpShelvesHsGain = 60
cpMackityInTrim = 120
cpMackityOutPad = 950
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 4
spBWModType = 2
spLFOShape = 3
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 0
spAmpEnvSlow = 0
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 5
spPresetType = 3
spPresetStyle = 6
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 0
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 2
spFilterMode = 0
spMackity = 0
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 1
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 2
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 5
matrixSlot0_dest = 13
matrixSlot0_via = 0
matrixSlot0_depth = 60
matrixSlot0_curve = 0
matrixSlot0_en = 1
matrixSlot1_src = 4
matrixSlot1_dest = 5
matrixSlot1_via = 13
matrixSlot1_depth = 70
matrixSlot1_curve = 2
matrixSlot1_en = 1
matrixSlot2_src = 1
matrixSlot2_dest = 11
matrixSlot2_via = 0
matrixSlot2_depth = 50
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 3
matrixSlot3_dest = 14
matrixSlot3_via = 0
matrixSlot3_depth = 55
matrixSlot3_curve = 0
matrixSlot3_en = 1
matrixSlot4_src = 7
matrixSlot4_dest = 14
matrixSlot4_via = 0
matrixSlot4_depth = 45
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 16
matrixSlot5_dest = 4
matrixSlot5_via = 0
matrixSlot5_depth = 30
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 9
matrixSlot6_dest = 15
matrixSlot6_via = 0
matrixSlot6_depth = 40
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 12
matrixSlot7_dest = 6
matrixSlot7_via = 0
matrixSlot7_depth = 45
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Ethereal EP chord arpeggiation / voicings
            const chords = [
                { start: 0, notes: [ 48, 55, 62, 64, 71 ] }, // Cmaj9
                { start: 2, notes: [ 45, 52, 60, 64, 67 ] }, // Am7/9
                { start: 4, notes: [ 41, 48, 57, 60, 65 ] }, // Fmaj7
                { start: 6, notes: [ 43, 50, 57, 62, 67 ] }  // G9
            ];
            chords.forEach(c => {
                c.notes.forEach((p, idx) => {
                    const on = (c.start * BAR) + (idx * (PPQ / 2));
                    track.addEvent(on, [0x90, p, 80 + idx * 8]);
                    track.addEvent(on + (BAR * 1.8), [0x80, p, 50]);
                });
            });
            for (let t = 0; t <= 8 * BAR; t += 30) {
                const phase = t / (8 * BAR);
                track.addEvent(t, [0xB0, 1, Math.round(30 + Math.sin(phase * Math.PI) * 80)]);
                track.addEvent(t, [0xB0, 74, Math.round(40 + Math.sin(phase * 4 * Math.PI) * 75)]);
                track.addEvent(t, [0xD0, Math.round(Math.max(0, Math.sin(phase * 8 * Math.PI) * 90))]);
            }
        }
    },
    {
        num: 55,
        name: "55 Tokyo Neon Synthwave Chords",
        bpm: 118,
        midiName: "04_Tokyo_Neon_Synthwave_Chords_118BPM.mid",
        conf: `presetName = Tokyo Neon Synthwave Chords
bank0 = AKWF_bw_perfectwaves
wave0 = AKWF_saw.wav
bank1 = AKWF_bw_perfectwaves
wave1 = AKWF_squ.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 999
cpABaseWMod = 0
cpBFreq = 0
cpBVol = 880
cpBBaseWMod = 0
cpDetune = 18
cpCutoff = 450
cpResonance = 160
cpFilEnvAmt = 450
cpFilKbdAmt = 350
cpWModAEnv = 0
cpFilAtt = 30
cpFilDec = 420
cpFilSus = 500
cpFilRel = 580
cpAmpAtt = 20
cpAmpDec = 380
cpAmpSus = 750
cpAmpRel = 580
cpLFOFreq = 200
cpLFOAmt = 0
cpLFOPitchAmt = 8
cpLFOWModAmt = 0
cpLFOFilAmt = 25
cpLFOAmpAmt = 0
cpLFO2Freq = 70
cpLFO2Amt = 0
cpModDelay = 50
cpGlide = 0
cpAmpVelocity = 450
cpFilVelocity = 650
cpMasterTune = 0
cpUnisonDetune = 15
cpNoiseVol = 5
cpLFO2PitchAmt = 12
cpLFO2WModAmt = 0
cpLFO2FilAmt = 30
cpLFO2AmpAmt = 0
cpLFOResAmt = 10
cpLFO2ResAmt = 10
cpWModAtt = 0
cpWModDec = 0
cpWModSus = 999
cpWModRel = 0
cpWModBEnv = 0
cpWModVelocity = 0
cpAmpLevel = 920
cpShelvesLsFreq = 160
cpShelvesLsGain = 35
cpShelvesP1Gain = 0
cpShelvesP2Freq = 700
cpShelvesP2Gain = 30
cpShelvesP2Q = 280
cpShelvesHsFreq = 800
cpShelvesHsGain = 45
cpMackityInTrim = 220
cpMackityOutPad = 920
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 0
spBWModType = 0
spLFOShape = 1
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 0
spAmpEnvSlow = 0
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 5
spPresetType = 2
spPresetStyle = 6
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 0
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 0
spFilterMode = 0
spMackity = 1
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 1
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 2
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 1
matrixSlot0_dest = 11
matrixSlot0_via = 0
matrixSlot0_depth = 65
matrixSlot0_curve = 1
matrixSlot0_en = 1
matrixSlot1_src = 5
matrixSlot1_dest = 11
matrixSlot1_via = 0
matrixSlot1_depth = 55
matrixSlot1_curve = 1
matrixSlot1_en = 1
matrixSlot2_src = 4
matrixSlot2_dest = 7
matrixSlot2_via = 0
matrixSlot2_depth = 45
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 3
matrixSlot3_dest = 12
matrixSlot3_via = 0
matrixSlot3_depth = 40
matrixSlot3_curve = 0
matrixSlot3_en = 1
matrixSlot4_src = 9
matrixSlot4_dest = 16
matrixSlot4_via = 0
matrixSlot4_depth = 35
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 14
matrixSlot5_dest = 4
matrixSlot5_via = 0
matrixSlot5_depth = 25
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 7
matrixSlot6_dest = 21
matrixSlot6_via = 0
matrixSlot6_depth = -30
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 6
matrixSlot7_dest = 23
matrixSlot7_via = 0
matrixSlot7_depth = 40
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Punchy synthwave chord stabs
            const stabChords = [
                { bar: 0.0, notes: [ 38, 50, 57, 62, 65, 69 ] }, // Dm9
                { bar: 1.0, notes: [ 34, 46, 53, 58, 62, 65 ] }, // Bbmaj7
                { bar: 2.0, notes: [ 36, 48, 55, 60, 64, 67 ] }, // C
                { bar: 3.0, notes: [ 41, 53, 60, 65, 69, 72 ] }  // Fmaj7
            ];
            stabChords.forEach(c => {
                // Main chord hit
                const on1 = Math.round(c.bar * BAR);
                c.notes.forEach(p => track.addEvent(on1, [0x90, p, 110]));
                c.notes.forEach(p => track.addEvent(on1 + (PPQ * 1.5), [0x80, p, 64]));
                // Syncopated 8th note stab
                const on2 = Math.round((c.bar + 0.75) * BAR);
                c.notes.forEach(p => track.addEvent(on2, [0x90, p, 95]));
                c.notes.forEach(p => track.addEvent(on2 + (PPQ * 0.75), [0x80, p, 64]));
            });
            for (let t = 0; t <= 4 * BAR; t += 30) {
                const mw = Math.round(20 + (t / (4 * BAR)) * 100);
                track.addEvent(t, [0xB0, 1, mw]);
                track.addEvent(t, [0xB0, 74, Math.round(30 + Math.sin(t / (BAR) * Math.PI) * 70)]);
            }
        }
    },
    {
        num: 56,
        name: "56 Quantum Granular Drone",
        bpm: 65,
        midiName: "05_Quantum_Granular_Drone_65BPM.mid",
        conf: `presetName = Quantum Granular Drone
bank0 = AKWF_granular
wave0 = AKWF_granular.wav
bank1 = ProphetVS
wave1 = VS 105.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 950
cpABaseWMod = 350
cpBFreq = 0
cpBVol = 850
cpBBaseWMod = -200
cpDetune = 14
cpCutoff = 350
cpResonance = 280
cpFilEnvAmt = 220
cpFilKbdAmt = 200
cpWModAEnv = 250
cpFilAtt = 650
cpFilDec = 700
cpFilSus = 800
cpFilRel = 850
cpAmpAtt = 550
cpAmpDec = 600
cpAmpSus = 950
cpAmpRel = 880
cpLFOFreq = 40
cpLFOAmt = 0
cpLFOPitchAmt = 15
cpLFOWModAmt = 60
cpLFOFilAmt = 50
cpLFOAmpAmt = 0
cpLFO2Freq = 25
cpLFO2Amt = 0
cpModDelay = 200
cpGlide = 250
cpAmpVelocity = 250
cpFilVelocity = 300
cpMasterTune = 0
cpUnisonDetune = 20
cpNoiseVol = 40
cpLFO2PitchAmt = 20
cpLFO2WModAmt = 50
cpLFO2FilAmt = 60
cpLFO2AmpAmt = 0
cpLFOResAmt = 40
cpLFO2ResAmt = 30
cpWModAtt = 500
cpWModDec = 600
cpWModSus = 750
cpWModRel = 800
cpWModBEnv = 220
cpWModVelocity = 200
cpAmpLevel = 880
cpShelvesLsFreq = 140
cpShelvesLsGain = 50
cpShelvesP1Gain = 0
cpShelvesP2Freq = 550
cpShelvesP2Gain = 30
cpShelvesP2Q = 250
cpShelvesHsFreq = 750
cpShelvesHsGain = 20
cpMackityInTrim = 280
cpMackityOutPad = 900
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 1
spBWModType = 5
spLFOShape = 0
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 1
spAmpEnvSlow = 1
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 5
spPresetType = 2
spPresetStyle = 6
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 1
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 1
spFilterMode = 0
spMackity = 1
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 1
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 2
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 13
matrixSlot0_dest = 6
matrixSlot0_via = 0
matrixSlot0_depth = 75
matrixSlot0_curve = 0
matrixSlot0_en = 1
matrixSlot1_src = 15
matrixSlot1_dest = 11
matrixSlot1_via = 1
matrixSlot1_depth = 60
matrixSlot1_curve = 0
matrixSlot1_en = 1
matrixSlot2_src = 4
matrixSlot2_dest = 13
matrixSlot2_via = 0
matrixSlot2_depth = 50
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 3
matrixSlot3_dest = 19
matrixSlot3_via = 0
matrixSlot3_depth = 65
matrixSlot3_curve = 0
matrixSlot3_en = 1
matrixSlot4_src = 1
matrixSlot4_dest = 16
matrixSlot4_via = 0
matrixSlot4_depth = 55
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 7
matrixSlot5_dest = 12
matrixSlot5_via = 0
matrixSlot5_depth = 40
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 9
matrixSlot6_dest = 9
matrixSlot6_via = 0
matrixSlot6_depth = 50
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 12
matrixSlot7_dest = 4
matrixSlot7_via = 0
matrixSlot7_depth = 35
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Sustained cinematic low drone cluster
            const droneNotes = [ 24, 31, 43, 48, 55, 62 ]; // C1, G1, G2, C3, G3, D4
            droneNotes.forEach(p => {
                track.addEvent(0, [0x90, p, 90]);
                track.addEvent(8 * BAR - 60, [0x80, p, 64]);
            });
            for (let t = 0; t <= 8 * BAR; t += 30) {
                const phase = t / (8 * BAR);
                track.addEvent(t, [0xB0, 1, Math.round(20 + Math.sin(phase * Math.PI) * 95)]);
                track.addEvent(t, [0xB0, 74, Math.round(30 + Math.sin(phase * 2 * Math.PI) * 85)]);
                track.addEvent(t, [0xD0, Math.round(Math.max(0, Math.sin(phase * 3 * Math.PI) * 110))]);
            }
        }
    },
    {
        num: 57,
        name: "57 Hyperdrive Acid Poly Arp",
        bpm: 138,
        midiName: "06_Hyperdrive_Acid_Arp_138BPM.mid",
        conf: `presetName = Hyperdrive Acid Poly Arp
bank0 = AKWF_bw_sawgap
wave0 = AKWF_saw.wav
bank1 = AKWF_bw_squ
wave1 = AKWF_squ.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 999
cpABaseWMod = 180
cpBFreq = 0
cpBVol = 850
cpBBaseWMod = 0
cpDetune = 10
cpCutoff = 320
cpResonance = 380
cpFilEnvAmt = 520
cpFilKbdAmt = 450
cpWModAEnv = 150
cpFilAtt = 0
cpFilDec = 280
cpFilSus = 220
cpFilRel = 200
cpAmpAtt = 0
cpAmpDec = 300
cpAmpSus = 450
cpAmpRel = 220
cpLFOFreq = 260
cpLFOAmt = 0
cpLFOPitchAmt = 0
cpLFOWModAmt = 30
cpLFOFilAmt = 20
cpLFOAmpAmt = 0
cpLFO2Freq = 110
cpLFO2Amt = 0
cpModDelay = 0
cpGlide = 0
cpAmpVelocity = 500
cpFilVelocity = 700
cpMasterTune = 0
cpUnisonDetune = 8
cpNoiseVol = 0
cpLFO2PitchAmt = 0
cpLFO2WModAmt = 0
cpLFO2FilAmt = 0
cpLFO2AmpAmt = 0
cpLFOResAmt = 0
cpLFO2ResAmt = 0
cpWModAtt = 0
cpWModDec = 250
cpWModSus = 200
cpWModRel = 200
cpWModBEnv = 0
cpWModVelocity = 350
cpAmpLevel = 940
cpShelvesLsFreq = 140
cpShelvesLsGain = 40
cpShelvesP1Gain = 0
cpShelvesP2Freq = 780
cpShelvesP2Gain = 35
cpShelvesP2Q = 320
cpShelvesHsFreq = 820
cpShelvesHsGain = 50
cpMackityInTrim = 260
cpMackityOutPad = 920
cpArpGate = 600
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 1
spBWModType = 0
spLFOShape = 1
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 0
spAmpEnvSlow = 0
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 5
spPresetType = 1
spPresetStyle = 2
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 0
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 0
spFilterMode = 0
spMackity = 1
spArpOctaves = 1
spArpRate = 3
spArpHold = 0
spArpMode = 3
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 0
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 0
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 1
matrixSlot0_dest = 11
matrixSlot0_via = 0
matrixSlot0_depth = 80
matrixSlot0_curve = 1
matrixSlot0_en = 1
matrixSlot1_src = 4
matrixSlot1_dest = 24
matrixSlot1_via = 0
matrixSlot1_depth = 70
matrixSlot1_curve = 0
matrixSlot1_en = 1
matrixSlot2_src = 3
matrixSlot2_dest = 25
matrixSlot2_via = 0
matrixSlot2_depth = 50
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 5
matrixSlot3_dest = 11
matrixSlot3_via = 0
matrixSlot3_depth = 60
matrixSlot3_curve = 1
matrixSlot3_en = 1
matrixSlot4_src = 13
matrixSlot4_dest = 6
matrixSlot4_via = 0
matrixSlot4_depth = 45
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 9
matrixSlot5_dest = 16
matrixSlot5_via = 0
matrixSlot5_depth = 40
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 7
matrixSlot6_dest = 12
matrixSlot6_via = 0
matrixSlot6_depth = -30
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 8
matrixSlot7_dest = 17
matrixSlot7_via = 0
matrixSlot7_depth = 55
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Arpeggiator input chords (Arp Engine in synth plays the 16th pattern!)
            const arpChords = [
                { bar: 0, notes: [ 36, 48, 55, 63 ] }, // Cm7
                { bar: 1, notes: [ 34, 46, 53, 62 ] }, // Bb
                { bar: 2, notes: [ 32, 44, 51, 60 ] }, // Ab
                { bar: 3, notes: [ 38, 50, 57, 65 ] }  // G7
            ];
            arpChords.forEach(c => {
                const on = c.bar * BAR;
                c.notes.forEach(p => track.addEvent(on, [0x90, p, 105]));
                c.notes.forEach(p => track.addEvent(on + BAR - 30, [0x80, p, 64]));
            });
            for (let t = 0; t <= 4 * BAR; t += 30) {
                const phase = t / (4 * BAR);
                track.addEvent(t, [0xB0, 1, Math.round(20 + phase * 100)]);
                track.addEvent(t, [0xB0, 74, Math.round(30 + Math.sin(phase * 4 * Math.PI) * 75)]);
                track.addEvent(t, [0xD0, Math.round(Math.sin(phase * Math.PI) * 90)]);
            }
        }
    },
    {
        num: 58,
        name: "58 Velvet Neo-Soul Rhodes Pad",
        bpm: 88,
        midiName: "07_Velvet_Neo_Soul_Rhodes_88BPM.mid",
        conf: `presetName = Velvet Neo-Soul Rhodes Pad
bank0 = AKWF_epiano
wave0 = AKWF_epiano.wav
bank1 = _basic
wave1 = sin.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 999
cpABaseWMod = 60
cpBFreq = 0
cpBVol = 750
cpBBaseWMod = 0
cpDetune = 8
cpCutoff = 520
cpResonance = 140
cpFilEnvAmt = 320
cpFilKbdAmt = 500
cpWModAEnv = 100
cpFilAtt = 10
cpFilDec = 450
cpFilSus = 550
cpFilRel = 650
cpAmpAtt = 8
cpAmpDec = 500
cpAmpSus = 700
cpAmpRel = 680
cpLFOFreq = 160
cpLFOAmt = 0
cpLFOPitchAmt = 6
cpLFOWModAmt = 20
cpLFOFilAmt = 30
cpLFOAmpAmt = 0
cpLFO2Freq = 60
cpLFO2Amt = 0
cpModDelay = 40
cpGlide = 0
cpAmpVelocity = 700
cpFilVelocity = 650
cpMasterTune = 0
cpUnisonDetune = 10
cpNoiseVol = 12
cpLFO2PitchAmt = 8
cpLFO2WModAmt = 20
cpLFO2FilAmt = 25
cpLFO2AmpAmt = 0
cpLFOResAmt = 10
cpLFO2ResAmt = 10
cpWModAtt = 15
cpWModDec = 400
cpWModSus = 450
cpWModRel = 600
cpWModBEnv = 0
cpWModVelocity = 350
cpAmpLevel = 920
cpShelvesLsFreq = 160
cpShelvesLsGain = 45
cpShelvesP1Gain = 0
cpShelvesP2Freq = 720
cpShelvesP2Gain = 35
cpShelvesP2Q = 280
cpShelvesHsFreq = 820
cpShelvesHsGain = 45
cpMackityInTrim = 150
cpMackityOutPad = 950
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 2
spBWModType = 0
spLFOShape = 3
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 0
spAmpEnvSlow = 0
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 5
spPresetType = 3
spPresetStyle = 6
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 0
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 1
spFilterMode = 0
spMackity = 1
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 1
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 2
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 5
matrixSlot0_dest = 11
matrixSlot0_via = 0
matrixSlot0_depth = 65
matrixSlot0_curve = 1
matrixSlot0_en = 1
matrixSlot1_src = 1
matrixSlot1_dest = 5
matrixSlot1_via = 0
matrixSlot1_depth = 50
matrixSlot1_curve = 0
matrixSlot1_en = 1
matrixSlot2_src = 3
matrixSlot2_dest = 18
matrixSlot2_via = 0
matrixSlot2_depth = 45
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 4
matrixSlot3_dest = 4
matrixSlot3_via = 0
matrixSlot3_depth = 35
matrixSlot3_curve = 0
matrixSlot3_en = 1
matrixSlot4_src = 9
matrixSlot4_dest = 15
matrixSlot4_via = 0
matrixSlot4_depth = 40
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 7
matrixSlot5_dest = 11
matrixSlot5_via = 0
matrixSlot5_depth = 55
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 6
matrixSlot6_dest = 23
matrixSlot6_via = 0
matrixSlot6_depth = 50
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 16
matrixSlot7_dest = 13
matrixSlot7_via = 0
matrixSlot7_depth = 30
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Smooth neo-soul chord progression
            const chords = [
                { start: 0, notes: [ 36, 43, 52, 59, 62, 69 ] }, // Cmaj9(13)
                { start: 1, notes: [ 38, 45, 53, 57, 60, 64 ] }, // Dm11
                { start: 2, notes: [ 40, 47, 55, 59, 62, 67 ] }, // Em9
                { start: 3, notes: [ 33, 40, 48, 55, 59, 64 ] }  // Am9
            ];
            chords.forEach(c => {
                const on = c.start * BAR;
                c.notes.forEach((p, i) => track.addEvent(on + i * 16, [0x90, p, 85 + i * 5]));
                c.notes.forEach(p => track.addEvent(on + BAR - 40, [0x80, p, 64]));
            });
            for (let t = 0; t <= 4 * BAR; t += 30) {
                const phase = t / (4 * BAR);
                track.addEvent(t, [0xB0, 1, Math.round(25 + Math.sin(phase * Math.PI) * 75)]);
                track.addEvent(t, [0xD0, Math.round(Math.max(0, Math.sin((t % BAR) / BAR * Math.PI) * 95))]);
            }
        }
    },
    {
        num: 59,
        name: "59 Dark Matter Industrial Bass",
        bpm: 124,
        midiName: "08_Dark_Matter_Industrial_Bass_124BPM.mid",
        conf: `presetName = Dark Matter Industrial Bass
bank0 = AKWF_distorted
wave0 = AKWF_distorted.wav
bank1 = AKWF_dbass
wave1 = AKWF_dbass.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 999
cpABaseWMod = 350
cpBFreq = 0
cpBVol = 850
cpBBaseWMod = 120
cpDetune = 12
cpCutoff = 280
cpResonance = 320
cpFilEnvAmt = 550
cpFilKbdAmt = 300
cpWModAEnv = 280
cpFilAtt = 0
cpFilDec = 300
cpFilSus = 150
cpFilRel = 180
cpAmpAtt = 0
cpAmpDec = 320
cpAmpSus = 300
cpAmpRel = 180
cpLFOFreq = 190
cpLFOAmt = 0
cpLFOPitchAmt = 0
cpLFOWModAmt = 40
cpLFOFilAmt = 0
cpLFOAmpAmt = 0
cpLFO2Freq = 90
cpLFO2Amt = 0
cpModDelay = 0
cpGlide = 60
cpAmpVelocity = 600
cpFilVelocity = 750
cpMasterTune = 0
cpUnisonDetune = 8
cpNoiseVol = 0
cpLFO2PitchAmt = 0
cpLFO2WModAmt = 0
cpLFO2FilAmt = 0
cpLFO2AmpAmt = 0
cpLFOResAmt = 0
cpLFO2ResAmt = 0
cpWModAtt = 0
cpWModDec = 280
cpWModSus = 200
cpWModRel = 180
cpWModBEnv = 150
cpWModVelocity = 450
cpAmpLevel = 950
cpShelvesLsFreq = 130
cpShelvesLsGain = 55
cpShelvesP1Gain = 0
cpShelvesP2Freq = 800
cpShelvesP2Gain = 45
cpShelvesP2Q = 350
cpShelvesHsFreq = 820
cpShelvesHsGain = 20
cpMackityInTrim = 420
cpMackityOutPad = 860
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 5
spBWModType = 1
spLFOShape = 1
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 0
spAmpEnvSlow = 0
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 0
spPresetType = 0
spPresetStyle = 2
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 0
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 1
spFilterMode = 0
spMackity = 1
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 0
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 0
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 5
matrixSlot0_dest = 16
matrixSlot0_via = 0
matrixSlot0_depth = 75
matrixSlot0_curve = 1
matrixSlot0_en = 1
matrixSlot1_src = 1
matrixSlot1_dest = 11
matrixSlot1_via = 0
matrixSlot1_depth = 80
matrixSlot1_curve = 1
matrixSlot1_en = 1
matrixSlot2_src = 4
matrixSlot2_dest = 6
matrixSlot2_via = 0
matrixSlot2_depth = 70
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 3
matrixSlot3_dest = 12
matrixSlot3_via = 0
matrixSlot3_depth = 60
matrixSlot3_curve = 0
matrixSlot3_en = 1
matrixSlot4_src = 10
matrixSlot4_dest = 2
matrixSlot4_via = 0
matrixSlot4_depth = 30
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 9
matrixSlot5_dest = 9
matrixSlot5_via = 0
matrixSlot5_depth = 45
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 7
matrixSlot6_dest = 16
matrixSlot6_via = 0
matrixSlot6_depth = 35
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 8
matrixSlot7_dest = 7
matrixSlot7_via = 0
matrixSlot7_depth = 50
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Heavy industrial bass riff
            const riff = [ 36, 36, 36, 48, 36, 39, 36, 42 ];
            for (let b = 0; b < 4; b++) {
                for (let i = 0; i < 8; i++) {
                    const tick = (b * BAR) + (i * (PPQ / 2));
                    track.addEvent(tick, [0x90, riff[i], 115]);
                    track.addEvent(tick + (PPQ / 2) - 30, [0x80, riff[i], 64]);
                }
            }
            for (let t = 0; t <= 4 * BAR; t += 30) {
                track.addEvent(t, [0xB0, 1, Math.round(20 + Math.sin(t / (4 * BAR) * Math.PI) * 100)]);
                track.addEvent(t, [0xB0, 74, Math.round(40 + (t / (4 * BAR)) * 80)]);
            }
        }
    },
    {
        num: 60,
        name: "60 Celestial Vocal Choir Pad",
        bpm: 78,
        midiName: "09_Celestial_Vocal_Choir_78BPM.mid",
        conf: `presetName = Celestial Vocal Choir Pad
bank0 = AKWF_hvoice
wave0 = AKWF_hvoice.wav
bank1 = ProphetVS
wave1 = VS 100.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 950
cpABaseWMod = 120
cpBFreq = 0
cpBVol = 850
cpBBaseWMod = -60
cpDetune = 14
cpCutoff = 480
cpResonance = 180
cpFilEnvAmt = 260
cpFilKbdAmt = 420
cpWModAEnv = 150
cpFilAtt = 350
cpFilDec = 550
cpFilSus = 650
cpFilRel = 750
cpAmpAtt = 260
cpAmpDec = 450
cpAmpSus = 920
cpAmpRel = 780
cpLFOFreq = 120
cpLFOAmt = 0
cpLFOPitchAmt = 10
cpLFOWModAmt = 35
cpLFOFilAmt = 45
cpLFOAmpAmt = 0
cpLFO2Freq = 65
cpLFO2Amt = 0
cpModDelay = 120
cpGlide = 100
cpAmpVelocity = 350
cpFilVelocity = 400
cpMasterTune = 0
cpUnisonDetune = 12
cpNoiseVol = 20
cpLFO2PitchAmt = 8
cpLFO2WModAmt = 30
cpLFO2FilAmt = 40
cpLFO2AmpAmt = 0
cpLFOResAmt = 20
cpLFO2ResAmt = 15
cpWModAtt = 300
cpWModDec = 500
cpWModSus = 600
cpWModRel = 700
cpWModBEnv = 120
cpWModVelocity = 250
cpAmpLevel = 900
cpShelvesLsFreq = 180
cpShelvesLsGain = 35
cpShelvesP1Gain = 0
cpShelvesP2Freq = 650
cpShelvesP2Gain = 45
cpShelvesP2Q = 320
cpShelvesHsFreq = 800
cpShelvesHsGain = 50
cpMackityInTrim = 140
cpMackityOutPad = 950
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 4
spBWModType = 2
spLFOShape = 3
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 1
spAmpEnvSlow = 1
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 5
spPresetType = 2
spPresetStyle = 6
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 1
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 2
spFilterMode = 0
spMackity = 0
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 1
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 2
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 1
matrixSlot0_dest = 14
matrixSlot0_via = 0
matrixSlot0_depth = 70
matrixSlot0_curve = 0
matrixSlot0_en = 1
matrixSlot1_src = 4
matrixSlot1_dest = 5
matrixSlot1_via = 13
matrixSlot1_depth = 60
matrixSlot1_curve = 2
matrixSlot1_en = 1
matrixSlot2_src = 3
matrixSlot2_dest = 11
matrixSlot2_via = 0
matrixSlot2_depth = 55
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 9
matrixSlot3_dest = 15
matrixSlot3_via = 0
matrixSlot3_depth = 45
matrixSlot3_curve = 0
matrixSlot3_en = 1
matrixSlot4_src = 15
matrixSlot4_dest = 13
matrixSlot4_via = 0
matrixSlot4_depth = 40
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 5
matrixSlot5_dest = 21
matrixSlot5_via = 0
matrixSlot5_depth = -40
matrixSlot5_curve = 1
matrixSlot5_en = 1
matrixSlot6_src = 7
matrixSlot6_dest = 14
matrixSlot6_via = 0
matrixSlot6_depth = 50
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 12
matrixSlot7_dest = 4
matrixSlot7_via = 0
matrixSlot7_depth = 25
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Ethereal choral hymn progression
            const choir = [
                { start: 0, notes: [ 36, 48, 55, 60, 64, 67 ] }, // C
                { start: 2, notes: [ 33, 45, 52, 57, 60, 64 ] }, // Am
                { start: 4, notes: [ 29, 41, 48, 53, 57, 60 ] }, // F
                { start: 6, notes: [ 31, 43, 50, 55, 59, 62 ] }  // G
            ];
            choir.forEach(c => {
                const on = c.start * BAR;
                c.notes.forEach((p, i) => track.addEvent(on + i * 20, [0x90, p, 90 + i * 4]));
                c.notes.forEach(p => track.addEvent(on + (BAR * 2) - 60, [0x80, p, 64]));
            });
            for (let t = 0; t <= 8 * BAR; t += 30) {
                const phase = t / (8 * BAR);
                track.addEvent(t, [0xB0, 1, Math.round(25 + Math.sin(phase * Math.PI) * 90)]);
                track.addEvent(t, [0xD0, Math.round(Math.max(0, Math.sin(phase * 4 * Math.PI) * 105))]);
                track.addEvent(t, [0xB0, 74, Math.round(30 + Math.sin(phase * 2 * Math.PI) * 80)]);
            }
        }
    },
    {
        num: 61,
        name: "61 Deep Space Pluck & Echo",
        bpm: 122,
        midiName: "10_Deep_Space_Pluck_122BPM.mid",
        conf: `presetName = Deep Space Pluck & Echo
bank0 = AKWF_pluckalgo
wave0 = AKWF_pluckalgo.wav
bank1 = AKWF_stereo
wave1 = AKWF_stereo.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 999
cpABaseWMod = 140
cpBFreq = 0
cpBVol = 850
cpBBaseWMod = 0
cpDetune = 12
cpCutoff = 380
cpResonance = 260
cpFilEnvAmt = 550
cpFilKbdAmt = 480
cpWModAEnv = 200
cpFilAtt = 0
cpFilDec = 320
cpFilSus = 180
cpFilRel = 350
cpAmpAtt = 0
cpAmpDec = 380
cpAmpSus = 280
cpAmpRel = 380
cpLFOFreq = 220
cpLFOAmt = 0
cpLFOPitchAmt = 0
cpLFOWModAmt = 30
cpLFOFilAmt = 20
cpLFOAmpAmt = 0
cpLFO2Freq = 90
cpLFO2Amt = 0
cpModDelay = 0
cpGlide = 0
cpAmpVelocity = 650
cpFilVelocity = 750
cpMasterTune = 0
cpUnisonDetune = 10
cpNoiseVol = 0
cpLFO2PitchAmt = 0
cpLFO2WModAmt = 0
cpLFO2FilAmt = 0
cpLFO2AmpAmt = 0
cpLFOResAmt = 0
cpLFO2ResAmt = 0
cpWModAtt = 0
cpWModDec = 280
cpWModSus = 200
cpWModRel = 300
cpWModBEnv = 0
cpWModVelocity = 400
cpAmpLevel = 940
cpShelvesLsFreq = 160
cpShelvesLsGain = 30
cpShelvesP1Gain = 0
cpShelvesP2Freq = 750
cpShelvesP2Gain = 40
cpShelvesP2Q = 300
cpShelvesHsFreq = 820
cpShelvesHsGain = 55
cpMackityInTrim = 180
cpMackityOutPad = 940
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 5
spBWModType = 0
spLFOShape = 1
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 0
spAmpEnvSlow = 0
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 5
spPresetType = 1
spPresetStyle = 6
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 0
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 0
spFilterMode = 0
spMackity = 1
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 1
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 2
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 1
matrixSlot0_dest = 11
matrixSlot0_via = 0
matrixSlot0_depth = 75
matrixSlot0_curve = 1
matrixSlot0_en = 1
matrixSlot1_src = 5
matrixSlot1_dest = 22
matrixSlot1_via = 0
matrixSlot1_depth = 60
matrixSlot1_curve = 0
matrixSlot1_en = 1
matrixSlot2_src = 4
matrixSlot2_dest = 6
matrixSlot2_via = 0
matrixSlot2_depth = 55
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 3
matrixSlot3_dest = 12
matrixSlot3_via = 0
matrixSlot3_depth = 50
matrixSlot3_curve = 0
matrixSlot3_en = 1
matrixSlot4_src = 7
matrixSlot4_dest = 11
matrixSlot4_via = 0
matrixSlot4_depth = 60
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 13
matrixSlot5_dest = 4
matrixSlot5_via = 0
matrixSlot5_depth = 30
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 9
matrixSlot6_dest = 16
matrixSlot6_via = 0
matrixSlot6_depth = 35
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 8
matrixSlot7_dest = 17
matrixSlot7_via = 0
matrixSlot7_depth = 45
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Cosmic rhythmic pluck sequence
            const pluckNotes = [ 60, 67, 63, 70, 62, 65, 68, 72, 58, 65, 62, 67, 56, 63, 60, 65 ];
            for (let b = 0; b < 2; b++) {
                for (let i = 0; i < 16; i++) {
                    const tick = (b * BAR * 2) + (i * (PPQ / 2));
                    track.addEvent(tick, [0x90, pluckNotes[i], 90 + (i % 3) * 15]);
                    track.addEvent(tick + (PPQ / 2) - 40, [0x80, pluckNotes[i], 64]);
                }
            }
            for (let t = 0; t <= 4 * BAR; t += 30) {
                const phase = t / (4 * BAR);
                track.addEvent(t, [0xB0, 1, Math.round(20 + phase * 95)]);
                track.addEvent(t, [0xB0, 74, Math.round(30 + Math.sin(phase * 4 * Math.PI) * 75)]);
            }
        }
    },
    {
        num: 62,
        name: "62 Cinematic Solstice Swell",
        bpm: 72,
        midiName: "11_Cinematic_Solstice_Swell_72BPM.mid",
        conf: `presetName = Cinematic Solstice Swell
bank0 = AKWF_stringbox
wave0 = AKWF_stringbox.wav
bank1 = AKWF_cello
wave1 = AKWF_cello.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 950
cpABaseWMod = 100
cpBFreq = 0
cpBVol = 900
cpBBaseWMod = -80
cpDetune = 15
cpCutoff = 320
cpResonance = 160
cpFilEnvAmt = 420
cpFilKbdAmt = 350
cpWModAEnv = 180
cpFilAtt = 450
cpFilDec = 600
cpFilSus = 750
cpFilRel = 820
cpAmpAtt = 380
cpAmpDec = 550
cpAmpSus = 950
cpAmpRel = 850
cpLFOFreq = 90
cpLFOAmt = 0
cpLFOPitchAmt = 12
cpLFOWModAmt = 35
cpLFOFilAmt = 55
cpLFOAmpAmt = 0
cpLFO2Freq = 45
cpLFO2Amt = 0
cpModDelay = 150
cpGlide = 120
cpAmpVelocity = 300
cpFilVelocity = 350
cpMasterTune = 0
cpUnisonDetune = 16
cpNoiseVol = 25
cpLFO2PitchAmt = 10
cpLFO2WModAmt = 40
cpLFO2FilAmt = 50
cpLFO2AmpAmt = 0
cpLFOResAmt = 25
cpLFO2ResAmt = 20
cpWModAtt = 400
cpWModDec = 550
cpWModSus = 700
cpWModRel = 800
cpWModBEnv = 160
cpWModVelocity = 250
cpAmpLevel = 900
cpShelvesLsFreq = 160
cpShelvesLsGain = 45
cpShelvesP1Gain = 0
cpShelvesP2Freq = 620
cpShelvesP2Gain = 35
cpShelvesP2Q = 280
cpShelvesHsFreq = 780
cpShelvesHsGain = 40
cpMackityInTrim = 160
cpMackityOutPad = 940
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 4
spBWModType = 5
spLFOShape = 3
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 1
spAmpEnvSlow = 1
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 5
spPresetType = 2
spPresetStyle = 6
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 1
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 1
spFilterMode = 0
spMackity = 1
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 1
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 2
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 9
matrixSlot0_dest = 11
matrixSlot0_via = 0
matrixSlot0_depth = 80
matrixSlot0_curve = 0
matrixSlot0_en = 1
matrixSlot1_src = 1
matrixSlot1_dest = 5
matrixSlot1_via = 0
matrixSlot1_depth = 65
matrixSlot1_curve = 0
matrixSlot1_en = 1
matrixSlot2_src = 3
matrixSlot2_dest = 18
matrixSlot2_via = 0
matrixSlot2_depth = 60
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 4
matrixSlot3_dest = 13
matrixSlot3_via = 0
matrixSlot3_depth = 50
matrixSlot3_curve = 0
matrixSlot3_en = 1
matrixSlot4_src = 16
matrixSlot4_dest = 4
matrixSlot4_via = 0
matrixSlot4_depth = 35
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 5
matrixSlot5_dest = 15
matrixSlot5_via = 0
matrixSlot5_depth = 45
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 7
matrixSlot6_dest = 12
matrixSlot6_via = 0
matrixSlot6_depth = -30
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 6
matrixSlot7_dest = 23
matrixSlot7_via = 0
matrixSlot7_depth = 60
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Massive orchestral pad progression
            const solstice = [
                { start: 0, notes: [ 33, 45, 52, 57, 60, 64 ] }, // Am9
                { start: 2, notes: [ 29, 41, 48, 53, 57, 60 ] }, // Fmaj7
                { start: 4, notes: [ 36, 48, 55, 60, 64, 67 ] }, // C
                { start: 6, notes: [ 31, 43, 50, 55, 59, 62 ] }  // G
            ];
            solstice.forEach(c => {
                const on = c.start * BAR;
                c.notes.forEach((p, i) => track.addEvent(on + i * 24, [0x90, p, 88 + i * 5]));
                c.notes.forEach(p => track.addEvent(on + (BAR * 2) - 60, [0x80, p, 64]));
            });
            for (let t = 0; t <= 8 * BAR; t += 30) {
                const phase = t / (8 * BAR);
                track.addEvent(t, [0xB0, 11, Math.round(30 + Math.sin(phase * Math.PI) * 95)]);
                track.addEvent(t, [0xB0, 1, Math.round(25 + Math.sin(phase * 2 * Math.PI) * 85)]);
                track.addEvent(t, [0xD0, Math.round(Math.max(0, Math.sin(phase * 4 * Math.PI) * 110))]);
            }
        }
    },
    {
        num: 63,
        name: "63 Glitch Cyber Modular Lead",
        bpm: 135,
        midiName: "12_Glitch_Modular_Lead_135BPM.mid",
        conf: `presetName = Glitch Cyber Modular Lead
bank0 = AKWF_bitreduced
wave0 = AKWF_bitreduced.wav
bank1 = AKWF_vgame
wave1 = AKWF_vgame.wav
bank2 = _basic
wave2 = sin.wav
bank3 = _basic
wave3 = saw.wav
cpAFreq = 0
cpAVol = 999
cpABaseWMod = 280
cpBFreq = 0
cpBVol = 850
cpBBaseWMod = 0
cpDetune = 16
cpCutoff = 420
cpResonance = 350
cpFilEnvAmt = 480
cpFilKbdAmt = 500
cpWModAEnv = 250
cpFilAtt = 0
cpFilDec = 250
cpFilSus = 350
cpFilRel = 220
cpAmpAtt = 0
cpAmpDec = 280
cpAmpSus = 650
cpAmpRel = 220
cpLFOFreq = 380
cpLFOAmt = 0
cpLFOPitchAmt = 0
cpLFOWModAmt = 50
cpLFOFilAmt = 40
cpLFOAmpAmt = 0
cpLFO2Freq = 140
cpLFO2Amt = 0
cpModDelay = 0
cpGlide = 100
cpAmpVelocity = 600
cpFilVelocity = 700
cpMasterTune = 0
cpUnisonDetune = 12
cpNoiseVol = 0
cpLFO2PitchAmt = 0
cpLFO2WModAmt = 0
cpLFO2FilAmt = 0
cpLFO2AmpAmt = 0
cpLFOResAmt = 0
cpLFO2ResAmt = 0
cpWModAtt = 0
cpWModDec = 220
cpWModSus = 300
cpWModRel = 200
cpWModBEnv = 0
cpWModVelocity = 400
cpAmpLevel = 940
cpShelvesLsFreq = 150
cpShelvesLsGain = 35
cpShelvesP1Gain = 0
cpShelvesP2Freq = 820
cpShelvesP2Gain = 50
cpShelvesP2Q = 350
cpShelvesHsFreq = 850
cpShelvesHsGain = 60
cpMackityInTrim = 300
cpMackityOutPad = 900
cpArpGate = 833
cpArpSwing = 500
cpArpBpm = 357
spAWModType = 6
spBWModType = 0
spLFOShape = 2
spLFOSpeed = 0
spLFOTargets = 3
spFilEnvSlow = 0
spAmpEnvSlow = 0
spBenderRange = 2
spBenderTarget = 2
spModwheelRange = 2
spModwheelTarget = 0
spUnison = 0
spAssignerPriority = 0
spChromaticPitch = 2
spOscSync = 0
spFilEnvLin = 0
spLFO2Shape = 1
spLFO2Speed = 0
spLFO2Targets = 3
spVoiceCount = 5
spPresetType = 1
spPresetStyle = 2
spAmpEnvLin = 0
spFilEnvLoop = 0
spAmpEnvLoop = 0
spWModEnvSlow = 0
spWModEnvLin = 0
spWModEnvLoop = 0
spPressureRange = 2
spPressureTarget = 0
spLFOTrig = 0
spLFO2Trig = 0
spFilterModel = 0
spFilterMode = 0
spMackity = 1
spArpOctaves = 0
spArpRate = 3
spArpHold = 0
spArpMode = 0
spArpSync = 1
spTimbreTarget = 4
spMPEMode = 1
spMPEPitchBendRange = 2
spReleaseVelocityAmt = 2
voicePattern0 = 0
voicePattern1 = 255
voicePattern2 = 255
voicePattern3 = 255
voicePattern4 = 255
voicePattern5 = 255
matrixSlot0_src = 13
matrixSlot0_dest = 11
matrixSlot0_via = 0
matrixSlot0_depth = 60
matrixSlot0_curve = 0
matrixSlot0_en = 1
matrixSlot1_src = 1
matrixSlot1_dest = 6
matrixSlot1_via = 0
matrixSlot1_depth = 75
matrixSlot1_curve = 0
matrixSlot1_en = 1
matrixSlot2_src = 3
matrixSlot2_dest = 1
matrixSlot2_via = 0
matrixSlot2_depth = 35
matrixSlot2_curve = 0
matrixSlot2_en = 1
matrixSlot3_src = 4
matrixSlot3_dest = 16
matrixSlot3_via = 0
matrixSlot3_depth = 65
matrixSlot3_curve = 0
matrixSlot3_en = 1
matrixSlot4_src = 5
matrixSlot4_dest = 17
matrixSlot4_via = 0
matrixSlot4_depth = 70
matrixSlot4_curve = 0
matrixSlot4_en = 1
matrixSlot5_src = 9
matrixSlot5_dest = 9
matrixSlot5_via = 0
matrixSlot5_depth = 50
matrixSlot5_curve = 0
matrixSlot5_en = 1
matrixSlot6_src = 7
matrixSlot6_dest = 12
matrixSlot6_via = 0
matrixSlot6_depth = 45
matrixSlot6_curve = 0
matrixSlot6_en = 1
matrixSlot7_src = 6
matrixSlot7_dest = 24
matrixSlot7_via = 0
matrixSlot7_depth = -40
matrixSlot7_curve = 0
matrixSlot7_en = 1`,
        generator: (track, PPQ, BAR) => {
            // Glitchy modular lead syncopated pattern
            const glitch = [ 60, 63, 67, 70, 72, 65, 68, 71 ];
            for (let b = 0; b < 4; b++) {
                for (let i = 0; i < 8; i++) {
                    const tick = (b * BAR) + (i * (PPQ / 2));
                    track.addEvent(tick, [0x90, glitch[i], 100 + (i % 2) * 20]);
                    track.addEvent(tick + (PPQ / 2) - 30, [0x80, glitch[i], 64]);
                }
            }
            for (let t = 0; t <= 4 * BAR; t += 30) {
                const phase = t / (4 * BAR);
                track.addEvent(t, [0xB0, 1, Math.round(30 + Math.sin(phase * 4 * Math.PI) * 75)]);
                track.addEvent(t, [0xB0, 74, Math.round(20 + phase * 100)]);
                track.addEvent(t, [0xD0, Math.round(Math.max(0, Math.sin(phase * 8 * Math.PI) * 110))]);
            }
        }
    }
];

// =============================================================================
// Write Presets and Generate MIDI Files
// =============================================================================
const presetsDir = path.join(__dirname, '..', 'disk', 'PRESETS');
const clipsUserDir = path.join(require('os').homedir(), 'Documents', 'Ableton', 'User Library', 'Clips');

console.log('Writing 12 presets to: ' + presetsDir);

presets.forEach(p => {
    // 1. Write Preset Conf File
    const confPath = path.join(presetsDir, `preset_${String(p.num).padStart(4, '0')}.conf`);
    fs.writeFileSync(confPath, p.conf.trim() + '\n', 'utf8');
    console.log(`[Preset] Saved preset_${String(p.num).padStart(4, '0')}.conf: ${p.name}`);

    // 2. Generate MIDI File
    const track = new MidiTrack();
    const PPQ = 480;
    const BAR = PPQ * 4;

    // Track Name
    const nameBuf = Buffer.from(p.name, 'utf8');
    track.addEvent(0, [0xFF, 0x03, nameBuf.length, ...nameBuf]);

    // Tempo
    const usPerQuarter = bpmToMicroseconds(p.bpm);
    const usBuf = Buffer.alloc(3);
    usBuf.writeUIntBE(usPerQuarter, 0, 3);
    track.addEvent(0, [0xFF, 0x51, 0x03, ...usBuf]);

    // Time Signature 4/4
    track.addEvent(0, [0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08]);

    // Call Generator
    p.generator(track, PPQ, BAR);

    const midiBuf = createMidiFile(track, PPQ);
    const localMidiPath = path.join(__dirname, p.midiName);
    fs.writeFileSync(localMidiPath, midiBuf);

    try {
        const userMidiPath = path.join(clipsUserDir, p.midiName);
        fs.writeFileSync(userMidiPath, midiBuf);
    } catch (e) {}

    console.log(`[MIDI] Generated ${p.midiName} (${p.bpm} BPM)`);
});

console.log('All 12 presets and MIDI files successfully generated!');
