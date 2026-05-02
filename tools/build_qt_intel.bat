@echo off
setlocal

set "QT_ROOT=C:\Qt"
set "QT_VERSION=6.11.0"
set "QT_NAME=intel_icx_64"

set "INTEL_ROOT=C:\Program Files (x86)\Intel\oneAPI"

set "QT_BASE_SRC=%QT_ROOT%\%QT_VERSION%\Src\qtbase"
set "QT_TOOLS_SRC=%QT_ROOT%\%QT_VERSION%\Src\qttools"

set "QT_BASE_BUILD_DIR=%QT_ROOT%\build\qtbase-%QT_VERSION%-%QT_NAME%"
set "QT_TOOLS_BUILD_DIR=%QT_ROOT%\build\qttools-%QT_VERSION%-%QT_NAME%"

set "QT_INSTALL_DIR=%QT_ROOT%\%QT_VERSION%\%QT_NAME%"
set "QT_CONFIGURE_MODULE=%QT_INSTALL_DIR%\bin\qt-configure-module.bat"

cd /d "%QT_ROOT%" || exit /b
if exist "%QT_BASE_BUILD_DIR%" rmdir /s /q "%QT_BASE_BUILD_DIR%"
if exist "%QT_TOOLS_BUILD_DIR%" rmdir /s /q "%QT_TOOLS_BUILD_DIR%"

if not exist "%QT_BASE_BUILD_DIR%" mkdir "%QT_BASE_BUILD_DIR%" || exit /b
cd /d "%QT_BASE_BUILD_DIR%" || exit /b

call "%INTEL_ROOT%\setvars.bat" intel64 vs2022 || exit /b
set "PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%"

for /f "delims=" %%i in ('where rc') do if not defined RC_PATH set "RC_PATH=%%i"
for /f "delims=" %%i in ('where mt') do if not defined MT_PATH set "MT_PATH=%%i"
set "RC_PATH_FWD=%RC_PATH:\=/%"
set "MT_PATH_FWD=%MT_PATH:\=/%"

call "%QT_BASE_SRC%\configure.bat" ^
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

if not exist "%QT_TOOLS_BUILD_DIR%" mkdir "%QT_TOOLS_BUILD_DIR%" || exit /b
cd /d "%QT_TOOLS_BUILD_DIR%" || exit /b

call "%QT_CONFIGURE_MODULE%" "%QT_TOOLS_SRC%" || exit /b

cmake --build . --parallel --config Release || exit /b
cmake --build . --parallel --config Debug || exit /b
cmake --install . --config Release || exit /b
cmake --install . --config Debug || exit /b

cd /d "%QT_ROOT%" || exit /b
if exist "%QT_BASE_BUILD_DIR%" rmdir /s /q "%QT_BASE_BUILD_DIR%"
if exist "%QT_TOOLS_BUILD_DIR%" rmdir /s /q "%QT_TOOLS_BUILD_DIR%"
if exist "%QT_ROOT%\build" rmdir /s /q "%QT_ROOT%\build"

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

reg add "HKCU\Software\QtProject\QtVsTools\Versions\%QT_VERSION%_%QT_NAME%" /v InstallDir /t REG_SZ /d "%QT_INSTALL_DIR%" /f >nul || exit /b

endlocal
