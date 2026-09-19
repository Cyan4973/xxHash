@echo off
setlocal enabledelayedexpansion
set /a errorno=1
: _E = set the line number of the first !__! - 1
set /a _E=8-1
set "__=set /a _E+=1"

!__! && set "TEST_NAME=%~n0"
!__! && for /F %%E in ('forfiles /m "%~nx0" /c "cmd /c echo 0x1b"') do set "_ESC=%%E"
!__! && set "ORG_DIR=!CD!"
!__! && :
!__! && : cd to the directory which contains this batch file
!__! && :
!__! && cd /d "%~dp0"                                             || goto :ERROR
!__! && :
!__! && : XXHASH_DIR = root level directory of xxhash repository
!__! && :
!__! && cd ..\..                                                  || goto :ERROR
!__! && set "XXHASH_DIR=!CD!"
!__! && :
!__! && : cd to %TMP%\xxHash_RANDOM_NAME
!__! && :
!__! && cd /d "!TMP!"                                             || goto :ERROR
!__! && set "TMPNAME=xxHash_%TIME::=-%%RANDOM%"
!__! && mkdir "!TMPNAME!"                                         || goto :ERROR
!__! && cd    "!TMPNAME!"                                         || goto :ERROR
!__! && set "TEST_DIR=!CD!"
!__! && :
!__! && : Create files whose names are normally interpreted as DOS devices
!__! && :
!__! && type "!XXHASH_DIR!\LICENSE" > "\\?\!CD!\NUL"              || goto :ERROR
!__! && type "!XXHASH_DIR!\LICENSE" > "\\?\!CD!\CON.txt"          || goto :ERROR
!__! && :
!__! && : Test xxhsum for literal reserved names
!__! && :
!__! && if not defined XXHSUM_EXE set "XXHSUM_EXE=!XXHASH_DIR!\my_build\Release\xxhsum.exe"
!__! && "!XXHSUM_EXE!" "NUL" "CON.txt" > test.xxh                 || goto :ERROR
!__! && type test.xxh                                             || goto :ERROR
!__! && "!XXHSUM_EXE!" -c test.xxh                                || goto :ERROR
!__! && "!XXHSUM_EXE!" "\\.\NUL" > nul                            || goto :ERROR

echo Status =!_ESC![92m OK !_ESC![0m (%TEST_NAME%) && set /a errorno=0 && goto :END

:ERROR
echo !_ESC![2K Error = !_E! && echo Status =!_ESC![91m NG !_ESC![0m (%TEST_NAME%)

:END
cd /d "!ORG_DIR!" || set /a errorno=1
if defined TEST_DIR rmdir /S /Q "\\?\!TEST_DIR!" || set /a errorno=1
exit /B !errorno!
