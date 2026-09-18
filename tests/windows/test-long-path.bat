@echo off
setlocal enabledelayedexpansion
set /a errorno=1
: _E = set the line number of the first !__! - 1
set /a _E=8-1
set "__=set /a _E+=1"

!__! && for /f "delims=. tokens=1,2" %%E in ("%~n0%~x0") do set "TEST_NAME=%%E"
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
!__! && : Create long path > 300 chars
!__! && :
!__! && set "LONG_PATH=0---------1---------2---------3---------4---------5---------6---------7---------8---------9---------\a---------b---------c---------d---------e---------f---------g---------h---------i---------j---------\k---------l---------m---------n---------o---------p---------q---------r---------s---------t---------"
!__! && rmdir /S /Q "!LONG_PATH!" 2>nul
!__! && mkdir "!LONG_PATH!"                                       || goto :ERROR
!__! && mkdir "!LONG_PATH!\child"                                 || goto :ERROR
!__! && :
!__! && : Copy the LICENSE file under !LONG_PATH!
!__! && :
!__! && copy "!XXHASH_DIR!\LICENSE" "!LONG_PATH!" >nul            || goto :ERROR
!__! && :
!__! && : Test xxhsum with relative, absolute, extended and optional UNC paths
!__! && :
!__! && if not defined XXHSUM_EXE set "XXHSUM_EXE=!XXHASH_DIR!\my_build\Release\xxhsum.exe"
!__! && set "ABS_LONG_PATH=!CD!\!LONG_PATH!\LICENSE"
!__! && set "DRIVE_RELATIVE_PATH=!CD:~0,2!!LONG_PATH!\LICENSE"
!__! && set "SLASH_LONG_PATH=!LONG_PATH:\=/!/LICENSE"
!__! && set "SLASH_UNC_ROOT=!XXHASH_TEST_UNC_ROOT:\=/!"
!__! && "!XXHSUM_EXE!" --version                                  || goto :ERROR
!__! && "!XXHSUM_EXE!"     "!LONG_PATH!\LICENSE"                  || goto :ERROR
!__! && "!XXHSUM_EXE!"     ".\!LONG_PATH!\LICENSE"                || goto :ERROR
!__! && "!XXHSUM_EXE!"     "!LONG_PATH!\child\..\LICENSE"         || goto :ERROR
!__! && "!XXHSUM_EXE!"     "!SLASH_LONG_PATH!"                    || goto :ERROR
!__! && "!XXHSUM_EXE!"     "!ABS_LONG_PATH!"                      || goto :ERROR
!__! && "!XXHSUM_EXE!"     "!DRIVE_RELATIVE_PATH!"                || goto :ERROR
!__! && if defined XXHASH_TEST_UNC_ROOT "!XXHSUM_EXE!" "!XXHASH_TEST_UNC_ROOT!\!TMPNAME!\!LONG_PATH!\LICENSE" || goto :ERROR
!__! && if defined XXHASH_TEST_UNC_ROOT "!XXHSUM_EXE!" "!SLASH_UNC_ROOT!/!TMPNAME!/!SLASH_LONG_PATH!" || goto :ERROR
!__! && for %%H in (0 1 2 3) do ("!XXHSUM_EXE!" -H%%H "!LONG_PATH!\LICENSE" > test.xxh%%H || goto :ERROR)
!__! && type *.xxh*                                               || goto :ERROR
!__! && "!XXHSUM_EXE!" -c test.xxh0 test.xxh1 test.xxh2 test.xxh3 || goto :ERROR

echo Status =!_ESC![92m OK !_ESC![0m (%TEST_NAME%) && set /a errorno=0 && goto :END

:ERROR
echo !_ESC![2K Error = !_E! && echo Status =!_ESC![91m NG !_ESC![0m (%TEST_NAME%)

:END
cd /d "!ORG_DIR!" || set /a errorno=1
if defined TEST_DIR rmdir /S /Q "\\?\!TEST_DIR!" || set /a errorno=1
exit /B !errorno!
