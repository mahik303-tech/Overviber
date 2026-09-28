param(
    [ValidateSet('x64', 'x86', 'arm64')]
    [string]$Architecture = 'x64'
)

$vsDevCmd = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $vsDevCmd)) {
    throw "Visual Studio Developer Command Prompt nicht gefunden: $vsDevCmd"
}

$envDump = & cmd.exe /d /s /c "call `"$vsDevCmd`" -arch=$Architecture >nul && set" 2>$null
if ($LASTEXITCODE -ne 0) {
    throw "VsDevCmd.bat konnte nicht initialisiert werden. Exit-Code: $LASTEXITCODE"
}

foreach ($line in $envDump) {
    $separator = $line.IndexOf('=')
    if ($separator -le 0) { continue }
    $name = $line.Substring(0, $separator)
    $value = $line.Substring($separator + 1)
    Set-Item -Path "Env:$name" -Value $value
}

$compilerDir = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64'
if ($Architecture -eq 'x86') { $compilerDir = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx86\x86' }
if ($env:PATH -notlike "*$compilerDir*") { $env:PATH = "$compilerDir;$env:PATH" }

Write-Host "Visual-Studio-Umgebung aktiviert ($Architecture)."
Write-Host "Compiler: $compilerDir\cl.exe"
