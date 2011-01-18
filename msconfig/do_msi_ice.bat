if "~%1"=="~" goto useage
if "~%2"=="~" goto useage

Msival2 %1 %2
goto finis

:useage
@echo USEAGE: %O {msi_path} {msi_fullpath}
@echo  {msi_path} is the location of the built Condor MSI.
@echo  .
@echo  {msi_fullpath} is the path to the ICE test file,
@echo  usually darice.cub.  This can be acquired from the
@echo  WiX bin folder.
:finis