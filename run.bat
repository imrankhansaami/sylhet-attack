@echo off
setlocal
set "DIR=%~dp0"
if not exist "%DIR%SylhetAttack.exe" (
  echo Building first...
  call "%DIR%build.bat" nopause
  if errorlevel 1 exit /b 1
)
start "" "%DIR%SylhetAttack.exe"
