param(
    [switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Clang = Join-Path $PSScriptRoot 'compiler\bin\clang++.exe'
$Windres = Join-Path $PSScriptRoot 'compiler\bin\llvm-windres.exe'
$Objdump = Join-Path $PSScriptRoot 'compiler\bin\llvm-objdump.exe'
$IncludeDir = Join-Path $PSScriptRoot 'include'
$LibDir = Join-Path $PSScriptRoot 'lib'
$OutExe = Join-Path $PSScriptRoot 'bin\wendo_injector.exe'
$SrcExe = Join-Path $PSScriptRoot 'src\wendo_injector.cpp'
$RcFile = Join-Path $PSScriptRoot 'src\resource.rc'
$ResObj = Join-Path $env:TEMP 'omnilynx_res.o'
$ModsCompiled = Join-Path $env:TEMP 'omnilynx_mod_verify'

if (-not (Test-Path $Clang)) {
    throw "Bundled clang++.exe not found at $Clang"
}

if (-not $VerifyOnly) {
    Write-Host "[1/3] Building wendo_injector.exe (Release x64, static, -Wall -Wextra -Wpedantic -Werror)..."
    & $Windres $RcFile -O coff -o $ResObj
    if ($LASTEXITCODE -ne 0) { throw "resource.rc compile failed" }
    & $Clang `
        --target=x86_64-w64-windows-gnu `
        -std=c++20 `
        -O2 `
        -Wall -Wextra -Wpedantic -Werror `
        -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -DUNICODE -D_UNICODE `
        -municode -static -static-libgcc -static-libstdc++ `
        "-I$IncludeDir" `
        $SrcExe $ResObj `
        -o $OutExe `
        -lkernel32 -luser32 -lshell32 -ldwmapi -lpsapi -lole32 -luuid -lwinhttp -lbcrypt -ladvapi32
    if ($LASTEXITCODE -ne 0) { throw "wendo_injector.exe build failed" }

    if (-not (Test-Path $ModsCompiled)) {
        New-Item -ItemType Directory -Path $ModsCompiled | Out-Null
    }

    Write-Host "[2/3] Building standalone mods in installed_mods/..."
    $mods = Get-ChildItem (Join-Path $Root 'installed_mods') -Filter '*.wh.cpp'
    foreach ($mod in $mods) {
        $modId = $mod.Name -replace '\.wh\.cpp$', ''
        $dllOut = Join-Path $ModsCompiled "$modId.dll"
        & $Clang `
            --target=x86_64-w64-windows-gnu `
            -std=c++20 `
            -O2 `
            -Wall -Wextra -Wpedantic -Werror `
            -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -DUNICODE -D_UNICODE `
            "-DWH_MOD_ID=L`"$modId`"" `
            -shared -static -static-libgcc -static-libstdc++ `
            "-I$IncludeDir" `
            $mod.FullName `
            -o $dllOut `
            "-L$LibDir" -lminhook -luser32 -lgdi32 -ldwmapi -lshell32
        if ($LASTEXITCODE -ne 0) { throw "Failed to compile $($mod.Name)" }
        Write-Host "  Compiled $modId.dll"
    }
}

Write-Host "[3/3] Verifying DLL dependencies..."
& $Objdump -p $OutExe | Select-String 'DLL Name:'
