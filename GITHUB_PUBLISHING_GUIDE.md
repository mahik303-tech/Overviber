# 🚀 GitHub Publishing Guide für Overviber (v0.9.0)

Diese Anleitung führt dich Schritt für Schritt durch die Veröffentlichung des **Overviber**-Synthesizers auf GitHub.

---

## Inhaltsverzeichnis
1. [Schritt 1: Lokale Vorbereitung (Bereits erledigt)](#schritt-1-lokale-vorbereitung)
2. [Schritt 2: Repository auf GitHub.com anlegen](#schritt-2-repository-auf-githubcom-anlegen)
3. [Schritt 3: Lokales Projekt mit GitHub verknüpfen](#schritt-3-lokales-projekt-mit-github-verknüpfen)
4. [Schritt 4: Code hochladen (Git Commit & Push)](#schritt-4-code-hochladen-git-commit--push)
5. [Schritt 5: Erstes Release mit VST3/EXE für Musiker bereitstellen](#schritt-5-erstes-release-mit-vst3exe-für-musiker-bereitstellen)
6. [Schritt 6: Screenshots & GitHub-Präsentation](#schritt-6-screenshots--github-präsentation)
7. [Schritt 7: Lizenz- & Open-Source-Hinweise](#schritt-7-lizenz---open-source-hinweise)

---

## Schritt 1: Lokale Vorbereitung
Die lokale Bereinigung wurde bereits vollständig eingerichtet:
- `.gitignore`: Der Ordner `build/` (>600 MB Compiler-Dateien) und temporäre Visual Studio / IDE-Dateien werden ignoriert.
- `juce`: Als Git-Submodul (`.gitmodules`) verknüpft, sodass das Repository leichtgewichtig bleibt und sich überall per `git clone --recurse-submodules` bauen lässt.

---

## Schritt 2: Repository auf GitHub.com anlegen
1. Öffne deinen Browser und logge dich auf [github.com](https://github.com) ein.
2. Klicke oben rechts auf das **`+`**-Symbol und wähle **`New repository`**.
3. Fülle die Felder aus:
   - **Repository name**: `overviber` (oder ein Name deiner Wahl, z. B. `Overviber-Synth`)
   - **Description**: `Studio-grade 6-voice hybrid wavetable / analog synthesizer VST3 & Standalone`
   - **Public**: Auswählen (damit dein Projekt öffentlich sichtbar ist).
4. ⚠️ **WICHTIG**: **Keine Häkchen setzen** bei:
   - *Add a README file*
   - *Add .gitignore*
   - *Choose a license*  
   *(Diese Dateien existieren bereits lokal in deinem Projekt).*
5. Klicke auf den grünen Button **`Create repository`**.

---

## Schritt 3: Lokales Projekt mit GitHub verknüpfen
Öffne ein Terminal (PowerShell) im Projektordner, dem Wurzelverzeichnis dieses Repositorys:

```powershell
# 1. GliGlis Original-Repo als 'upstream' behalten:
git remote rename origin upstream

# 2. Dein neues GitHub-Repo als 'origin' hinzufügen:
# (Ersetze DEIN-NUTZERNAME und DEIN-REPO durch deine tatsächlichen GitHub-Daten!)
git remote add origin https://github.com/DEIN-NUTZERNAME/DEIN-REPO.git

# 3. Haupt-Branch auf 'main' festlegen (moderner GitHub-Standard):
git branch -M main
```

---

## Schritt 4: Code hochladen (Git Commit & Push)
Führe im selben Terminal folgende Befehle aus:

```powershell
# 1. Alle Quellcodedateien, Presets und Dokumente vormerken:
git add .

# 2. Den ersten Release-Commit erstellen:
git commit -m "feat: Overviber v0.9.0 - Studio-grade VST3/Standalone Synth & Modern Industrial Skin"

# 3. Code auf GitHub hochladen:
git push -u origin main
```

*Hinweis*: Falls GitHub nach deinen Zugangsdaten fragt, kannst du dich per Browser-Login oder GitHub Personal Access Token (PAT) authentifizieren.

---

## Schritt 5: Erstes Release mit VST3/EXE für Musiker bereitstellen
Musiker und DAW-Nutzer möchten nicht erst C++ kompilieren, sondern das fertige Plugin direkt als VST3 herunterladen.

### 1. Release-ZIP packen
Erstelle ein Zip-Archiv mit dem Namen `Overviber_v0.9.0_Win64.zip`. Es sollte folgende Dateien enthalten:
- Die VST3-Datei: `build/Overviber_artefacts/Release/VST3/Overviber.vst3`
- Die Standalone-App: `build/Overviber_artefacts/Release/Standalone/Overviber.exe`
- Den Skin-Designer: `build/ModernSkinDesigner_artefacts/Release/Overviber Skin Designer.exe`
- Den Preset-Ordner: `disk/` (enthält alle Factory-Presets und Single-Cycle-Wavetables)
- Eine kurze Textdatei `INSTALL.txt` mit folgendem Inhalt:
  ```text
  Overviber Installation (Windows 64-Bit):
  Kopieren Sie den Ordner 'Overviber.vst3' in Ihr System-VST3-Verzeichnis:
  C:\Program Files\Common Files\VST3\
  ```

### 2. Release auf GitHub hochladen
1. Gehe auf deiner GitHub-Projektseite rechts auf **`Releases`** $\rightarrow$ **`Draft a new release`**.
2. **Choose a tag**: Tippe `v0.9.0` ein und klicke auf *Create new tag*.
3. **Release title**: `Overviber v0.9.0 — SST Ladder, ConsoleX & AFX Workstation Release`
4. **Description**: Den Text aus `RELEASE_NOTES.md` einfügen.
5. Ziehe deine `Overviber_v0.9.0_Win64.zip` in das Dateifeld.
6. Klicke auf **`Publish release`**.

---

## Schritt 6: Screenshots & GitHub-Präsentation
- Mache 1–2 Bildschirmfotos vom Synth (Oszillatoren-Tab mit Wellenform-Editor, Filter mit Frequenzgang und ARP-Visualizer).
- Lege diese z. B. in `docs/` ab und binde sie in der `README.md` ein.
- **GitHub Topics**: Klicke oben rechts auf deiner GitHub-Startseite auf das Zahnrad bei *About* und trage Schlagwörter ein:
  `vst3`, `synthesizer`, `audio-plugin`, `juce`, `cpp`, `dsp`, `wavetable-synth`.

---

## Schritt 7: Lizenz- & Open-Source-Hinweise
Das Projekt unterliegt der **GNU General Public License v3.0 (GPLv3)**. Alle externen Algorithmen sind im Quellcode und in der `README.md` vorbildlich attributiert:
- **GliGli Overcycler**: GPLv3
- **Mutable Instruments (Ripples, Shelves)**: GPLv3
- **Airwindows Mackity**: MIT License
- **D-DIN Typography**: SIL Open Font License 1.1

Alles ist bereits korrekt hinterlegt und rechtlich abgesichert.
