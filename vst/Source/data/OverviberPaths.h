#pragma once

#include <juce_core/juce_core.h>

// ==============================================================================
// OverviberPaths: Unified Cross-Platform Storage & Directory Management
// Supports: Windows, macOS, Linux
// ==============================================================================
class OverviberPaths {
public:
    /** Returns the platform-specific application config directory.
     *  Windows: %APPDATA%/Overviber (e.g. C:\Users\<User>\AppData\Roaming\Overviber)
     *  macOS:   ~/Library/Application Support/Overviber
     *  Linux:   ~/.config/Overviber ($XDG_CONFIG_HOME/Overviber)
     */
    static juce::File getAppConfigDirectory() {
        if (configDirectoryOverride() != juce::File()) return configDirectoryOverride();
#if JUCE_MAC
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Application Support")
            .getChildFile("Overviber");
#elif JUCE_LINUX
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Overviber");
#else // Windows
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Overviber");
#endif
    }

    /** Redirects getAppConfigDirectory(), e.g. so UI tests never touch the user's
     *  real skin_config.conf / user_palettes.conf. Pass juce::File() to reset. */
    static void setAppConfigDirectoryOverride(const juce::File& directory) {
        configDirectoryOverride() = directory;
    }

    /** Returns the cross-platform user Documents directory for Overviber.
     *  Windows: C:\Users\<User>\Documents\Overviber
     *  macOS:   /Users/<User>/Documents/Overviber
     *  Linux:   ~/Documents/Overviber (or $XDG_DOCUMENTS_DIR/Overviber)
     */
    static juce::File getDocumentsDirectory() {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Overviber");
    }

    /** Returns the user Presets directory (Documents/Overviber/PRESETS). */
    static juce::File getPresetsDirectory() {
        return getDocumentsDirectory().getChildFile("PRESETS");
    }

    /** Returns the user Wavetables directory (Documents/Overviber/WAVEDATA). */
    static juce::File getWaveDataDirectory() {
        return getDocumentsDirectory().getChildFile("WAVEDATA");
    }

    /** Returns the user custom wave directory (Documents/Overviber/WAVEDATA/User). */
    static juce::File getUserWaveDirectory() {
        return getWaveDataDirectory().getChildFile("User");
    }

    /** Returns the folder of the Lua skin script (Documents/Overviber/LUA). */
    static juce::File getLuaDirectory() {
        if (luaDirectoryOverride() != juce::File()) return luaDirectoryOverride();
        return getDocumentsDirectory().getChildFile("LUA");
    }

    /** Redirects getLuaDirectory(), so tests never read or write the user's
     *  real skin.lua. Pass juce::File() to reset. */
    static void setLuaDirectoryOverride(const juce::File& directory) {
        luaDirectoryOverride() = directory;
    }

    /** Finds the factory disk directory containing PRESETS and WAVEDATA across bundle & binary locations. */
    static juce::File findFactoryDiskDirectory() {
        auto checkDir = [](const juce::File& dir) -> juce::File {
            if (!dir.exists()) return {};
            if (dir.getChildFile("PRESETS").isDirectory() && dir.getChildFile("WAVEDATA").isDirectory()) {
                return dir;
            }
            if (dir.getChildFile("disk").getChildFile("PRESETS").isDirectory() &&
                dir.getChildFile("disk").getChildFile("WAVEDATA").isDirectory()) {
                return dir.getChildFile("disk");
            }
            return {};
        };

        // 1. On macOS: Check inside plugin / application bundle Contents/Resources
#if JUCE_MAC
        juce::File appBundle = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
        if (appBundle.exists()) {
            auto bundleRes = checkDir(appBundle.getChildFile("Contents").getChildFile("Resources"));
            if (bundleRes.exists()) return bundleRes;
        }
#endif

        // 2. Probe around current executable / plugin file hierarchy
        juce::File exec = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        juce::File curr = exec.getParentDirectory();
        for (int depth = 0; depth < 6 && curr.exists(); ++depth) {
            auto found = checkDir(curr);
            if (found.exists()) return found;

            // Check bundle / sibling Resources
            auto resFound = checkDir(curr.getChildFile("Resources"));
            if (resFound.exists()) return resFound;

            curr = curr.getParentDirectory();
        }

        // 3. Probe current working directory
        auto cwdFound = checkDir(juce::File::getCurrentWorkingDirectory());
        if (cwdFound.exists()) return cwdFound;

        // 4. Probe common system application data (Windows %PROGRAMDATA%, Mac /Library/Application Support)
        auto commonApp = checkDir(juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory).getChildFile("Overviber"));
        if (commonApp.exists()) return commonApp;

