@echo off
setlocal enabledelayedexpansion
set /a errorno=1
: _E = set the line number of the first !__! - 1
set /a _E=8-1
set "__=set /a _E+=1"

!__! && for /F %%E in ('forfiles /m "%~nx0" /c "cmd /c echo 0x1b"') do set "_ESC=%%E"
!__! && set "ORG_DIR=!CD!"
!__! &&
!__! && : cd to the directory which contains this batch file
!__! &&
!__! && cd /d "%~dp0"                                             || goto :ERROR
!__! &&
!__! && : XXHASH_DIR = root level directory of xxhash repository
!__! &&
!__! && cd ..\..                                                  || goto :ERROR
!__! && set "XXHASH_DIR=!CD!"
!__! &&
!__! && : cd to C:\Users\Public
!__! &&
!__! && cd /d "!PUBLIC!"                                          || goto :ERROR
!__! &&
!__! && : Create long path > 300 chars
!__! &&
!__! && set "LONG_PATH=0---------1---------2---------3---------4---------5---------6---------7---------8---------9---------\a---------b---------c---------d---------e---------f---------g---------h---------i---------j---------\k---------l---------m---------n---------o---------p---------q---------r---------s---------t---------"
!__! && rmdir /S /Q "!LONG_PATH!" 2>nul
!__! && mkdir "!LONG_PATH!"                                       || goto :ERROR
!__! &&
!__! && : Copy the LICENSE file under !LONG_PATH!
!__! &&
!__! && copy "!XXHASH_DIR!\LICENSE" "!LONG_PATH!" >nul            || goto :ERROR
!__! &&
!__! && : Test xxhsum for !LONG_PATH!\LICENSE
!__! &&
!__! && set "XXHSUM_EXE=!XXHASH_DIR!\my_build\Release\xxhsum.exe"
!__! && "!XXHSUM_EXE!" --version                                  || goto :ERROR
!__! && "!XXHSUM_EXE!"     "!LONG_PATH!\LICENSE"                  || goto :ERROR
!__! && "!XXHSUM_EXE!" -H0 "!LONG_PATH!\LICENSE" > test.xxh0      || goto :ERROR
!__! && "!XXHSUM_EXE!" -H1 "!LONG_PATH!\LICENSE" > test.xxh1      || goto :ERROR
!__! && "!XXHSUM_EXE!" -H2 "!LONG_PATH!\LICENSE" > test.xxh2      || goto :ERROR
!__! && "!XXHSUM_EXE!" -H3 "!LONG_PATH!\LICENSE" > test.xxh3      || goto :ERROR
!__! && type *.xxh*                                               || goto :ERROR
!__! && "!XXHSUM_EXE!" -c test.xxh0 test.xxh1 test.xxh2 test.xxh3 || goto :ERROR

echo Status =!_ESC![92m OK !_ESC![0m && set /a errorno=0 && goto :END

:ERROR
echo !_ESC![2K Error = !_E! && echo Status =!_ESC![91m NG !_ESC![0m

:END
cd /d "!ORG_DIR!" && exit /B !errorno!
