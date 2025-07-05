@echo off

mkdir vcpkglib
cd vcpkglib || goto :error

IF EXIST vcpkg.windows (
	echo "vcpkg.windows already exists."
	cd vcpkg.windows || goto :error
) ELSE (
	git clone https://github.com/Microsoft/vcpkg.git vcpkg.windows || goto :error
	cd vcpkg.windows || goto :error
	git checkout 2024.08.23 || goto :error
	call bootstrap-vcpkg.bat || goto :error
)

rem handle the vcpkg update, auto process
IF "%1" == "forceinstall" (
	git checkout 2024.08.23 || goto :error
	call bootstrap-vcpkg.bat || goto :error
)

rem add if want avif libavif[aom]:x64-windows-static ^
vcpkg.exe install --recurse ^
	openmesh:x64-windows-static ^
	metis:x64-windows-static ^
	assimp:x64-windows-static || goto :error

IF "%1" == "avif" (
	vcpkg.exe install --recurse libavif[aom]:x64-windows-static || goto :error
)

cd ..
cd ..
pause

@REM exit /b

:error
pause
@REM echo Failed with error #%errorlevel%.
@REM exit /b %errorlevel%

