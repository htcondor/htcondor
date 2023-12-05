*condor_upgrade_check*
======================

Check a current install of HTCondor for incompatibilites that may cause
issues when upgrading to a new major version.

:index:`condor_upgrade_check<double: condor_upgrade_check; HTCondor commands>`

Synopsis
--------

**condor_upgrade_check** [**--help**]

**condor_upgrade_check** [**--CE**] [**--all**]
[**--ignore** *TAG {TAG...}*] [**--only** *TAG {TAG...}*]
[**--tags**] [**--warnings**] [**--no-warnings**]
[**--dump**] [**--verbose**]

Description
-----------

**condor_upgrade_check** is a tool intended to be used by administrators
before upgrading an HTCondor install to a new major version. This tool
will perform various checks for the current installation against known
incompatibilities introduced in the Feature series of HTCondor for
a given major version. If a check fails, indicating the current install
will have issues with an upgrade, then the tool will do its best to
suggest a course of action to take.

**condor_upgrade_check** is intended to be ran on a per-host basis for
upgrading a system. Since the CHTC recommends upgrading between major
versions in steps (i.e. V23 -> V24 -> V25), the available checks change
between major versions. New ones will be added and old ones removed.

Some checks ran by **condor_upgrade_check** are classified as warnings.
These checks output warnings about incompatibilites that the tool is
not capable of testing and thus give accurrate feedback. Warnings tend
to unconditionally output information.

.. note::

    Some checks ran by this tool require the tool to be ran as ``root``.
    If this tool is executed with out ``root`` privileges or on a Windows
    host then any checks that require ``root`` will be skipped.

Options
-------

 **-h/--help**
    Display **condor_upgrade_checks** usage to the terminal

 **--CE**
    Run available checks for installed HTCondor-CE on host

 **-a/--all**
    Run all available checks ignoring the version check to run

 **-i/--ignore** *TAG [TAG ...]*
    Ignore checks with a matching *TAG*. Takes precendence over **-only**

 **-o/--only** *TAG [TAG ...]*
    Only run checks with a matching *TAG*. If given the *TAG* ``WARNINGS``
    then only checks classified as warnings will be ran.

 **-w/--warnings/--no-warnings**
    Enable/disable output of checks classified as warnings. Default is enabled.

 **-t/--tags**
    Display all available check *TAGS* to be used by **--only** and **--ignore**

 **-d/--dump**
    Display information about all available checks.

 **-v/--verbose**
    Increase tool verbosity


Examples
--------

Check hosts installed HTCondor for potential issues caused by incompatibilities
when upgrading between major versions.

.. code-block:: bash

    condor_upgrade_check

Check hosts installed HTCondor-CE for potential issues caused by incompatibilites
when upgrading between major versions.

.. code-block:: bash

    condor_upgrade_check -ce

List all available check *TAGS*

.. code-block:: bash

    condor_upgrade_check --tags

List information about all available checks

.. code-block:: bash

    condor_upgrade_check --dump

Run checks while ignoring specific checks for a host installed HTCondor

.. code-block:: bash

    condor_upgrade_check --ignore BAR BAZ

Run only checks classified as warnings for a host installed HTCondor

.. code-block:: bash

    condor_upgrade_check --only warnings

Exit Status
-----------

Returns ``0`` when tool is finished running.
Returns ``1`` for fatal internal errors.

