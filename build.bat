@echo off
setlocal
rem ============================================================
rem  Sylhet Attack - one-click build (MinGW g++)
rem  Usage:  build.bat          -> release build (no console)
rem          build.bat debug    -> console build (shows errors)
rem ============================================================
set "DIR=%~dp0"
set "OUT=%DIR%SylhetAttack.exe"
set "FG=%DIR%third_party\freeglut"

rem ---- find a C++ compiler ----
set "CXX="
if exist "C:\Program Files\CodeBlocks\MinGW\bin\g++.exe" set "CXX=C:\Program Files\CodeBlocks\MinGW\bin\g++.exe"
if not defined CXX if exist "C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\g++.exe" set "CXX=C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\g++.exe"
if not defined CXX for %%X in (g++.exe) do set "CXX=%%~$PATH:X"
if not defined CXX (
  echo.
  echo Could not find g++. Install Code::Blocks with MinGW, or add g++ to PATH.
  pause
  exit /b 1
)
echo Using compiler: %CXX%

if not exist "%FG%\lib\libfreeglut.a" (
  echo freeglut library not found - building it once, please wait...
  call "%DIR%tools\build_freeglut.bat"
  if errorlevel 1 goto :fail
)

set "DEFS=-O2 -DFREEGLUT_STATIC -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0601 -DWINVER=0x0601"
if /i "%~1"=="debug" (set "WINFLAGS=") else (set "WINFLAGS=-mwindows")

"%CXX%" %DEFS% %WINFLAGS% -I"%FG%\include" -I"%DIR%src" ^
  "%DIR%src\main.cpp" "%DIR%src\game.cpp" "%DIR%src\world.cpp" ^
  "%DIR%src\traffic.cpp" "%DIR%src\aircraft.cpp" "%DIR%src\missions.cpp" ^
  "%DIR%src\hud.cpp" "%DIR%src\glutil.cpp" "%DIR%src\textures.cpp" ^
  "%DIR%src\demo.cpp" "%DIR%src\video.cpp" "%DIR%src\enemy.cpp" "%DIR%src\sound.cpp" ^
  -L"%FG%\lib" -lfreeglut -lopengl32 -lglu32 -lgdi32 -lwinmm -luser32 ^
  -static ^
  -o "%OUT%"

if errorlevel 1 goto :fail
echo.
echo Build OK: %OUT%
if /i not "%~1"=="nopause" pause
exit /b 0

:fail
echo.
echo BUILD FAILED - see the messages above.
pause
exit /b 1
