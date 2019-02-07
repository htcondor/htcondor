      

Installation, Start Up, Shut Down, and Reconfiguration
======================================================

This section contains the instructions for installing HTCondor. The
installation will have a default configuration that can be customized.
Sections of the manual below explain customization.

Please read this entire section before starting installation.

Please read the copyright and disclaimer information in
section \ ` <contentsname.html#x2-2000doc>`__. Installation and use of
HTCondor is acknowledgment that you have read and agree to the terms.

Before installing HTCondor, please consider joining the htcondor-world
mailing list. Traffic on this list is kept to an absolute minimum; it is
only used to announce new releases of HTCondor. To subscribe, go to
`https://lists.cs.wisc.edu/mailman/listinfo/htcondor-world <https://lists.cs.wisc.edu/mailman/listinfo/htcondor-world>`__,
and fill out the online form.

You might also want to consider joining the htcondor-users mailing list.
This list is meant to be a forum for HTCondor users to learn from each
other and discuss using HTCondor. It is an excellent place to ask the
HTCondor community about using and configuring HTCondor. To subscribe,
go to
`https://lists.cs.wisc.edu/mailman/listinfo/htcondor-users <https://lists.cs.wisc.edu/mailman/listinfo/htcondor-users>`__,
and fill out the online form.

**Note that forward and reverse DNS lookup must be enabled for HTCondor
to work properly.**

Obtaining the HTCondor Software
-------------------------------

The first step to installing HTCondor is to download it from the
HTCondor web site, `http://htcondor.org/ <http://htcondor.org/>`__. The
downloads are available from the downloads page, at
`http://htcondor.org/downloads/ <http://htcondor.org/downloads/>`__.

Installation on Unix
--------------------

The HTCondor binary distribution is packaged in the following files and
directories:

 ``LICENSE-2.0.txt``
    the licensing agreement. By installing HTCondor, you agree to the
    contents of this file
 ``README``
    general information
 ``bin``
    directory which contains the distribution HTCondor user programs.
 ``bosco_install``
    the Perl script used to install Bosco.
 ``condor_configure``
    the Perl script used to install and configure HTCondor.
 ``condor_install``
    the Perl script used to install HTCondor.
 ``etc``
    directory which contains the distribution HTCondor configuration
    data.
 ``examples``
    directory containing C, Fortran and C++ example programs to run with
    HTCondor.
 ``include``
    directory containing HTCondor header files.
 ``lib``
    directory which contains the distribution HTCondor libraries.
 ``libexec``
    directory which contains the distribution HTCondor auxiliary
    programs for use internally by HTCondor.
 ``man``
    directory which contains the distribution HTCondor manual pages.
 ``sbin``
    directory containing HTCondor daemon binaries and admin tools.
 ``src``
    directory containing source for some interfaces.

Preparation
~~~~~~~~~~~

Before installation, you need to make a few important decisions about
the basic layout of your pool. These decisions answer the following
questions:

#. What machine will be the central manager?
#. What machines should be allowed to submit jobs?
#. Will HTCondor run as root or not?
#. Who will be administering HTCondor on the machines in your pool?
#. Will you have a Unix user named condor and will its home directory be
   shared?
#. Where should the machine-specific directories for HTCondor go?
#. Where should the parts of the HTCondor system be installed?

   -  Configuration files
   -  Release directory

      -  user binaries
      -  system binaries
      -  ``lib`` directory
      -  ``etc`` directory

   -  Documentation

#. Am I using AFS?
#. Do I have enough disk space for HTCondor?

 1. What machine will be the central manager?
    One machine in your pool must be the central manager. Install
    HTCondor on this machine first. This is the centralized information
    repository for the HTCondor pool, and it is also the machine that
    does match-making between available machines and submitted jobs. If
    the central manager machine crashes, any currently active matches in
    the system will keep running, but no new matches will be made.
    Moreover, most HTCondor tools will stop working. Because of the
    importance of this machine for the proper functioning of HTCondor,
    install the central manager on a machine that is likely to stay up
    all the time, or on one that will be rebooted quickly if it does
    crash.

    Also consider network traffic and your network layout when choosing
    your central manager. All the daemons send updates (by default,
    every 5 minutes) to this machine. Memory requirements for the
    central manager differ by the number of machines in the pool: a pool
    with up to about 100 machines will require approximately 25 Mbytes
    of memory for the central manager’s tasks, and a pool with about
    1000 machines will require approximately 100 Mbytes of memory for
    the central manager’s tasks.

    A faster CPU will speed up matchmaking.

    Generally jobs should not be either submitted or run on the central
    manager machine.

 2. Which machines should be allowed to submit jobs?
    HTCondor can restrict the machines allowed to submit jobs.
    Alternatively, it can allow any machine the network allows to
    connect to a submit machine to submit jobs. If the HTCondor pool is
    behind a firewall, and all machines inside the firewall are trusted,
    the ``ALLOW_WRITE`` configuration entry can be set to \*/\*.
    Otherwise, it should be set to reflect the set of machines permitted
    to submit jobs to this pool. HTCondor tries to be secure by default:
    it is shipped with an invalid value that allows no machine to
    connect and submit jobs.

 3. Will HTCondor run as root or not?
    We strongly recommend that the HTCondor daemons be installed and run
    as the Unix user root. Without this, HTCondor can do very little to
    enforce security and policy decisions. You can install HTCondor as
    any user; however there are serious security and performance
    consequences do doing a non-root installation. Please see
    section \ `3.8.13 <Security.html#x36-2970003.8.13>`__ in the manual
    for the details and ramifications of installing and running HTCondor
    as a Unix user other than root.

 4. Who will administer HTCondor?
    Either root will be administering HTCondor directly, or someone else
    will be acting as the HTCondor administrator. If root has delegated
    the responsibility to another person, keep in mind that as long as
    HTCondor is started up as root, it should be clearly understood that
    whoever has the ability to edit the condor configuration files can
    effectively run arbitrary programs as root.

    The HTCondor administrator will be regularly updating HTCondor by
    following these instructions or by using the system-specific
    installation methods below. The administrator will also customize
    policies of the HTCondor submit and execute nodes. This person will
    also receive information from HTCondor if something goes wrong with
    the pool, as described in the documentation of the ``CONDOR_ADMIN``
    configuration variable.

 5. Will you have a Unix user named condor, and will its home directory
