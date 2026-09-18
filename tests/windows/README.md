Windows test scripts
====================

This directory contains test scripts for Windows.


Prerequisites
-------------

- Windows 10, version 1703 or later
- Visual C++
- git
- cmake


How to use
----------

```bat
cmd.exe
cd /d "%PUBLIC%"
git clone https://github.com/Cyan4973/xxHash
cd xxHash
.\tests\windows\00-test-all.bat
```


Failure diagnostics
-------------------

The test scripts prefix commands with `!__!` to track their source line.
For example, `Error = 23` means that the command on line 23 failed.
