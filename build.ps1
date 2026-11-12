# Build Fustimage with the portable MinGW-w64 toolchain.
# Usage:  powershell -ExecutionPolicy Bypass -File .\build.ps1

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root 'src'
$out = Join-Path $root 'build'

$preferred = 'D:\ruanjian\rust-env\w64devkit\bin'
if (Test-Path (Join-Path $preferred 'gcc.exe')) {
    $bin = $preferred
} else {
    $gccCmd = Get-Command gcc -ErrorAction SilentlyContinue
    if ($null -eq $gccCmd) {
        throw 'gcc not found. Install MinGW-w64 or set the path in build.ps1.'
    }
    $bin = Split-Path -Parent $gccCmd.Source
}

$gcc = Join-Path $bin 'gcc.exe'
$windres = Join-Path $bin 'windres.exe'
New-Item -ItemType Directory -Force -Path $out | Out-Null

if ((Test-Path (Join-Path $src 'app.rc')) -and (Test-Path $windres)) {
    # windres' -I does not survive spaces in the path; run it from src with
    # relative names, then move the object into build.
    Push-Location $src
    try {
        & $windres -I. -i app.rc -o app_res.o
        if ($LASTEXITCODE -ne 0) { throw 'windres failed' }
        Move-Item -Force 'app_res.o' (Join-Path $out 'app_res.o')
    } finally {
        Pop-Location
    }
}

$gccArgs = @(
    '-std=c17', '-O2', '-Wall', '-Wextra',
    '-municode', '-mwindows',
    '-finput-charset=UTF-8', '-fexec-charset=UTF-8', '-fwide-exec-charset=UTF-16LE',
    (Join-Path $src 'main.c'),
    (Join-Path $src 'image.c')
)
$res = Join-Path $out 'app_res.o'
if (Test-Path $res) { $gccArgs += $res }
$gccArgs += @(
    '-o', (Join-Path $out 'Fustimage.exe'),
    '-ld2d1', '-ldwrite', '-lwindowscodecs', '-ldwmapi', '-lshcore',
    '-lole32', '-loleaut32', '-luuid', '-lshell32', '-lcomdlg32',
    '-lgdi32', '-luser32', '-lpropsys'
)

& $gcc @gccArgs
if ($LASTEXITCODE -ne 0) { throw 'gcc link failed' }

$exe = Join-Path $out 'Fustimage.exe'
Write-Host ''
Write-Host ("Built: {0}" -f $exe)
Write-Host ("Size:  {0:N0} bytes" -f (Get-Item $exe).Length)
