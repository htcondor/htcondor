##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

##
## Makefile for the Windows Installer for Condor.
##

CWD=$(CURDIR)
BSE=$(CWD)/../..
BASE=$(subst /,\\,$(BSE))

# Tell the build where all the bin, lib, etc directories are
CONDORRELEASEDIR=$(BASE)\\release-dir
CONDOROUTPUTDIR=$(BASE)\\public

# for testing
#PATH=C:\Windows\system32;C:\condor\workspaces\v67\NTconfig;C:\Perl\bin


all: clean license condor_setup condor_set_acls config_file version_file msi 
	"mkdir" -p $(CONDOROUTPUTDIR)
	cmd /c copy /y out\\condor.mm\\MSI\\*.msi $(CONDOROUTPUTDIR)

# Build the config file setter-upper
condor_setup: condor_setup.c 
	cl /nologo /DWIN32 /I $(BASE)\\src\\h /I $(BASE)\\src\\condor_includes condor_setup.c $(BASE)\\src\\condor_util_lib\\my_getopt.c /link /nologo

# Build the utility for calling cacls in a language-independent way
condor_set_acls: condor_set_acls.c
	cl /nologo condor_set_acls.c /link /nologo advapi32.lib user32.lib 

# build the actual MSI package
msi: condor_setup condor.mm
	cmd /c if not exist MakeMSI ..\\tar xf makemsi.tar
	cmd /c mm condor.mm

# put together the MSI version file, which tells MAKEMSI what the
# version number is, and what the filename of the package should be.
version_file:
	cmd /c copy /Y condor.ver.template condor.ver
	$(CONDORRELEASEDIR)\\bin\\condor_version.exe | awk "{print \"; MsiName     = condor-\" $$2 \"-winnt50-x86\"; exit}" >> condor.ver
	echo ; Guid.UpgradeCode = {EA1CEE00-C44F-4BC4-8035-440AA32C1F33}>> condor.ver
	echo ;++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ >> condor.ver
	echo.>> condor.ver
	echo.>> condor.ver
	echo.>> condor.ver
	echo ;############################################################################>> condor.ver
	$(CONDORRELEASEDIR)\\bin\\condor_version.exe | awk "{print \"VERSION : \" $$2; exit}" >> condor.ver
	echo a | awk "{print \"DATE    : \"  strftime(\"%%d %%b %%Y\")}" >> condor.ver
	echo a | awk "{print \"CHANGES : none\"}" >> condor.ver
	echo.>> condor.ver

# run Todd's fancy awk script over the stock config file to make it
# Windows-happy
config_file:
	cmd /c copy /Y condor_setup.exe    $(CONDORRELEASEDIR)
	cmd /c copy /Y condor_set_acls.exe $(CONDORRELEASEDIR)
	cmd /c copy /Y condor_config.local $(CONDORRELEASEDIR)
	cmd /c awk -f $(BASE)\\src\\condor_examples\\convert_config_to_win32.awk $(BASE)\\src\\condor_examples\\condor_config.generic > $(CONDORRELEASEDIR)\\condor_config

# generate an RTF version of the License file, since MSI won't take plain text
license:
	perl txt2rtf.pl --font="Courier New" --points=7 $(BASE)\\LICENSE-2.0.txt > license.rtf

clean:
	rm -rf out
	rm -rf *.exe *.obj
	rm -f condor.ver
	rm -f license.rtf
	rm -f $(CONDORRELEASEDIR)\\*.msi
	rm -f $(CONDORRELEASEDIR)\\*.zip