be shared?
    To simplify installation of HTCondor, you should create a Unix user
    named condor on all machines in the pool. The HTCondor daemons will
    create files (such as the log files) owned by this user, and the
    home directory can be used to specify the location of files and
    directories needed by HTCondor. The home directory of this user can
    either be shared among all machines in your pool, or could be a
    separate home directory on the local partition of each machine. Both
    approaches have advantages and disadvantages. Having the directories
    centralized can make administration easier, but also concentrates
    the resource usage such that you potentially need a lot of space for
    a single shared home directory. See the section below on
    machine-specific directories for more details.

    Note that the user condor must not be an account into which a person
    can log in. If a person can log in as user condor, it permits a
    major security breach, in that the user condor could submit jobs
    that run as any other user, providing complete access to the user’s
    data by the jobs. A standard way of not allowing log in to an
    account on Unix platforms is to enter an invalid shell in the
    password file.

    If you choose not to create a user named condor, then you must
    specify either via the ``CONDOR_IDS`` environment variable or the
    ``CONDOR_IDS`` config file setting which uid.gid pair should be used
    for the ownership of various HTCondor files. See
    section \ `3.8.13 <Security.html#x36-2960003.8.13>`__ on UIDs in
    HTCondor in the Administrator’s Manual for details.

 6. Where should the machine-specific directories for HTCondor go?
    HTCondor needs a few directories that are unique on every machine in
    your pool. These are ``execute``, ``spool``, ``log``, (and possibly
    ``lock``). Generally, all of them are subdirectories of a single
    machine specific directory called the local directory (specified by
    the ``LOCAL_DIR`` macro in the configuration file). Each should be
    owned by the user that HTCondor is to be run as. Do not stage other
    files in any of these directories; any files not created by HTCondor
    in these directories are subject to removal.

    If you have a Unix user named condor with a local home directory on
    each machine, the ``LOCAL_DIR`` could just be user condor’s home
    directory (``LOCAL_DIR`` = ``$(TILDE)`` in the configuration file).
    If this user’s home directory is shared among all machines in your
    pool, you would want to create a directory for each host (named by
    host name) for the local directory (for example, ``LOCAL_DIR`` =
    ``$(TILDE)``/hosts/``$(HOSTNAME)``). If you do not have a condor
    account on your machines, you can put these directories wherever
    you’d like. However, where to place the directories will require
    some thought, as each one has its own resource needs:

     ``execute``
        This is the directory that acts as the current working directory
        for any HTCondor jobs that run on a given execute machine. The
        binary for the remote job is copied into this directory, so
        there must be enough space for it. (HTCondor will not send a job
        to a machine that does not have enough disk space to hold the
        initial binary..) In addition, if the remote job dumps core for
        some reason, it is first dumped to the execute directory before
        it is sent back to the submit machine. So, put the execute
        directory on a partition with enough space to hold a possible
        core file from the jobs submitted to your pool.
     ``spool``
        The ``spool`` directory holds the job queue and history files,
        and the checkpoint files for all jobs submitted from a given
        machine. As a result, disk space requirements for the ``spool``
        directory can be quite large, particularly if users are
        submitting jobs with very large executables or image sizes. By
        using a checkpoint server (see
        section \ `3.10 <TheCheckpointServer.html#x38-3250003.10>`__ on
        Installing a Checkpoint Server on for details), you can ease the
        disk space requirements, since all checkpoint files are stored
        on the server instead of the spool directories for each machine.
        However, the initial checkpoint files (the executables for all
        the clusters you submit) are still stored in the spool
        directory, so you will need some space, even with a checkpoint
        server. The amount of space will depend on how many executables,
        and what size they are, that need to be stored in the spool
        directory.
     ``log``
        Each HTCondor daemon writes its own log file, and each log file
        is placed in the ``log`` directory. You can specify what size
        you want these files to grow to before they are rotated, so the
        disk space requirements of the directory are configurable. The
        larger the log files, the more historical information they will
        hold if there is a problem, but the more disk space they use up.
        If you have a network file system installed at your pool, you
        might want to place the log directories in a shared location
        (such as ``/usr/local/condor/logs/$(HOSTNAME)``), so that you
        can view the log files from all your machines in a single
        location. However, if you take this approach, you will have to
        specify a local partition for the ``lock`` directory (see
        below).
     ``lock``
        HTCondor uses a small number of lock files to synchronize access
        to certain files that are shared between multiple daemons.
        Because of problems encountered with file locking and network
        file systems (particularly NFS), these lock files should be
        placed on a local partition on each machine. By default, they
        are placed in the ``log`` directory. If you place your ``log``
        directory on a network file system partition, specify a local
        partition for the lock files with the ``LOCK`` parameter in the
        configuration file (such as ``/var/lock/condor``).

    Generally speaking, it is recommended that you do not put these
    directories (except ``lock``) on the same partition as ``/var``,
    since if the partition fills up, you will fill up ``/var`` as well.
    This will cause lots of problems for your machines. Ideally, you
    will have a separate partition for the HTCondor directories. Then,
    the only consequence of filling up the directories will be
    HTCondor’s malfunction, not your whole machine.

 7. Where should the parts of the HTCondor system be installed?
    -  Configuration Files
    -  Release directory

       -  User Binaries
       -  System Binaries
       -  ``lib`` Directory
       -  ``etc`` Directory

    -  Documentation

     Configuration Files
        There can be more than one configuration file. They allow
        different levels of control over how HTCondor is configured on
        each machine in the pool. The global configuration file is
        shared by all machines in the pool. For ease of administration,
        this file should be located on a shared file system, if
        possible. Local configuration files override settings in the
        global file permitting different daemons to run, different
        policies for when to start and stop HTCondor jobs, and so on.
        There may be configuration files specific to each platform in
        the pool. See
        section \ `3.14.4 <SettingUpforSpecialEnvironments.html#x42-3530003.14.4>`__
        on about Configuring HTCondor for Multiple Platforms for
        details.

        The location of configuration files is described in
        section \ `3.3.2 <IntroductiontoConfiguration.html#x31-1710003.3.2>`__.

     Release Directory
        Every binary distribution contains a contains five
        subdirectories: ``bin``, ``etc``, ``lib``, ``sbin``, and
        ``libexec``. Wherever you choose to install these five
        directories we call the release directory (specified by the
        ``RELEASE_DIR`` macro in the configuration file). Each release
        directory contains platform-dependent binaries and libraries, so
        you will need to install a separate one for each kind of machine
        in your pool. For ease of administration, these directories
        should be located on a shared file system, if possible.

        -  User Binaries:

           All of the files in the ``bin`` directory are programs that
           HTCondor users should expect to have in their path. You could
           either put them in a well known location (such as
           ``/usr/local/condor/bin``) which you have HTCondor users add
           to their ``PATH`` environment variable, or copy those files
           directly into a well known place already in the user’s PATHs
           (such as ``/usr/local/bin``). With the above examples, you
           could also leave the binaries in ``/usr/local/condor/bin``
           and put in soft links from ``/usr/local/bin`` to point to
           each program.

        -  System Binaries:

           All of the files in the ``sbin`` directory are HTCondor
           daemons and agents, or programs that only the HTCondor
           administrator would need to run. Therefore, add these
           programs only to the ``PATH`` of the HTCondor administrator.

        -  Private HTCondor Binaries:

           All of the files in the ``libexec`` directory are HTCondor
           programs that should never be run by hand, but are only used
           internally by HTCondor.

        -  ``lib`` Directory:

           The files in the ``lib`` directory are the HTCondor libraries
           that must be linked in with user jobs for all of HTCondor’s
           checkpointing and migration features to be used. ``lib`` also
           contains scripts used by the *condor\_compile* program to
           help re-link jobs with the HTCondor libraries. These files
           should be placed in a location that is world-readable, but
           they do not need to be placed in anyone’s ``PATH``. The
           *condor\_compile* script checks the configuration file for
           the location of the ``lib`` directory.

        -  ``etc`` Directory:

           ``etc`` contains an ``examples`` subdirectory which holds
           various example configuration files and other files used for
           installing HTCondor. ``etc`` is the recommended location to
           keep the master copy of your configuration files. You can put
           in soft links from one of the places mentioned above that
           HTCondor checks automatically to find its global
           configuration file.

     Documentation
        The documentation provided with HTCondor is currently available
        in HTML, Postscript and PDF (Adobe Acrobat). It can be locally
        installed wherever is customary at your site. You can also find
        the HTCondor documentation on the web at:
        `http://htcondor.org/manual <http://htcondor.org/manual>`__.

 8. Am I using AFS?
    If you are using AFS at your site, be sure to read the
    section \ `3.14.1 <SettingUpforSpecialEnvironments.html#x42-3450003.14.1>`__
    in the manual. HTCondor does not currently have a way to
    authenticate itself to AFS. A solution is not ready for Version
    8.9.1. This implies that you are probably not going to want to have
    the ``LOCAL_DIR`` for HTCondor on AFS. However, you can (and
    probably should) have the HTCondor ``RELEASE_DIR`` on AFS, so that
    you can share one copy of those files and upgrade them in a
    centralized location. You will also have to do something special if
    you submit jobs to HTCondor from a directory on AFS. Again, read
    manual
    section \ `3.14.1 <SettingUpforSpecialEnvironments.html#x42-3450003.14.1>`__
    for all the details.

 9. Do I have enough disk space for HTCondor?
    The compressed downloads of HTCondor currently range from a low of
    about 13 Mbytes for 64-bit Ubuntu 12/Linux to about 115 Mbytes for
    Windows. The compressed source code takes approximately 17 Mbytes.

    In addition, you will need a lot of disk space in the local
    directory of any machines that are submitting jobs to HTCondor. See
    question 6 above for details on this.

