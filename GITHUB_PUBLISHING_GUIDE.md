# GitHub Publishing Guide für Overviber

Das Repository liegt unter [mahik303-tech/Overviber](https://github.com/mahik303-tech/Overviber).
Releases baut und veröffentlicht der Workflow
[`.github/workflows/build-test-release.yml`](.github/workflows/build-test-release.yml);
ZIP-Archive von Hand packen ist nicht mehr nötig.

---

## 1. Neue Version veröffentlichen

Die drei Schritte stehen in [MULTIPLATFORM_GUIDE.md, Abschnitt 5](MULTIPLATFORM_GUIDE.md#neue-version-veröffentlichen):
Version in `CMakeLists.txt`, Release Notes unter `.github/release-notes/v<version>.md`,
nach dem Merge nach `master` den Tag `v<version>` setzen.

Was danach passiert:
- Der Tag startet den Workflow. Er baut und testet Windows, macOS und Linux
  (etwa 10–15 Minuten) und hängt die drei Pakete an das Release:
  `Overviber-v<version>-Windows-x64.zip`, `-macOS-Universal.zip`, `-Linux-x64.tar.gz`.
- Der Text des Releases kommt aus der Release-Notes-Datei, der Titel lautet
  „Overviber v<version>“. Wer das Release über *Releases → Draft a new release*
  anlegt, sieht es sofort; die Pakete erscheinen, sobald der Lauf fertig ist.
- Passt der Tag nicht zur Version oder fehlen die Release Notes, bricht der
  Lauf vor dem Bauen mit einer Fehlermeldung ab.

**Falscher Tag gesetzt?** Den Lauf in *Actions* abbrechen, Release und Tag
löschen (*Releases* bzw. *Tags*) und neu anlegen. Ein Release lässt sich auch
per *Edit* auf einen anderen Tag umstellen.

**Nur die Pakete, ohne Release?** Jeder Lauf auf `master` und jeder Pull
Request legt sie unter *Actions → Lauf → Artifacts* ab.

---

## 2. Präsentation auf GitHub
- 1–2 Bildschirmfotos vom Synth (z. B. Oszillatoren-Tab mit Wellenform-Editor,
  MOD MATRIX, AFX) in `doc/` ablegen und in der `README.md` einbinden.
- **GitHub Topics**: Auf der Startseite des Repositorys beim Zahnrad unter
  *About* Schlagwörter eintragen, etwa
  `vst3`, `synthesizer`, `audio-plugin`, `juce`, `cpp`, `dsp`, `wavetable-synth`.

---

## 3. Lizenz- & Open-Source-Hinweise
Das Projekt unterliegt der **GNU General Public License v3.0 (GPLv3)**. Die
vollständige Liste der verwendeten Arbeiten und ihrer Lizenzen steht in der
[README, Abschnitt „License & Acknowledgements“](README.md#license--acknowledgements);
neue Fremdalgorithmen dort eintragen.
