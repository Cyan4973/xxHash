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
!__! && : Build xxhsum with cmake
!__! &&
!__! && rmdir /S /Q   my_build 2>nul
!__! && mkdir         my_build                                    || goto :ERROR
!__! && cmake -B      my_build -S build/cmake                     || goto :ERROR
!__! && cmake --build my_build --config Release                   || goto :ERROR
!__! && set "XXHSUM_EXE=!XXHASH_DIR!\my_build\Release\xxhsum.exe"
!__! && echo "!XXHSUM_EXE!" --version
!__! &&      "!XXHSUM_EXE!" --version                             || goto :ERROR

echo Status =!_ESC![92m OK !_ESC![0m && set /a errorno=0 && goto :END

:ERROR
echo !_ESC![2K Error = !_E! && echo Status =!_ESC![91m NG !_ESC![0m

:END
cd /d "!ORG_DIR!" && exit /B !errorno!
