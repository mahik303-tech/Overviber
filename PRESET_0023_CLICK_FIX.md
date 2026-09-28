# Preset 0023: Note-Off-Klicks mit test.mid

Korrektur vom 27.09.2026 für **Saw That Miles Away ST**.

Die bereitgestellte Datei `test.mid` wurde
unverändert als `vst/Tests/fixtures/test.mid` für einen reproduzierbaren Test
übernommen. Sie enthält einen Track mit 34 Note-Ons; mit der MIDI-Zeitbasis
dauert sie 12,8438 Sekunden. Gerendert wurde bei 44,1 kHz, einschließlich
0,5 Sekunden Ausklang, mit Main-Preset-Routing und unverändertem Preset 0023.

## Ursache und Korrektur

`cpAmpRel = 0` beendet die digitale Hüllkurve sehr schnell. `Voice::isActive()`
und `updateEnvelopes()` schalteten daraufhin die Audioberechnung ab, obwohl die
geglättete VCA-Verstärkung noch deutlich über null lag. Ihre vorhandene
Ausblendung konnte nicht fertig werden. Das verursacht einen abrupten Sprung
und ist kein ASIO-Pufferproblem.

Die Stimme bleibt jetzt aktiv, bis die VCA-Verstärkung unter 0,00001 gefallen
ist. Der vorhandene Verlauf mit 0,6 ms Zeitkonstante klingt damit aus. Das
Preset, seine Attack-/Release-Werte und seine Wellenformen wurden nicht geändert.
Die Korrektur gilt für kalibriertes und Legacy-Gain-Staging.

## Messung

| Modus | Größter Sample-Sprung vorher | Nachher |
| --- | ---: | ---: |
| Kalibriert | 0,125114 | 0,022151 |
| Legacy | 0,043041 | 0,007271 |

Der größte Sprung vorher lag bei 2,66175 Sekunden, unmittelbar nach dem
Note-Off von MIDI-Note 35 bei 2,66146 Sekunden. Der größte verbleibende Sprung
liegt an einer anderen Stelle innerhalb des Signals; Sample-Sprünge allein
sind bei einem Sägezahnklang kein universelles Maß für hörbare Klicks.

Eine isolierte Regression mit konstantem Oszillatorsignal prüft zusätzlich,
dass die VCA beim Abschalten ausgeklungen ist. Bei 44,1/48/96 kHz liegt der
Abschaltsprung unter 0,00000008. Die MIDI-Renderings mit Blockgrößen 64 und 512
sind identisch. Der vollständige Release-Build und **13/13 CTest-Tests** bestehen.
Ein abschließender Hörtest in Ableton mit dem UMC1820 wurde hier nicht ausgeführt.

Audiovergleiche und Logs liegen in `build-analysis/`:

- `preset23-before-calibrated.wav` / `preset23-after-calibrated.wav`
- `preset23-before-legacy.wav` / `preset23-after-legacy.wav`
- `preset23-before.log`, `preset23-after.log`, `preset23-after-events.csv`
- `preset23-build.log`, `preset23-tests.log`

Die neue VST3 ist unter `C:\Program Files\Common Files\VST3\Overviber.vst3`
installiert. Der Hash der installierten Binärdatei stimmt mit dem Build überein:
`77159444DBA132C8C1425892F636C14ED95C9B72791678E88A86B46746540CDA`.
Sicherung: `build-analysis/installed-backups/Overviber-before-midi23-fix.vst3`.
