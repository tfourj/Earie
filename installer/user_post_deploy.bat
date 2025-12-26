@echo off
setlocal

rem ============================================
rem Qt Installer Template - Post Deployment Script
rem ============================================
rem Usage: user_post_deploy.bat "C:\path\to\DEPLOY_DIR" "VERSION"
rem
rem CUSTOMIZE: Update APPNAME variable below
rem ============================================

rem ============================================
rem CONFIGURATION - CUSTOMIZE THESE VALUES
rem ============================================
set "APPNAME=Earie"
set "DEFAULT_VERSION=1.0.0"

rem ============================================
rem Script Logic (Do not modify below)
rem ============================================

set "DEPLOY_DIR=%~1"
set "VERSION=%~2"
if not defined DEPLOY_DIR (
  echo Usage: %~nx0 path\to\deploy_dir version
  exit /b 1
)

rem Set default version if not provided
if not defined VERSION set "VERSION=%DEFAULT_VERSION%"

set "OUTPUT_ZIP=%DEPLOY_DIR%\..\%APPNAME%.zip"
echo Repacking "%DEPLOY_DIR%" into "%OUTPUT_ZIP%" with folder "%APPNAME%" using WinRAR...

rem go into the deploy dir so * matches its contents
pushd "%DEPLOY_DIR%" || (echo Failed to cd "%DEPLOY_DIR%" & exit /b 1)

rem -r = recurse into subfolders
rem -afzip = create .zip archive
rem * = all files/dirs in current dir
rem -ap%APPNAME%\ = store files inside archive under the APPNAME\ prefix
"C:\Program Files\WinRAR\WinRAR.exe" a -afzip -r "%OUTPUT_ZIP%" * -ap%APPNAME%\

popd

rem Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"

rem Call create_installer.bat from the same directory as this script
echo Calling installer creation script...
call "%SCRIPT_DIR%create_installer.bat" "%DEPLOY_DIR%" "%VERSION%"

echo Done! Created "%OUTPUT_ZIP%"
endlocal

