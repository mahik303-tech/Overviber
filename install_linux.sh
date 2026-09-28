#!/usr/bin/env bash
# ==============================================================================
# Overviber Linux VST3 & Asset Installer
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_VST3_DIR="${HOME}/.vst3"
TARGET_DOCS_DIR="${HOME}/Documents/Overviber"

echo "========================================================"
echo " Overviber v0.9.0 - Linux Installation"
echo "========================================================"

# 1. Install VST3 plugin bundle
echo "[1/3] Installing VST3 plugin..."
mkdir -p "${TARGET_VST3_DIR}"
if [ -d "${SCRIPT_DIR}/Overviber.vst3" ]; then
    cp -r "${SCRIPT_DIR}/Overviber.vst3" "${TARGET_VST3_DIR}/"
    chmod -R 755 "${TARGET_VST3_DIR}/Overviber.vst3"
    echo "      -> Copied to: ${TARGET_VST3_DIR}/Overviber.vst3"
else
    echo "      [ERROR] Overviber.vst3 directory not found in ${SCRIPT_DIR}"
    exit 1
fi

# 2. Seed factory disk presets and wavetables
echo "[2/3] Seeding factory presets & wavetables..."
mkdir -p "${TARGET_DOCS_DIR}"
if [ -d "${SCRIPT_DIR}/disk" ]; then
    cp -r "${SCRIPT_DIR}/disk" "${TARGET_DOCS_DIR}/"
    chmod -R 755 "${TARGET_DOCS_DIR}/disk"
    echo "      -> Seeded into: ${TARGET_DOCS_DIR}/disk"
fi

# 3. Optional Standalone executable
if [ -f "${SCRIPT_DIR}/Overviber" ]; then
    echo "[3/3] Setting up Standalone executable..."
    chmod +x "${SCRIPT_DIR}/Overviber"
    echo "      -> Standalone binary ready at: ${SCRIPT_DIR}/Overviber"
fi

echo ""
echo "========================================================"
echo " Installation Complete!"
echo " Rescan your DAW (Reaper, Bitwig, Ardour, Renoise, Carla)."
echo "========================================================"
