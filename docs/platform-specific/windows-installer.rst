Windows Installer
=================

This section includes detailed information about the options offered by
the Windows Installer, including how to run it unattended for automated
installations.  If you're not an experienced user, you may wish to follow
the quick start guide's
:doc:`instructions <../getting-htcondor/install-windows-as-administrator>`
instead.

Detailed Installation Instructions Using the MSI Program
''''''''''''''''''''''''''''''''''''''''''''''''''''''''

This section describes the different HTCondor Installer options in
greater detail.

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

STEP 3: This Machine's Roles.
    Each machine within an HTCondor pool can either submit jobs or
    execute submitted jobs, or both submit and execute jobs. A check box
    determines if this machine will be a submit point for the pool.

    A set of radio buttons determines the ability and configuration of
    the ability to execute jobs. There are four choices:

    - Do not run jobs on this machine. This machine will not execute HTCondor jobs.
    - Always run jobs and never suspend them.
    - Run jobs when the keyboard has been idle for 15 minutes.
    - Run jobs when the keyboard has been idle for 15 minutes, and the CPU is idle.

    If you are setting up HTCondor as a single installation for testing,
    make sure you check the box to make the machine a submit point, and
    also choose the second option from the list above.

    For a machine that is to execute jobs and the choice is one of the
    last two in the list, HTCondor needs to further know what to do with
    the currently running jobs. There are two choices:

     - Keep the job in memory and continue when the machine meets the
       condition chosen for when to run jobs.
     - Restart the job on a different machine.

    This choice involves a trade off. Restarting the job on a different
    machine is less intrusive on the workstation owner than leaving the
    job in memory for a later time. A suspended job left in memory will
    require swap space, which could be a scarce resource. Leaving a job
    in memory, however, has the benefit that accumulated run time is not
    lost for a partially completed job.

STEP 4: The Account Domain.
    Enter the machine's accounting (or UID) domain. On this version of
    HTCondor for Windows, this setting is only used for user priorities
    (see the :doc:`/admin-manual/user-priorities-negotiation` section)
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
        extended permission to do things such as change other user's
        priorities, modify the job queue, turn HTCondor services on and
        off, and restart HTCondor. The central manager should be given
        administrator access and is the default listed. This setting is
        granted to the entire machine, so care should be taken not to
        make this too open.

    For more details on these access permissions, and others that can be
    manually changed in your configuration file, please see the section
    titled Setting Up Security in HTCondor in the
    :ref:`admin-manual/security:authorization` section.

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
    :index:`location of files<single: location of files; installation>`

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

Unattended Installation Procedure Using the MSI Installer
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''

:index:`unattended install<single: unattended install; installation>`

This section details how to run the HTCondor for Windows installer in an
unattended batch mode. This mode is one that occurs completely from the
command prompt, without the GUI interface.

The HTCondor for Windows installer uses the Microsoft Installer (MSI)
technology, and it can be configured for unattended installs analogous
to any other ordinary MSI installer.

The following is a sample batch file that is used to set all the
properties necessary for an unattended install.

.. code-block:: bat

    @echo on
    set ARGS=
    set ARGS=NEWPOOL="N"
    set ARGS=%ARGS% POOLNAME=""
    set ARGS=%ARGS% RUNJOBS="C"
    set ARGS=%ARGS% VACATEJOBS="Y"
    set ARGS=%ARGS% SUBMITJOBS="Y"
    set ARGS=%ARGS% CONDOREMAIL="you@yours.com"
    set ARGS=%ARGS% SMTPSERVER="smtp.localhost"
    set ARGS=%ARGS% ALLOWREAD="*"
    set ARGS=%ARGS% ALLOWWRITE="*"
    set ARGS=%ARGS% ALLOWADMINISTRATOR="$(IP_ADDRESS)"
    set ARGS=%ARGS% INSTALLDIR="C:\Condor"
    set ARGS=%ARGS% POOLHOSTNAME="$(IP_ADDRESS)"
    set ARGS=%ARGS% ACCOUNTINGDOMAIN="none"
    set ARGS=%ARGS% JVMLOCATION="C:\Windows\system32\java.exe"
    set ARGS=%ARGS% USEVMUNIVERSE="N"
    set ARGS=%ARGS% VMMEMORY="128"
    set ARGS=%ARGS% VMMAXNUMBER="$(NUM_CPUS)"
    set ARGS=%ARGS% VMNETWORKING="N"
    REM set ARGS=%ARGS% LOCALCONFIG="http://my.example.com/condor_config.$(FULL_HOSTNAME)"

    msiexec /qb /l* condor-install-log.txt /i condor-8.0.0-133173-Windows-x86.msi %ARGS%

