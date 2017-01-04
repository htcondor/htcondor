if "~%1"=="~" goto usage
if "~%2"=="~" goto usage

Msival2 %1 %2
goto finis

:usage
@echo USAGE: %O {msi_path} {msi_fullpath}
@echo  {msi_path} is the location of the built Condor MSI.
@echo  .
@echo  {msi_fullpath} is the path to the ICE test file,
@echo  usually darice.cub.  This can be acquired from the
@echo  WiX bin folder.
:finis
