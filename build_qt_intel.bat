@echo off
setlocal

set "QT_SRC=C:\Qt\6.11.0\Src\qtbase"
set "QT_BUILD_DIR=C:\Qt\build\qtbase-6.11.0-intel\build"
set "QT_VS_KEY=6.11.0_intel_icx_64"
set "QT_INSTALL_DIR=C:\Qt\%QT_VS_KEY%"
set "INTEL_ROOT=C:\Program Files (x86)\Intel\oneAPI"

if not exist "%QT_BUILD_DIR%" mkdir "%QT_BUILD_DIR%" || exit /b
cd /d "%QT_BUILD_DIR%" || exit /b

call "%INTEL_ROOT%\setvars.bat" intel64 vs2022 || exit /b
set "PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%"

for /f "delims=" %%i in ('where rc') do if not defined RC_PATH set "RC_PATH=%%i"
for /f "delims=" %%i in ('where mt') do if not defined MT_PATH set "MT_PATH=%%i"
set "RC_PATH_FWD=%RC_PATH:\=/%"
set "MT_PATH_FWD=%MT_PATH:\=/%"

call "%QT_SRC%\configure.bat" ^
  -prefix "%QT_INSTALL_DIR%" ^
  -debug-and-release ^
  -opensource -confirm-license ^
  -nomake examples -nomake tests ^
  -- ^
  -DCMAKE_C_COMPILER=icx-cl ^
  -DCMAKE_CXX_COMPILER=icx-cl ^
  -DCMAKE_RC_COMPILER="%RC_PATH_FWD%" ^
  -DCMAKE_MT="%MT_PATH_FWD%" || exit /b

cmake --build . --parallel || exit /b
ninja install || exit /b

if exist "%QT_BUILD_DIR%" (
    cd /d "%QT_BUILD_DIR%\..\..\.." || exit /b
    rmdir /s /q ".\build_tmp"
)

set "QMAKE_CONF=%QT_INSTALL_DIR%\mkspecs\win32-clang-msvc\qmake.conf"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p = $env:QMAKE_CONF;" ^
  "if(-not (Test-Path -LiteralPath $p)){ throw \"Missing qmake.conf: $p\" };" ^
  "$c = Get-Content -LiteralPath $p -Raw;" ^
  "$c = $c -replace '(?m)^\s*QMAKE_CC\s*=.*$', 'QMAKE_CC                = icx-cl';" ^
  "$c = $c -replace '(?m)^\s*QMAKE_CXX\s*=.*$', 'QMAKE_CXX               = icx-cl';" ^
  "$c = $c -replace '(?m)^\s*QMAKE_LINK\s*=.*$', 'QMAKE_LINK              = icx-cl';" ^
  "$c = $c -replace '(?m)^\s*QMAKE_LIB\s*=.*$', 'QMAKE_LIB               = lib /NOLOGO';" ^
  "$c = $c -replace '(?m)^.*microsoft-enum-value.*(\r?\n)?', '';" ^
  "if($c -notmatch '(?m)^\s*MAKEFILE_GENERATOR\s*=\s*MSBUILD\s*$'){ $c += \"`r`nMAKEFILE_GENERATOR      = MSBUILD\" };" ^
  "if($c -notmatch '(?m)^\s*VCPROJ_EXTENSION\s*=\s*\.vcxproj\s*$'){ $c += \"`r`nVCPROJ_EXTENSION        = .vcxproj\" };" ^
  "Set-Content -LiteralPath $p -Value $c -Encoding ASCII;"
if errorlevel 1 exit /b

reg add "HKCU\Software\QtProject\QtVsTools\Versions\%QT_VS_KEY%" /v InstallDir /t REG_SZ /d "%QT_INSTALL_DIR%" /f >nul || exit /b

endlocal
