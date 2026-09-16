@echo off
rem ============================================================================
rem CodeAsMetal - Visual Studio / CMake Build Bridge
rem
rem Purpose:
rem   Provides the build bridge used by CodeAsMetal.slnx / CodeAsMetal.vcxproj.
rem   CMake remains the authoritative build definition.
rem
rem Responsibilities:
rem   1. Select Debug or Release CMake preset.
rem   2. Preserve CodeAsMetal's dedicated vcpkg installation.
rem   3. Initialize Visual Studio 2026 x64 MSVC.
rem   4. Prevent Visual Studio's bundled vcpkg from replacing the project vcpkg.
rem   5. Prefer Visual Studio's CMake and Ninja.
rem   6. Configure, build, and test CodeAsMetal.
rem   7. Deploy the Qt runtime required by the desktop application.
rem
rem Expected VCPKG_ROOT:
rem   C:\dev\vcpkg-codeasmetal
rem ============================================================================

setlocal


rem ============================================================================
rem Build configuration
rem ============================================================================

set "CAM_CONFIG=%~1"

if /I "%CAM_CONFIG%"=="Release" (
    set "CAM_PRESET=windows-release"
) else (
    set "CAM_PRESET=windows-debug"
)

set "CAM_ROOT=%~dp0.."


rem ============================================================================
rem Validate and preserve CodeAsMetal vcpkg
rem ============================================================================

if not defined VCPKG_ROOT (
    echo.
    echo ERROR: VCPKG_ROOT is missing.
    echo Expected:
    echo   C:\dev\vcpkg-codeasmetal
    echo.
    exit /b 1
)

set "CAM_VCPKG_ROOT=%VCPKG_ROOT%"

if not exist "%CAM_VCPKG_ROOT%\vcpkg.exe" (
    echo.
    echo ERROR: vcpkg.exe was not found:
    echo   "%CAM_VCPKG_ROOT%\vcpkg.exe"
    echo.
    exit /b 1
)

set "CAM_VCPKG_TOOLCHAIN=%CAM_VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

if not exist "%CAM_VCPKG_TOOLCHAIN%" (
    echo.
    echo ERROR: vcpkg CMake toolchain was not found:
    echo   "%CAM_VCPKG_TOOLCHAIN%"
    echo.
    exit /b 1
)


rem ============================================================================
rem Locate Visual Studio 2026
rem ============================================================================

if not defined VSINSTALLDIR (
    set "VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio\18\Community\"
)

if not exist "%VSINSTALLDIR%Common7\Tools\VsDevCmd.bat" (
    echo.
    echo ERROR: Visual Studio 2026 developer environment was not found.
    echo Expected:
    echo   "%VSINSTALLDIR%Common7\Tools\VsDevCmd.bat"
    echo.
    exit /b 1
)


rem ============================================================================
rem Initialize MSVC x64
rem ============================================================================

call "%VSINSTALLDIR%Common7\Tools\VsDevCmd.bat" ^
    -arch=amd64 ^
    -host_arch=amd64

if errorlevel 1 (
    echo.
    echo ERROR: Visual Studio 2026 developer environment initialization failed.
    echo.
    exit /b 1
)


rem ============================================================================
rem Restore project-controlled vcpkg
rem
rem VsDevCmd.bat may replace VCPKG_ROOT with Visual Studio's bundled vcpkg.
rem Restore the CodeAsMetal-controlled value immediately after initialization.
rem ============================================================================

set "VCPKG_ROOT=%CAM_VCPKG_ROOT%"


rem ============================================================================
rem Prefer Visual Studio CMake and Ninja
rem
rem Visual Studio's CMake/Ninja directories are placed before the user's normal
rem PATH so installations such as MSYS2 cannot become authoritative.
rem ============================================================================

set "CAM_VS_CMAKE=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
set "CAM_VS_NINJA=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"

set "PATH=%CAM_VS_CMAKE%;%CAM_VS_NINJA%;%PATH%"


rem ============================================================================
rem Diagnostics
rem ============================================================================

echo.
echo CodeAsMetal build environment
echo   Configuration : %CAM_CONFIG%
echo   CMake preset  : %CAM_PRESET%
echo   VCPKG_ROOT    : %VCPKG_ROOT%
echo   Vcpkg toolchain:
echo     %CAM_VCPKG_TOOLCHAIN%
echo   VSINSTALLDIR  : %VSINSTALLDIR%
echo.

where cmake
if errorlevel 1 (
    echo.
    echo ERROR: CMake was not found.
    echo.
    exit /b 1
)

cmake --version
if errorlevel 1 (
    echo.
    echo ERROR: CMake could not be executed.
    echo.
    exit /b 1
)

where ninja
if errorlevel 1 (
    echo.
    echo ERROR: Ninja was not found.
    echo.
    exit /b 1
)

echo.


rem ============================================================================
rem Enter repository root
rem ============================================================================

pushd "%CAM_ROOT%"

if errorlevel 1 (
    echo.
    echo ERROR: Unable to enter repository root:
    echo   "%CAM_ROOT%"
    echo.
    exit /b 1
)


