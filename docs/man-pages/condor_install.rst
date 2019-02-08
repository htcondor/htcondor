      

condor\_install
===============

Configure or install HTCondor

Synopsis
--------

**** [--**help**] [--**usage**]

**condor\_configure** or **condor\_install**
[--**install[=<path/to/release>]**] [--**install-dir=<path>**]
[--**prefix=<path>**] [--**local-dir=<path>**]
[--**make-personal-condor**] [--**bosco**] [--**type = < submit,
execute, manager >**] [--**central-manager = < hostname>**] [--**owner =
< ownername >**] [--**maybe-daemon-owner**] [--**install-log = < file
>**] [--**overwrite**] [--**ignore-missing-libs**] [--**force**]
[--**no-env-scripts**] [--**env-scripts-dir = < directory >**]
[--**backup**] [--**credd**] [--**verbose**]

Description
-----------

*condor\_configure* and *condor\_install* refer to a single script that
installs and/or configures HTCondor on Unix machines. As the names
imply, *condor\_install* is intended to perform a HTCondor installation,
and *condor\_configure* is intended to configure (or reconfigure) an
existing installation. Both will run with Perl 5.6.0 or more recent
versions.

*condor\_configure* (and *condor\_install*) are designed to be run more
than one time where required. It can install HTCondor when invoked with
a correct configuration via

::

    condor_install

or

::

    condor_configure --install

or, it can change the configuration files when invoked via

::

    condor_configure

Note that changes in the configuration files do not result in changes
while HTCondor is running. To effect changes while HTCondor is running,
it is necessary to further use the *condor\_reconfig* or
*condor\_restart* command. *condor\_reconfig* is required where the
currently executing daemons need to be informed of configuration
changes. *condor\_restart* is required where the options
--**make-personal-condor** or --**type** are used, since these affect
which daemons are running.

Running *condor\_configure* or *condor\_install* with no options results
in a usage screen being printed. The --**help** option can be used to
display a full help screen.

Within the options given below, the phrase release directories is the
list of directories that are released with HTCondor. This list includes:
``bin``, ``etc``, ``examples``, ``include``, ``lib``, ``libexec``,
``man``, ``sbin``, ``sql`` and ``src``.

