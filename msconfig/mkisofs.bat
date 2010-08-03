@ECHO OFF
@REM $mkisofs, "-quiet", "-o", $isofile, "-input-charset", "iso8859-1", "-J", "-A", $cdlabel, "-V", $cdlabel, $tmpdir
@REM
@REM CDMAKE [-q] [-v] [-p] [-s N] [-m] [-b bootimage] [-j] source volume image
@REM
@REM  source : specifications of base directory containing all files
@REM           to be written to CD-ROM image
@REM  volume : volume label
@REM  image  : image file or device
@REM  -q quiet mode - display nothing but error messages
@REM  -v verbose mode - display file information as files
@REM     are scanned and written - overrides -p option
@REM  -p show progress while writing
@REM  -s N   abort operation before beginning write if image will be
@REM         larger than N megabytes (i.e. 1024*1024*N bytes)
@REM  -m accept punctuation marks other than underscores in
@REM     names and extensions
@REM  -b bootimage create bootable ElTorito CD-ROM using 'no emulation' mode
@REM  -j generate Joliet filename records
IF "%1" == "--version" GOTO VERSION
shift
shift
c:\condor\bin\cdmake.exe -q -j -m %9 %6 %1
GOTO END

:VERSION
ECHO cdmake
:END