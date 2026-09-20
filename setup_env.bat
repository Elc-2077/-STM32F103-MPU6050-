@echo off
REM ============================================================================
REM  Setup environment for STM32 development
REM
REM  Adds the STM32CubeIDE bundled toolchain (arm-none-eabi-gcc, cmake, ninja,
REM  make) to PATH for the current shell.
REM
REM  The install directory and plugin version numbers differ on every machine,
REM  so this script locates them automatically. If auto-detection fails, or you
REM  keep CubeIDE somewhere unusual, set CUBEIDE_ROOT before running:
REM
REM      set CUBEIDE_ROOT=D:\Program Files\ST\STM32CubeIDE_1.16.0\STM32CubeIDE
REM      setup_env.bat
REM
REM  CUBEIDE_ROOT must be the directory that contains the "plugins" folder.
REM ============================================================================

setlocal EnableDelayedExpansion

REM --- Locate the CubeIDE installation -------------------------------------
if defined CUBEIDE_ROOT (
    REM Tolerate "set CUBEIDE_ROOT=C:\path " written without quotes: a trailing
    REM space becomes part of the value and would break every exist test below.
    set "_root=%CUBEIDE_ROOT%"
    for /l %%N in (1,1,8) do if "!_root:~-1!"==" " set "_root=!_root:~0,-1!"
    if "!_root:~-1!"=="\" set "_root=!_root:~0,-1!"
    if not exist "!_root!\plugins" (
        echo [ERROR] CUBEIDE_ROOT is set but has no "plugins" folder:
        echo         !_root!
        echo         It should point at the STM32CubeIDE directory itself,
        echo         i.e. the folder containing "plugins".
        goto :fail
    )
    set "IDE_ROOT=!_root!"
    goto :found_root
)

REM Common install locations, newest version first within each base directory.
for %%B in (
    "C:\ST"
    "%ProgramFiles%\ST"
    "%ProgramFiles(x86)%\ST"
    "%LOCALAPPDATA%\Programs\ST"
    "D:\ST"
) do (
    if exist "%%~B" (
        for /f "delims=" %%D in ('dir /b /ad /o-n "%%~B\STM32CubeIDE*" 2^>nul') do (
            if not defined IDE_ROOT (
                if exist "%%~B\%%D\STM32CubeIDE\plugins" set "IDE_ROOT=%%~B\%%D\STM32CubeIDE"
                if exist "%%~B\%%D\plugins"              set "IDE_ROOT=%%~B\%%D"
            )
        )
    )
)

if not defined IDE_ROOT (
    echo [ERROR] Could not find a STM32CubeIDE installation.
    echo         Searched under: C:\ST, %ProgramFiles%\ST, D:\ST and friends.
    echo.
    echo         Set CUBEIDE_ROOT to your install path and re-run, e.g.:
    echo             set CUBEIDE_ROOT=C:\ST\STM32CubeIDE_1.16.0\STM32CubeIDE
    goto :fail
)

:found_root
echo Found STM32CubeIDE: %IDE_ROOT%
echo.

REM --- Resolve each tool's plugin directory --------------------------------
REM Plugin folder names carry a version suffix, so match by prefix and keep the
REM highest-sorting (newest) match that actually contains the expected binary.
call :find_tool ST_TOOLCHAIN "externaltools.gnu-tools-for-stm32" "arm-none-eabi-gcc.exe"
call :find_tool ST_CMAKE    "externaltools.cmake"                "cmake.exe"
call :find_tool ST_NINJA    "externaltools.ninja"                "ninja.exe"
call :find_tool ST_MAKE     "externaltools.make"                 "make.exe"

if not defined ST_TOOLCHAIN (
    echo [ERROR] arm-none-eabi-gcc not found under %IDE_ROOT%\plugins
    echo         The install may be incomplete.
    goto :fail
)

REM --- Build the new PATH ---------------------------------------------------
set "NEW_PATH="
if defined ST_TOOLCHAIN set "NEW_PATH=%NEW_PATH%%ST_TOOLCHAIN%;"
if defined ST_CMAKE     set "NEW_PATH=%NEW_PATH%%ST_CMAKE%;"
if defined ST_NINJA     set "NEW_PATH=%NEW_PATH%%ST_NINJA%;"
if defined ST_MAKE      set "NEW_PATH=%NEW_PATH%%ST_MAKE%;"

echo Environment setup complete!
echo.
echo   ARM GCC Toolchain: %ST_TOOLCHAIN%
if defined ST_CMAKE (echo   CMake:             %ST_CMAKE%) else (echo   CMake:             not found ^(falling back to PATH^))
if defined ST_NINJA (echo   Ninja:             %ST_NINJA%) else (echo   Ninja:             not found ^(falling back to PATH^))
if defined ST_MAKE  (echo   Make:              %ST_MAKE%)  else (echo   Make:              not found ^(falling back to PATH^))
echo.
echo You can now run: cmake --version
echo Build with:      cmake -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake -S ./ -B Debug -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
echo.

REM Hand the resolved PATH to an interactive shell. endlocal would discard the
REM variables set above, so pass PATH through on the same line.
endlocal & set "PATH=%NEW_PATH%%PATH%" & cmd /k

exit /b 0

REM ============================================================================
REM  :find_tool <out_var> <plugin_name_fragment> <binary_to_verify>
REM  Sets <out_var> to the tools\bin directory of the newest matching plugin.
REM ============================================================================
:find_tool
set "_out=%~1"
set "_frag=%~2"
set "_bin=%~3"
set "_hit="
for /f "delims=" %%P in ('dir /b /ad /o-n "%IDE_ROOT%\plugins\*%_frag%*" 2^>nul') do (
    if not defined _hit (
        if exist "%IDE_ROOT%\plugins\%%P\tools\bin\%_bin%" set "_hit=%IDE_ROOT%\plugins\%%P\tools\bin"
    )
)
if defined _hit (set "%_out%=%_hit%") else (echo   [warn] %_bin% not found ^(plugin *%_frag%*^))
exit /b 0

:fail
echo.
pause
exit /b 1
