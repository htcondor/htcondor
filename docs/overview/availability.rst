      

Availability
============

HTCondor is currently available as a free download from the Internet via
the World Wide Web at URL
`http://htcondor.org/downloads/ <http://htcondor.org/downloads/>`__.
Binary distributions of this HTCondor Version 8.9.1 release are
available for the platforms detailed in Table \ `1.1 <#x8-80071>`__. A
platform is an architecture/operating system combination.

In the table, clipped means that HTCondor does not support checkpointing
or remote system calls on the given platform. This means that standard
universe jobs are not supported. Some clipped platforms will have
further limitations with respect to supported universes. See
section \ `2.4.1 <RunningaJobtheStepsToTake.html#x16-180002.4.1>`__ on
page \ `32 <RunningaJobtheStepsToTake.html#x16-180002.4.1>`__ for more
details on job universes within HTCondor and their abilities and
limitations.

The HTCondor source code is available for public download alongside the
binary distributions.

--------------

Architecture

Operating System

Intel x86

- RedHat Enterprise Linux 6

- All versions Windows 7 or greater (clipped)

x86\_64

- Red Hat Enterprise Linux 6

- All versions Windows 7 or greater (clipped)

- Red Hat Enterprise Linux 7

- Debian Linux 8.0 (jessie)

- Debian Linux 9.0 (stretch) (clipped)

- Macintosh OS X 10.10 through 10.13 (clipped)

- Ubuntu 14.04; Trusty Tahr

- Ubuntu 16.04; Xenial Xerus (clipped)

- Ubuntu 18.04; Bionic Beaver (clipped)

--------------

--------------

| 

Table 1.1: Supported platforms in HTCondor Version 8.9.1

--------------

NOTE: Other Linux distributions likely work, but are not tested or
supported.

For more platform-specific information about HTCondor’s support for
various operating systems, see
Chapter \ `8 <PlatformSpecificInformation.html#x74-5700008>`__ on
page \ `1616 <PlatformSpecificInformation.html#x74-5700008>`__.

Jobs submitted to the standard universe utilize *condor\_compile* to
relink programs with libraries provided by HTCondor.
Table \ `1.2 <#x8-80112>`__ lists supported compilers by platform for
this Version 8.9.1 release. Other compilers may work, but are not
supported.

--------------

**Platform**

**Compiler**

**Notes**

Red Hat Enterprise Linux 6 on x86\_64

gcc, g++, and g77

as shipped

Red Hat Enterprise Linux 7 on x86\_64

gcc, g++, and g77

as shipped

Debian Linux 8.0 (jessie) on x86\_64

gcc, g++, gfortran

as shipped

Ubuntu 14.04 on x86\_64

gcc, g++, gfortran

as shipped

--------------

--------------

--------------

| 

Table 1.2: Supported compilers in HTCondor Version 8.9.1

--------------

      
