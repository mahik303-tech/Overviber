const fs = require('fs');
const path = require('path');

function createMaxPatcher() {
    let boxIdCounter = 1;
    function nextId() {
        return "obj-" + (boxIdCounter++);
    }

    const boxes = [];
    const lines = [];
    const parameters = {};
    const paramBank0 = [];
    let paramOrder = 1;

    function registerLiveParam(boxId, longname, shortname) {
        parameters[boxId] = [ longname, shortname, paramOrder++ ];
        if (paramBank0.length < 8) {
            paramBank0.push(longname);
        }
    }

    function addBox(box) {
        boxes.push({ box: box });
        return box.id;
    }

    function addLine(srcId, srcOutlet, dstId, dstInlet) {
        lines.push({
            patchline: {
                destination: [dstId, dstInlet],
                source: [srcId, srcOutlet]
            }
        });
    }

    // --- Core MIDI & Engine Objects ---
    const midiInId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 1,
        numoutlets: 1,
        outlettype: [ "int" ],
        patching_rect: [ 30.0, 30.0, 48.0, 22.0 ],
        text: "midiin"
    });

    const liveThisDeviceId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 1,
        numoutlets: 3,
        outlettype: [ "bang", "int", "int" ],
        patching_rect: [ 100.0, 30.0, 85.0, 22.0 ],
        text: "live.thisdevice"
    });

    const midiParseId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 1,
        numoutlets: 8,
        outlettype: [ "", "", "", "int", "int", "", "int", "" ],
        patching_rect: [ 30.0, 65.0, 100.0, 22.0 ],
        text: "midiparse"
    });
    addLine(midiInId, 0, midiParseId, 0);

    const jsEngineId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 1,
        numoutlets: 5,
        outlettype: [ "", "", "", "", "" ],
        patching_rect: [ 30.0, 300.0, 160.0, 22.0 ],
        text: "js overviber_engine.js",
        saved_object_attributes: {
            filename: "overviber_engine.js",
            parameter_enable: 0
        }
    });

    // Unpack incoming note list [pitch, vel]
    const unpackNoteId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "int", "int" ],
        patching_rect: [ 30.0, 100.0, 60.0, 22.0 ],
        text: "unpack 0 0"
    });
    addLine(midiParseId, 0, unpackNoteId, 0);

    const msgNoteId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 30.0, 135.0, 95.0, 22.0 ],
        text: "pak note 0 0"
    });
    addLine(unpackNoteId, 0, msgNoteId, 1);
    addLine(unpackNoteId, 1, msgNoteId, 2);
    addLine(msgNoteId, 0, jsEngineId, 0);

    // Metro for real-time LFO & Modulation Tick (40Hz = 25ms)
    const toggleMetroId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 1,
        numoutlets: 1,
        outlettype: [ "int" ],
        patching_rect: [ 250.0, 100.0, 60.0, 22.0 ],
        text: "loadmess 1"
    });

    const metroId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "bang" ],
        patching_rect: [ 250.0, 135.0, 65.0, 22.0 ],
        text: "qmetro 25"
    });
    addLine(toggleMetroId, 0, metroId, 0);

    const msgTickId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 250.0, 170.0, 75.0, 22.0 ],
        text: "tick 0.025"
    });
    addLine(metroId, 0, msgTickId, 0);
    addLine(msgTickId, 0, jsEngineId, 0);

    // --- MIDI Output Formatting ---
    const midiFormatId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 7,
        numoutlets: 1,
        outlettype: [ "int" ],
        patching_rect: [ 30.0, 420.0, 140.0, 22.0 ],
        text: "midiformat"
    });

    const makeNoteId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 2,
        numoutlets: 2,
        outlettype: [ "int", "int" ],
        patching_rect: [ 30.0, 350.0, 65.0, 22.0 ],
        text: "unpack 0 0"
    });
    addLine(jsEngineId, 0, makeNoteId, 0);
    addLine(makeNoteId, 0, midiFormatId, 0); // Note Pitch
    addLine(makeNoteId, 1, midiFormatId, 1); // Note Velocity

    // JS Outlet 1: [ccNum, ccVal] -> pak / midiformat CC inlet 2
    const unpackCcId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "int", "int" ],
        patching_rect: [ 80.0, 350.0, 65.0, 22.0 ],
        text: "unpack 0 0"
    });
    addLine(jsEngineId, 1, unpackCcId, 0);

    const pakCcId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 80.0, 380.0, 50.0, 22.0 ],
        text: "pak 0 0"
    });
    addLine(unpackCcId, 1, pakCcId, 0); // Value
    addLine(unpackCcId, 0, pakCcId, 1); // CC Number
    addLine(pakCcId, 0, midiFormatId, 2);

    // JS Outlet 2: Pressure -> midiformat Aftertouch (inlet 4)
    addLine(jsEngineId, 2, midiFormatId, 4);

    // JS Outlet 3: Pitchbend -> midiformat Pitchbend (inlet 5)
    addLine(jsEngineId, 3, midiFormatId, 5);

    const midiOutId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 1,
        numoutlets: 0,
        patching_rect: [ 30.0, 470.0, 55.0, 22.0 ],
        text: "midiout"
    });
    addLine(midiFormatId, 0, midiOutId, 0);

    // =========================================================================
    // PRESENTATION UI CONTROLS (Live Styled)
    // =========================================================================

    // Header Title
    addBox({
        id: nextId(),
        maxclass: "comment",
        numinlets: 1,
        numoutlets: 0,
        patching_rect: [ 350.0, 10.0, 240.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 10.0, 6.0, 260.0, 18.0 ],
        text: "OVERVIBER  //  EXPRESSIVE CHORD & MOD",
        textcolor: [ 0.95, 0.65, 0.15, 1.0 ],
        fontname: "Ableton Sans Bold",
        fontsize: 10.0
    });

    // Panic Button
    const panicBtnBoxId = nextId();
    const panicBtnId = addBox({
        id: panicBtnBoxId,
        maxclass: "live.text",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "" ],
        patching_rect: [ 580.0, 10.0, 50.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 660.0, 6.0, 48.0, 16.0 ],
        text: "PANIC",
        texton: "PANIC",
        mode: 0,
        varname: "Panic",
        saved_attribute_attributes: {
            valueof: {
                parameter_enum: [ "val1", "val2" ],
                parameter_type: 2,
                parameter_shortname: "Panic",
                parameter_longname: "Panic"
            }
        }
    });
    registerLiveParam(panicBtnBoxId, "Panic", "Panic");

    const msgPanicId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 580.0, 40.0, 45.0, 22.0 ],
        text: "panic"
    });
    addLine(panicBtnId, 0, msgPanicId, 0);
    addLine(msgPanicId, 0, jsEngineId, 0);

    // --- SECTION 1: CHORDS & VOICING (Left: X=10, Y=26, W=230, H=145) ---

    // Mode Tab
    const modeTabBoxId = nextId();
    const modeTabId = addBox({
        id: modeTabBoxId,
        maxclass: "live.tab",
        numinlets: 1,
        numoutlets: 3,
        outlettype: [ "", "", "float" ],
        patching_rect: [ 400.0, 100.0, 140.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 10.0, 26.0, 220.0, 18.0 ],
        varname: "ChordMode",
        saved_attribute_attributes: {
            valueof: {
                parameter_enum: [ "Direct", "Key>Chord", "Progression" ],
                parameter_type: 2,
                parameter_unitstyle: 0,
                parameter_initial: [ 1.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "Mode",
                parameter_longname: "ChordMode"
            }
        }
    });
    registerLiveParam(modeTabBoxId, "ChordMode", "Mode");

    const msgModeId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 400.0, 130.0, 100.0, 22.0 ],
        text: "setChordMode $1"
    });
    addLine(modeTabId, 0, msgModeId, 0);
    addLine(msgModeId, 0, jsEngineId, 0);

    // Bank Menu
    const bankMenuBoxId = nextId();
    const bankMenuId = addBox({
        id: bankMenuBoxId,
        maxclass: "live.menu",
        numinlets: 1,
        numoutlets: 3,
        outlettype: [ "", "", "int" ],
        patching_rect: [ 400.0, 160.0, 140.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 10.0, 48.0, 220.0, 16.0 ],
        varname: "ChordBank",
        saved_attribute_attributes: {
            valueof: {
                parameter_enum: [
                    "Bank 1: Neo-Soul Velvet",
                    "Bank 2: Ambient Horizons",
                    "Bank 3: Cyberwave / CS80",
                    "Bank 4: Jazz Extensions"
                ],
                parameter_type: 2,
                parameter_initial: [ 0.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "Bank",
                parameter_longname: "ChordBank"
            }
        }
    });
    registerLiveParam(bankMenuBoxId, "ChordBank", "Bank");

    const msgBankId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 400.0, 190.0, 75.0, 22.0 ],
        text: "setBank $1"
    });
    addLine(bankMenuId, 0, msgBankId, 0);
    addLine(msgBankId, 0, jsEngineId, 0);

    // 8 Trigger Pads (2 rows x 4 cols)
    const padNames = [ "Pad 1", "Pad 2", "Pad 3", "Pad 4", "Pad 5", "Pad 6", "Pad 7", "Pad 8" ];
    for (let i = 0; i < 8; i++) {
        const row = Math.floor(i / 4);
        const col = i % 4;
        const padX = 10.0 + (col * 56.0);
        const padY = 68.0 + (row * 24.0);

        const padBoxId = nextId();
        const padBtnId = addBox({
            id: padBoxId,
            maxclass: "live.text",
            numinlets: 1,
            numoutlets: 2,
            outlettype: [ "", "" ],
            patching_rect: [ 400.0 + (i * 60), 220.0, 50.0, 20.0 ],
            presentation: 1,
            presentation_rect: [ padX, padY, 52.0, 20.0 ],
            text: padNames[i],
            texton: padNames[i],
            mode: 0,
            varname: "ChordPad" + (i + 1),
            saved_attribute_attributes: {
                valueof: {
                    parameter_enum: [ "val1", "val2" ],
                    parameter_type: 2,
                    parameter_shortname: padNames[i],
                    parameter_longname: "ChordPad" + (i + 1)
                }
            }
        });
        registerLiveParam(padBoxId, "ChordPad" + (i + 1), padNames[i]);

        const msgPadTriggerId = addBox({
            id: nextId(),
            maxclass: "message",
            numinlets: 2,
            numoutlets: 1,
            outlettype: [ "" ],
            patching_rect: [ 400.0 + (i * 60), 250.0, 30.0, 22.0 ],
            text: "" + i
        });
        addLine(padBtnId, 0, msgPadTriggerId, 0);
        addLine(msgPadTriggerId, 0, jsEngineId, 0);
    }

    // Strum Dial
    const strumDialBoxId = nextId();
    const strumDialId = addBox({
        id: strumDialBoxId,
        maxclass: "live.dial",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 400.0, 300.0, 44.0, 48.0 ],
        presentation: 1,
        presentation_rect: [ 10.0, 116.0, 44.0, 48.0 ],
        varname: "StrumTime",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 2,
                parameter_mmin: 0.0,
                parameter_mmax: 150.0,
                parameter_initial: [ 35.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "Strum",
                parameter_longname: "StrumTime"
            }
        }
    });
    registerLiveParam(strumDialBoxId, "StrumTime", "Strum");

    const msgStrumId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 400.0, 355.0, 75.0, 22.0 ],
        text: "setStrum $1"
    });
    addLine(strumDialId, 0, msgStrumId, 0);
    addLine(msgStrumId, 0, jsEngineId, 0);

    // Strum Direction Tab
    const strumDirBoxId = nextId();
    const strumDirTabId = addBox({
        id: strumDirBoxId,
        maxclass: "live.tab",
        numinlets: 1,
        numoutlets: 3,
        outlettype: [ "", "", "float" ],
        patching_rect: [ 450.0, 300.0, 80.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 58.0, 120.0, 64.0, 38.0 ],
        varname: "StrumDirection",
        saved_attribute_attributes: {
            valueof: {
                parameter_enum: [ "Up", "Dn", "Alt", "Rnd" ],
                parameter_type: 2,
                parameter_initial: [ 0.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "Dir",
                parameter_longname: "StrumDirection"
            }
        }
    });
    registerLiveParam(strumDirBoxId, "StrumDirection", "Dir");

    const msgStrumDirId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 450.0, 355.0, 90.0, 22.0 ],
        text: "setStrumDir $1"
    });
    addLine(strumDirTabId, 0, msgStrumDirId, 0);
    addLine(msgStrumDirId, 0, jsEngineId, 0);

    // Humanize Dial
    const humDialBoxId = nextId();
    const humanizeDialId = addBox({
        id: humDialBoxId,
        maxclass: "live.dial",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 550.0, 300.0, 44.0, 48.0 ],
        presentation: 1,
        presentation_rect: [ 126.0, 116.0, 44.0, 48.0 ],
        varname: "HumanizeAmount",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 5,
                parameter_mmin: 0.0,
                parameter_mmax: 100.0,
                parameter_initial: [ 40.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "Humanize",
                parameter_longname: "HumanizeAmount"
            }
        }
    });
    registerLiveParam(humDialBoxId, "HumanizeAmount", "Humanize");

    const msgHumanizeId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 550.0, 355.0, 95.0, 22.0 ],
        text: "setHumanize $1"
    });
    addLine(humanizeDialId, 0, msgHumanizeId, 0);
    addLine(msgHumanizeId, 0, jsEngineId, 0);

    // Spread Mode Tab
    const spreadBoxId = nextId();
    const spreadTabId = addBox({
        id: spreadBoxId,
        maxclass: "live.tab",
        numinlets: 1,
        numoutlets: 3,
        outlettype: [ "", "", "float" ],
        patching_rect: [ 650.0, 300.0, 80.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 174.0, 120.0, 56.0, 38.0 ],
        varname: "VoicingSpread",
        saved_attribute_attributes: {
            valueof: {
                parameter_enum: [ "Tight", "Drop2", "Wide" ],
                parameter_type: 2,
                parameter_initial: [ 1.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "Spread",
                parameter_longname: "VoicingSpread"
            }
        }
    });
    registerLiveParam(spreadBoxId, "VoicingSpread", "Spread");

    const msgSpreadId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 650.0, 355.0, 85.0, 22.0 ],
        text: "setSpread $1"
    });
    addLine(spreadTabId, 0, msgSpreadId, 0);
    addLine(msgSpreadId, 0, jsEngineId, 0);

    // --- SECTION 2: XY PAD & MACRO MODULATION (Center: X=242, Y=26, W=230, H=145) ---

    // XY Controller Slider Pair
    const sliderXBoxId = nextId();
    const sliderXId = addBox({
        id: sliderXBoxId,
        maxclass: "live.slider",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 250.0, 420.0, 40.0, 100.0 ],
        presentation: 1,
        presentation_rect: [ 242.0, 26.0, 50.0, 95.0 ],
        varname: "ModWheelCutoff",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 0,
                parameter_mmin: 0.0,
                parameter_mmax: 127.0,
                parameter_initial: [ 64.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "CC1 Cutoff",
                parameter_longname: "ModWheelCutoff"
            }
        }
    });
    registerLiveParam(sliderXBoxId, "ModWheelCutoff", "CC1 Cutoff");

    const msgMwId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 250.0, 530.0, 105.0, 22.0 ],
        text: "setModWheel $1"
    });
    addLine(sliderXId, 0, msgMwId, 0);
    addLine(msgMwId, 0, jsEngineId, 0);

    const sliderYBoxId = nextId();
    const sliderYId = addBox({
        id: sliderYBoxId,
        maxclass: "live.slider",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 300.0, 420.0, 40.0, 100.0 ],
        presentation: 1,
        presentation_rect: [ 296.0, 26.0, 50.0, 95.0 ],
        varname: "TimbreWaveMod",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 0,
                parameter_mmin: 0.0,
                parameter_mmax: 127.0,
                parameter_initial: [ 64.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "CC74 WaveMod",
                parameter_longname: "TimbreWaveMod"
            }
        }
    });
    registerLiveParam(sliderYBoxId, "TimbreWaveMod", "CC74 WaveMod");

    const msgTimbreId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 300.0, 530.0, 85.0, 22.0 ],
        text: "setTimbre $1"
    });
    addLine(sliderYId, 0, msgTimbreId, 0);
    addLine(msgTimbreId, 0, jsEngineId, 0);

    // CC 11 Expression Dial
    const exprDialBoxId = nextId();
    const exprDialId = addBox({
        id: exprDialBoxId,
        maxclass: "live.dial",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 350.0, 420.0, 44.0, 48.0 ],
        presentation: 1,
        presentation_rect: [ 350.0, 26.0, 44.0, 48.0 ],
        varname: "ExpressionDial",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 0,
                parameter_mmin: 0.0,
                parameter_mmax: 127.0,
                parameter_initial: [ 100.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "CC11 Expr",
                parameter_longname: "ExpressionDial"
            }
        }
    });
    registerLiveParam(exprDialBoxId, "ExpressionDial", "CC11 Expr");

    const msgExprId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 350.0, 480.0, 105.0, 22.0 ],
        text: "setExpression $1"
    });
    addLine(exprDialId, 0, msgExprId, 0);
    addLine(msgExprId, 0, jsEngineId, 0);

    // CC 2 Breath / Saturation Dial
    const breathDialBoxId = nextId();
    const breathDialId = addBox({
        id: breathDialBoxId,
        maxclass: "live.dial",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 400.0, 420.0, 44.0, 48.0 ],
        presentation: 1,
        presentation_rect: [ 400.0, 26.0, 44.0, 48.0 ],
        varname: "BreathMackityDial",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 0,
                parameter_mmin: 0.0,
                parameter_mmax: 127.0,
                parameter_initial: [ 30.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "CC2 Mackity",
                parameter_longname: "BreathMackityDial"
            }
        }
    });
    registerLiveParam(breathDialBoxId, "BreathMackityDial", "CC2 Mackity");

    const msgBreathId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 400.0, 480.0, 90.0, 22.0 ],
        text: "setBreath $1"
    });
    addLine(breathDialId, 0, msgBreathId, 0);
    addLine(msgBreathId, 0, jsEngineId, 0);

    // Pressure / Aftertouch Manual Slider
    const pressureDialBoxId = nextId();
    const pressureDialId = addBox({
        id: pressureDialBoxId,
        maxclass: "live.dial",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 350.0, 520.0, 44.0, 48.0 ],
        presentation: 1,
        presentation_rect: [ 350.0, 80.0, 44.0, 48.0 ],
        varname: "ManualPressure",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 0,
                parameter_mmin: 0.0,
                parameter_mmax: 127.0,
                parameter_initial: [ 0.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "Aftertouch",
                parameter_longname: "ManualPressure"
            }
        }
    });
    registerLiveParam(pressureDialBoxId, "ManualPressure", "Aftertouch");

    const msgPressId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 350.0, 575.0, 95.0, 22.0 ],
        text: "setPressure $1"
    });
    addLine(pressureDialId, 0, msgPressId, 0);
    addLine(msgPressId, 0, jsEngineId, 0);

    // Octave Offset
    const octBoxId = nextId();
    const octNumId = addBox({
        id: octBoxId,
        maxclass: "live.numbox",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 400.0, 520.0, 44.0, 15.0 ],
        presentation: 1,
        presentation_rect: [ 400.0, 95.0, 44.0, 15.0 ],
        varname: "OctaveShift",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 1,
                parameter_mmin: -2.0,
                parameter_mmax: 2.0,
                parameter_initial: [ 0.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "Octave",
                parameter_longname: "OctaveShift"
            }
        }
    });
    registerLiveParam(octBoxId, "OctaveShift", "Octave");

    const msgOctId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 400.0, 550.0, 85.0, 22.0 ],
        text: "setOctave $1"
    });
    addLine(octNumId, 0, msgOctId, 0);
    addLine(msgOctId, 0, jsEngineId, 0);

    // --- SECTION 3: DUAL MODULATION LFOs (Right: X=456, Y=26, W=252, H=145) ---

    // LFO 1 Controls
    const lfo1ShapeBoxId = nextId();
    const lfo1ShapeTabId = addBox({
        id: lfo1ShapeBoxId,
        maxclass: "live.tab",
        numinlets: 1,
        numoutlets: 3,
        outlettype: [ "", "", "float" ],
        patching_rect: [ 500.0, 420.0, 100.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 456.0, 26.0, 120.0, 16.0 ],
        varname: "Lfo1Shape",
        saved_attribute_attributes: {
            valueof: {
                parameter_enum: [ "Sin", "Tri", "Saw", "S&H" ],
                parameter_type: 2,
                parameter_initial: [ 0.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "LFO1 Shape",
                parameter_longname: "Lfo1Shape"
            }
        }
    });
    registerLiveParam(lfo1ShapeBoxId, "Lfo1Shape", "LFO1 Shape");

    const lfo1TargetBoxId = nextId();
    const lfo1TargetMenuId = addBox({
        id: lfo1TargetBoxId,
        maxclass: "live.menu",
        numinlets: 1,
        numoutlets: 3,
        outlettype: [ "", "", "int" ],
        patching_rect: [ 500.0, 450.0, 100.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 456.0, 46.0, 120.0, 16.0 ],
        varname: "Lfo1Dest",
        saved_attribute_attributes: {
            valueof: {
                parameter_enum: [ "CC1 Cutoff", "CC74 Timbre", "CC11 Expr" ],
                parameter_type: 2,
                parameter_initial: [ 0.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "LFO1 Dest",
                parameter_longname: "Lfo1Dest"
            }
        }
    });
    registerLiveParam(lfo1TargetBoxId, "Lfo1Dest", "LFO1 Dest");

    const lfo1RateBoxId = nextId();
    const lfo1RateDialId = addBox({
        id: lfo1RateBoxId,
        maxclass: "live.dial",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 500.0, 480.0, 44.0, 48.0 ],
        presentation: 1,
        presentation_rect: [ 456.0, 68.0, 44.0, 48.0 ],
        varname: "Lfo1Rate",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 3,
                parameter_mmin: 0.05,
                parameter_mmax: 10.0,
                parameter_initial: [ 0.5 ],
                parameter_initial_enable: 1,
                parameter_shortname: "LFO1 Rate",
                parameter_longname: "Lfo1Rate"
            }
        }
    });
    registerLiveParam(lfo1RateBoxId, "Lfo1Rate", "LFO1 Rate");

    const lfo1DepthBoxId = nextId();
    const lfo1DepthDialId = addBox({
        id: lfo1DepthBoxId,
        maxclass: "live.dial",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 550.0, 480.0, 44.0, 48.0 ],
        presentation: 1,
        presentation_rect: [ 506.0, 68.0, 44.0, 48.0 ],
        varname: "Lfo1Depth",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 5,
                parameter_mmin: 0.0,
                parameter_mmax: 1.0,
                parameter_initial: [ 0.4 ],
                parameter_initial_enable: 1,
                parameter_shortname: "LFO1 Depth",
                parameter_longname: "Lfo1Depth"
            }
        }
    });
    registerLiveParam(lfo1DepthBoxId, "Lfo1Depth", "LFO1 Depth");

    const pakLfo1Id = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 4,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 500.0, 540.0, 110.0, 22.0 ],
        text: "pak 0.5 0.4 0 1"
    });
    addLine(lfo1RateDialId, 0, pakLfo1Id, 0);
    addLine(lfo1DepthDialId, 0, pakLfo1Id, 1);
    addLine(lfo1ShapeTabId, 0, pakLfo1Id, 2);
    addLine(lfo1TargetMenuId, 0, pakLfo1Id, 3);

    const msgLfo1Id = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 500.0, 570.0, 120.0, 22.0 ],
        text: "setLfo1 $1 $2 $3 $4"
    });
    addLine(pakLfo1Id, 0, msgLfo1Id, 0);
    addLine(msgLfo1Id, 0, jsEngineId, 0);

    // LFO 2 Controls
    const lfo2ShapeBoxId = nextId();
    const lfo2ShapeTabId = addBox({
        id: lfo2ShapeBoxId,
        maxclass: "live.tab",
        numinlets: 1,
        numoutlets: 3,
        outlettype: [ "", "", "float" ],
        patching_rect: [ 630.0, 420.0, 100.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 588.0, 26.0, 120.0, 16.0 ],
        varname: "Lfo2Shape",
        saved_attribute_attributes: {
            valueof: {
                parameter_enum: [ "Sin", "Tri", "Drift" ],
                parameter_type: 2,
                parameter_initial: [ 2.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "LFO2 Shape",
                parameter_longname: "Lfo2Shape"
            }
        }
    });
    registerLiveParam(lfo2ShapeBoxId, "Lfo2Shape", "LFO2 Shape");

    const lfo2TargetBoxId = nextId();
    const lfo2TargetMenuId = addBox({
        id: lfo2TargetBoxId,
        maxclass: "live.menu",
        numinlets: 1,
        numoutlets: 3,
        outlettype: [ "", "", "int" ],
        patching_rect: [ 630.0, 450.0, 100.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 588.0, 46.0, 120.0, 16.0 ],
        varname: "Lfo2Dest",
        saved_attribute_attributes: {
            valueof: {
                parameter_enum: [ "Aftertouch", "CC2 Breath", "Pitch Drift" ],
                parameter_type: 2,
                parameter_initial: [ 0.0 ],
                parameter_initial_enable: 1,
                parameter_shortname: "LFO2 Dest",
                parameter_longname: "Lfo2Dest"
            }
        }
    });
    registerLiveParam(lfo2TargetBoxId, "Lfo2Dest", "LFO2 Dest");

    const lfo2RateBoxId = nextId();
    const lfo2RateDialId = addBox({
        id: lfo2RateBoxId,
        maxclass: "live.dial",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 630.0, 480.0, 44.0, 48.0 ],
        presentation: 1,
        presentation_rect: [ 588.0, 68.0, 44.0, 48.0 ],
        varname: "Lfo2Rate",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 3,
                parameter_mmin: 0.02,
                parameter_mmax: 5.0,
                parameter_initial: [ 0.25 ],
                parameter_initial_enable: 1,
                parameter_shortname: "LFO2 Rate",
                parameter_longname: "Lfo2Rate"
            }
        }
    });
    registerLiveParam(lfo2RateBoxId, "Lfo2Rate", "LFO2 Rate");

    const lfo2DepthBoxId = nextId();
    const lfo2DepthDialId = addBox({
        id: lfo2DepthBoxId,
        maxclass: "live.dial",
        numinlets: 1,
        numoutlets: 2,
        outlettype: [ "", "float" ],
        patching_rect: [ 680.0, 480.0, 44.0, 48.0 ],
        presentation: 1,
        presentation_rect: [ 638.0, 68.0, 44.0, 48.0 ],
        varname: "Lfo2Depth",
        saved_attribute_attributes: {
            valueof: {
                parameter_type: 0,
                parameter_unitstyle: 5,
                parameter_mmin: 0.0,
                parameter_mmax: 1.0,
                parameter_initial: [ 0.35 ],
                parameter_initial_enable: 1,
                parameter_shortname: "LFO2 Depth",
                parameter_longname: "Lfo2Depth"
            }
        }
    });
    registerLiveParam(lfo2DepthBoxId, "Lfo2Depth", "LFO2 Depth");

    const pakLfo2Id = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 4,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 630.0, 540.0, 110.0, 22.0 ],
        text: "pak 0.25 0.35 2 1"
    });
    addLine(lfo2RateDialId, 0, pakLfo2Id, 0);
    addLine(lfo2DepthDialId, 0, pakLfo2Id, 1);
    addLine(lfo2ShapeTabId, 0, pakLfo2Id, 2);
    addLine(lfo2TargetMenuId, 0, pakLfo2Id, 3);

    const msgLfo2Id = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 630.0, 570.0, 120.0, 22.0 ],
        text: "setLfo2 $1 $2 $3 $4"
    });
    addLine(pakLfo2Id, 0, msgLfo2Id, 0);
    addLine(msgLfo2Id, 0, jsEngineId, 0);

    // --- STATUS BAR ---
    const statusTextId = addBox({
        id: nextId(),
        maxclass: "comment",
        numinlets: 1,
        numoutlets: 0,
        patching_rect: [ 200.0, 300.0, 250.0, 20.0 ],
        presentation: 1,
        presentation_rect: [ 10.0, 168.0, 698.0, 18.0 ],
        text: "Ready: Trigger chords from keyboard or pads to enjoy Overviber's 6-Voice matrix.",
        textcolor: [ 0.7, 0.75, 0.8, 1.0 ],
        fontname: "Ableton Sans Light",
        fontsize: 9.5
    });

    const routeStatusId = addBox({
        id: nextId(),
        maxclass: "newobj",
        numinlets: 1,
        numoutlets: 4,
        outlettype: [ "", "", "", "" ],
        patching_rect: [ 200.0, 340.0, 180.0, 22.0 ],
        text: "route chordName bankName voicingNotes"
    });
    addLine(jsEngineId, 4, routeStatusId, 0);

    const msgFormatStatusId = addBox({
        id: nextId(),
        maxclass: "message",
        numinlets: 2,
        numoutlets: 1,
        outlettype: [ "" ],
        patching_rect: [ 200.0, 380.0, 150.0, 22.0 ],
        text: "set Voicing: $1"
    });
    addLine(routeStatusId, 0, msgFormatStatusId, 0);
    addLine(msgFormatStatusId, 0, statusTextId, 0);

    // Pad parameter bank 0 to 8 items
    while (paramBank0.length < 8) {
        paramBank0.push("-");
    }

    parameters["parameterbanks"] = {
        "0": {
            "index": 0,
            "name": "Main",
            "parameters": paramBank0,
            "buttons": [ "-", "-", "-", "-", "-", "-", "-", "-" ]
        }
    };
    parameters["inherited_shortname"] = 1;

    return {
        patcher: {
            fileversion: 1,
            appversion: {
                major: 9,
                minor: 0,
                revision: 10,
                architecture: "x64",
                modernui: 1
            },
            classnamespace: "box",
            rect: [ 80.0, 80.0, 850.0, 680.0 ],
            openrect: [ 0.0, 0.0, 715.0, 195.0 ],
            openinpresentation: 1,
            default_fontsize: 12.0,
            default_fontface: 0,
            default_fontname: "Ableton Sans Medium",
            gridsize: [ 15.0, 15.0 ],
            devicewidth: 715.0,
            boxes: boxes,
            lines: lines,
            parameters: parameters,
            dependency_cache: [
                {
                    name: "overviber_engine.js",
                    bootpath: "",
                    type: "TEXT",
                    implicit: 1
                }
            ],
            autosave: 0
        }
    };
}