Unix Installation from a repository
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Installing HTCondor from repositories preferred for systems that you
administer. If you do not have administrative access, use the tarball
instructions below.

Repositories are available Red Hat Enterprise Linux and derivatives such
as CentOS and Scientific Linux. Repositories are also available for
Debian and Ubuntu LTS. Visit the installation documentation at
`https://research.cs.wisc.edu/htcondor/instructions/ <https://research.cs.wisc.edu/htcondor/instructions/>`__

Unix Installation from a Tarball
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Note that installation from a tarball is no longer the preferred
method for installing HTCondor on Unix systems. Installation via RPM or
Debian package is recommended if available for your Unix version.**

An overview of the tarball-based installation process is as follows:

#. Untar the HTCondor software.
#. Run *condor\_install* or *condor\_configure* to install the software.

Details are given below.

After download, all the files are in a compressed, tar format. They need
to be untarred, as

::

      tar xzf <completename>.tar.gz

After untarring, the directory will have the Perl scripts
*condor\_configure* and *condor\_install* (and *bosco\_install*), as
well as ``bin``, ``etc``, ``examples``, ``include``, ``lib``,
``libexec``, ``man``, ``sbin``, ``sql`` and ``src`` subdirectories.

The Perl script *condor\_configure* installs HTCondor. Command-line
arguments specify all needed information to this script. The script can
be executed multiple times, to modify or further set the configuration.
*condor\_configure* has been tested using Perl 5.003. Use this or a more
recent version of Perl.

*condor\_configure* and *condor\_install* are the same program, but have
different default behaviors. *condor\_install* is identical to running

::

      condor_configure --install=.

*condor\_configure* and *condor\_install* work on the named directories.
As the names imply, *condor\_install* is used to install HTCondor,
whereas *condor\_configure* is used to modify the configuration of an
existing HTCondor install.

*condor\_configure* and *condor\_install* are completely command-line
driven and are not interactive. Several command-line arguments are
always needed with *condor\_configure* and *condor\_install*. The
argument

::

      --install=/path/to/release

specifies the path to the HTCondor release directories. The default
command-line argument for *condor\_install* is

::

      --install=.

The argument

::

      --install-dir=<directory>

or

::

      --prefix=<directory>

specifies the path to the install directory.

The argument

::

      --local-dir=<directory>

specifies the path to the local directory.

The --**type** option to *condor\_configure* specifies one or more of
the roles that a machine can take on within the HTCondor pool: central
manager, submit or execute. These options are given in a comma separated
list. So, if a machine is both a submit and execute machine, the proper
command-line option is

::

      --type=submit,execute

Install HTCondor on the central manager machine first. If HTCondor will
run as root in this pool (Item 3 above), run *condor\_install* as root,
and it will install and set the file permissions correctly. On the
central manager machine, run *condor\_install* as follows.

::

    % condor_install --prefix=~condor \ 
    --local-dir=/scratch/condor --type=manager

To update the above HTCondor installation, for example, to also be
submit machine:

::

    % condor_configure --prefix=~condor \ 
    --local-dir=/scratch/condor --type=manager,submit

As in the above example, the central manager can also be a submit point
or an execute machine, but this is only recommended for very small
pools. If this is the case, the --**type** option changes to
``manager,execute`` or ``manager,submit`` or ``manager,submit,execute``.

After the central manager is installed, the execute and submit machines
should then be configured. Decisions about whether to run HTCondor as
root should be consistent throughout the pool. For each machine in the
pool, run

::

    % condor_install --prefix=~condor \ 
    --local-dir=/scratch/condor --type=execute,submit

See the *condor\_configure* manual
page \ `1850 <Condorconfigure.html#x106-73900012>`__ for details.

Starting HTCondor Under Unix After Installation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now that HTCondor has been installed on the machine(s), there are a few
things to check before starting up HTCondor.