rem ============================================================================
rem Clean
rem
rem The Visual Studio Makefile project may invoke this script with "clean" as
rem the second argument. A clean is performed only when the selected CMake
rem preset has already produced a Ninja build tree.
rem ============================================================================

if /I "%~2"=="clean" (

    if exist "out\build\%CAM_PRESET%\build.ninja" (

        echo Cleaning %CAM_PRESET%...
        echo.

        cmake --build --preset %CAM_PRESET% --target clean

        if errorlevel 1 (
            goto failed
        )
    )

    popd
    exit /b 0
)


rem ============================================================================
rem Configure
rem
rem CMAKE_TOOLCHAIN_FILE is supplied explicitly even though CMakePresets.json
rem also derives it from VCPKG_ROOT. This keeps dependency resolution bound to
rem CodeAsMetal's dedicated vcpkg installation.
rem ============================================================================

echo Configuring CodeAsMetal...
echo.

cmake --preset %CAM_PRESET% ^
    -DCMAKE_TOOLCHAIN_FILE="%CAM_VCPKG_TOOLCHAIN%"

if errorlevel 1 (
    goto failed
)


rem ============================================================================
rem Build
rem ============================================================================

echo.
echo Building CodeAsMetal...
echo.

cmake --build --preset %CAM_PRESET% --parallel 4

if errorlevel 1 (
    goto failed
)


rem ============================================================================
rem Tests
rem ============================================================================

echo.
echo Running CodeAsMetal tests...
echo.

ctest --preset %CAM_PRESET%

if errorlevel 1 (
    goto failed
)


rem ============================================================================
rem Qt deployment
rem
rem CodeAsMetal uses Qt Core, GUI, Widgets, PrintSupport, and SQL.
rem windeployqt is responsible for copying the Qt runtime and plugins required
rem to launch CodeAsMetal.exe directly from the generated build directory.
rem
rem Important vcpkg behavior:
rem
rem   The vcpkg Qt installation uses different runtime layouts for Release and
rem   Debug:
rem
rem     Release binaries:
rem       installed\x64-windows\bin
rem
rem     Debug binaries:
rem       installed\x64-windows\debug\bin
rem
rem   windeployqt.exe alone does not reliably select the vcpkg Debug layout.
rem   vcpkg provides qtpaths.debug.bat with qt.debug.conf specifically describing
rem   the Debug paths. Passing it explicitly through --qtpaths ensures that
rem   dependencies such as Qt6PrintSupportd.dll are resolved from debug\bin.
rem
rem   Release uses the normal qtpaths.exe configuration.
rem
rem --no-translations:
rem   CodeAsMetal V1 does not deploy Qt translation catalogs.
rem
rem Do not exclude PrintSupport. It is a direct CodeAsMetal Qt dependency.
rem ============================================================================

set "CAM_QT_BIN=%VCPKG_ROOT%\installed\x64-windows\tools\Qt6\bin"
set "CAM_QT=%CAM_QT_BIN%\windeployqt.exe"
set "CAM_EXE=out\build\%CAM_PRESET%\CodeAsMetal.exe"

if not exist "%CAM_QT%" (
    echo.
    echo ERROR: windeployqt.exe was not found:
    echo   "%CAM_QT%"
    echo.
    goto failed
)

if not exist "%CAM_EXE%" (
    echo.
    echo ERROR: CodeAsMetal.exe was not produced:
    echo   "%CAM_EXE%"
    echo.
    goto failed
)


rem ============================================================================
rem Select Qt deployment configuration
rem ============================================================================

if /I "%CAM_CONFIG%"=="Release" (

    set "CAM_QT_MODE=--release"
    set "CAM_QTPATHS=%CAM_QT_BIN%\qtpaths.exe"

) else (

    set "CAM_QT_MODE=--debug"
    set "CAM_QTPATHS=%CAM_QT_BIN%\qtpaths.debug.bat"
)

if not exist "%CAM_QTPATHS%" (
    echo.
    echo ERROR: Qt path resolver was not found:
    echo   "%CAM_QTPATHS%"
    echo.
    goto failed
)


rem ============================================================================
rem Deploy Qt
rem ============================================================================

echo.
echo Deploying Qt runtime...
echo   Mode    : %CAM_QT_MODE%
echo   qtpaths : %CAM_QTPATHS%
echo.

"%CAM_QT%" ^
    %CAM_QT_MODE% ^
    --no-translations ^
    --qtpaths "%CAM_QTPATHS%" ^
    "%CAM_EXE%"

if errorlevel 1 (
    echo.
    echo ERROR: Qt runtime deployment failed.
    echo.
    echo The application compiled and its tests completed, but the Qt runtime
    echo required for direct execution was not deployed successfully.
    echo.
    goto failed
)


rem ============================================================================
rem Success
rem ============================================================================

echo.
echo CodeAsMetal build completed successfully.
echo.

popd
exit /b 0


rem ============================================================================
rem Failure
rem ============================================================================

:failed

echo.
echo CodeAsMetal build failed.
echo.

popd
exit /b 1