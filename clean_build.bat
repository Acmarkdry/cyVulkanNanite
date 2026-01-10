@echo off
echo [clean_build] Cleaning CMake cache in build/ ...

REM ── 只清理 CMake 缓存和生成文件，保留 _deps（FetchContent 下载缓存）──
if exist build\CMakeCache.txt (
    del /f /q build\CMakeCache.txt
    echo   Deleted: build\CMakeCache.txt
)
if exist build\CMakeFiles (
    rmdir /s /q build\CMakeFiles
    echo   Deleted: build\CMakeFiles\
)
if exist build\*.sln (
    del /f /q build\*.sln
    echo   Deleted: build\*.sln
)
if exist build\*.vcxproj (
    del /f /q build\*.vcxproj
    echo   Deleted: build\*.vcxproj (root level)
)

echo.
echo [clean_build] Done. Re-running CMake configure...
echo.

REM ── 查找 cmake ──
for %%X in (cmake.exe) do (set FOUND=%%~$PATH:X)
if defined FOUND (
    set CMAKE=cmake
) ELSE (
    set CMAKE="%CD%\vcpkglib\vcpkg.windows\downloads\tools\cmake-3.27.1-windows\cmake-3.27.1-windows-i386\bin\cmake.exe"
)

mkdir build 2>nul
cd build || goto :error

%CMAKE% ^
    -D WITH_OIDN=0 ^
    -D WITH_AVIF=0 ^
    -D VCPKG_TARGET_TRIPLET=x64-windows-static ^
    -D CMAKE_TOOLCHAIN_FILE=../vcpkglib/vcpkg.windows/scripts/buildsystems/vcpkg.cmake ^
    -G "Visual Studio 17 2022" ^
    -A "x64" ^
    .. || goto :error

echo.
echo [clean_build] CMake configure succeeded!
echo To build, run:  _windows.bat
cd ..
exit /b 0

:error
echo.
echo [clean_build] FAILED with error #%errorlevel%.
cd ..
pause
exit /b %errorlevel%