#. Read through the ``<release_dir>/etc/condor_config`` file. There are
   a lot of possible settings and you should at least take a look at the
   first two main sections to make sure everything looks okay. In
   particular, you might want to set up security for HTCondor. See the
   section \ `3.8.1 <Security.html#x36-2690003.8.1>`__ to learn how to
   do this.
#. For Linux platforms, run the *condor\_kbdd* to monitor keyboard and
   mouse activity on all machines within the pool that will run a
   *condor\_startd*; these are machines that execute jobs. To do this,
   the subsystem ``KBDD`` will need to be added to the ``DAEMON_LIST``
   configuration variable definition.

   For Unix platforms other than Linux, HTCondor can monitor the
   activity of your mouse and keyboard, provided that you tell it where
   to look. You do this with the ``CONSOLE_DEVICES`` entry in the
   condor\_startd section of the configuration file. On most platforms,
   reasonable defaults are provided. For example, the default device for
   the mouse is ’mouse’, since most installations have a soft link from
   ``/dev/mouse`` that points to the right device (such as ``tty00`` if
   you have a serial mouse, ``psaux`` if you have a PS/2 bus mouse,
   etc). If you do not have a ``/dev/mouse`` link, you should either
   create one (you will be glad you did), or change the
   ``CONSOLE_DEVICES`` entry in HTCondor’s configuration file. This
   entry is a comma separated list, so you can have any devices in
   ``/dev`` count as ’console devices’ and activity will be reported in
   the condor\_startd’s ClassAd as ``ConsoleIdleTime``.

#. (Linux only) HTCondor needs to be able to find the ``utmp`` file.
   According to the Linux File System Standard, this file should be
   ``/var/run/utmp``. If HTCondor cannot find it there, it looks in
   ``/var/adm/utmp``. If it still cannot find it, it gives up. So, if
   your Linux distribution places this file somewhere else, be sure to
   put a soft link from ``/var/run/utmp`` to point to the real location.

To start up the HTCondor daemons, execute the command
``<release_dir>/sbin/condor_master``. This is the HTCondor master, whose
only job in life is to make sure the other HTCondor daemons are running.
The master keeps track of the daemons, restarts them if they crash, and
periodically checks to see if you have installed new binaries (and, if
so, restarts the affected daemons).

If you are setting up your own pool, you should start HTCondor on your
central manager machine first. If you have done a submit-only
installation and are adding machines to an existing pool, the start
order does not matter.

To ensure that HTCondor is running, you can run either:

::

            ps -ef | egrep condor_

or

::

            ps -aux | egrep condor_

depending on your flavor of Unix. On a central manager machine that can
submit jobs as well as execute them, there will be processes for:

-  condor\_master
-  condor\_collector
-  condor\_negotiator
-  condor\_startd
-  condor\_schedd

On a central manager machine that does not submit jobs nor execute them,
there will be processes for:

-  condor\_master
-  condor\_collector
-  condor\_negotiator

For a machine that only submits jobs, there will be processes for:

-  condor\_master
-  condor\_schedd

For a machine that only executes jobs, there will be processes for:

-  condor\_master
-  condor\_startd

Once you are sure the HTCondor daemons are running, check to make sure
that they are communicating with each other. You can run
*condor\_status* to get a one line summary of the status of each machine
in your pool.

Once you are sure HTCondor is working properly, you should add
*condor\_master* into your startup/bootup scripts (i.e. ``/etc/rc`` ) so
that your machine runs *condor\_master* upon bootup. *condor\_master*
will then fire up the necessary HTCondor daemons whenever your machine
is rebooted.

If your system uses System-V style init scripts, you can look in
``<release_dir>/etc/examples/condor.boot`` for a script that can be used
to start and stop HTCondor automatically by init. Normally, you would
install this script as ``/etc/init.d/condor`` and put in soft link from
various directories (for example, ``/etc/rc2.d``) that point back to
``/etc/init.d/condor``. The exact location of these scripts and links
will vary on different platforms.

If your system uses BSD style boot scripts, you probably have an
``/etc/rc.local`` file. Add a line to start up
``<release_dir>/sbin/condor_master``.

Now that the HTCondor daemons are running, there are a few things you
can and should do:

#. (Optional) Do a full install for the *condor\_compile* script.
   condor\_compile assists in linking jobs with the HTCondor libraries
   to take advantage of all of HTCondor’s features. As it is currently
   installed, it will work by placing it in front of any of the
   following commands that you would normally use to link your code:
   gcc, g++, g77, cc, acc, c89, CC, f77, fort77 and ld. If you complete
   the full install, you will be able to use condor\_compile with any
   command whatsoever, in particular, make. See
   section \ `3.14.5 <SettingUpforSpecialEnvironments.html#x42-3560003.14.5>`__
   in the manual for directions.
#. Try building and submitting some test jobs. See ``examples/README``
   for details.
#. If your site uses the AFS network file system, see
   section \ `3.14.1 <SettingUpforSpecialEnvironments.html#x42-3450003.14.1>`__
   in the manual.
#. We strongly recommend that you start up HTCondor (run the
   *condor\_master* daemon) as user root. If you must start HTCondor as
   some user other than root, see
   section \ `3.8.13 <Security.html#x36-2970003.8.13>`__.

Installation on Windows
-----------------------

This section contains the instructions for installing the Windows
version of HTCondor. The install program will set up a slightly
customized configuration file that can be further customized after the
installation has completed.

Be sure that the HTCondor tools are of the same version as the daemons
installed. The HTCondor executable for distribution is packaged in a
single file named similarly to:

::

      condor-8.4.11-390598-Windows-x86.msi

This file is approximately 107 Mbytes in size, and it can be removed
once HTCondor is fully installed.

For any installation, HTCondor services are installed and run as the
Local System account. Running the HTCondor services as any other account
(such as a domain user) is not supported and could be problematic.

Installation Requirements
~~~~~~~~~~~~~~~~~~~~~~~~~

-  HTCondor for Windows is supported for Windows Vista or a more recent
   version.
-  300 megabytes of free disk space is recommended. Significantly more
   disk space could be necessary to be able to run jobs with large data
   files.
-  HTCondor for Windows will operate on either an NTFS or FAT32 file
   system. However, for security purposes, NTFS is preferred.
-  HTCondor for Windows uses the Visual C++ 2012 C runtime library.

Preparing to Install HTCondor under Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Before installing the Windows version of HTCondor, there are two major
decisions to make about the basic layout of the pool.

#. What machine will be the central manager?
#. Is there enough disk space for HTCondor?

