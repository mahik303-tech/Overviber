# Overviber: frischer Release-Build mit dem neuesten Visual-Studio-Generator,
# komplette CTest-Suite und - nur wenn alles besteht - Commit aller Änderungen.
#
# Aufruf aus dem Projektordner:
#   powershell -ExecutionPolicy Bypass -File scripts\Build-Test-Commit.ps1 -Message "scripts: ..."
#   powershell -ExecutionPolicy Bypass -File scripts\Build-Test-Commit.ps1 -NoCommit
#
# Eine laufende Overviber.exe aus diesem Projektordner wird vor dem Bauen beendet.
#
# Build-Ordner: build-check\ (per .gitignore ausgeschlossen)
# Protokoll:    build-check\run-log.txt

# CmdletBinding: unbekannte oder falsch geschriebene Parameter brechen ab,
# statt stillschweigend ignoriert zu werden.
[CmdletBinding()]
param(
    [string]$Message,
    [string]$BuildDir = 'build-check',
    [switch]$NoCommit
)

if (-not $NoCommit -and [string]::IsNullOrWhiteSpace($Message)) {
    Write-Host 'Bitte -Message "..." angeben (oder -NoCommit für Build und Tests ohne Commit).' -ForegroundColor Red
    exit 2
}

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $repo
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$log = Join-Path $repo "$BuildDir\run-log.txt"
Start-Transcript -Path $log -Force | Out-Null

# Ausgaben externer Programme (cmake, ctest, git) gehen sonst direkt an die
# Konsole und fehlen im Transkript. Durch die Pipeline geleitet, landen
# stdout und stderr vollständig in run-log.txt.
function Write-NativeOutput {
    process {
        if ($_ -is [System.Management.Automation.ErrorRecord]) { Write-Host $_.Exception.Message }
        else { Write-Host $_ }
    }
}

function Invoke-Native([scriptblock]$command) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'   # stderr-Zeilen sind hier kein Abbruchgrund
    try { & $command 2>&1 | Write-NativeOutput }
    finally { $ErrorActionPreference = $previous }
}

function Invoke-Step([string]$title, [scriptblock]$command) {
    Write-Host "`n=== $title ===" -ForegroundColor Cyan
    $started = Get-Date
    Invoke-Native $command
    $code = $LASTEXITCODE
    Write-Host ("--- {0}: Exit-Code {1}, Dauer {2:mm\:ss} ---" -f $title, $code, ((Get-Date) - $started))
    if ($code -ne 0) { throw "$title fehlgeschlagen (Exit-Code $code)." }
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
    Invoke-Native { & $cmake --version }

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

    # Eine offene Standalone aus diesem Projekt sperrt Overviber.exe, dann
    # scheitert der Linker (LNK1104). Sie wird deshalb vor dem Bauen beendet.
    $standalone = Get-Process -Name 'Overviber' -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and $_.Path.StartsWith($repo, [System.StringComparison]::OrdinalIgnoreCase) }
    foreach ($process in $standalone) {
        Write-Host "Beende offene Standalone: $($process.Path) (PID $($process.Id))"
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit(5000) | Out-Null
    }

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
            Invoke-Native { git diff --cached --stat }
            $message = "$Message`n`nBuilt with $generator; all CTest tests passed.`n"
            $messageFile = Join-Path $repo "$BuildDir\commit-message.txt"
            [System.IO.File]::WriteAllText($messageFile, $message, (New-Object System.Text.UTF8Encoding $false))
            Invoke-Step 'Git: Commit' { git commit -F $messageFile }
            Invoke-Native { git log -1 --oneline }
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
