      

*condor_update_machine_ad*
=============================

update a machine ClassAd
:index:`condor_update_machine_ad<single: condor_update_machine_ad; HTCondor commands>`\ :index:`condor_update_machine_ad command`

Synopsis
--------

**condor_update_machine_ad** [**-help | -version** ]

**condor_update_machine_ad**
[**-pool** *centralmanagerhostname[:portnumber]*]
[**-name** *startdname*] *path/to/update-ad*

Description
-----------

*condor_update_machine_ad* modifies the specified *condor_startd*
daemon's machine ClassAd. The ClassAd in the file given by
``path/to/update-ad`` represents the changed attributes. The changes
persists until the *condor_startd* restarts. If no file is specified on
the command line, *condor_update_machine_ad* reads the update ClassAd
from ``stdin``.

Contents of the file or ``stdin`` must contain a complete ClassAd. Each
line must be terminated by a newline character, including the last line
of the file. Lines are of the form

.. code-block:: text

    <attribute> = <value>

Changes to certain ClassAd attributes will cause the *condor_startd* to
regenerate values for other ClassAd attributes. An example of this is
setting :ad-attr:`HasVM`. This will cause :ad-attr:`OfflineUniverses`,
:ad-attr:`VMOfflineTime`, and :ad-attr:`VMOfflineReason` to change.

Options
-------

 **-help**
    Display usage information and exit
 **-version**
    Display the HTCondor version and exit
 **-pool** *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number
 **-name** *startdname*
    Send the command to a machine identified by *startdname*

General Remarks
---------------

This tool is intended for the use of system administrators when dealing
with offline universes.

Examples
--------

To re-enable matching with the VM universe jobs, place on ``stdin`` a
complete ClassAd (including the ending newline character) to change the
value of ClassAd attribute :ad-attr:`HasVM`:

.. code-block:: console

    $ echo "HasVM = True
    " | condor_update_machine_ad

To prevent vm universe jobs from matching with the machine:

.. code-block:: console

    $ echo "HasVM = False
    " | condor_update_machine_ad

To prevent vm universe jobs from matching with the machine and specify a
reason:

.. code-block:: console

    $ echo "HasVM = False
    VMOfflineReason = \"Cosmic rays.\" 
    " | condor_update_machine_ad

Note that the quotes around the reason are required by ClassAds, and
they must be escaped because of the shell. Using a file instead of
``stdin`` may be preferable in these situations, because neither quoting
nor escape characters are needed.

Exit Status
-----------

*condor_update_machine_ad* will exit with a status value of 0 (zero)
upon success, and it will exit with the value 1 (one) upon failure.

