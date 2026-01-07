@echo off
setlocal

set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "SRCDIR=%~dp0"

REM Build 64-bit benchmark
echo === Building 64-bit benchmark ===
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "%SRCDIR%"
cl /nologo /O2 /EHsc /std:c++17 benchmark.cpp /Fe:benchmark_x64.exe /link qmat.lib
if errorlevel 1 (
    echo 64-bit build FAILED
    exit /b 1
)
del benchmark.obj 2>NUL

REM Build 32-bit benchmark
echo.
echo === Building 32-bit benchmark ===
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86
cd /d "%SRCDIR%"
cl /nologo /O2 /EHsc /std:c++17 benchmark.cpp /Fe:benchmark_x86.exe /link qmat_x86.lib
if errorlevel 1 (
    echo 32-bit build FAILED
    exit /b 1
)
del benchmark.obj 2>NUL

echo.
echo === Build complete ===
dir /b benchmark_x64.exe benchmark_x86.exe

endlocal
