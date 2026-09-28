// =============================================================================
// Overviber Performance Engine (JavaScript for Max for Live)
// Chord Progression Generator, Humanized Strummer & Multi-Axis CC Modulator
// Designed for Overviber / Overcycler 6-Voice Wavetable Synthesizer
// =============================================================================

inlets = 1;
outlets = 5;
// Outlet 0: [note, velocity] -> to makeNote / midiformat
// Outlet 1: [ccNum, ccVal]   -> to midiformat CC
// Outlet 2: [pressure]      -> to midiformat Channel Pressure
// Outlet 3: [pitchbend]     -> to midiformat Pitch Bend (0..16383)
// Outlet 4: [ui_status]     -> to comment / text displays

// --- Synth Constraints ---
var MAX_VOICES = 6; // Overviber is a 6-voice hybrid synth

// --- State Variables ---
var activeBank = 0;
var chordMode = 1; // 0 = Pass-thru, 1 = Single-key to Chord, 2 = Progression Auto
var strumTimeMs = 35.0; // Strum spread time (0..200 ms)
var strumDirection = 0; // 0 = Up, 1 = Down, 2 = Alt, 3 = Random
var humanizeTiming = 12.0; // Micro-timing deviation (+/- ms)
var humanizeVel = 15.0; // Velocity randomization (+/-)
var rootOctaveOffset = 0; // -2..+2 octaves
var spreadMode = 1; // 0 = Compact, 1 = Wide Pad (Drop-2 & 4), 2 = Open Cluster
var sustainHold = 0;

// XY Modulation
var modWheelVal = 64; // CC 1 (Cutoff)
var timbreVal = 64;   // CC 74 (WaveMod)
var expressionVal = 100; // CC 11
var breathVal = 30;   // CC 2 (Mackity Drive)
var pressureVal = 0;  // Channel Pressure

// LFO Modulation State
var lfo1Shape = 0; // 0 = Sine, 1 = Triangle, 2 = Saw, 3 = Random S&H
var lfo1Rate = 0.5; // Hz or synced beat factor
var lfo1Depth = 0.4;
var lfo1Target = 1; // 1 = ModWheel (CC1), 2 = Timbre (CC74), 3 = Expression (CC11)
var lfo1Phase = 0.0;

var lfo2Shape = 1; // 0 = Sine, 1 = Triangle, 2 = Smooth Drift
var lfo2Rate = 0.25;
var lfo2Depth = 0.35;
var lfo2Target = 1; // 1 = Aftertouch, 2 = Breath (CC2), 3 = Pitch Wobble
var lfo2Phase = 0.0;

var lfoActive = 1;

// Active sounding notes tracking for note-offs
var activeChordNotes = [];