Each property corresponds to answers that would have been supplied while
running the interactive installer. The following is a brief explanation
of each property as it applies to unattended installations; see the above explanations 
for more detail.

    NEWPOOL = < Y | N >
        determines whether the installer will create a new pool with the
        target machine as the central manager.

    POOLNAME
        sets the name of the pool, if a new pool is to be created. Possible
        values are either the name or the empty string "".

    RUNJOBS = < N | A | I | C >
        determines when HTCondor will run jobs. This can be set to:

        -  Never run jobs (N)
        -  Always run jobs (A)
        -  Only run jobs when the keyboard and mouse are Idle (I)
        -  Only run jobs when the keyboard and mouse are idle and the CPU
           usage is low (C)

    VACATEJOBS = < Y | N >
        determines what HTCondor should do when it has to stop the execution
        of a user job. When set to Y, HTCondor will vacate the job and start
        it somewhere else if possible. When set to N, HTCondor will merely
        suspend the job in memory and wait for the machine to become
        available again.

    SUBMITJOBS = < Y | N >
        will cause the installer to configure the machine as a submit node
        when set to Y.

    CONDOREMAIL
        sets the e-mail address of the HTCondor administrator. Possible
        values are an e-mail address or the empty string "".

    ALLOWREAD
        is a list of names that are allowed to issue READ commands to
        HTCondor daemons. This value should be set in accordance with the
        ``ALLOW_READ`` :index:`ALLOW_READ` setting in the
        configuration file, as described in
        the :ref:`admin-manual/security:authorization` section.

    ALLOWWRITE
        is a list of names that are allowed to issue WRITE commands to
        HTCondor daemons. This value should be set in accordance with the
        ``ALLOW_WRITE`` :index:`ALLOW_WRITE` setting in the
        configuration file, as described in
        the :ref:`admin-manual/security:authorization` section.

    ALLOWADMINISTRATOR
        is a list of names that are allowed to issue ADMINISTRATOR commands
        to HTCondor daemons. This value should be set in accordance with the
        ``ALLOW_ADMINISTRATOR`` :index:`ALLOW_ADMINISTRATOR` setting
        in the configuration file, as described in
        the :ref:`admin-manual/security:authorization` section.

    INSTALLDIR
        defines the path to the directory where HTCondor will be installed.

    POOLHOSTNAME
        defines the host name of the pool's central manager.

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

    VMNETWORKING = < N | A | B | C >
        determines if VM Universe can use networking. This can be set to:

        -  None (N)
        -  NAT (A)
        -  Bridged (B)
        -  NAT and Bridged (C)

    USEVMUNIVERSE = < Y | N >
        will cause the installer to enable VM Universe jobs on the target
        machine.

    LOCALCONFIG
        defines the location of the local configuration file. The value can
        be the path to a file on the local machine, or it can be a URL
        beginning with ``http``. If the value is a URL, then the
        *condor_urlfetch* tool is invoked to fetch configuration whenever
        the configuration is read.

    PERLLOCATION
        defines the path to *Perl* on the target machine. This is required
        in order to use the **vm** universe.

After defining each of these properties for the MSI installer, the
installer can be started with the *msiexec* command. The following
command starts the installer in unattended mode, and it dumps a journal
of the installer's progress to a log file:

.. code-block:: doscon

    > msiexec /qb /lxv* condor-install-log.txt /i condor-8.0.0-173133-Windows-x86.msi [property=value] ...

More information on the features of *msiexec* can be found at
Microsoft's website at
`http://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/msiexec.mspx <http://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/msiexec.mspx>`_.

Manual Installation of HTCondor on Windows
------------------------------------------

:index:`manual install<single: manual install; Windows>`

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
    Desktop". The program that is run is *condor_master.exe*.

    The HTCondor service can be installed and removed using the
    ``sc.exe`` tool, which is included in Windows XP and Windows 2003
    Server. The tool is also available as part of the Windows 2000
    Resource Kit.

    Installation can be done as follows:

    .. code-block:: doscon

        > sc create Condor binpath= c:\condor\bin\condor_master.exe

    To remove the service, use:

    .. code-block:: doscon

        > sc delete Condor

 The Registry
    HTCondor uses a few registry entries in its operation. The key that
    HTCondor uses is HKEY_LOCAL_MACHINE/Software/Condor. The values
    that HTCondor puts in this registry key serve two purposes.

    #. The values of CONDOR_CONFIG and RELEASE_DIR are used for
       HTCondor to start its service.

       CONDOR_CONFIG should point to the ``condor_config`` file. In
       this version of HTCondor, it **must** reside on the local disk.

       RELEASE_DIR should point to the directory where HTCondor is
       installed. This is typically C:\\Condor, and again, this **must**
       reside on the local disk.

    #. The other purpose is storing the entries from the last
       installation so that they can be used for the next one.

 The File System
    The files that are needed for HTCondor to operate are identical to
    the Unix version of HTCondor, except that executable files end in
    ``.exe``. For example the on Unix one of the files is
    *condor_master* and on HTCondor the corresponding file is
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

For any installation, HTCondor services are installed and run as the
Local System account. Running the HTCondor services as any other account
(such as a domain user) is not supported and could be problematic.

