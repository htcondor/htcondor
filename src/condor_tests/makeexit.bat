echo off
if "%1" == "" goto skip
	echo passed in %1
	exit /b %1
:skip
	echo no arg passed in
	exit /b 5
