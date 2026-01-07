@echo off
setlocal

REM Q-MAT DLL Build with MSVC
REM Builds a shared library with the same API as the WASM module

set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "SRCDIR=%~dp0"

call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
    echo ERROR: Could not initialize Visual Studio environment
    exit /b 1
)

cd /d "%SRCDIR%"

echo Building Q-MAT DLL with MSVC...

cl /nologo /EHsc /O2 /arch:AVX2 /std:c++17 /DQMAT_NO_CGAL /LD /I src ^
    src\Wm4Math.cpp ^
    src\Wm4Matrix.cpp ^
    src\Wm4Vector.cpp ^
    src\GeometryObjects.cpp ^
    src\PrimMesh.cpp ^
    src\SlabMesh.cpp ^
    qmat_wasm_api.cpp ^
    /Fe:qmat.dll ^
    /link /EXPORT:wasm_malloc /EXPORT:wasm_free /EXPORT:qmat_simplify

if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)

del *.obj *.exp >nul 2>&1
echo BUILD SUCCESS: qmat.dll
dir /b qmat.dll qmat.lib

endlocal