Options
-------

 **—help**
    Print help screen and exit
 **—usage**
    Print short usage and exit
 **—install**
    Perform installation, assuming that the current working directory
    contains the release directories. Without further options, the
    configuration is that of a Personal HTCondor, a complete one-machine
    pool. If used as an upgrade within an existing installation
    directory, existing configuration files and local directory are
    preserved. This is the default behavior of *condor\_install*.
 **—install-dir=<path>**
    Specifies the path where HTCondor should be installed or the path
    where it already is installed. The default is the current working
    directory.
 **—prefix=<path>**
    This is an alias for **–install-dir**.
 **—local-dir=<path>**
    Specifies the location of the local directory, which is the
    directory that generally contains the local (machine-specific)
    configuration file as well as the directories where HTCondor daemons
    write their run-time information (``spool``, ``log``, ``execute``).
    This location is indicated by the ``LOCAL_DIR`` variable in the
    configuration file. When installing (that is, if **–install** is
    specified), *condor\_configure* will properly create the local
    directory in the location specified. If none is specified, the
    default value is given by the evaluation of
    ``$(RELEASE_DIR)/local.$(HOSTNAME)``.

    | During subsequent invocations of *condor\_configure* (that is,
    without the —install option), if the —local-dir option is specified,
    the new directory will be created and the ``log``, ``spool`` and
    ``execute`` directories will be moved there from their current
    location.

 **—make-personal-condor**
    Installs and configures for Personal HTCondor, a fully-functional,
    one-machine pool.
 **—bosco**
    Installs and configures Bosco, a personal HTCondor that submits jobs
    to remote batch systems.
 **—type= < submit, execute, manager >**
    One or more of the types may be listed. This determines the roles
    that a machine may play in a pool. In general, any machine can be a
    submit and/or execute machine, and there is one central manager per
    pool. In the case of a Personal HTCondor, the machine fulfills all
    three of these roles.
 **—central-manager=<hostname>**
    Instructs the current HTCondor installation to use the specified
    machine as the central manager. This modifies the configuration
    variable ``COLLECTOR_HOST`` to point to the given host name. The
    central manager machine’s HTCondor configuration needs to be
    independently configured to act as a manager using the option
    **–type=manager**.
 **—owner=<ownername>**
    Set configuration such that HTCondor daemons will be executed as the
    given owner. This modifies the ownership on the ``log``, ``spool``
    and ``execute`` directories and sets the ``CONDOR_IDS`` value in the
    configuration file, to ensure that HTCondor daemons start up as the
    specified effective user. This is only applicable when
    *condor\_configure* is run by root. If not run as root, the owner is
    the user running the *condor\_configure* command.
 **—maybe-daemon-owner**
    If **–owner** is not specified and no appropriate user can be found
    to run Condor, then this option will allow the daemon user to be
    selected. This option is rarely needed by users but can be useful
    for scripts that invoke condor\_configure to install Condor.
 **—install-log=<file>**
    Save information about the installation in the specified file. This
    is normally only needed when condor\_configure is called by a
    higher-level script, not when invoked by a person.
 **—overwrite**
    Always overwrite the contents of the ``sbin`` directory in the
    installation directory. By default, *condor\_install* will not
    install if it finds an existing ``sbin`` directory with HTCondor
    programs in it. In this case, *condor\_install* will exit with an
    error message. Specify **–overwrite** or **–backup** to tell
    *condor\_install* what to do.

    This prevents *condor\_install* from moving an ``sbin`` directory
    out of the way that it should not move. This is particularly useful
    when trying to install HTCondor in a location used by other things
    (``/usr``, ``/usr/local``, etc.) For example: *condor\_install*
    **–prefix=/usr** will not move ``/usr/sbin`` out of the way unless
    you specify the **–backup** option.

    | The **–backup** behavior is used to prevent *condor\_install* from
    overwriting running daemons – Unix semantics will keep the existing
    binaries running, even if they have been moved to a new directory.

 **—backup**
    Always backup the ``sbin`` directory in the installation directory.
    By default, *condor\_install* will not install if it finds an
    existing ``sbin`` directory with HTCondor programs in it. In this
    case, *condor\_install* with exit with an error message. You must
    specify **–overwrite** or **–backup** to tell *condor\_install* what
    to do.

    This prevents *condor\_install* from moving an ``sbin`` directory
    out of the way that it should not move. This is particularly useful
    if you’re trying to install HTCondor in a location used by other
    things (``/usr``, ``/usr/local``, etc.) For example:
    *condor\_install* **–prefix=/usr** will not move ``/usr/sbin`` out
    of the way unless you specify the **–backup** option.

    | The **–backup** behavior is used to prevent *condor\_install* from
    overwriting running daemons – Unix semantics will keep the existing
    binaries running, even if they have been moved to a new directory.

 **—ignore-missing-libs**
    Ignore missing shared libraries that are detected by
    *condor\_install*. By default, *condor\_install* will detect missing
    shared libraries such as ``libstdc++.so.5`` on Linux; it will print
    messages and exit if missing libraries are detected. The
    **—ignore-missing-libs** will cause *condor\_install* to not exit,
    and to proceed with the installation if missing libraries are
    detected.
 **—force**
    This is equivalent to enabling both the **—overwrite** and
    **—ignore-missing-libs** command line options.
 **—no-env-scripts**
    By default, *condor\_configure* writes simple sh and csh shell
    scripts which can be sourced by their respective shells to set the
    user’s ``PATH`` and ``CONDOR_CONFIG`` environment variables. This
    option prevents *condor\_configure* from generating these scripts.
 **—env-scripts-dir=<directory>**
    By default, the simple *sh* and *csh* shell scripts (see
    **—no-env-scripts** for details) are created in the root directory
    of the HTCondor installation. This option causes *condor\_configure*
    to generate these scripts in the specified directory.
 **—credd**
    Configure the the *condor\_credd* daemon (credential manager
    daemon).
 **—verbose**
    Print information about changes to configuration variables as they
    occur.

Exit Status
-----------

*condor\_configure* will exit with a status value of 0 (zero) upon
success, and it will exit with a nonzero value upon failure.

Examples
--------

Install HTCondor on the machine (machine1@cs.wisc.edu) to be the pool’s
central manager. On machine1, within the directory that contains the
unzipped HTCondor distribution directories:

::

    % condor_install --type=submit,execute,manager

This will allow the machine to submit and execute HTCondor jobs, in
addition to being the central manager of the pool.

To change the configuration such that machine2@cs.wisc.edu is an
execute-only machine (that is, a dedicated computing node) within a pool
with central manager on machine1@cs.wisc.edu, issue the command on that
machine2@cs.wisc.edu from within the directory where HTCondor is
installed:

::

    % condor_configure --central-manager=machine1@cs.wisc.edu --type=execute

To change the location of the ``LOCAL_DIR`` directory in the
configuration file, do (from the directory where HTCondor is installed):

::

    % condor_configure --local-dir=/path/to/new/local/directory

This will move the ``log``,\ ``spool``,\ ``execute`` directories to
``/path/to/new/local/directory`` from the current local directory.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
