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
!__! && :
!__! && : Delete test-specimen.
!__! && :
!__! && : Copy the LICENSE file as "test-specimen."
!__! && :
!__! && type "!XXHASH_DIR!\LICENSE" > "\\?\!CD!\test-specimen."   || goto :ERROR
!__! && :
!__! && : Test xxhsum for "test-specimen."
!__! && :
!__! && set "XXHSUM_EXE=!XXHASH_DIR!\my_build\Release\xxhsum.exe"
!__! && "!XXHSUM_EXE!" --version                                  || goto :ERROR
!__! && "!XXHSUM_EXE!"     "test-specimen."                       || goto :ERROR
!__! && "!XXHSUM_EXE!" -H0 "test-specimen." > test.xxh0           || goto :ERROR
!__! && "!XXHSUM_EXE!" -H1 "test-specimen." > test.xxh1           || goto :ERROR
!__! && "!XXHSUM_EXE!" -H2 "test-specimen." > test.xxh2           || goto :ERROR
!__! && "!XXHSUM_EXE!" -H3 "test-specimen." > test.xxh3           || goto :ERROR
!__! && type *.xxh*                                               || goto :ERROR
!__! && "!XXHSUM_EXE!" -c test.xxh0 test.xxh1 test.xxh2 test.xxh3 || goto :ERROR

echo Status =!_ESC![92m OK !_ESC![0m (%TEST_NAME%) && set /a errorno=0 && goto :END

:ERROR
echo !_ESC![2K Error = !_E! && echo Status =!_ESC![91m NG !_ESC![0m (%TEST_NAME%)

:END
cd /d "!ORG_DIR!" && exit /B !errorno!
