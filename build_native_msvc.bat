@echo on
setlocal

REM Q-MAT Native Build with MSVC
REM Run this from any directory - it handles paths internally

set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "SRCDIR=%~dp0"

echo SRCDIR=%SRCDIR%

REM Initialize VS environment
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 (
    echo ERROR: Could not initialize Visual Studio environment
    exit /b 1
)

REM Change to source directory
cd /d "%SRCDIR%"

echo Building Q-MAT native with MSVC...
echo Source: %SRCDIR%

cl /nologo /EHsc /O2 /std:c++17 /DQMAT_NO_CGAL /I src ^
    src\Wm4Math.cpp ^
    src\Wm4Matrix.cpp ^
    src\Wm4Vector.cpp ^
    src\GeometryObjects.cpp ^
    src\PrimMesh.cpp ^
    src\SlabMesh.cpp ^
    main_nocgal.cpp ^
    /Fe:qmat_native.exe

if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)

del *.obj >nul 2>&1
echo BUILD SUCCESS: qmat_native.exe
dir /b qmat_native.exe

endlocal
