@echo off
setlocal EnableExtensions EnableDelayedExpansion

:: Usage: clean_release.bat "C:\path\to\release"
if "%~1"=="" (
  echo Usage: %~nx0 "release_dir"
  pause
  exit /b 1
)

set "RELEASE_DIR=%~1"
echo Target: "%RELEASE_DIR%"

if not exist "%RELEASE_DIR%\" (
  echo Error: release directory not found.
  pause
  exit /b 1
)

pushd "%RELEASE_DIR%" || (echo Error: cannot cd & pause & exit /b 1)

if not exist "Earie.exe" (
  echo Error: Earie.exe not found. Aborting.
  popd
  pause
  exit /b 1
)

:: -------------------------
:: CONFIG: what to remove (example .cpp .res .h .log)
:: -------------------------
set "REMOVE_EXTS=cpp obj h res log"
:: Folders to remove (relative to RELEASE_DIR). Leave empty if none.
set "REMOVE_FOLDERS="

echo.
echo Removing extensions: %REMOVE_EXTS%
for %%E in (%REMOVE_EXTS%) do (
  for /r %%F in (*.%%E) do (
    echo Del: "%%F"
    del /q "%%F" 2>nul
  )
)

if defined REMOVE_FOLDERS (
  echo.
  echo Removing folders: %REMOVE_FOLDERS%
  for %%D in (%REMOVE_FOLDERS%) do (
    if exist "%%~D\" (
      echo Rmdir: "%%~D"
      rmdir /s /q "%%~D"
    )
  )
)

echo.
echo Done.
popd
pause