// =============================================================================
// Chord Definition Banks (Tailored for 6-Voice Evolving Overviber Pads)
// Each chord is defined as intervals relative to root note (Semitones)
// =============================================================================
var chordBanks = [
    // Bank 0: Neo-Soul & Warm Velvet (Lush extended chords)
    {
        name: "Neo-Soul & Warm Velvet",
        chords: [
            { name: "C Maj9 (Warm)", intervals: [0, 12, 16, 19, 23, 26] },       // Root, Oct, Maj3, 5th, Maj7, 9th (6 notes)
            { name: "D min11 (Silky)", intervals: [0, 12, 15, 19, 22, 29] },      // Root, Oct, Min3, 5th, Min7, 11th
            { name: "E min9 (Airy)", intervals: [0, 7, 15, 19, 23, 26] },        // Root, 5th, Min3, 5th, Maj7, 9th
            { name: "F Maj7#11 (Lydian)", intervals: [0, 12, 16, 18, 23, 28] },   // Root, Oct, Maj3, #11, Maj7, 9th
            { name: "G 13sus4 (Gospel)", intervals: [0, 7, 17, 21, 22, 28] },     // Root, 5th, 4th, 6th, Min7, 9th
            { name: "A min9 (Deep)", intervals: [0, 12, 15, 19, 22, 26] },       // Root, Oct, Min3, 5th, Min7, 9th
            { name: "Bb Maj9#11 (Cosmic)", intervals: [0, 12, 16, 18, 23, 26] },  // Root, Oct, Maj3, #11, Maj7, 9th
            { name: "Eb Maj13 (Velvet)", intervals: [0, 12, 16, 21, 23, 26] }     // Root, Oct, Maj3, 13th, Maj7, 9th
        ]
    },
    // Bank 1: Ambient & Cinematic Horizons (Open spread panoramic clusters)
    {
        name: "Ambient & Cinematic Horizons",
        chords: [
            { name: "Aurora Sus2add9", intervals: [0, 7, 14, 19, 26, 31] },      // Root, 5th, 9th, 5th+8, 9th+8, 3rd+8 (Spans 3 octaves!)
            { name: "Nebula Min11/9", intervals: [0, 7, 15, 22, 26, 29] },       // Wide open minor ambient pad
            { name: "Elysium Maj7 (Open)", intervals: [0, 12, 19, 23, 28, 35] },  // Open 10th & 14th spread
            { name: "Solstice sus4/9", intervals: [0, 7, 17, 21, 26, 33] },      // Quartal ambient voicing
            { name: "Deep Drift m9(11)", intervals: [0, 10, 15, 22, 26, 29] },    // Low 7th anchor + high shimmer
            { name: "Celestial Lydian", intervals: [0, 7, 16, 18, 23, 30] },     // Lydian dream pad
            { name: "Zenith Add9", intervals: [0, 12, 16, 19, 26, 31] },         // Crystalline wide spread
            { name: "Event Horizon m11", intervals: [0, 5, 10, 15, 22, 29] }      // Ultra-deep 6-voice cluster
        ]
    },
    // Bank 2: Cyberwave & Analog Nostalgia
    {
        name: "Cyberwave & Analog Nostalgia",
        chords: [
            { name: "Neon D min9", intervals: [0, 12, 15, 19, 22, 26] },
            { name: "Blade Runner Sus4", intervals: [0, 7, 12, 17, 22, 26] },
            { name: "Vangelis CS80 Brass", intervals: [0, 12, 16, 19, 23, 28] },
            { name: "Tokyo Nights 7#9", intervals: [0, 12, 16, 22, 27, 31] },
            { name: "Outrun Minor 7", intervals: [0, 7, 12, 15, 19, 22] },
            { name: "Synthwave Maj9", intervals: [0, 12, 16, 19, 23, 26] },
            { name: "Dark City Dim7/9", intervals: [0, 12, 15, 18, 21, 26] },
            { name: "Analog Tape Drift", intervals: [0, 7, 14, 16, 23, 28] }
        ]
    },
    // Bank 3: Jazz Fusion & Harmonic Exploration
    {
        name: "Jazz Fusion & Extensions",
        chords: [
            { name: "C Maj7(9,13)", intervals: [0, 16, 21, 23, 26, 28] },
            { name: "F# 7alt (Hendrix/Jazz)", intervals: [0, 10, 15, 16, 22, 25] },
            { name: "B min(Maj7,9)", intervals: [0, 12, 15, 19, 23, 26] },
            { name: "E 13b9 (Dominant)", intervals: [0, 10, 16, 21, 25, 28] },
            { name: "Ab Maj7#5#11", intervals: [0, 12, 16, 18, 20, 23] },
            { name: "D min9(b5)", intervals: [0, 12, 15, 18, 22, 26] },
            { name: "G 7#11 (Lydian Dom)", intervals: [0, 12, 16, 18, 22, 28] },
            { name: "Db Maj9 (Tender)", intervals: [0, 12, 16, 19, 23, 26] }
        ]
    }
];

// Progression Sequences (indices into active bank)
var progressions = [
    [0, 1, 2, 3], // Progression 1 (Gentle cycle)
    [0, 3, 1, 4], // Progression 2 (Pop/Cinematic uplift)
    [5, 3, 0, 4], // Progression 3 (Minor melancholy)
    [0, 6, 1, 7]  // Progression 4 (Extended colorful turnaround)
];
var currentProgStep = 0;
var currentProgId = 0;

// =============================================================================
// MIDI & Message Inlets Processing
// =============================================================================

function msg_int(val) {
    // Single int triggers chord index 0..7
    if (val >= 0 && val < 8) {
        triggerChord(val, 60, 100);
    }
}

function note(pitch, vel) {
    if (chordMode === 0) {
        // Direct Pass-Through
        outlet(0, pitch, vel);
        return;
    }

    if (vel > 0) {
        // Calculate Chord Index from key (C=0, D=1, E=2, F=3, G=4, A=5, B=6, C2=7...)
        var chordIdx = (pitch - 48) % 8;
        if (chordIdx < 0) chordIdx = (chordIdx + 8) % 8;

        if (chordMode === 2) {
            // Progression Sequencer Mode: advance progression step on note press
            var pList = progressions[currentProgId];
            chordIdx = pList[currentProgStep % pList.length];
            currentProgStep++;
        }

        triggerChord(chordIdx, pitch, vel);
    } else {
        // Release Active Notes
        releaseAllActiveNotes();
    }
}

