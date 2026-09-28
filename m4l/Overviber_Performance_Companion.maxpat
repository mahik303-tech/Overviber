{
	"patcher": {
		"fileversion": 1,
		"appversion": {
			"major": 9,
			"minor": 0,
			"revision": 10,
			"architecture": "x64",
			"modernui": 1
		},
		"classnamespace": "box",
		"rect": [
			80,
			80,
			850,
			680
		],
		"openrect": [
			0,
			0,
			715,
			195
		],
		"openinpresentation": 1,
		"default_fontsize": 12,
		"default_fontface": 0,
		"default_fontname": "Ableton Sans Medium",
		"gridsize": [
			15,
			15
		],
		"devicewidth": 715,
		"boxes": [
			{
				"box": {
					"id": "obj-1",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"int"
					],
					"patching_rect": [
						30,
						30,
						48,
						22
					],
					"text": "midiin"
				}
			},
			{
				"box": {
					"id": "obj-2",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"bang",
						"int",
						"int"
					],
					"patching_rect": [
						100,
						30,
						85,
						22
					],
					"text": "live.thisdevice"
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 8,
					"outlettype": [
						"",
						"",
						"",
						"int",
						"int",
						"",
						"int",
						""
					],
					"patching_rect": [
						30,
						65,
						100,
						22
					],
					"text": "midiparse"
				}
			},
			{
				"box": {
					"id": "obj-4",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 5,
					"outlettype": [
						"",
						"",
						"",
						"",
						""
					],
					"patching_rect": [
						30,
						300,
						160,
						22
					],
					"text": "js overviber_engine.js",
					"saved_object_attributes": {
						"filename": "overviber_engine.js",
						"parameter_enable": 0
					}
				}
			},
			{
				"box": {
					"id": "obj-5",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"int",
						"int"
					],
					"patching_rect": [
						30,
						100,
						60,
						22
					],
					"text": "unpack 0 0"
				}
			},
			{
				"box": {
					"id": "obj-6",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						30,
						135,
						95,
						22
					],
					"text": "pak note 0 0"
				}
			},
			{
				"box": {
					"id": "obj-7",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"int"
					],
					"patching_rect": [
						250,
						100,
						60,
						22
					],
					"text": "loadmess 1"
				}
			},
			{
				"box": {
					"id": "obj-8",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						"bang"
					],
					"patching_rect": [
						250,
						135,
						65,
						22
					],
					"text": "qmetro 25"
				}
			},
			{
				"box": {
					"id": "obj-9",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						250,
						170,
						75,
						22
					],
					"text": "tick 0.025"
				}
			},
			{
				"box": {
					"id": "obj-10",
					"maxclass": "newobj",
					"numinlets": 7,
					"numoutlets": 1,
					"outlettype": [
						"int"
					],
					"patching_rect": [
						30,
						420,
						140,
						22
					],
					"text": "midiformat"
				}
			},
			{
				"box": {
					"id": "obj-11",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 2,
					"outlettype": [
						"int",
						"int"
					],
					"patching_rect": [
						30,
						350,
						65,
						22
					],
					"text": "unpack 0 0"
				}
			},
			{
				"box": {
					"id": "obj-12",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"int",
						"int"
					],
					"patching_rect": [
						80,
						350,
						65,
						22
					],
					"text": "unpack 0 0"
				}
			},
			{
				"box": {
					"id": "obj-13",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						80,
						380,
						50,
						22
					],
					"text": "pak 0 0"
				}
			},
			{
				"box": {
					"id": "obj-14",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						30,
						470,
						55,
						22
					],
					"text": "midiout"
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						350,
						10,
						240,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						10,
						6,
						260,
						18
					],
					"text": "OVERVIBER  //  EXPRESSIVE CHORD & MOD",
					"textcolor": [
						0.95,
						0.65,
						0.15,
						1
					],
					"fontname": "Ableton Sans Bold",
					"fontsize": 10
				}
			},
			{
				"box": {
					"id": "obj-16",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"patching_rect": [
						580,
						10,
						50,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						660,
						6,
						48,
						16
					],
					"text": "PANIC",
					"texton": "PANIC",
					"mode": 0,
					"varname": "Panic",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"val1",
								"val2"
							],
							"parameter_type": 2,
							"parameter_shortname": "Panic",
							"parameter_longname": "Panic"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-17",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						580,
						40,
						45,
						22
					],
					"text": "panic"
				}
			},
			{
				"box": {
					"id": "obj-18",
					"maxclass": "live.tab",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"float"
					],
					"patching_rect": [
						400,
						100,
						140,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						10,
						26,
						220,
						18
					],
					"varname": "ChordMode",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"Direct",
								"Key>Chord",
								"Progression"
							],
							"parameter_type": 2,
							"parameter_unitstyle": 0,
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "Mode",
							"parameter_longname": "ChordMode"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-19",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						400,
						130,
						100,
						22
					],
					"text": "setChordMode $1"
				}
			},
			{
				"box": {
					"id": "obj-20",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"int"
					],
					"patching_rect": [
						400,
						160,
						140,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						10,
						48,
						220,
						16
					],
					"varname": "ChordBank",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"Bank 1: Neo-Soul Velvet",
								"Bank 2: Ambient Horizons",
								"Bank 3: Cyberwave / CS80",
								"Bank 4: Jazz Extensions"
							],
							"parameter_type": 2,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "Bank",
							"parameter_longname": "ChordBank"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-21",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						400,
						190,
						75,
						22
					],
					"text": "setBank $1"
				}
			},
			{
				"box": {
					"id": "obj-22",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"patching_rect": [
						400,
						220,
						50,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						10,
						68,
						52,
						20
					],
					"text": "Pad 1",
					"texton": "Pad 1",
					"mode": 0,
					"varname": "ChordPad1",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"val1",
								"val2"
							],
							"parameter_type": 2,
							"parameter_shortname": "Pad 1",
							"parameter_longname": "ChordPad1"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-23",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						400,
						250,
						30,
						22
					],
					"text": "0"
				}
			},
			{
				"box": {
					"id": "obj-24",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"patching_rect": [
						460,
						220,
						50,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						66,
						68,
						52,
						20
					],
					"text": "Pad 2",
					"texton": "Pad 2",
					"mode": 0,
					"varname": "ChordPad2",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"val1",
								"val2"
							],
							"parameter_type": 2,
							"parameter_shortname": "Pad 2",
							"parameter_longname": "ChordPad2"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-25",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						460,
						250,
						30,
						22
					],
					"text": "1"
				}
			},
			{
				"box": {
					"id": "obj-26",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"patching_rect": [
						520,
						220,
						50,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						122,
						68,
						52,
						20
					],
					"text": "Pad 3",
					"texton": "Pad 3",
					"mode": 0,
					"varname": "ChordPad3",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"val1",
								"val2"
							],
							"parameter_type": 2,
							"parameter_shortname": "Pad 3",
							"parameter_longname": "ChordPad3"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-27",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						520,
						250,
						30,
						22
					],
					"text": "2"
				}
			},
			{
				"box": {
					"id": "obj-28",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"patching_rect": [
						580,
						220,
						50,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						178,
						68,
						52,
						20
					],
					"text": "Pad 4",
					"texton": "Pad 4",
					"mode": 0,
					"varname": "ChordPad4",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"val1",
								"val2"
							],
							"parameter_type": 2,
							"parameter_shortname": "Pad 4",
							"parameter_longname": "ChordPad4"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-29",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						580,
						250,
						30,
						22
					],
					"text": "3"
				}
			},
			{
				"box": {
					"id": "obj-30",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"patching_rect": [
						640,
						220,
						50,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						10,
						92,
						52,
						20
					],
					"text": "Pad 5",
					"texton": "Pad 5",
					"mode": 0,
					"varname": "ChordPad5",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"val1",
								"val2"
							],
							"parameter_type": 2,
							"parameter_shortname": "Pad 5",
							"parameter_longname": "ChordPad5"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-31",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						640,
						250,
						30,
						22
					],
					"text": "4"
				}
			},
			{
				"box": {
					"id": "obj-32",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"patching_rect": [
						700,
						220,
						50,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						66,
						92,
						52,
						20
					],
					"text": "Pad 6",
					"texton": "Pad 6",
					"mode": 0,
					"varname": "ChordPad6",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"val1",
								"val2"
							],
							"parameter_type": 2,
							"parameter_shortname": "Pad 6",
							"parameter_longname": "ChordPad6"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-33",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						700,
						250,
						30,
						22
					],
					"text": "5"
				}
			},
			{
				"box": {
					"id": "obj-34",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"patching_rect": [
						760,
						220,
						50,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						122,
						92,
						52,
						20
					],
					"text": "Pad 7",
					"texton": "Pad 7",
					"mode": 0,
					"varname": "ChordPad7",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"val1",
								"val2"
							],
							"parameter_type": 2,
							"parameter_shortname": "Pad 7",
							"parameter_longname": "ChordPad7"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-35",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						760,
						250,
						30,
						22
					],
					"text": "6"
				}
			},
			{
				"box": {
					"id": "obj-36",
					"maxclass": "live.text",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						""
					],
					"patching_rect": [
						820,
						220,
						50,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						178,
						92,
						52,
						20
					],
					"text": "Pad 8",
					"texton": "Pad 8",
					"mode": 0,
					"varname": "ChordPad8",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"val1",
								"val2"
							],
							"parameter_type": 2,
							"parameter_shortname": "Pad 8",
							"parameter_longname": "ChordPad8"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-37",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						820,
						250,
						30,
						22
					],
					"text": "7"
				}
			},
			{
				"box": {
					"id": "obj-38",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						400,
						300,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						10,
						116,
						44,
						48
					],
					"varname": "StrumTime",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 2,
							"parameter_mmin": 0,
							"parameter_mmax": 150,
							"parameter_initial": [
								35
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "Strum",
							"parameter_longname": "StrumTime"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-39",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						400,
						355,
						75,
						22
					],
					"text": "setStrum $1"
				}
			},
			{
				"box": {
					"id": "obj-40",
					"maxclass": "live.tab",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"float"
					],
					"patching_rect": [
						450,
						300,
						80,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						58,
						120,
						64,
						38
					],
					"varname": "StrumDirection",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"Up",
								"Dn",
								"Alt",
								"Rnd"
							],
							"parameter_type": 2,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "Dir",
							"parameter_longname": "StrumDirection"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-41",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						450,
						355,
						90,
						22
					],
					"text": "setStrumDir $1"
				}
			},
			{
				"box": {
					"id": "obj-42",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						550,
						300,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						126,
						116,
						44,
						48
					],
					"varname": "HumanizeAmount",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 5,
							"parameter_mmin": 0,
							"parameter_mmax": 100,
							"parameter_initial": [
								40
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "Humanize",
							"parameter_longname": "HumanizeAmount"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-43",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						550,
						355,
						95,
						22
					],
					"text": "setHumanize $1"
				}
			},
			{
				"box": {
					"id": "obj-44",
					"maxclass": "live.tab",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"float"
					],
					"patching_rect": [
						650,
						300,
						80,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						174,
						120,
						56,
						38
					],
					"varname": "VoicingSpread",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"Tight",
								"Drop2",
								"Wide"
							],
							"parameter_type": 2,
							"parameter_initial": [
								1
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "Spread",
							"parameter_longname": "VoicingSpread"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-45",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						650,
						355,
						85,
						22
					],
					"text": "setSpread $1"
				}
			},
			{
				"box": {
					"id": "obj-46",
					"maxclass": "live.slider",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						250,
						420,
						40,
						100
					],
					"presentation": 1,
					"presentation_rect": [
						242,
						26,
						50,
						95
					],
					"varname": "ModWheelCutoff",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 0,
							"parameter_mmin": 0,
							"parameter_mmax": 127,
							"parameter_initial": [
								64
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "CC1 Cutoff",
							"parameter_longname": "ModWheelCutoff"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-47",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						250,
						530,
						105,
						22
					],
					"text": "setModWheel $1"
				}
			},
			{
				"box": {
					"id": "obj-48",
					"maxclass": "live.slider",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						300,
						420,
						40,
						100
					],
					"presentation": 1,
					"presentation_rect": [
						296,
						26,
						50,
						95
					],
					"varname": "TimbreWaveMod",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 0,
							"parameter_mmin": 0,
							"parameter_mmax": 127,
							"parameter_initial": [
								64
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "CC74 WaveMod",
							"parameter_longname": "TimbreWaveMod"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-49",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						300,
						530,
						85,
						22
					],
					"text": "setTimbre $1"
				}
			},
			{
				"box": {
					"id": "obj-50",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						350,
						420,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						350,
						26,
						44,
						48
					],
					"varname": "ExpressionDial",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 0,
							"parameter_mmin": 0,
							"parameter_mmax": 127,
							"parameter_initial": [
								100
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "CC11 Expr",
							"parameter_longname": "ExpressionDial"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-51",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						350,
						480,
						105,
						22
					],
					"text": "setExpression $1"
				}
			},
			{
				"box": {
					"id": "obj-52",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						400,
						420,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						400,
						26,
						44,
						48
					],
					"varname": "BreathMackityDial",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 0,
							"parameter_mmin": 0,
							"parameter_mmax": 127,
							"parameter_initial": [
								30
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "CC2 Mackity",
							"parameter_longname": "BreathMackityDial"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-53",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						400,
						480,
						90,
						22
					],
					"text": "setBreath $1"
				}
			},
			{
				"box": {
					"id": "obj-54",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						350,
						520,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						350,
						80,
						44,
						48
					],
					"varname": "ManualPressure",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 0,
							"parameter_mmin": 0,
							"parameter_mmax": 127,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "Aftertouch",
							"parameter_longname": "ManualPressure"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-55",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						350,
						575,
						95,
						22
					],
					"text": "setPressure $1"
				}
			},
			{
				"box": {
					"id": "obj-56",
					"maxclass": "live.numbox",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						400,
						520,
						44,
						15
					],
					"presentation": 1,
					"presentation_rect": [
						400,
						95,
						44,
						15
					],
					"varname": "OctaveShift",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 1,
							"parameter_mmin": -2,
							"parameter_mmax": 2,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "Octave",
							"parameter_longname": "OctaveShift"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-57",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						400,
						550,
						85,
						22
					],
					"text": "setOctave $1"
				}
			},
			{
				"box": {
					"id": "obj-58",
					"maxclass": "live.tab",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"float"
					],
					"patching_rect": [
						500,
						420,
						100,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						456,
						26,
						120,
						16
					],
					"varname": "Lfo1Shape",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"Sin",
								"Tri",
								"Saw",
								"S&H"
							],
							"parameter_type": 2,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "LFO1 Shape",
							"parameter_longname": "Lfo1Shape"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-59",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"int"
					],
					"patching_rect": [
						500,
						450,
						100,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						456,
						46,
						120,
						16
					],
					"varname": "Lfo1Dest",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"CC1 Cutoff",
								"CC74 Timbre",
								"CC11 Expr"
							],
							"parameter_type": 2,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "LFO1 Dest",
							"parameter_longname": "Lfo1Dest"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-60",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						500,
						480,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						456,
						68,
						44,
						48
					],
					"varname": "Lfo1Rate",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 3,
							"parameter_mmin": 0.05,
							"parameter_mmax": 10,
							"parameter_initial": [
								0.5
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "LFO1 Rate",
							"parameter_longname": "Lfo1Rate"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-61",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						550,
						480,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						506,
						68,
						44,
						48
					],
					"varname": "Lfo1Depth",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 5,
							"parameter_mmin": 0,
							"parameter_mmax": 1,
							"parameter_initial": [
								0.4
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "LFO1 Depth",
							"parameter_longname": "Lfo1Depth"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-62",
					"maxclass": "newobj",
					"numinlets": 4,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						500,
						540,
						110,
						22
					],
					"text": "pak 0.5 0.4 0 1"
				}
			},
			{
				"box": {
					"id": "obj-63",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						500,
						570,
						120,
						22
					],
					"text": "setLfo1 $1 $2 $3 $4"
				}
			},
			{
				"box": {
					"id": "obj-64",
					"maxclass": "live.tab",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"float"
					],
					"patching_rect": [
						630,
						420,
						100,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						588,
						26,
						120,
						16
					],
					"varname": "Lfo2Shape",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"Sin",
								"Tri",
								"Drift"
							],
							"parameter_type": 2,
							"parameter_initial": [
								2
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "LFO2 Shape",
							"parameter_longname": "Lfo2Shape"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-65",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"",
						"",
						"int"
					],
					"patching_rect": [
						630,
						450,
						100,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						588,
						46,
						120,
						16
					],
					"varname": "Lfo2Dest",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"Aftertouch",
								"CC2 Breath",
								"Pitch Drift"
							],
							"parameter_type": 2,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "LFO2 Dest",
							"parameter_longname": "Lfo2Dest"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-66",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						630,
						480,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						588,
						68,
						44,
						48
					],
					"varname": "Lfo2Rate",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 3,
							"parameter_mmin": 0.02,
							"parameter_mmax": 5,
							"parameter_initial": [
								0.25
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "LFO2 Rate",
							"parameter_longname": "Lfo2Rate"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-67",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"",
						"float"
					],
					"patching_rect": [
						680,
						480,
						44,
						48
					],
					"presentation": 1,
					"presentation_rect": [
						638,
						68,
						44,
						48
					],
					"varname": "Lfo2Depth",
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_type": 0,
							"parameter_unitstyle": 5,
							"parameter_mmin": 0,
							"parameter_mmax": 1,
							"parameter_initial": [
								0.35
							],
							"parameter_initial_enable": 1,
							"parameter_shortname": "LFO2 Depth",
							"parameter_longname": "Lfo2Depth"
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-68",
					"maxclass": "newobj",
					"numinlets": 4,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						630,
						540,
						110,
						22
					],
					"text": "pak 0.25 0.35 2 1"
				}
			},
			{
				"box": {
					"id": "obj-69",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						630,
						570,
						120,
						22
					],
					"text": "setLfo2 $1 $2 $3 $4"
				}
			},
			{
				"box": {
					"id": "obj-70",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						200,
						300,
						250,
						20
					],
					"presentation": 1,
					"presentation_rect": [
						10,
						168,
						698,
						18
					],
					"text": "Ready: Trigger chords from keyboard or pads to enjoy Overviber's 6-Voice matrix.",
					"textcolor": [
						0.7,
						0.75,
						0.8,
						1
					],
					"fontname": "Ableton Sans Light",
					"fontsize": 9.5
				}
			},
			{
				"box": {
					"id": "obj-71",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 4,
					"outlettype": [
						"",
						"",
						"",
						""
					],
					"patching_rect": [
						200,
						340,
						180,
						22
					],
					"text": "route chordName bankName voicingNotes"
				}
			},
			{
				"box": {
					"id": "obj-72",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						200,
						380,
						150,
						22
					],
					"text": "set Voicing: $1"
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"destination": [
						"obj-3",
						0
					],
					"source": [
						"obj-1",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-5",
						0
					],
					"source": [
						"obj-3",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-6",
						1
					],
					"source": [
						"obj-5",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-6",
						2
					],
					"source": [
						"obj-5",
						1
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-6",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-8",
						0
					],
					"source": [
						"obj-7",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-9",
						0
					],
					"source": [
						"obj-8",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-9",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-11",
						0
					],
					"source": [
						"obj-4",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-10",
						0
					],
					"source": [
						"obj-11",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-10",
						1
					],
					"source": [
						"obj-11",
						1
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-12",
						0
					],
					"source": [
						"obj-4",
						1
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						0
					],
					"source": [
						"obj-12",
						1
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-13",
						1
					],
					"source": [
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-10",
						2
					],
					"source": [
						"obj-13",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-10",
						4
					],
					"source": [
						"obj-4",
						2
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-10",
						5
					],
					"source": [
						"obj-4",
						3
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-14",
						0
					],
					"source": [
						"obj-10",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-17",
						0
					],
					"source": [
						"obj-16",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-17",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-19",
						0
					],
					"source": [
						"obj-18",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-19",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-21",
						0
					],
					"source": [
						"obj-20",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-21",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-23",
						0
					],
					"source": [
						"obj-22",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-23",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-25",
						0
					],
					"source": [
						"obj-24",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-25",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-27",
						0
					],
					"source": [
						"obj-26",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-27",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-29",
						0
					],
					"source": [
						"obj-28",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-29",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-31",
						0
					],
					"source": [
						"obj-30",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-31",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-33",
						0
					],
					"source": [
						"obj-32",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-33",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-35",
						0
					],
					"source": [
						"obj-34",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-35",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-37",
						0
					],
					"source": [
						"obj-36",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-37",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-39",
						0
					],
					"source": [
						"obj-38",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-39",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-41",
						0
					],
					"source": [
						"obj-40",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-41",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-43",
						0
					],
					"source": [
						"obj-42",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-43",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-45",
						0
					],
					"source": [
						"obj-44",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-45",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-47",
						0
					],
					"source": [
						"obj-46",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-47",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-49",
						0
					],
					"source": [
						"obj-48",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-49",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-51",
						0
					],
					"source": [
						"obj-50",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-51",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-53",
						0
					],
					"source": [
						"obj-52",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-53",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-55",
						0
					],
					"source": [
						"obj-54",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-55",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-57",
						0
					],
					"source": [
						"obj-56",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-57",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-62",
						0
					],
					"source": [
						"obj-60",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-62",
						1
					],
					"source": [
						"obj-61",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-62",
						2
					],
					"source": [
						"obj-58",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-62",
						3
					],
					"source": [
						"obj-59",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-63",
						0
					],
					"source": [
						"obj-62",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-63",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-68",
						0
					],
					"source": [
						"obj-66",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-68",
						1
					],
					"source": [
						"obj-67",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-68",
						2
					],
					"source": [
						"obj-64",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-68",
						3
					],
					"source": [
						"obj-65",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-69",
						0
					],
					"source": [
						"obj-68",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-4",
						0
					],
					"source": [
						"obj-69",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-71",
						0
					],
					"source": [
						"obj-4",
						4
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-72",
						0
					],
					"source": [
						"obj-71",
						0
					]
				}
			},
			{
				"patchline": {
					"destination": [
						"obj-70",
						0
					],
					"source": [
						"obj-72",
						0
					]
				}
			}
		],
		"parameters": {
			"obj-16": [
				"Panic",
				"Panic",
				1
			],
			"obj-18": [
				"ChordMode",
				"Mode",
				2
			],
			"obj-20": [
				"ChordBank",
				"Bank",
				3
			],
			"obj-22": [
				"ChordPad1",
				"Pad 1",
				4
			],
			"obj-24": [
				"ChordPad2",
				"Pad 2",
				5
			],
			"obj-26": [
				"ChordPad3",
				"Pad 3",
				6
			],
			"obj-28": [
				"ChordPad4",
				"Pad 4",
				7
			],
			"obj-30": [
				"ChordPad5",
				"Pad 5",
				8
			],
			"obj-32": [
				"ChordPad6",
				"Pad 6",
				9
			],
			"obj-34": [
				"ChordPad7",
				"Pad 7",
				10
			],
			"obj-36": [
				"ChordPad8",
				"Pad 8",
				11
			],
			"obj-38": [
				"StrumTime",
				"Strum",
				12
			],
			"obj-40": [
				"StrumDirection",
				"Dir",
				13
			],
			"obj-42": [
				"HumanizeAmount",
				"Humanize",
				14
			],
			"obj-44": [
				"VoicingSpread",
				"Spread",
				15
			],
			"obj-46": [
				"ModWheelCutoff",
				"CC1 Cutoff",
				16
			],
			"obj-48": [
				"TimbreWaveMod",
				"CC74 WaveMod",
				17
			],
			"obj-50": [
				"ExpressionDial",
				"CC11 Expr",
				18
			],
			"obj-52": [
				"BreathMackityDial",
				"CC2 Mackity",
				19
			],
			"obj-54": [
				"ManualPressure",
				"Aftertouch",
				20
			],
			"obj-56": [
				"OctaveShift",
				"Octave",
				21
			],
			"obj-58": [
				"Lfo1Shape",
				"LFO1 Shape",
				22
			],
			"obj-59": [
				"Lfo1Dest",
				"LFO1 Dest",
				23
			],
			"obj-60": [
				"Lfo1Rate",
				"LFO1 Rate",
				24
			],
			"obj-61": [
				"Lfo1Depth",
				"LFO1 Depth",
				25
			],
			"obj-64": [
				"Lfo2Shape",
				"LFO2 Shape",
				26
			],
			"obj-65": [
				"Lfo2Dest",
				"LFO2 Dest",
				27
			],
			"obj-66": [
				"Lfo2Rate",
				"LFO2 Rate",
				28
			],
			"obj-67": [
				"Lfo2Depth",
				"LFO2 Depth",
				29
			],
			"parameterbanks": {
				"0": {
					"index": 0,
					"name": "Main",
					"parameters": [
						"Panic",
						"ChordMode",
						"ChordBank",
						"ChordPad1",
						"ChordPad2",
						"ChordPad3",
						"ChordPad4",
						"ChordPad5"
					],
					"buttons": [
						"-",
						"-",
						"-",
						"-",
						"-",
						"-",
						"-",
						"-"
					]
				}
			},
			"inherited_shortname": 1
		},
		"dependency_cache": [
			{
				"name": "overviber_engine.js",
				"bootpath": "",
				"type": "TEXT",
				"implicit": 1
			}
		],
		"autosave": 0
	}
}