If the answers to these questions are already known, skip to the Windows
Installation Procedure section below,
section \ `3.2.3 <#x30-1610003.2.3>`__. If unsure, read on.

-  What machine will be the central manager?

   One machine in your pool must be the central manager. This is the
   centralized information repository for the HTCondor pool and is also
   the machine that matches available machines with waiting jobs. If the
   central manager machine crashes, any currently active matches in the
   system will keep running, but no new matches will be made. Moreover,
   most HTCondor tools will stop working. Because of the importance of
   this machine for the proper functioning of HTCondor, we recommend
   installing it on a machine that is likely to stay up all the time, or
   at the very least, one that will be rebooted quickly if it does
   crash. Also, because all the services will send updates (by default
   every 5 minutes) to this machine, it is advisable to consider network
   traffic and network layout when choosing the central manager.

   Install HTCondor on the central manager before installing on the
   other machines within the pool.

   Generally jobs should not be either submitted or run on the central
   manager machine.

-  Is there enough disk space for HTCondor?

   The HTCondor release directory takes up a fair amount of space. The
   size requirement for the release directory is approximately 250
   Mbytes. HTCondor itself, however, needs space to store all of the
   jobs and their input files. If there will be large numbers of jobs,
   consider installing HTCondor on a volume with a large amount of free
   space.

Installation Procedure Using the MSI Program
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Installation of HTCondor must be done by a user with administrator
privileges. After installation, the HTCondor services will be run under
the local system account. When HTCondor is running a user job, however,
it will run that user job with normal user permissions.

Download HTCondor, and start the installation process by running the
installer. The HTCondor installation is completed by answering questions
and choosing options within the following steps.

 If HTCondor is already installed.
    If HTCondor has been previously installed, a dialog box will appear
    before the installation of HTCondor proceeds. The question asks if
    you wish to preserve your current HTCondor configuration files.
    Answer yes or no, as appropriate.

    If you answer yes, your configuration files will not be changed, and
    you will proceed to the point where the new binaries will be
    installed.

    If you answer no, then there will be a second question that asks if
    you want to use answers given during the previous installation as
    default answers.

 STEP 1: License Agreement.
    The first step in installing HTCondor is a welcome screen and
    license agreement. You are reminded that it is best to run the
    installation when no other Windows programs are running. If you need
    to close other Windows programs, it is safe to cancel the
    installation and close them. You are asked to agree to the license.
    Answer yes or no. If you should disagree with the License, the
    installation will not continue.

    Also fill in name and company information, or use the defaults as
    given.

 STEP 2: HTCondor Pool Configuration.
    The HTCondor configuration needs to be set based upon if this is a
    new pool or to join an existing one. Choose the appropriate radio
    button.

    For a new pool, enter a chosen name for the pool. To join an
    existing pool, enter the host name of the central manager of the
    pool.

 STEP 3: This Machine’s Roles.
    Each machine within an HTCondor pool can either submit jobs or
    execute submitted jobs, or both submit and execute jobs. A check box
    determines if this machine will be a submit point for the pool.

    A set of radio buttons determines the ability and configuration of
    the ability to execute jobs. There are four choices:

     Do not run jobs on this machine.
        This machine will not execute HTCondor jobs.
     Always run jobs and never suspend them.
     Run jobs when the keyboard has been idle for 15 minutes.
     Run jobs when the keyboard has been idle for 15 minutes, and the
    CPU is idle.

    For testing purposes, it is often helpful to use the always run
    HTCondor jobs option.

    For a machine that is to execute jobs and the choice is one of the
    last two in the list, HTCondor needs to further know what to do with
    the currently running jobs. There are two choices:

     Keep the job in memory and continue when the machine meets the
    condition chosen for when to run jobs.
     Restart the job on a different machine.

    This choice involves a trade off. Restarting the job on a different
    machine is less intrusive on the workstation owner than leaving the
    job in memory for a later time. A suspended job left in memory will
    require swap space, which could be a scarce resource. Leaving a job
    in memory, however, has the benefit that accumulated run time is not
    lost for a partially completed job.

 STEP 4: The Account Domain.
    Enter the machine’s accounting (or UID) domain. On this version of
    HTCondor for Windows, this setting is only used for user priorities
    (see
    section \ `3.6 <UserPrioritiesandNegotiation.html#x34-2320003.6>`__)
    and to form a default e-mail address for the user.

 STEP 5: E-mail Settings.
    Various parts of HTCondor will send e-mail to an HTCondor
    administrator if something goes wrong and requires human attention.
    Specify the e-mail address and the SMTP relay host of this
    administrator. Please pay close attention to this e-mail, since it
    will indicate problems in the HTCondor pool.

 STEP 6: Java Settings.
    In order to run jobs in the **java** universe, HTCondor must have
    the path to the jvm executable on the machine. The installer will
    search for and list the jvm path, if it finds one. If not, enter the
    path. To disable use of the **java** universe, leave the field
    blank.
 STEP 7: Host Permission Settings.
    Machines within the HTCondor pool will need various types of access
    permission. The three categories of permission are read, write, and
    administrator. Enter the machines or domain to be given access
    permissions, or use the defaults provided. Wild cards and macros are
    permitted.

     Read
        Read access allows a machine to obtain information about
        HTCondor such as the status of machines in the pool and the job
        queues. All machines in the pool should be given read access. In
        addition, giving read access to \*.cs.wisc.edu will allow the
        HTCondor team to obtain information about the HTCondor pool, in
        the event that debugging is needed.
     Write
        All machines in the pool should be given write access. It allows
        the machines you specify to send information to your local
        HTCondor daemons, for example, to start an HTCondor job. Note
        that for a machine to join the HTCondor pool, it must have both
        read and write access to all of the machines in the pool.
     Administrator
        A machine with administrator access will be allowed more
        extended permission to do things such as change other user’s
        priorities, modify the job queue, turn HTCondor services on and
        off, and restart HTCondor. The central manager should be given
        administrator access and is the default listed. This setting is
        granted to the entire machine, so care should be taken not to
        make this too open.

    For more details on these access permissions, and others that can be
    manually changed in your configuration file, please see the section
    titled Setting Up IP/Host-Based Security in HTCondor in section
    section \ `3.8.9 <Security.html#x36-2920003.8.9>`__.

 STEP 8: VM Universe Setting.
    A radio button determines whether this machine will be configured to
    run **vm** universe jobs utilizing VMware. In addition to having the
    VMware Server installed, HTCondor also needs *Perl* installed. The
    resources available for **vm** universe jobs can be tuned with these
    settings, or the defaults listed can be used.

     Version
        Use the default value, as only one version is currently
        supported.
     Maximum Memory
        The maximum memory that each virtual machine is permitted to use
        on the target machine.
     Maximum Number of VMs
        The number of virtual machines that can be run in parallel on
        the target machine.
     Networking Support
        The VMware instances can be configured to use network support.
        There are four options in the pull-down menu.

        -  None: No networking support.
        -  NAT: Network address translation.
        -  Bridged: Bridged mode.
        -  NAT and Bridged: Allow both methods.

     Path to Perl Executable
        The path to the *Perl* executable.

 STEP 9: Choose Setup Type
    The next step is where the destination of the HTCondor files will be
    decided. We recommend that HTCondor be installed in the location
    shown as the default in the install choice: C:\\Condor. This is due
    to several hard coded paths in scripts and configuration files.
    Clicking on the Custom choice permits changing the installation
    directory.

    Installation on the local disk is chosen for several reasons. The
    HTCondor services run as local system, and within Microsoft Windows,
    local system has no network privileges. Therefore, for HTCondor to
    operate, HTCondor should be installed on a local hard drive, as
    opposed to a network drive (file server).

    The second reason for installation on the local disk is that the
    Windows usage of drive letters has implications for where HTCondor
    is placed. The drive letter used must be not change, even when
    different users are logged in. Local drive letters do not change
    under normal operation of Windows.

    While it is strongly discouraged, it may be possible to place
    HTCondor on a hard drive that is not local, if a dependency is added
    to the service control manager such that HTCondor starts after the
    required file services are available.

