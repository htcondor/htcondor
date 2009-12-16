@ECHO OFF
@REM $mkisofs, "-quiet", "-o", $isofile, "-input-charset", "iso8859-1", "-J", "-A", $cdlabel, "-V", $cdlabel, $tmpdir
IF "%1" == "--version" GOTO VERSION
shift
shift
cdmake -q -j %9 %6 %1
GOTO END

:VERSION
ECHO cdmake
:END