        // 5. On Linux: Probe standard distribution data directories (/usr/share, /usr/local/share, /opt)
#if JUCE_LINUX
        auto linuxShare = checkDir(juce::File("/usr/share/overviber"));
        if (linuxShare.exists()) return linuxShare;
        auto linuxLocalShare = checkDir(juce::File("/usr/local/share/overviber"));
        if (linuxLocalShare.exists()) return linuxLocalShare;
        auto linuxOpt = checkDir(juce::File("/opt/overviber"));
        if (linuxOpt.exists()) return linuxOpt;
#endif

        // 6. Probe Documents/Overviber/disk
        auto docsDisk = checkDir(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Overviber"));
        if (docsDisk.exists()) return docsDisk;

        return {};
    }

    /** Automatically creates necessary folders and seeds factory presets & wavetables on first run. */
    static void initializeStorage() {
        initializeStorage(getAppConfigDirectory(), getDocumentsDirectory(), findFactoryDiskDirectory());
    }

    // Explicit destinations let tests and portable installations seed their own
    // storage without writing to the user's real Documents or configuration.
    static void initializeStorage(const juce::File& configDir, const juce::File& docDir,
                                  const juce::File& factoryDisk) {
        // 1. Config directory (skin_config.conf, user_palettes.conf)
        if (!configDir.exists()) {
            configDir.createDirectory();
        }

        // 2. Documents directory structure
        if (!docDir.exists()) docDir.createDirectory();

        auto presetsDir = docDir.getChildFile("PRESETS");
        if (!presetsDir.exists()) presetsDir.createDirectory();

        auto waveDir = docDir.getChildFile("WAVEDATA");
        if (!waveDir.exists()) waveDir.createDirectory();

        auto userWaveDir = waveDir.getChildFile("User");
        if (!userWaveDir.exists()) userWaveDir.createDirectory();

        // 3. Auto-seed factory assets if user directories are empty
        if (factoryDisk.exists()) {
            // Seed presets if destination has no .conf files
            auto existingPresets = presetsDir.findChildFiles(juce::File::findFiles, false, "*.conf");
            if (existingPresets.isEmpty()) {
                auto factoryPresets = factoryDisk.getChildFile("PRESETS");
                if (factoryPresets.isDirectory()) {
                    auto files = factoryPresets.findChildFiles(juce::File::findFiles, false, "*.conf");
                    for (const auto& f : files) {
                        f.copyFileTo(presetsDir.getChildFile(f.getFileName()));
                    }
                }
            }

            // Seed wavetables if destination has no wave subfolders other than "User"
            auto existingWaveDirs = waveDir.findChildFiles(juce::File::findDirectories, false);
            bool hasFactoryWaves = false;
            for (const auto& d : existingWaveDirs) {
                if (d.getFileName().toLowerCase() != "user") {
                    hasFactoryWaves = true;
                    break;
                }
            }
            if (!hasFactoryWaves) {
                auto factoryWaves = factoryDisk.getChildFile("WAVEDATA");
                if (factoryWaves.isDirectory()) {
                    auto subDirs = factoryWaves.findChildFiles(juce::File::findDirectories, false);
                    for (const auto& d : subDirs) {
                        d.copyDirectoryTo(waveDir.getChildFile(d.getFileName()));
                    }
                }
            }

            // Seed initial skin_config.conf and user_palettes.conf if missing in config dir
            auto skinConf = configDir.getChildFile("skin_config.conf");
            if (!skinConf.existsAsFile()) {
                auto factorySkinConf = factoryDisk.getChildFile("skin_config.conf");
                if (factorySkinConf.existsAsFile()) {
                    factorySkinConf.copyFileTo(skinConf);
                }
            }

            auto userPalettes = configDir.getChildFile("user_palettes.conf");
            if (!userPalettes.existsAsFile()) {
                auto factoryPalettes = factoryDisk.getChildFile("user_palettes.conf");
                if (factoryPalettes.existsAsFile()) {
                    factoryPalettes.copyFileTo(userPalettes);
                }
            }
        }
    }

private:
    static juce::File& configDirectoryOverride() {
        static juce::File directory;
        return directory;
    }
    static juce::File& luaDirectoryOverride() {
        static juce::File directory;
        return directory;
    }
};
