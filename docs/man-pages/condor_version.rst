      

*condor_version*
=================

print HTCondor version and platform information
:index:`condor_version<single: condor_version; HTCondor commands>`\ :index:`condor_version command`

Synopsis
--------

**condor_version** [**-help** ]

**condor_version** [**-arch** ] [**-opsys** ] [**-syscall** ]

Description
-----------

With no arguments, *condor_version* prints the currently installed
HTCondor version number and platform information. The version number
includes a build identification number, as well as the date built.

Options
-------

 **-help**
    Print usage information
 **-arch**
    Print this machine's ClassAd value for ``Arch``
 **-opsys**
    Print this machine's ClassAd value for ``OpSys``
 **-syscall**
    Get any requested version and/or platform information from the
    ``libcondorsyscall.a`` that this HTCondor pool is configured to use,
    instead of using the values that are compiled into the tool itself.
    This option may be used in combination with any other options to
    modify where the information is coming from.

Exit Status
-----------

*condor_version* will exit with a status value of 0 (zero) upon
success, and it should never exit with a failing value.

