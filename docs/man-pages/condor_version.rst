*condor_version*
================

Display current HTCondor installation details.

:index:`condor_version<double: condor_version; HTCondor commands>`

Synopsis
--------

**condor_version** [**-help**]

**condor_version** [**-arch**] [**-opsys**]

Description
-----------

Display the current HTCondor installation's version number and platform
information.

Options
-------

 **-help**
    Print usage information.
 **-arch**
    Print this machine's computer architecture (:ad-attr:`Arch`).
 **-opsys**
    Print this machine's operating system (:ad-attr:`OpSys`).

General Remarks
---------------

The version number includes the following information:

    - The build date.
    - A build identification number.
    - A package identification string.
    - The GitSHA for the build.

Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

Get current HTCondor installation's version and platform information:

.. code-block:: console

    $ condor_version

Get current machine's computer architecture:

.. code-block:: console

    $ condor_version -arch

See Also
--------

None

Availability
------------

Linux, MacOS, Windows
