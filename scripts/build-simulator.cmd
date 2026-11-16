@echo off
setlocal

if not defined QNX_HOST goto missing_sdk
if not defined QNX_TARGET goto missing_sdk
where qmake >nul 2>&1
if errorlevel 1 goto missing_sdk
where make >nul 2>&1
if errorlevel 1 goto missing_sdk
where blackberry-nativepackager >nul 2>&1
if errorlevel 1 goto missing_sdk

pushd "%~dp0.."
if errorlevel 1 exit /b 1
if not exist "build\simulator" mkdir "build\simulator"

call qmake -spec blackberry-x86-qcc -o build/simulator/Makefile berrygram.pro "CONFIG+=simulator debug" "CONFIG-=release"
if errorlevel 1 goto build_failed
call make -C build/simulator
if errorlevel 1 goto build_failed
call blackberry-nativepackager -package build/simulator/berrygram-simulator.bar -devMode -configuration Simulator-Debug bar-descriptor.xml
if errorlevel 1 goto build_failed

echo Built: %CD%\build\simulator\berrygram-simulator.bar
popd
exit /b 0

:missing_sdk
echo Initialize the BB10 NDK environment in cmd.exe first.
echo Example: call C:\bbndk\bbndk-env_10_3_1_995.bat
echo Required tools: BB10 Qt 4 qmake, make, blackberry-nativepackager.
exit /b 1

:build_failed
echo Simulator build failed. See the first error above.
popd
exit /b 1
