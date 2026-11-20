@echo off
setlocal
rem ============================================================
rem  Build the freeglut static library from the vendored source.
rem  You only need this if dist/lib/libfreeglut.a is missing.
rem ============================================================
set "DIR=%~dp0.."
set "FG=%DIR%\third_party\freeglut"
set "OBJ=%DIR%\build\fgobj"

set "GCC="
if exist "C:\Program Files\CodeBlocks\MinGW\bin\gcc.exe" set "GCC=C:\Program Files\CodeBlocks\MinGW\bin\gcc.exe"
if not defined GCC if exist "C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\gcc.exe" set "GCC=C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\gcc.exe"
if not defined GCC for %%X in (gcc.exe) do set "GCC=%%~$PATH:X"
if not defined GCC (
  echo gcc not found.
  exit /b 1
)
for %%D in ("%GCC%") do set "BINDIR=%%~dpD"
set "AR=%BINDIR%ar.exe"
if not exist "%AR%" set "AR=ar.exe"

if not exist "%OBJ%" mkdir "%OBJ%"
if not exist "%FG%\lib" mkdir "%FG%\lib"

echo Compiling freeglut with: %GCC%
pushd "%FG%"

set "CF=-c -O2 -w -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -DFREEGLUT_STATIC -Iinclude -Isrc"

for %%F in (fg_callbacks fg_cursor fg_display fg_ext fg_font_data fg_gamemode fg_geometry fg_gl2 fg_init fg_input_devices fg_joystick fg_main fg_misc fg_overlay fg_spaceball fg_state fg_stroke_mono_roman fg_stroke_roman fg_structure fg_teapot fg_videoresize fg_window fg_font fg_menu) do (
  "%GCC%" %CF% -o "..\..\build\fgobj\%%F.o" "src\%%F.c"
  if errorlevel 1 (echo FAILED on %%F & popd & exit /b 1)
)
for %%F in (fg_cursor_mswin fg_display_mswin fg_ext_mswin fg_gamemode_mswin fg_init_mswin fg_input_devices_mswin fg_joystick_mswin fg_main_mswin fg_menu_mswin fg_spaceball_mswin fg_state_mswin fg_structure_mswin fg_window_mswin fg_cmap_mswin) do (
  "%GCC%" %CF% -o "..\..\build\fgobj\%%F.o" "src\mswin\%%F.c"
  if errorlevel 1 (echo FAILED on %%F & popd & exit /b 1)
)
"%GCC%" %CF% -o "..\..\build\fgobj\xparsegeometry_repl.o" "src\util\xparsegeometry_repl.c"
if errorlevel 1 (echo FAILED on xparsegeometry_repl & popd & exit /b 1)

"%AR%" rcs "lib\libfreeglut.a" ^
 "..\..\build\fgobj\fg_callbacks.o" "..\..\build\fgobj\fg_cursor.o" "..\..\build\fgobj\fg_display.o" ^
 "..\..\build\fgobj\fg_ext.o" "..\..\build\fgobj\fg_font_data.o" "..\..\build\fgobj\fg_gamemode.o" ^
 "..\..\build\fgobj\fg_geometry.o" "..\..\build\fgobj\fg_gl2.o" "..\..\build\fgobj\fg_init.o" ^
 "..\..\build\fgobj\fg_input_devices.o" "..\..\build\fgobj\fg_joystick.o" "..\..\build\fgobj\fg_main.o" ^
 "..\..\build\fgobj\fg_misc.o" "..\..\build\fgobj\fg_overlay.o" "..\..\build\fgobj\fg_spaceball.o" ^
 "..\..\build\fgobj\fg_state.o" "..\..\build\fgobj\fg_stroke_mono_roman.o" "..\..\build\fgobj\fg_stroke_roman.o" ^
 "..\..\build\fgobj\fg_structure.o" "..\..\build\fgobj\fg_teapot.o" "..\..\build\fgobj\fg_videoresize.o" ^
 "..\..\build\fgobj\fg_window.o" "..\..\build\fgobj\fg_font.o" "..\..\build\fgobj\fg_menu.o" ^
 "..\..\build\fgobj\fg_cursor_mswin.o" "..\..\build\fgobj\fg_display_mswin.o" "..\..\build\fgobj\fg_ext_mswin.o" ^
 "..\..\build\fgobj\fg_gamemode_mswin.o" "..\..\build\fgobj\fg_init_mswin.o" "..\..\build\fgobj\fg_input_devices_mswin.o" ^
 "..\..\build\fgobj\fg_joystick_mswin.o" "..\..\build\fgobj\fg_main_mswin.o" "..\..\build\fgobj\fg_menu_mswin.o" ^
 "..\..\build\fgobj\fg_spaceball_mswin.o" "..\..\build\fgobj\fg_state_mswin.o" "..\..\build\fgobj\fg_structure_mswin.o" ^
 "..\..\build\fgobj\fg_window_mswin.o" "..\..\build\fgobj\fg_cmap_mswin.o" "..\..\build\fgobj\xparsegeometry_repl.o"

if errorlevel 1 (echo ARCHIVE FAILED & popd & exit /b 1)
popd
echo freeglut library built: %FG%\lib\libfreeglut.a
exit /b 0