function triggerChord(chordIdx, rootNote, baseVel) {
    if (chordIdx < 0 || chordIdx >= 8) chordIdx = 0;

    var bank = chordBanks[activeBank];
    var chordDef = bank.chords[chordIdx];

    // Status UI
    outlet(4, "chordName", chordDef.name);
    outlet(4, "bankName", bank.name);
    outlet(4, "voicingNotes", chordDef.intervals.length + " Voices");

    releaseAllActiveNotes();

    var basePitch = rootNote + (rootOctaveOffset * 12);
    // Keep root within playable synth range 24..84
    while (basePitch < 36) basePitch += 12;
    while (basePitch > 68) basePitch -= 12;

    var count = Math.min(chordDef.intervals.length, MAX_VOICES);

    // Build voicings with spread mode
    var pitches = [];
    for (var i = 0; i < count; i++) {
        var p = basePitch + chordDef.intervals[i];

        if (spreadMode === 1 && i % 2 === 1 && p > 48) {
            // Drop voicing (Drop 2/4 down an octave for rich analog pad density)
            p -= 12;
        } else if (spreadMode === 2 && i >= 4) {
            // High shimmer octave up
            p += 12;
        }

        // Clamp to MIDI note range
        if (p < 0) p = 0;
        if (p > 127) p = 127;
        pitches.push(p);
    }

    // Sort or re-order based on strum direction
    if (strumDirection === 1) {
        // Down (High to Low)
        pitches.reverse();
    } else if (strumDirection === 2) {
        // Alternate
        if (currentProgStep % 2 === 1) pitches.reverse();
    } else if (strumDirection === 3) {
        // Random
        for (var k = pitches.length - 1; k > 0; k--) {
            var j = Math.floor(Math.random() * (k + 1));
            var temp = pitches[k];
            pitches[k] = pitches[j];
            pitches[j] = temp;
        }
    }

    // Schedule strummed notes with humanize
    for (var n = 0; n < pitches.length; n++) {
        var notePitch = pitches[n];

        // Velocity humanization
        var vJitter = (Math.random() * 2.0 - 1.0) * humanizeVel;
        var noteVel = Math.round(baseVel + vJitter);
        if (noteVel < 1) noteVel = 1;
        if (noteVel > 127) noteVel = 127;

        // Timing strum delay + micro jitter
        var timeDelay = (n * strumTimeMs) + ((Math.random() * 2.0 - 1.0) * humanizeTiming);
        if (timeDelay < 0) timeDelay = 0;

        scheduleNoteOn(notePitch, noteVel, timeDelay);
    }
}

function scheduleNoteOn(pitch, vel, delayMs) {
    if (delayMs <= 1.0) {
        outlet(0, pitch, vel);
        activeChordNotes.push(pitch);
    } else {
        var task = new Task(function() {
            outlet(0, pitch, vel);
            activeChordNotes.push(pitch);
        });
        task.schedule(delayMs);
    }
}

function releaseAllActiveNotes() {
    for (var i = 0; i < activeChordNotes.length; i++) {
        outlet(0, activeChordNotes[i], 0);
    }
    activeChordNotes = [];
}

// =============================================================================
// XY Modulation & Parameter Handlers
// =============================================================================

function xyPad(x, y) {
    // x: 0.0..1.0 -> CC 1 Mod Wheel (Cutoff sweep)
    // y: 0.0..1.0 -> CC 74 Timbre / Slide (WaveMod sweep)
    var cc1 = Math.round(Math.max(0.0, Math.min(1.0, x)) * 127);
    var cc74 = Math.round(Math.max(0.0, Math.min(1.0, y)) * 127);

    modWheelVal = cc1;
    timbreVal = cc74;

    outlet(1, 1, cc1);   // ModWheel
    outlet(1, 74, cc74); // Timbre / Slide
}

function setModWheel(val) {
    modWheelVal = Math.max(0, Math.min(127, Math.round(val)));
    outlet(1, 1, modWheelVal);
}

function setTimbre(val) {
    timbreVal = Math.max(0, Math.min(127, Math.round(val)));
    outlet(1, 74, timbreVal);
}

function setExpression(val) {
    expressionVal = Math.max(0, Math.min(127, Math.round(val)));
    outlet(1, 11, expressionVal);
}

function setBreath(val) {
    breathVal = Math.max(0, Math.min(127, Math.round(val)));
    outlet(1, 2, breathVal);
}

function setPressure(val) {
    pressureVal = Math.max(0, Math.min(127, Math.round(val)));
    outlet(2, pressureVal);
}

// =============================================================================
// Real-time Modulation Engine Tick (Called by Max metro ~20-50Hz)
// =============================================================================

