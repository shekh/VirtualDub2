@echo off
setlocal enabledelayedexpansion

set _vdub=..\out\release\vdub.exe
set _outdir=..\out\testmasters

:argloop
if {%1}=={} goto :argloopexit
if {%1}=={/?} goto :usage
if {%1}=={/vdub} (
	if not exist "%2" (
		echo Program does not exist: %2
		exit /b 5
	)

	set _vdub=%2
	shift
	shift
	goto :argloop
)

echo Unknown parameter: %1

:usage
echo VirtualDub filter test script - generate filter images
echo.
echo Usage: %~n0 [/vdub ^<program path^>]
exit /b 0

:argloopexit

if not exist "!_outdir!" md "!_outdir!"

for /f "delims=" %%x in (filters.txt) do (
	!_vdub! /cmd "VirtualDub.__OpenTest(0); VirtualDub.video.SetInputFormat(8); VirtualDub.video.SetOutputFormat(8); VirtualDub.video.filters.Add('%%x'); VirtualDub.subset.Clear(); VirtualDub.subset.AddRange(VirtualDub.video.framerate, 1); VirtualDub.SaveImageSequence(U'!_outdir!\master RGB32 - %%~nx', '.png', 0, 3)"
	if errorlevel 1 (
		echo.
		echo RGB master creation FAILED for filter: %%x
		goto :EOF
	)

	!_vdub! /cmd "VirtualDub.__OpenTest(0); VirtualDub.video.SetInputFormat(11); VirtualDub.video.SetOutputFormat(8); VirtualDub.video.filters.Add('%%x'); VirtualDub.subset.Clear(); VirtualDub.subset.AddRange(VirtualDub.video.framerate, 1); VirtualDub.SaveImageSequence(U'!_outdir!\master YUY2 - %%~nx', '.png', 0, 3)"
	if errorlevel 1 (
		echo.
		echo YCbCr Master creation FAILED for filter: %%x
		goto :EOF
	)

	!_vdub! /cmd "VirtualDub.__OpenTest(0); VirtualDub.video.SetInputFormat(8); VirtualDub.video.SetOutputFormat(8); VirtualDub.video.filters.Add('%%x'); VirtualDub.video.filters.instance[0].SetClipping(32, 24, 32, 24); VirtualDub.subset.Clear(); VirtualDub.subset.AddRange(VirtualDub.video.framerate, 1); VirtualDub.SaveImageSequence(U'!_outdir!\master cropped RGB32 - %%~nx', '.png', 0, 3)"
	if errorlevel 1 (
		echo.
		echo RGB cropped master creation FAILED for filter: %%x
		goto :EOF
	)

	!_vdub! /cmd "VirtualDub.__OpenTest(0); VirtualDub.video.SetInputFormat(11); VirtualDub.video.SetOutputFormat(8); VirtualDub.video.filters.Add('%%x'); VirtualDub.video.filters.instance[0].SetClipping(32, 24, 32, 24); VirtualDub.subset.Clear(); VirtualDub.subset.AddRange(VirtualDub.video.framerate, 1); VirtualDub.SaveImageSequence(U'!_outdir!\master cropped YUY2 - %%~nx', '.png', 0, 3)"
	if errorlevel 1 (
		echo.
		echo YCbCr cropped Master creation FAILED for filter: %%x
		goto :EOF
	)
)


