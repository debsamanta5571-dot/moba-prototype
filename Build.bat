@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem Compile MobaProject C++ against Unreal 5.8.
rem   Build.bat              Editor (default)
rem   Build.bat Editor
rem   Build.bat Game
rem   Build.bat Client
rem   Build.bat Server
rem   Build.bat All
rem
rem Engine search order: UE_ROOT, UNREAL_ENGINE, .uproject EngineAssociation
rem registry, then common 5.8 install folders.

if exist "%~dp0MobaProject\MobaProject.uproject" (
	set "PROJECT=%~dp0MobaProject\MobaProject.uproject"
) else if exist "%~dp0MobaProject.uproject" (
	set "PROJECT=%~dp0MobaProject.uproject"
) else (
	echo Could not find MobaProject.uproject
	exit /b 1
)

set "WHAT=%~1"
if "%WHAT%"=="" set "WHAT=Editor"

if /i "%WHAT%"=="Editor" (
	set "TARGETS=MobaProjectEditor"
) else if /i "%WHAT%"=="Game" (
	set "TARGETS=MobaProject"
) else if /i "%WHAT%"=="Client" (
	set "TARGETS=MobaProjectClient"
) else if /i "%WHAT%"=="Server" (
	set "TARGETS=MobaProjectServer"
) else if /i "%WHAT%"=="All" (
	set "TARGETS=MobaProjectEditor MobaProject MobaProjectClient MobaProjectServer"
) else (
	echo Unknown target "%WHAT%"
	echo Use: Editor, Game, Client, Server, or All
	exit /b 1
)

call :FindEngine
if not defined ENGINE_ROOT (
	echo Could not find Unreal Engine 5.8.
	echo Set UE_ROOT to the engine root ^(the folder that contains Engine\^).
	exit /b 1
)

set "BUILD_BAT=%ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat"
if not exist "%BUILD_BAT%" (
	echo Missing "%BUILD_BAT%"
	exit /b 1
)

echo Engine:  %ENGINE_ROOT%
echo Project: %PROJECT%
echo.

for %%T in (%TARGETS%) do (
	echo Building %%T Win64 Development
	call "%BUILD_BAT%" %%T Win64 Development -Project="%PROJECT%" -WaitMutex
	if errorlevel 1 (
		echo.
		echo %%T failed.
		exit /b 1
	)
	echo.
)

echo Done.
exit /b 0

:FindEngine
if defined UE_ROOT if exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" (
	set "ENGINE_ROOT=%UE_ROOT%"
	goto :eof
)
if defined UNREAL_ENGINE if exist "%UNREAL_ENGINE%\Engine\Build\BatchFiles\Build.bat" (
	set "ENGINE_ROOT=%UNREAL_ENGINE%"
	goto :eof
)

set "GUID="
for /f "tokens=2 delims={}" %%A in ('findstr EngineAssociation "%PROJECT%"') do set "GUID={%%A}"
if defined GUID (
	for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Epic Games\Unreal Engine\Builds" /v "%GUID%" 2^>nul ^| findstr REG_SZ') do (
		set "CAND=%%B"
	)
	if defined CAND (
		set "CAND=!CAND:/=\!"
		if exist "!CAND!\Engine\Build\BatchFiles\Build.bat" (
			set "ENGINE_ROOT=!CAND!"
			goto :eof
		)
	)
)

for %%P in (
	"E:\UnrealEngineSource\UnrealEngine-5.8"
	"E:\UE_5.8"
	"C:\UE_5.8"
	"C:\UnrealEngine\UE_5.8"
	"%ProgramFiles%\Epic Games\UE_5.8"
) do (
	if exist "%%~P\Engine\Build\BatchFiles\Build.bat" (
		set "ENGINE_ROOT=%%~P"
		goto :eof
	)
)
goto :eof