function tick(deltaSec) {
    if (!lfoActive) return;
    if (deltaSec === undefined || deltaSec <= 0) deltaSec = 0.025; // 25ms (40Hz)

    // Advance LFO 1
    lfo1Phase += lfo1Rate * deltaSec * Math.PI * 2.0;
    if (lfo1Phase > Math.PI * 2.0) lfo1Phase -= Math.PI * 2.0;

    var lfo1Val = 0.0;
    if (lfo1Shape === 0) {
        lfo1Val = Math.sin(lfo1Phase); // Sine
    } else if (lfo1Shape === 1) {
        lfo1Val = (2.0 / Math.PI) * Math.asin(Math.sin(lfo1Phase)); // Triangle
    } else if (lfo1Shape === 2) {
        lfo1Val = (lfo1Phase / Math.PI) - 1.0; // Saw
    } else {
        if (Math.random() < 0.05) lfo1Val = (Math.random() * 2.0) - 1.0;
    }

    // Apply LFO 1 to target
    var lfo1Mod = lfo1Val * lfo1Depth * 63.0;
    if (lfo1Target === 1) {
        var modCC1 = Math.round(Math.max(0, Math.min(127, modWheelVal + lfo1Mod)));
        outlet(1, 1, modCC1);
    } else if (lfo1Target === 2) {
        var modCC74 = Math.round(Math.max(0, Math.min(127, timbreVal + lfo1Mod)));
        outlet(1, 74, modCC74);
    } else if (lfo1Target === 3) {
        var modCC11 = Math.round(Math.max(0, Math.min(127, expressionVal + lfo1Mod)));
        outlet(1, 11, modCC11);
    }

    // Advance LFO 2
    lfo2Phase += lfo2Rate * deltaSec * Math.PI * 2.0;
    if (lfo2Phase > Math.PI * 2.0) lfo2Phase -= Math.PI * 2.0;

    var lfo2Val = 0.0;
    if (lfo2Shape === 0) {
        lfo2Val = Math.sin(lfo2Phase);
    } else if (lfo2Shape === 1) {
        lfo2Val = (2.0 / Math.PI) * Math.asin(Math.sin(lfo2Phase));
    } else {
        lfo2Val = 0.5 * (Math.sin(lfo2Phase) + Math.sin(lfo2Phase * 1.618)); // Organic golden-ratio drift
    }

    // Apply LFO 2 to target (Channel Pressure / Aftertouch for Overviber Mod Matrix Slot 2)
    var lfo2Mod = Math.max(0.0, lfo2Val * lfo2Depth); // Unipolar 0..1
    if (lfo2Target === 1) {
        var atVal = Math.round(lfo2Mod * 127.0);
        outlet(2, atVal); // Aftertouch!
    } else if (lfo2Target === 2) {
        var brVal = Math.round(Math.max(0, Math.min(127, breathVal + (lfo2Val * lfo2Depth * 63.0))));
        outlet(1, 2, brVal); // Breath CC2
    } else if (lfo2Target === 3) {
        var bend = Math.round(8192 + (lfo2Val * lfo2Depth * 2048.0));
        outlet(3, bend); // Pitchbend wow/flutter
    }
}

// =============================================================================
// Configuration Controls (Called from Live UI dials/tabs)
// =============================================================================

function setBank(bankIdx) {
    if (bankIdx >= 0 && bankIdx < chordBanks.length) {
        activeBank = bankIdx;
        outlet(4, "bankName", chordBanks[activeBank].name);
    }
}

function setChordMode(mode) {
    chordMode = mode;
    outlet(4, "chordMode", chordMode === 0 ? "Pass-Thru" : (chordMode === 1 ? "Key-to-Chord" : "Progression"));
}

function setStrum(timeMs) {
    strumTimeMs = Math.max(0.0, Math.min(200.0, timeMs));
}

function setStrumDir(dir) {
    strumDirection = Math.max(0, Math.min(3, dir));
}

function setHumanize(amt) {
    humanizeTiming = amt * 0.25; // 0..25ms
    humanizeVel = amt * 0.20;    // 0..20 vel
}

function setOctave(oct) {
    rootOctaveOffset = Math.max(-2, Math.min(2, oct));
}

function setSpread(mode) {
    spreadMode = Math.max(0, Math.min(2, mode));
}

function setLfo1(rateHz, depth, shape, target) {
    lfo1Rate = Math.max(0.01, Math.min(20.0, rateHz));
    lfo1Depth = Math.max(0.0, Math.min(1.0, depth));
    if (shape !== undefined) lfo1Shape = shape;
    if (target !== undefined) lfo1Target = target;
}

function setLfo2(rateHz, depth, shape, target) {
    lfo2Rate = Math.max(0.01, Math.min(20.0, rateHz));
    lfo2Depth = Math.max(0.0, Math.min(1.0, depth));
    if (shape !== undefined) lfo2Shape = shape;
    if (target !== undefined) lfo2Target = target;
}

function setProgression(progId) {
    if (progId >= 0 && progId < progressions.length) {
        currentProgId = progId;
        currentProgStep = 0;
    }
}

function panic() {
    releaseAllActiveNotes();
    outlet(1, 123, 0); // All Notes Off CC
}
