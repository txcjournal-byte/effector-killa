@echo off
rem Sestavi 808 KILLA (VST3 + Standalone) pres Visual Studio (2022 nebo novejsi).
rem Spoustej z "Developer Command Prompt for VS" (nebo mej cmake v PATH).
cd /d "%~dp0"

cmake -B build -A x64
if errorlevel 1 goto error

cmake --build build --config Release --parallel
if errorlevel 1 goto error

echo.
echo HOTOVO. Plugin je ve slozce:
echo   %~dp0build\K808_artefacts\Release\VST3\808 KILLA.vst3
echo Zkopiruj celou slozku "808 KILLA.vst3" do C:\Program Files\Common Files\VST3
explorer "%~dp0build\K808_artefacts\Release\VST3"
pause
exit /b 0

:error
echo.
echo BUILD SELHAL - zkontroluj chyby vyse.
pause
exit /b 1
