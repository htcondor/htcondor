Availability
============

:index:`platforms available<single: platforms available; HTCondor>`
:index:`available platforms`
:index:`supported platforms`
:index:`platforms supported`

HTCondor is currently available as a free download from the Internet via
the World Wide Web at URL
`http://htcondor.org/downloads/ <http://htcondor.org/downloads/>`_.
Binary distributions of this HTCondor Version |release| release are
available for the platforms detailed in Table `1.1 <#x8-80071>`_.
A platform is an architecture/operating system combination.
:index:`definition of<single: definition of; clipped platform>`
:index:`availability<single: availability; clipped platform>`

The HTCondor source code is available for public download alongside the
binary distributions.

+--------------------------------------+--------------------------------------+
| Architecture                         | Operating System                     |
+--------------------------------------+--------------------------------------+
| Intel x86                            | - All versions Windows 7 or greater  |
+--------------------------------------+--------------------------------------+
| x86_64                               | - All versions Windows 7 or greater  |
|                                      | - Red Hat Enterprise Linux 7         |
|                                      | - Red Hat Enterprise Linux 8         |
|                                      | - Debian Linux 9; Stretch            |
|                                      | - Debian Linux 10; Buster            |
|                                      | - Macintosh OS X 10.10 through 10.15 |
|                                      | - Ubuntu 16.04; Xenial Xerus         |
|                                      | - Ubuntu 18.04; Bionic Beaver        |
+--------------------------------------+--------------------------------------+


NOTE: Other Linux distributions likely work, but are not tested or
supported.

For more platform-specific information about HTCondor's support for
various operating systems, see the :doc:`/platform-specific/index` chapter.

Jobs submitted to the standard universe utilize *condor_compile* to
relink programs with libraries provided by HTCondor.
The following table lists supported compilers by platform for
this Version |release| release. Other compilers may work, but are not
supported.
:index:`list of supported compilers<single: list of supported compilers; condor_compile>`
:index:`list of supported compilers<single: list of supported compilers; condor_compile command>`
:index:`supported with condor_compile<single: supported with condor_compile; compilers>`

+--------------------------------------+--------------------+------------+
| **Platform**                         | **Compiler**       | **Notes**  |
+======================================+====================+============+
| Red Hat Enterprise Linux 6 on x86_64 | gcc, g++, and g77  | as shipped |
+--------------------------------------+--------------------+------------+
| Red Hat Enterprise Linux 7 on x86_64 | gcc, g++, and g77  | as shipped |
+--------------------------------------+--------------------+------------+
| Debian Linux 8.0 (jessie) on x86_64  | gcc, g++, gfortran | as shipped |
+--------------------------------------+--------------------+------------+
| Ubuntu 14.04 on x86_64               | gcc, g++, gfortran | as shipped |
+--------------------------------------+--------------------+------------+


