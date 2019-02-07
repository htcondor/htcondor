      

condor\_update\_machine\_ad
===========================

update a machine ClassAd

Synopsis
--------

**condor\_update\_machine\_ad** [**-help \| -version**\ ]

**condor\_update\_machine\_ad**
[**-pool  **\ *centralmanagerhostname[:portnumber]*]
[**-name  **\ *startdname*] *path/to/update-ad*

Description
-----------

*condor\_update\_machine\_ad* modifies the specified *condor\_startd*
daemon’s machine ClassAd. The ClassAd in the file given by
``path/to/update-ad`` represents the changed attributes. The changes
persists until the *condor\_startd* restarts. If no file is specified on
the command line, *condor\_update\_machine\_ad* reads the update ClassAd
from ``stdin``.

Contents of the file or ``stdin`` must contain a complete ClassAd. Each
line must be terminated by a newline character, including the last line
of the file. Lines are of the form

::

    <attribute> = <value>

Changes to certain ClassAd attributes will cause the *condor\_startd* to
regenerate values for other ClassAd attributes. An example of this is
setting ``HasVM``. This will cause ``OfflineUniverses``,
``VMOfflineTime``, and ``VMOfflineReason`` to change.

Options
-------

 **-help**
    Display usage information and exit
 **-version**
    Display the HTCondor version and exit
 **-pool **\ *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager’s host name and an
    optional port number
 **-name **\ *startdname*
    Send the command to a machine identified by *startdname*

General Remarks
---------------

This tool is intended for the use of system administrators when dealing
with offline universes.

Examples
--------

To re-enable matching with the VM universe jobs, place on ``stdin`` a
complete ClassAd (including the ending newline character) to change the
value of ClassAd attribute ``HasVM``:

::

    echo "HasVM = True 
    " | condor_update_machine_ad

To prevent vm universe jobs from matching with the machine:

::

    echo "HasVM = False 
    " | condor_update_machine_ad

To prevent vm universe jobs from matching with the machine and specify a
reason:

::

    echo "HasVM = False 
    VMOfflineReason = \"Cosmic rays.\" 
    " | condor_update_machine_ad

Note that the quotes around the reason are required by ClassAds, and
they must be escaped because of the shell. Using a file instead of
``stdin`` may be preferable in these situations, because neither quoting
nor escape characters are needed.

Exit Status
-----------

*condor\_update\_machine\_ad* will exit with a status value of 0 (zero)
upon success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
