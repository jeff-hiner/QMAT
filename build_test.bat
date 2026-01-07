@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>NUL

echo.
echo === Testing Q-MAT CGAL-free compilation ===
echo.

cd /d "%~dp0"

cl /c /EHsc /DQMAT_NO_CGAL /I src src\Wm4Math.cpp src\Wm4Matrix.cpp src\Wm4Vector.cpp src\GeometryObjects.cpp src\PrimMesh.cpp src\SlabMesh.cpp main_nocgal.cpp

if %ERRORLEVEL% EQU 0 (
    echo.
    echo === Compilation successful! ===
    del *.obj 2>NUL
) else (
    echo.
    echo === Compilation FAILED ===
)

endlocal
