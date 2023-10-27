# Installing HTCondor

This page discusses building and installing HTCondor from source code.
There are many options for installing pre-compiled binaries, which are
generally easier and faster, those are documented in the 
[manual](https://htcondor.readthedocs.io/en/latest/getting-htcondor/index.html)

## Building on Linux inside a build container

Building using a container with dependencies and compilers preinstalled:

There are many dependencies that must be installed before compiling the
HTCondor source code.  These change from time to time, so any document is
likely to be out of date.  The official binaries are built inside containers
which are kept up to date.  So, the easiest way to build HTCondor from source
is using one of these containers.  The list of these containers is
located in the [source code](https://github.com/htcondor/htcondor/blob/main/nmi_tools/nmi-build-platforms)

Picking one for example, `docker://htcondor/nmi-build:x86_64_Ubuntu22-23000100`
which is surely out of date by the time you are reading this:

Launch the build container (without the docker:// prefix):

```sh
docker run --rm=true --user condor -t -i htcondor/nmi-build:x86_64_Ubuntu22-23000100 /bin/bash
```

Inside the container, cd into your home directory, and git clone the 
source code:

```sh
cd /tmp
git clone https://github.com/htcondor/htcondor
```

Note that we don't support building inside the source tree, HTCondor can only
be built with an out-of-tree build directory.  So, After the code is
downloaded, make a \_\_build directory (note that the source code already has a
directory named `build`, so that can't be used). Then use that directory to
configure cmake and launch the build:

```sh
cd htcondor
mkdir __build
cd __build
cmake ..
make -j 8 install
```


OR, if you want to build an RPM or .deb file, run the following inside the container:

```sh
cd /tmp
git clone https://github.com/htcondor/htcondor

mkdir __build
cd __build
OMP_NUM_THREADS=8 ../htcondor/build-on-linux.sh ../htcondor
```


And that's it!  Enjoy!

## Building on Linux outside of a container

If you'd like to build outside of a container, on raw hardware, then
several dependencies will need to be installed.  HTCondor often
requires a newer version of the system compiler than ships with older
distributions, so the devtoolset compiler may need to be installed.
For a complete list of these dependencies, we recommand looking
at the dockerfile we use to build the container above,
[this script](https://github.com/htcondor/htcondor/blob/main/build/nmi-build/setup.sh) generates those.


# Building HTCondor on Windows

1. Install your tools (make sure to have their paths added to system path)
	- a) Visual Studio
	- b) Perl
	    * http://www.activestate.com/activeperl/downloads
	- c) Python (if python bindings are desired) (3.4+ version is fine)
		* https://www.python.org/downloads/
	- d) CMake
		* http://www.cmake.org/download/
	- e) Git
		* http://git-scm.com/download/win
	- f) Wix (if you wish to craft installer msi file)
		* http://wix.codeplex.com/ 
	- g) 7-zip
		* http://www.7-zip.org/download.html

2. Configure your environment
	- a) You can invoke any shell you wish but make sure that the paths
		to the tools are available. Additionally you should add an additional
		path (b) for building in Windows.  These can be added after the source 
		code is on the system since the paths are relative to the condor source.
		- 1) I use the command prompt shell provided with Visual Studio installation.
			This sets up the paths to the Visual Studio toolsets automatically. 
			e.g.
			```sh
			"C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Microsoft Visual Studio 2012\Visual Studio Tools\Developer Command Prompt for VS2012.lnk"
			```
	- b)Add <PATH_TO_CONDOR_SRC>\condor\msconfig to path

3. Get the source code. 
	- a) in an environment shell change directories to where you would like the condor
		folder created by the git clone command. 
	- b) clone the condor repository
		```sh
		git clone https://github.com/htcondor/htcondor
		```
	This will take a while and once done you will have the condor source tree in the current
	working directory. 
	* remember to add the condor\msconfig to your path

4. build condor
	HTCondor only supports out of source builds
	- a) Out of source builds preserves the source tree and creates all files out side of the source tree
		- 1) cd to within the condor folder and set _condor_sources environment variable
			```sh
			cd condor
			set _condor_sources=%CD%
			```
		- 2) Make a directory somewhere else you would like to build and cd there
			```sh
			md c:\src\build_dest
			cd /d c:\src\build_dest
			```
		- 3) Generate the Visual Studio solution files with CMake
			```sh
			cmake CMakeLists.txt -G "Visual Studio 11 2012" %_condor_sources%
			```
	- b) Build condor
		- 1) within the IDE
			- a) Launch Visual Studio editor and open the CONDOR.sln
			- b) Select the build type Debug or RelWithDebInfo (Release is being deprecated)
			- c) Choose the All_BUILD project
			- d) build
		- 2) with the command line
			- a) With devenv
				```sh
				devenv CONDOR.sln /Build RelWithDebInfo
				       or
				devenv CONDOR.sln /Build Debug
				```
			- b) With msbuild
				```sh
				msbuild CONDOR.sln /t:ALL_BUILD /p:Configuration:RelWithDebugInfo
				        or
				msbuild CONDOR.sln /t:ALL_BUILD /p:Configuration:Debug
				```

5. Creating installer
	An msi style installer can be created with included scripts and with the aid of the Wix tools.
	- a) Copy the build sources to an installer staging area with cmake. (this will create a directory
		at the CMAKE_INSTALL_PREFIX path)
		```sh
		cmake -DBUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX:PATH=c:\src\installer\release_dir -P cmake_install.cmake
		```
	- b) Change directory to the copied location and run the do_wix.bat batch file
		```sh
		cd c:\src\installer\release_dir
		etc\WiX\do_wix.bat %CD% c:\src\installer\Condor_installer.msi
		```
