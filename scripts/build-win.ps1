<#
=====================================================================================
 TrapWin BEND DEMON - Windows release build
 MSVC (Visual Studio 2022) x64 Release build of VST3 + Standalone, optional Authenticode
 signing, and an Inno Setup installer.

 Requirements: Visual Studio 2022 (Desktop C++ workload), CMake 3.22+, git,
               Inno Setup 6 (iscc.exe) for the installer, Windows SDK signtool for signing.

 Environment variables (optional):
   BD_SIGN_CERT_SHA1   thumbprint of a code-signing cert in the Windows cert store  -> signtool /sha1
   BD_SIGN_PFX         path to a .pfx file (alternative to the thumbprint)
   BD_SIGN_PFX_PASS    password for the .pfx
   BD_TIMESTAMP_URL    RFC3161 timestamp server (default http://timestamp.digicert.com)
   BD_BUILD_DIR        build directory (default build-win)
   BD_JUCE_DIR         local JUCE checkout instead of FetchContent
   BD_SKIP_TESTS=1     skip the engine unit tests

 Usage (PowerShell):  .\scripts\build-win.ps1
 Output:              dist\win\TrapWinBendDemon-<version>-Windows-Setup.exe
=====================================================================================
#>
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
$Root = (Get-Location).Path

$Version = (Select-String -Path CMakeLists.txt -Pattern 'project\(BendDemon VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$BuildDir = if ($env:BD_BUILD_DIR) { $env:BD_BUILD_DIR } else { "build-win" }
$Dist = Join-Path $Root "dist\win"
$TimestampUrl = if ($env:BD_TIMESTAMP_URL) { $env:BD_TIMESTAMP_URL } else { "http://timestamp.digicert.com" }

Write-Host "==> BEND DEMON $Version (x64 Release)"

# ---------------------------------------------------------------------------- configure
$cmakeArgs = @("-B", $BuildDir, "-G", "Visual Studio 17 2022", "-A", "x64", "-DBEND_DEMON_BUILD_TESTS=ON")
if ($env:BD_JUCE_DIR) { $cmakeArgs += "-DJUCE_DIR=$($env:BD_JUCE_DIR)" }
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

# ---------------------------------------------------------------------------- tests
if ($env:BD_SKIP_TESTS -ne "1") {
    cmake --build $BuildDir --config Release --target BendDemonTests --parallel
    if ($LASTEXITCODE -ne 0) { throw "Test build failed" }
    & "$BuildDir\BendDemonTests_artefacts\Release\BendDemonTests.exe"
    if ($LASTEXITCODE -ne 0) { throw "Engine unit tests failed" }
}

# ---------------------------------------------------------------------------- build
cmake --build $BuildDir --config Release --target BendDemon_VST3 BendDemon_Standalone --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$Art = Join-Path $BuildDir "BendDemon_artefacts\Release"
if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
New-Item -ItemType Directory -Force -Path "$Dist\bundles" | Out-Null
Copy-Item -Recurse "$Art\VST3\BEND DEMON.vst3"         "$Dist\bundles\"
Copy-Item          "$Art\Standalone\BEND DEMON.exe"    "$Dist\bundles\"

# ---------------------------------------------------------------------------- sign
function Find-SignTool {
    $cmd = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $kits = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending
    if ($kits) { return $kits[0].FullName }
    throw "signtool.exe not found (install the Windows 10/11 SDK)"
}

function Sign-File([string]$path) {
    $st = Find-SignTool
    $signArgs = @("sign", "/fd", "sha256", "/tr", $TimestampUrl, "/td", "sha256")
    if ($env:BD_SIGN_CERT_SHA1) { $signArgs += @("/sha1", $env:BD_SIGN_CERT_SHA1) }
    else { $signArgs += @("/f", $env:BD_SIGN_PFX); if ($env:BD_SIGN_PFX_PASS) { $signArgs += @("/p", $env:BD_SIGN_PFX_PASS) } }
    & $st @signArgs $path
    if ($LASTEXITCODE -ne 0) { throw "signtool failed for $path" }
}

$DoSign = [bool]($env:BD_SIGN_CERT_SHA1 -or $env:BD_SIGN_PFX)
if ($DoSign) {
    Write-Host "==> Signing binaries"
    Get-ChildItem -Recurse "$Dist\bundles" -Include *.vst3,*.exe,*.dll |
        Where-Object { -not $_.PSIsContainer } | ForEach-Object { Sign-File $_.FullName }
} else {
    Write-Host "==> No signing certificate configured: skipping signing"
}

# ---------------------------------------------------------------------------- installer
$iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue
if (-not $iscc) {
    $candidate = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    if (Test-Path $candidate) { $iscc = Get-Item $candidate }
}
if ($iscc) {
    Write-Host "==> Building installer (Inno Setup)"
    $isccPath = if ($iscc -is [System.Management.Automation.CommandInfo]) { $iscc.Source } else { $iscc.FullName }
    & $isccPath "/DAppVersion=$Version" "/DSourceDir=$Dist\bundles" "/DOutputDir=$Dist" "scripts\installer\BendDemon.iss"
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed" }
    $Setup = Join-Path $Dist "TrapWinBendDemon-$Version-Windows-Setup.exe"
    if ($DoSign) { Sign-File $Setup }
    Write-Host "==> Done: $Setup"
} else {
    Write-Host "==> Inno Setup (iscc.exe) not found - bundles are in $Dist\bundles"
}
