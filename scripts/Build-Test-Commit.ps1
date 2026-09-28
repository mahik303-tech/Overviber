# Overviber: frischer Release-Build mit dem neuesten Visual-Studio-Generator,
# komplette CTest-Suite und - nur wenn alles besteht - Commit aller Änderungen.
#
# Aufruf aus dem Projektordner:
#   powershell -ExecutionPolicy Bypass -File scripts\Build-Test-Commit.ps1
#
# Build-Ordner: build-check\ (per .gitignore ausgeschlossen)
# Protokoll:    build-check\run-log.txt

param(
    [string]$BuildDir = 'build-check',
    [switch]$NoCommit
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $repo
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$log = Join-Path $repo "$BuildDir\run-log.txt"
Start-Transcript -Path $log -Force | Out-Null

function Invoke-Step([string]$title, [scriptblock]$command) {
    Write-Host "`n=== $title ===" -ForegroundColor Cyan
    & $command
    if ($LASTEXITCODE -ne 0) { throw "$title fehlgeschlagen (Exit-Code $LASTEXITCODE)." }
}

$exitCode = 0
try {
    # CMake finden: zuerst PATH, sonst die mit Visual Studio gelieferte Version.
    $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
    $ctest = (Get-Command ctest -ErrorAction SilentlyContinue).Source
    if (-not $cmake) {
        $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        $vsPath = & $vswhere -latest -prerelease -products * -property installationPath
        $bin = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
        $cmake = Join-Path $bin 'cmake.exe'; $ctest = Join-Path $bin 'ctest.exe'
    }
    if (-not (Test-Path -LiteralPath $cmake)) { throw 'CMake nicht gefunden.' }
    Write-Host "CMake: $cmake"
    & $cmake --version | Select-Object -First 1

    # Neuesten Visual-Studio-Generator aus der CMake-Hilfe ermitteln.
    $generators = & $cmake --help | ForEach-Object {
        if ($_ -match '(Visual Studio (\d+) (\d{4}))') {
            [pscustomobject]@{ Name = $Matches[1]; Version = [int]$Matches[2] }
        }
    } | Sort-Object Version -Descending -Unique
    if (-not $generators) { throw 'Kein Visual-Studio-Generator in dieser CMake-Version.' }
    $generator = $generators[0].Name
    Write-Host "Generator: $generator"

    # Geänderte CI-Datei übernehmen (liegt im ignorierten Build-Ordner bereit).
    $ciSource = Join-Path $repo "$BuildDir\ci-staging\build-test-release.yml"
    if (Test-Path -LiteralPath $ciSource) {
        Copy-Item -LiteralPath $ciSource -Destination '.github\workflows\build-test-release.yml' -Force
        Remove-Item -LiteralPath (Split-Path $ciSource) -Recurse -Force
        Write-Host 'CI-Workflow aktualisiert.'
    }

    # Ein Cache aus einem anderen Generator lässt sich nicht weiterverwenden.
    $cache = Join-Path $BuildDir 'CMakeCache.txt'
    if ((Test-Path $cache) -and -not (Select-String -Path $cache -SimpleMatch "CMAKE_GENERATOR:INTERNAL=$generator" -Quiet)) {
        Remove-Item $cache -Force
        Remove-Item (Join-Path $BuildDir 'CMakeFiles') -Recurse -Force -ErrorAction SilentlyContinue
    }

    Invoke-Step 'Konfigurieren' { & $cmake -S . -B $BuildDir -G $generator -A x64 -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF }
    Invoke-Step 'Bauen (Release)' { & $cmake --build $BuildDir --config Release --parallel }
    Invoke-Step 'Tests (CTest)' { & $ctest --test-dir $BuildDir -C Release --output-on-failure }

    if ($NoCommit) {
        Write-Host "`nBuild und Tests erfolgreich. Commit übersprungen (-NoCommit)." -ForegroundColor Green
    } else {
        Invoke-Step 'Git: Änderungen vormerken' { git add -A }
        git diff --cached --quiet
        if ($LASTEXITCODE -eq 0) {
            Write-Host 'Keine Änderungen zu committen.'
        } else {
            git diff --cached --stat
            $message = @"
build: macOS 10.15, opt-in plug-in copy, version 0.9.0, docs

- Raise the macOS deployment target to 10.15; PresetManager and WaveManager
  use std::filesystem, which is unavailable on 10.13 (CMake, CI, guide).
- Make JUCE_COPY_PLUGIN_AFTER_BUILD opt-in (default OFF); installing into
  the system plug-in folders needs administrator rights.
- Set the project version to 0.9.0 in CMake, CI packages, install_linux.sh
  and the publishing guide; release notes 0.9.0 name C++17.
- README and multiplatform guide: 16 CTest tests via ctest, corrected
  directory structure, documented build options.
- Replace the outdated project analysis with the state of 2026-09-28.
- Add scripts/Build-Test-Commit.ps1 (latest VS generator, CTest, commit).

Built with $generator; all CTest tests passed.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_019rffy1yzLucsaAo4fDm5HH
"@
            $messageFile = Join-Path $repo "$BuildDir\commit-message.txt"
            [System.IO.File]::WriteAllText($messageFile, $message, (New-Object System.Text.UTF8Encoding $false))
            Invoke-Step 'Git: Commit' { git commit -F $messageFile }
            git log -1 --oneline
        }
        Write-Host "`nFertig." -ForegroundColor Green
    }
} catch {
    $exitCode = 1
    Write-Host "`nABBRUCH: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'Es wurde nichts committet.' -ForegroundColor Red
} finally {
    Stop-Transcript | Out-Null
    Write-Host "Protokoll: $log"
}
exit $exitCode
