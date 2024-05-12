@echo off
setlocal

REM Set the URL and file name
set "url=https://drive.google.com/uc?export=download&id=1-3uWo7s3Fho7-Sm9zEO15DG5iRltEb-Z"
set "filename=MinGW.tar.xz"

REM Set the destination directory
set "destination=%APPDATA%\occult"

REM Create the destination directory if it doesn't exist
if not exist "%destination%" (
    mkdir "%destination%"
)

REM Download the file using curl
curl -L -o "%destination%\%filename%" "%url%"

REM Extract the tarball
tar -xf "%destination%\%filename%" -C "%destination%"

REM Delete the downloaded file
del "%destination%\%filename%"

REM Navigate to the MinGW\bin directory
cd /d "%destination%\MinGW\bin"

REM Loop through all files in the directory and rename them with "cb_" prefix, excluding DLL files
for %%F in (*) do (
    if not "%%~xF"==".dll" (
        ren "%%F" "cb_%%F"
    )
)

REM Add %APPDATA%\occult\MinGW\bin to the system PATH
setx PATH "%PATH%;%APPDATA%\occult\MinGW\bin"

echo File downloaded, extracted, and moved successfully.
echo MinGW binaries renamed and added to the system PATH.
exit
