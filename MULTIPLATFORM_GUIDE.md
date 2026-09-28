# 🌐 Overviber Multiplatform Guide: Windows, macOS & Linux

Dieses Dokument beschreibt die plattformübergreifende Architektur, Installation und Kompilierung von **Overviber** für Musiker und Entwickler auf **Windows**, **macOS** und **Linux**.

---

## 1. Unterstützte Plattformen & Formate

| Betriebssystem | Unterstützte Architekturen | Plugin-Formate | Mindestanforderung |
| :--- | :--- | :--- | :--- |
| **Windows** | `x86_64` (64-Bit) | VST3, Standalone App | Windows 10 oder neuer |
| **macOS** | Universal: `arm64` (Apple Silicon M1–M4) + `x86_64` (Intel) | VST3, AU (Audio Unit v2), Standalone App | macOS 10.15+ (Intel) / macOS 11.0+ (Silicon) |
| **Linux** | `x86_64` (Standard), `aarch64` (ARM) | VST3, Standalone App | Ubuntu 20.04+, Debian 11+, Arch Linux, Fedora |

---

## 2. Installationsanleitung für Musiker

### 🪟 Windows 10 / 11
1. **VST3-Plugin installieren:**
   Kopiere den Ordner `Overviber.vst3` in das Standard-VST3-Verzeichnis deines Systems:
   ```text
   C:\Program Files\Common Files\VST3\
   ```
2. **Presets & Wavetables:**
   Werden beim ersten Start automatisch nach `C:\Users\<DeinNutzer>\Documents\Overviber\` entpackt.
3. **DAW starten:**
   Starte deine DAW (Ableton Live, FL Studio, Reaper, Cubase, Bitwig Studio). Overviber erscheint in deiner Instrumentenliste.
4. **Standalone:**
   Starte `Overviber.exe` direkt für den Betrieb ohne DAW mit ASIO- oder DirectSound-Treibern.

---

### 🍏 macOS (Apple Silicon & Intel)
1. **VST3-Plugin kopieren:**
   Kopiere `Overviber.vst3` nach:
   ```bash
   /Library/Audio/Plug-Ins/VST3/
   # oder für den aktuellen Benutzer:
   ~/Library/Audio/Plug-Ins/VST3/
   ```
2. **Audio Unit (für Apple Logic Pro & GarageBand):**
   Kopiere `Overviber.component` nach:
   ```bash
   /Library/Audio/Plug-Ins/Components/
   # oder:
   ~/Library/Audio/Plug-Ins/Components/
   ```
3. **Standalone App:**
   Ziehe `Overviber.app` in deinen Ordner `/Applications`.

4. ⚠️ **macOS Gatekeeper Quarantäne aufheben:**
   Da GitHub-Builds von Open-Source-Plugins ohne kostenpflichtige Apple-Entwickler-Signatur ausgeliefert werden, blockiert macOS den Start standardmäßig mit *"kann nicht geöffnet werden"*. Führe diesen Befehl im macOS-Terminal aus:
   ```bash
   sudo xattr -cr /Library/Audio/Plug-Ins/VST3/Overviber.vst3
   sudo xattr -cr /Library/Audio/Plug-Ins/Components/Overviber.component
   sudo xattr -cr /Applications/Overviber.app
   ```

---

### 🐧 Linux (Ubuntu, Debian, Arch, Fedora)
1. **Automatische Installation per Skript:**
   Öffne ein Terminal im entpackten Release-Ordner und führe aus:
   ```bash
   chmod +x install_linux.sh
   ./install_linux.sh
   ```
2. **Manuelle Installation:**
   - Erstelle das VST3-Verzeichnis: `mkdir -p ~/.vst3`
   - Kopiere das Plugin: `cp -r Overviber.vst3 ~/.vst3/`
   - Kopiere die Factory-Assets: `cp -r disk ~/Documents/Overviber/`
3. **DAW-Rescan:**
   Starte **Reaper (Native Linux)**, **Bitwig Studio**, **Ardour 8**, **Renoise** oder **Carla** und führe einen Plugin-Rescan durch.

---

## 3. Dateisystem- und Speicherarchitektur

Overviber nutzt die Klasse `OverviberPaths` für einheitliche, vollkommen host-unabhängige Pfade:

| Zweck | Windows | macOS | Linux |
| :--- | :--- | :--- | :--- |
| **Konfigurationen** (Skins, Paletten) | `%APPDATA%\Overviber\` | `~/Library/Application Support/Overviber/` | `~/.config/Overviber/` (`$XDG_CONFIG_HOME`) |
| **Benutzer-Presets** | `Documents\Overviber\PRESETS\` | `~/Documents/Overviber/PRESETS/` | `~/Documents/Overviber/PRESETS/` |
| **Wavetables (AKWF & User)** | `Documents\Overviber\WAVEDATA\` | `~/Documents/Overviber/WAVEDATA/` | `~/Documents/Overviber/WAVEDATA/` |
| **Factory-Asset-Quellen** | Plugin-Ordner / `%PROGRAMDATA%` | Bundle `Contents/Resources/disk/` | `/usr/share/overviber/` oder `~/.vst3/` |

---

## 4. Für Entwickler: Kompilieren aus dem Quellcode

### Voraussetzungen
* **Git** mit Submodul-Unterstützung (`git clone --recurse-submodules`)
* **CMake 3.22** oder neuer
* C++17-fähiger Compiler
* Optional: `-DJUCE_COPY_PLUGIN_AFTER_BUILD=ON` kopiert die Plugins nach jedem Build in die System-Pluginordner (unter Windows mit Administratorrechten). Standard ist `OFF`.

### Windows (MSVC 2022 / Ninja)
```powershell
# 1. Konfiguration
cmake -B build -G "Visual Studio 17 2022" -A x64

# 2. Bauen (Release)
cmake --build build --config Release --parallel

# 3. Tests ausführen
ctest --test-dir build -C Release --output-on-failure
```

### macOS (Universal Binary arm64 + x86_64)
```bash
# 1. Konfiguration für Universal Binary
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15"

# 2. Bauen
cmake --build build --config Release --parallel

# 3. Tests ausführen
ctest --test-dir build -C Release --output-on-failure
```

### Linux (Ubuntu / Debian)
```bash
# 1. Abhängigkeiten installieren
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build \
  libasound2-dev libjack-jackd2-dev libfreetype6-dev libgl1-mesa-dev \
  libx11-dev libxinerama-dev libxext-dev libxrandr-dev libxcursor-dev libxcomposite-dev

# 2. Konfigurieren & Bauen
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

# 3. Tests ausführen (ModernSkinScenarioTest wird ohne X11-Display übersprungen)
ctest --test-dir build -C Release --output-on-failure
```

---

## 5. Automatisierte GitHub Actions CI/CD Pipeline

Im Verzeichnis [`.github/workflows/build-test-release.yml`](.github/workflows/build-test-release.yml) ist eine kontinuierliche Integrations-Pipeline eingerichtet:
* **Matrix-Builds:** Parallele Builds auf Windows (MSVC 2022), macOS (Apple Clang Universal) und Ubuntu 22.04 (GCC).
* **Headless-Tests:** Automatische Ausführung von `FilterScenarioTest`, `ArpScenarioTest`, `StorageScenarioTest`, `ModMatrixScenarioTest` und `AdvancedMidiScenarioTest` bei jedem Commit und Pull Request.
* **Release-Upload:** Sobald ein Git-Tag (z. B. `v0.9.0`) gepusht wird, werden automatisch fertige ZIP- und TAR.GZ-Archive für alle drei Betriebssysteme geschnürt und in GitHub Releases veröffentlicht.
