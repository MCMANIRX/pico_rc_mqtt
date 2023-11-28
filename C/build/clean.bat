@echo off
cd /d "%~dp0"

set "fileToKeep1=build.bat"
set "fileToKeep2=deploy.bat"
set "fileToKeep3=copy.bat"
set "fileToKeep4=clean.bat"

echo Deleting files in the current folder except for %fileToKeep1%, %fileToKeep2%, and %fileToKeep3%...

for %%F in (*) do (
    if /i "%%F" neq "%fileToKeep1%" if /i "%%F" neq "%fileToKeep2%" if /i "%%F" neq "%fileToKeep3%" if /i "%%F" neq "%fileToKeep4%" (
        echo Deleting: "%%F"
        del "%%F"
    )
)

echo Deletion complete.