Unattended Installation Procedure Using the Included Setup Program
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section details how to run the HTCondor for Windows installer in an
unattended batch mode. This mode is one that occurs completely from the
command prompt, without the GUI interface.

The HTCondor for Windows installer uses the Microsoft Installer (MSI)
technology, and it can be configured for unattended installs analogous
to any other ordinary MSI installer.

The following is a sample batch file that is used to set all the
properties necessary for an unattended install.

::

    @echo on 
    set ARGS= 
    set ARGS=NEWPOOL="N" 
    set ARGS=%ARGS% POOLNAME="" 
    set ARGS=%ARGS% RUNJOBS="C" 
    set ARGS=%ARGS% VACATEJOBS="Y" 
    set ARGS=%ARGS% SUBMITJOBS="Y" 
    set ARGS=%ARGS% CONDOREMAIL="you@yours.com" 
    set ARGS=%ARGS% SMTPSERVER="smtp.localhost" 
    set ARGS=%ARGS% HOSTALLOWREAD="*" 
    set ARGS=%ARGS% HOSTALLOWWRITE="*" 
    set ARGS=%ARGS% HOSTALLOWADMINISTRATOR="$(IP_ADDRESS)" 
    set ARGS=%ARGS% INSTALLDIR="C:\Condor" 
    set ARGS=%ARGS% POOLHOSTNAME="$(IP_ADDRESS)" 
    set ARGS=%ARGS% ACCOUNTINGDOMAIN="none" 
    set ARGS=%ARGS% JVMLOCATION="C:\Windows\system32\java.exe" 
    set ARGS=%ARGS% USEVMUNIVERSE="N" 
    set ARGS=%ARGS% VMMEMORY="128" 
    set ARGS=%ARGS% VMMAXNUMBER="$(NUM_CPUS)" 
    set ARGS=%ARGS% VMNETWORKING="N" 
    REM set ARGS=%ARGS% LOCALCONFIG="http://my.example.com/condor_config.$(FULL_HOSTNAME)" 
     
    msiexec /qb /l* condor-install-log.txt /i condor-8.0.0-133173-Windows-x86.msi %ARGS%

Each property corresponds to answers that would have been supplied while
running an interactive installer. The following is a brief explanation
of each property as it applies to unattended installations:

 NEWPOOL = < Y \| N >
    determines whether the installer will create a new pool with the
    target machine as the central manager.
 POOLNAME
    sets the name of the pool, if a new pool is to be created. Possible
    values are either the name or the empty string "".
 RUNJOBS = < N \| A \| I \| C >
    determines when HTCondor will run jobs. This can be set to:

    -  Never run jobs (N)
    -  Always run jobs (A)
    -  Only run jobs when the keyboard and mouse are Idle (I)
    -  Only run jobs when the keyboard and mouse are idle and the CPU
       usage is low (C)

 VACATEJOBS = < Y \| N >
    determines what HTCondor should do when it has to stop the execution
    of a user job. When set to Y, HTCondor will vacate the job and start
    it somewhere else if possible. When set to N, HTCondor will merely
    suspend the job in memory and wait for the machine to become
    available again.
 SUBMITJOBS = < Y \| N >
    will cause the installer to configure the machine as a submit node
    when set to Y.
 CONDOREMAIL
    sets the e-mail address of the HTCondor administrator. Possible
    values are an e-mail address or the empty string "".
 HOSTALLOWREAD
    is a list of host names that are allowed to issue READ commands to
    HTCondor daemons. This value should be set in accordance with the
    ``HOSTALLOW_READ`` setting in the configuration file, as described
    in section \ `3.8.9 <Security.html#x36-2920003.8.9>`__.
 HOSTALLOWWRITE
    is a list of host names that are allowed to issue WRITE commands to
    HTCondor daemons. This value should be set in accordance with the
    ``HOSTALLOW_WRITE`` setting in the configuration file, as described
    in section \ `3.8.9 <Security.html#x36-2920003.8.9>`__.
 HOSTALLOWADMINISTRATOR
    is a list of host names that are allowed to issue ADMINISTRATOR
    commands to HTCondor daemons. This value should be set in accordance
    with the ``HOSTALLOW_ADMINISTRATOR`` setting in the configuration
    file, as described in
    section \ `3.8.9 <Security.html#x36-2920003.8.9>`__.
 INSTALLDIR
    defines the path to the directory where HTCondor will be installed.
 POOLHOSTNAME
    defines the host name of the pool’s central manager.
 ACCOUNTINGDOMAIN
    defines the accounting (or UID) domain the target machine will be
    in.
 JVMLOCATION
    defines the path to Java virtual machine on the target machine.
 SMTPSERVER
    defines the host name of the SMTP server that the target machine is
    to use to send e-mail.
 VMMEMORY
    an integer value that defines the maximum memory each VM run on the
    target machine.
 VMMAXNUMBER
    an integer value that defines the number of VMs that can be run in
    parallel on the target machine.
 VMNETWORKING = < N \| A \| B \| C >
    determines if VM Universe can use networking. This can be set to:

    -  None (N)
    -  NAT (A)
    -  Bridged (B)
    -  NAT and Bridged (C)

 USEVMUNIVERSE = < Y \| N >
    will cause the installer to enable VM Universe jobs on the target
    machine.
 LOCALCONFIG
    defines the location of the local configuration file. The value can
    be the path to a file on the local machine, or it can be a URL
    beginning with ``http``. If the value is a URL, then the
    *condor\_urlfetch* tool is invoked to fetch configuration whenever
    the configuration is read.
 PERLLOCATION
    defines the path to *Perl* on the target machine. This is required
    in order to use the **vm** universe.