function pad4(str) {
    const b = Buffer.from(str, 'utf8');
    const padLen = (4 - (b.length % 4)) % 4;
    return Buffer.concat([b, Buffer.alloc(padLen)]);
}

function makeField(tag, valBuf) {
    const len = 8 + valBuf.length;
    const h = Buffer.alloc(8);
    h.write(tag, 0, 4, 'ascii');
    h.writeUInt32BE(len, 4);
    return Buffer.concat([h, valBuf]);
}

function makeIntField(tag, val) {
    const b = Buffer.alloc(4);
    b.writeUInt32BE(val, 0);
    return makeField(tag, b);
}

function buildAmxd(patcherJsonStr, amxdName, subfiles) {
    const jsonBuf = Buffer.from(patcherJsonStr, 'utf8');
    
    let curOffset = 16 + jsonBuf.length;
    const subBuffers = [];
    const direBuffers = [];
    
    for (const sf of subfiles) {
        const fileBuf = Buffer.from(sf.content, 'utf8');
        subBuffers.push(fileBuf);
        
        const fnamBuf = pad4(sf.name + '\0');
        const dType = makeField('type', pad4(sf.type));
        const dFnam = makeField('fnam', fnamBuf);
        const dSz32 = makeIntField('sz32', fileBuf.length);
        const dOf32 = makeIntField('of32', curOffset);
        const dVers = makeIntField('vers', 0);
        const dFlag = makeIntField('flag', 0);
        const dMdat = makeIntField('mdat', Math.floor(Date.now() / 1000));
        
        const subDireContent = Buffer.concat([dType, dFnam, dSz32, dOf32, dVers, dFlag, dMdat]);
        const direField = makeField('dire', subDireContent);
        direBuffers.push(direField);
        
        curOffset += fileBuf.length;
    }
    
    const mainFnam = pad4(amxdName + '\0');
    const mType = makeField('type', pad4('JSON'));
    const mFnam = makeField('fnam', mainFnam);
    const mSz32 = makeIntField('sz32', jsonBuf.length);
    const mOf32 = makeIntField('of32', 16);
    const mVers = makeIntField('vers', 0);
    const mFlag = makeIntField('flag', 0x11);
    const mMdat = makeIntField('mdat', Math.floor(Date.now() / 1000));
    
    const footerBuf = Buffer.concat([mType, mFnam, mSz32, mOf32, mVers, mFlag, mMdat, ...direBuffers]);
    
    const totalPayloadLen = jsonBuf.length + subBuffers.reduce((a, b) => a + b.length, 0);
    
    const mxcHead = Buffer.alloc(16);
    mxcHead.write('mx@c', 0, 4, 'ascii');
    mxcHead.writeUInt32BE(16, 4);
    mxcHead.writeUInt32BE(0, 8);
    mxcHead.writeUInt32BE(totalPayloadLen, 12);
    
    const ptchPayload = Buffer.concat([mxcHead, jsonBuf, ...subBuffers, footerBuf]);
    
    const ptchHead = Buffer.alloc(8);
    ptchHead.write('ptch', 0, 4, 'ascii');
    ptchHead.writeUInt32LE(ptchPayload.length, 4);
    
    const ampfHead = Buffer.alloc(12);
    ampfHead.write('ampf', 0, 4, 'ascii');
    ampfHead.writeUInt32LE(4, 4);
    ampfHead.write('mmmm', 8, 4, 'ascii'); // MIDI Effect
    
    const metaHead = Buffer.alloc(12);
    metaHead.write('meta', 0, 4, 'ascii');
    metaHead.writeUInt32LE(4, 4);
    metaHead.writeUInt32LE(7, 8);
    
    return Buffer.concat([ampfHead, metaHead, ptchHead, ptchPayload]);
}

const patcherObj = createMaxPatcher();
const jsonContent = JSON.stringify(patcherObj, null, "\t");

const outMaxpatPath = path.join(__dirname, 'Overviber_Performance_Companion.maxpat');
const outAmxdPath = path.join(__dirname, 'Overviber_Performance_Companion.amxd');

fs.writeFileSync(outMaxpatPath, jsonContent, 'utf8');

const jsCode = fs.readFileSync(path.join(__dirname, 'overviber_engine.js'), 'utf8');
const amxdBuf = buildAmxd(jsonContent, 'Overviber_Performance_Companion.amxd', [{
    name: 'overviber_engine.js',
    type: 'TEXT',
    content: jsCode
}]);

fs.writeFileSync(outAmxdPath, amxdBuf);

console.log('Successfully generated:');
console.log(' - ' + outMaxpatPath + ' (' + Buffer.byteLength(jsonContent) + ' bytes)');
console.log(' - ' + outAmxdPath + ' (' + amxdBuf.length + ' bytes)');
