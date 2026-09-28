const fs = require('fs');
const path = require('path');

// Helper to write Variable-Length Quantity (VLQ) for MIDI delta times
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
        // Sort all events chronologically
        this.events.sort((a, b) => a.tick - b.tick);

        const chunks = [];
        let lastTick = 0;

        for (const ev of this.events) {
            const delta = Math.max(0, ev.tick - lastTick);
            chunks.push(writeVarLen(delta));
            chunks.push(ev.bytes);
            lastTick = ev.tick;
        }

        // End of Track Meta Event
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
    header.writeUInt16BE(0, 8); // Format 0 (single track)
    header.writeUInt16BE(1, 10); // 1 Track
    header.writeUInt16BE(ticksPerQuarter, 12); // PPQN

    return Buffer.concat([header, trackBuf]);
}

// -----------------------------------------------------------------------------
// Generate Expressive MIDI Sequence
// -----------------------------------------------------------------------------
const track = new MidiTrack();
const PPQ = 480; // Ticks per beat (Quarter note)
const BAR = PPQ * 4; // 1920 ticks per bar (4/4)

// Meta: Track Name
const nameStr = "Overviber Evolving Pad & Mod";
const nameBuf = Buffer.from(nameStr, 'utf8');
track.addEvent(0, [0xFF, 0x03, nameBuf.length, ...nameBuf]);

// Meta: Tempo 92 BPM -> 652,173 us/beat (0x09F37D)
track.addEvent(0, [0xFF, 0x51, 0x03, 0x09, 0xF3, 0x7D]);

// Meta: Time Signature 4/4
track.addEvent(0, [0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08]);

// Initial CC & Controller Setup
track.addEvent(0, [0xB0, 1, 20]);   // CC1 Mod Wheel: Low cutoff start
track.addEvent(0, [0xB0, 74, 30]);  // CC74 Timbre: Smooth start
track.addEvent(0, [0xB0, 11, 75]);  // CC11 Expression
track.addEvent(0, [0xB0, 2, 25]);   // CC2 Breath / Mackity Drive
track.addEvent(0, [0xD0, 0]);       // Channel Aftertouch: 0
track.addEvent(0, [0xE0, 0x00, 0x40]); // Pitch Bend Center (8192)

// =============================================================================
// 8-Bar Harmonic Progression (Tailored for Overviber 6-Voice Pad & Matrix)
// 1. C Maj9(13)     [C2, G2, E3, B3, D4, A4]     (Bar 1..2)
// 2. A min11        [A1, E2, G3, C4, D4, G4]     (Bar 3..4)
// 3. F Maj7(#11)    [F1, C2, A3, E4, G4, B4]     (Bar 5..6)
// 4. G 13sus4 > Em9 [G1, D2, F3, B3, E4, A4]     (Bar 7..8)
// =============================================================================

const chordProgression = [
    {
        startBar: 0,
        numBars: 2,
        notes: [ 36, 43, 52, 59, 62, 69 ], // C2, G2, E3, B3, D4, A4
        name: "C Maj9(13)"
    },
    {
        startBar: 2,
        numBars: 2,
        notes: [ 33, 40, 55, 60, 62, 67 ], // A1, E2, G3, C4, D4, G4
        name: "A min11"
    },
    {
        startBar: 4,
        numBars: 2,
        notes: [ 29, 36, 57, 64, 67, 71 ], // F1, C2, A3, E4, G4, B4
        name: "F Maj7(#11)"
    },
    {
        startBar: 6,
        numBars: 2,
        notes: [ 31, 38, 53, 59, 64, 69 ], // G1, D2, F3, B3, E4, A4
        name: "G 13sus4"
    }
];

const totalBars = 8;
const totalTicks = totalBars * BAR;

// Add Chords with Strumming & Humanized Velocity
chordProgression.forEach((chord, chordIdx) => {
    const chordStartTick = chord.startBar * BAR;
    const chordDurationTicks = (chord.numBars * BAR) - 60; // Leave 60 ticks breathing space before next chord

    // Strum delay: 24 ticks (~30ms at 92 BPM) per voice
    const strumStep = 24;

    chord.notes.forEach((pitch, noteIdx) => {
        const noteOnTick = chordStartTick + (noteIdx * strumStep);
        const noteOffTick = chordStartTick + chordDurationTicks;
        
        // Humanized dynamic velocities
        const baseVel = 88 + (noteIdx * 4); // Higher voices slightly more pronounced
        const velJitter = ((chordIdx * 3 + noteIdx * 5) % 9) - 4;
        const velocity = Math.max(60, Math.min(115, baseVel + velJitter));

        // Note On
        track.addEvent(noteOnTick, [0x90, pitch, velocity]);

        // Note Off
        track.addEvent(noteOffTick, [0x80, pitch, 64]);
    });
});