After defining each of these properties for the MSI installer, the
installer can be started with the *msiexec* command. The following
command starts the installer in unattended mode, and it dumps a journal
of the installer’s progress to a log file:

::

    msiexec /qb /lxv* condor-install-log.txt /i condor-8.0.0-173133-Windows-x86.msi [property=value] ...

More information on the features of *msiexec* can be found at
Microsoft’s website at
`http://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/msiexec.mspx <http://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/msiexec.mspx>`__.

Manual Installation HTCondor on Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you are to install HTCondor on many different machines, you may wish
to use some other mechanism to install HTCondor on additional machines
rather than running the Setup program described above on each machine.

WARNING: This is for advanced users only! All others should use the
Setup program described above.

Here is a brief overview of how to install HTCondor manually without
using the provided GUI-based setup program:

 The Service
    The service that HTCondor will install is called "Condor". The
    Startup Type is Automatic. The service should log on as System
    Account, but **do not enable** "Allow Service to Interact with
    Desktop". The program that is run is *condor\_master.exe*.

    The HTCondor service can be installed and removed using the
    ``sc.exe`` tool, which is included in Windows XP and Windows 2003
    Server. The tool is also available as part of the Windows 2000
    Resource Kit.

    Installation can be done as follows:

    ::

        sc create Condor binpath= c:\condor\bin\condor_master.exe

    To remove the service, use:

    ::

        sc delete Condor

 The Registry
    HTCondor uses a few registry entries in its operation. The key that
    HTCondor uses is HKEY\_LOCAL\_MACHINE/Software/Condor. The values
    that HTCondor puts in this registry key serve two purposes.

    #. The values of CONDOR\_CONFIG and RELEASE\_DIR are used for
       HTCondor to start its service.

       CONDOR\_CONFIG should point to the ``condor_config`` file. In
       this version of HTCondor, it **must** reside on the local disk.

       RELEASE\_DIR should point to the directory where HTCondor is
       installed. This is typically C:\\Condor, and again, this **must**
       reside on the local disk.

    #. The other purpose is storing the entries from the last
       installation so that they can be used for the next one.

 The File System
    The files that are needed for HTCondor to operate are identical to
    the Unix version of HTCondor, except that executable files end in
    ``.exe``. For example the on Unix one of the files is
    ``condor_master`` and on HTCondor the corresponding file is
    ``condor_master.exe``.

    These files currently must reside on the local disk for a variety of
    reasons. Advanced Windows users might be able to put the files on
    remote resources. The main concern is twofold. First, the files must
    be there when the service is started. Second, the files must always
    be in the same spot (including drive letter), no matter who is
    logged into the machine.

    Note also that when installing manually, you will need to create the
    directories that HTCondor will expect to be present given your
    configuration. This normally is simply a matter of creating the
    ``log``, ``spool``, and ``execute`` directories. Do not stage other
    files in any of these directories; any files not created by HTCondor
    in these directories are subject to removal.

Starting HTCondor Under Windows After Installation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After the installation of HTCondor is completed, the HTCondor service
must be started. If you used the GUI-based setup program to install
HTCondor, the HTCondor service should already be started. If you
installed manually, HTCondor must be started by hand, or you can simply
reboot. NOTE: The HTCondor service will start automatically whenever you
reboot your machine.

To start HTCondor by hand:

#. From the Start menu, choose Settings.
#. From the Settings menu, choose Control Panel.
#. From the Control Panel, choose Services.
#. From Services, choose Condor, and Start.

Or, alternatively you can enter the following command from a command
prompt:

::

             net start condor

Run the Task Manager (Control-Shift-Escape) to check that HTCondor
services are running. The following tasks should be running:

-  *condor\_master.exe*
-  *condor\_negotiator.exe*, if this machine is a central manager.
-  *condor\_collector.exe*, if this machine is a central manager.
-  *condor\_startd.exe*, if you indicated that this HTCondor node should
   start jobs
-  *condor\_schedd.exe*, if you indicated that this HTCondor node should
   submit jobs to the HTCondor pool.

Also, you should now be able to open up a new cmd (DOS prompt) window,
and the HTCondor bin directory should be in your path, so you can issue
the normal HTCondor commands, such as *condor\_q* and *condor\_status*.

HTCondor is Running Under Windows ... Now What?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once HTCondor services are running, try submitting test jobs. Example 2
within section \ `2.5.1 <SubmittingaJob.html#x17-290002.5.1>`__ presents
a vanilla universe job.

Upgrading – Installing a New Version on an Existing Pool
--------------------------------------------------------

An upgrade changes the running version of HTCondor from the current
installation to a newer version. The safe method to install and start
running a newer version of HTCondor in essence is: shut down the current
installation of HTCondor, install the newer version, and then restart
HTCondor using the newer version. To allow for falling back to the
current version, place the new version in a separate directory. Copy the
existing configuration files, and modify the copy to point to and use
the new version, as well as incorporate any configuration variables that
are new or changed in the new version. Set the ``CONDOR_CONFIG``
environment variable to point to the new copy of the configuration, so
the new version of HTCondor will use the new configuration when
restarted.

As of HTCondor version 8.2.0, the default configuration file has been
substantially reduced in size by defining compile-time default values
for most configuration variables. Therefore, when upgrading from a
version of HTCondor earlier than 8.2.0 to a more recent version, the
option of reducing the size of the configuration file is an option. The
goal is to identify and use only the configuration variable values that
differ from the compile-time default values. This is facilitated by
using *condor\_config\_val* with the
**-writeconfig:upgrade **\ *a*\ rgument, to create a file that behaves
the same as the current configuration, but is much smaller, because
values matching the default values (as well as some obsolete variables)
have been removed. Items in the file created by running
*condor\_config\_val* with the **-writeconfig:upgrade **\ *a*\ rgument
will be in the order that they were read from the original configuration
files. This file is a convenient guide to stripping the cruft from old
configuration files.

When upgrading from a version of HTCondor earlier than 6.8 to more
recent version, note that the configuration settings must be modified
for security reasons. Specifically, the ``HOSTALLOW_WRITE``
configuration variable must be explicitly changed, or no jobs can be
submitted, and error messages will be issued by HTCondor tools.