// =============================================================================
// Generate Continuous Dynamic Modulation Curves (CC1, CC74, CC11, CC2, AT, PB)
// =============================================================================

// High resolution modulation updates every 16th note (120 ticks)
const stepTicks = 30; // ~38ms resolution for ultra-smooth sweeps

for (let tick = 0; tick <= totalTicks; tick += stepTicks) {
    const progress = tick / totalTicks; // 0.0 .. 1.0 (over 8 bars)
    const barPhase = (tick % BAR) / BAR; // 0.0 .. 1.0 (within current bar)
    const chordPhase = (tick % (BAR * 2)) / (BAR * 2); // 0.0 .. 1.0 (within 2-bar chord)

    // 1. CC 1 (Mod Wheel / Overviber Filter Cutoff - Slot 0)
    // Starts closed (25), swells gently across 8 bars up to peak (115) at bar 6-7, then recedes
    const macroSwell = Math.sin(progress * Math.PI); // 0 -> 1 -> 0
    const localSwell = Math.sin(chordPhase * Math.PI) * 0.35;
    const cc1Val = Math.round(Math.max(15, Math.min(127, 25 + (macroSwell * 75) + (localSwell * 30))));
    track.addEvent(tick, [0xB0, 1, cc1Val]);

    // 2. CC 74 (Timbre / WaveMod Depth - Slot 1)
    // Sweeps the Wavefolder & Crossover shape in dynamic waves
    const waveModShape = 0.5 + 0.5 * Math.sin((progress * 4.0 * Math.PI) + (barPhase * Math.PI));
    const cc74Val = Math.round(Math.max(20, Math.min(125, 30 + waveModShape * 85)));
    track.addEvent(tick, [0xB0, 74, cc74Val]);

    // 3. CC 11 (Expression / Pad Amplitude Swell - Slot 3)
    // Breathing pad swell: softens at chord changes, blossoms during chord sustain
    const exprShape = Math.sin(chordPhase * Math.PI);
    const cc11Val = Math.round(Math.max(45, Math.min(127, 60 + exprShape * 65)));
    track.addEvent(tick, [0xB0, 11, cc11Val]);

    // 4. CC 2 (Breath / Mackity Saturation Drive)
    // Drive pushes into warm console saturation during intense peaks (Bar 5-7)
    const drivePeak = Math.pow(Math.sin(progress * Math.PI), 2.0);
    const cc2Val = Math.round(Math.max(10, Math.min(115, 20 + drivePeak * 85)));
    track.addEvent(tick, [0xB0, 2, cc2Val]);

    // 5. Channel Aftertouch (Pressure - Overviber Mod Matrix Slot 2)
    // Expressive finger pressure pushes into the pad during the middle of each 2-bar chord
    // This activates LFO 1 depth in our pad preset for organic vibrato & shimmer
    let atVal = 0;
    if (chordPhase > 0.25 && chordPhase < 0.85) {
        const atPhase = (chordPhase - 0.25) / 0.60;
        atVal = Math.round(Math.sin(atPhase * Math.PI) * 110);
    }
    track.addEvent(tick, [0xD0, atVal]);

    // 6. Pitch Bend (Subtle Analog Tape Wow / Drift)
    // Subtle gentle tape flutter (+/- 40 units out of 8192)
    const drift = Math.sin(tick * 0.005) * 45;
    const bendVal = Math.round(8192 + drift);
    const lsb = bendVal & 0x7F;
    const msb = (bendVal >> 7) & 0x7F;
    track.addEvent(tick, [0xE0, lsb, msb]);
}

// Reset Controllers at the end
track.addEvent(totalTicks + 240, [0xB0, 1, 0]);
track.addEvent(totalTicks + 240, [0xB0, 74, 0]);
track.addEvent(totalTicks + 240, [0xB0, 11, 100]);
track.addEvent(totalTicks + 240, [0xD0, 0]);
track.addEvent(totalTicks + 240, [0xE0, 0x00, 0x40]);

// Build MIDI file buffer
const midiBuf = createMidiFile(track, PPQ);

// Output paths
const localPath = path.join(__dirname, 'Overviber_Pad_Demo_Progression.mid');
const abletonClipsPath = path.join(require('os').homedir(), 'Documents', 'Ableton', 'User Library', 'Clips', 'Overviber_Pad_Demo_Progression.mid');

fs.writeFileSync(localPath, midiBuf);
console.log('Generated local MIDI file: ' + localPath + ' (' + midiBuf.length + ' bytes)');

try {
    fs.writeFileSync(abletonClipsPath, midiBuf);
    console.log('Saved directly to Ableton Clips: ' + abletonClipsPath);
} catch (err) {
    console.warn('Could not copy to Clips folder:', err.message);
}