Another way to upgrade leaves HTCondor running. HTCondor will
automatically restart itself if the *condor\_master* binary is updated,
and this method takes advantage of this. Download the newer version,
placing it such that it does not overwrite the currently running
version. With the download will be a new set of configuration files;
update this new set with any specializations implemented in the
currently running version of HTCondor. Then, modify the currently
running installation by changing its configuration such that the path to
binaries points instead to the new binaries. One way to do that (under
Unix) is to use a symbolic link that points to the current HTCondor
installation directory (for example, ``/opt/condor``). Change the
symbolic link to point to the new directory. If HTCondor is configured
to locate its binaries via the symbolic link, then after the symbolic
link changes, the *condor\_master* daemon notices the new binaries and
restarts itself. How frequently it checks is controlled by the
configuration variable ``MASTER_CHECK_NEW_EXEC_INTERVAL`` , which
defaults 5 minutes.

When the *condor\_master* notices new binaries, it begins a graceful
restart. On an execute machine, a graceful restart means that running
jobs are preempted. Standard universe jobs will attempt to take a
checkpoint. This could be a bottleneck if all machines in a large pool
attempt to do this at the same time. If they do not complete within the
cutoff time specified by the ``KILL`` policy expression (defaults to 10
minutes), then the jobs are killed without producing a checkpoint. It
may be appropriate to increase this cutoff time, and a better approach
may be to upgrade the pool in stages rather than all at once.

For universes other than the standard universe, jobs are preempted. If
jobs have been guaranteed a certain amount of uninterrupted run time
with ``MaxJobRetirementTime``, then the job is not killed until the
specified amount of retirement time has been exceeded (which is 0 by
default). The first step of killing the job is a soft kill signal, which
can be intercepted by the job so that it can exit gracefully, perhaps
saving its state. If the job has not gone away once the ``KILL``
expression fires (10 minutes by default), then the job is forcibly
hard-killed. Since the graceful shutdown of jobs may rely on shared
resources such as disks where state is saved, the same reasoning applies
as for the standard universe: it may be appropriate to increase the
cutoff time for large pools, and a better approach may be to upgrade the
pool in stages to avoid jobs running out of time.

Another time limit to be aware of is the configuration variable
``SHUTDOWN_GRACEFUL_TIMEOUT``. This defaults to 30 minutes. If the
graceful restart is not completed within this time, a fast restart
ensues. This causes jobs to be hard-killed.

Shutting Down and Restarting an HTCondor Pool
---------------------------------------------

All of the commands described in this section are subject to the
security policy chosen for the HTCondor pool. As such, the commands must
be either run from a machine that has the proper authorization, or run
by a user that is authorized to issue the commands.
Section \ `3.8 <Security.html#x36-2680003.8>`__ details the
implementation of security in HTCondor.

 Shutting Down HTCondor
    There are a variety of ways to shut down all or parts of an HTCondor
    pool. All utilize the *condor\_off* tool.

    To stop a single execute machine from running jobs, the
    *condor\_off* command specifies the machine by host name.

    ::

          condor_off -startd <hostname>

    A running **standard** universe job will be allowed to take a
    checkpoint before the job is killed. A running job under another
    universe will be killed. If it is instead desired that the machine
    stops running jobs only after the currently executing job completes,
    the command is

    ::

          condor_off -startd -peaceful <hostname>

    Note that this waits indefinitely for the running job to finish,
    before the *condor\_startd* daemon exits.

    Th shut down all execution machines within the pool,

    ::

          condor_off -all -startd

    To wait indefinitely for each machine in the pool to finish its
    current HTCondor job, shutting down all of the execute machines as
    they no longer have a running job,

    ::

          condor_off -all -startd -peaceful

    To shut down HTCondor on a machine from which jobs are submitted,

    ::

          condor_off -schedd <hostname>

    If it is instead desired that the submit machine shuts down only
    after all jobs that are currently in the queue are finished, first
    disable new submissions to the queue by setting the configuration
    variable

    ::

          MAX_JOBS_SUBMITTED = 0

    See instructions below in section \ `3.2.6 <#x30-1680003.2.6>`__ for
    how to reconfigure a pool. After the reconfiguration, the command to
    wait for all jobs to complete and shut down the submission of jobs
    is

    ::

          condor_off -schedd -peaceful <hostname>

    Substitute the option **-all** for the host name, if all submit
    machines in the pool are to be shut down.

 Restarting HTCondor, If HTCondor Daemons Are Not Running
    If HTCondor is not running, perhaps because one of the *condor\_off*
    commands was used, then starting HTCondor daemons back up depends on
    which part of HTCondor is currently not running.

    If no HTCondor daemons are running, then starting HTCondor is a
    matter of executing the *condor\_master* daemon. The
    *condor\_master* daemon will then invoke all other specified daemons
    on that machine. The *condor\_master* daemon executes on every
    machine that is to run HTCondor.

    If a specific daemon needs to be started up, and the
    *condor\_master* daemon is already running, then issue the command
    on the specific machine with

    ::

          condor_on -subsystem <subsystemname>

    where <subsystemname> is replaced by the daemon’s subsystem name.
    Or, this command might be issued from another machine in the pool
    (which has administrative authority) with

    ::

          condor_on <hostname> -subsystem <subsystemname>

    where <subsystemname> is replaced by the daemon’s subsystem name,
    and <hostname> is replaced by the host name of the machine where
    this *condor\_on* command is to be directed.

 Restarting HTCondor, If HTCondor Daemons Are Running
    If HTCondor daemons are currently running, but need to be killed and
    newly invoked, the *condor\_restart* tool does this. This would be
    the case for a new value of a configuration variable for which using
    *condor\_reconfig* is inadequate.

    To restart all daemons on all machines in the pool,

    ::

          condor_restart -all

    To restart all daemons on a single machine in the pool,

    ::

          condor_restart <hostname>

    where <hostname> is replaced by the host name of the machine to be
    restarted.

Reconfiguring an HTCondor Pool
------------------------------

To change a global configuration variable and have all the machines
start to use the new setting, change the value within the file, and send
a *condor\_reconfig* command to each host. Do this with a single
command,

::

      condor_reconfig -all

If the global configuration file is not shared among all the machines,
as it will be if using a shared file system, the change must be made to
each copy of the global configuration file before issuing the
*condor\_reconfig* command.

Issuing a *condor\_reconfig* command is inadequate for some
configuration variables. For those, a restart of HTCondor is required.
Those configuration variables that require a restart are listed in
section \ `3.3.11 <IntroductiontoConfiguration.html#x31-1800003.3.11>`__.
The manual page for *condor\_restart* is at
 `2051 <Condorrestart.html#x136-97600012>`__.

      
