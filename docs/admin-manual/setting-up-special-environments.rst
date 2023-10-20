Setting Up for Special Environments
===================================

The following sections describe how to set up HTCondor for use in
special environments or configurations.

Configuring HTCondor for Multiple Platforms
-------------------------------------------

A single, initial configuration file may be used for all platforms in an
HTCondor pool, with platform-specific settings placed in separate files.  This
greatly simplifies administration of a heterogeneous pool by allowing
specification of platform-independent, global settings in one place, instead of
separately for each platform. This is made possible by treating the
:macro:`LOCAL_CONFIG_FILE` configuration variable as a list of files, instead
of a single file. Of course, this only helps when using a shared file system
for the machines in the pool, so that multiple machines can actually share a
single set of configuration files.

With multiple platforms, put all platform-independent settings (the vast
majority) into the single initial configuration file, which will be
shared by all platforms. Then, set the :macro:`LOCAL_CONFIG_FILE`
configuration variable from that global configuration file to specify
both a platform-specific configuration file and optionally, a local,
machine-specific configuration file.

The name of platform-specific configuration files may be specified by
using ``$(ARCH)`` and ``$(OPSYS)``, as defined automatically by
HTCondor. For example, for 32-bit Intel Windows 7 machines and 64-bit
Intel Linux machines, the files ought to be named:

.. code-block:: console

      $ condor_config.INTEL.WINDOWS
      condor_config.X86_64.LINUX

Then, assuming these files are in the directory defined by the ``ETC``
configuration variable, and machine-specific configuration files are in
the same directory, named by each machine's host name,
:macro:`LOCAL_CONFIG_FILE` becomes:

.. code-block:: condor-config

    LOCAL_CONFIG_FILE = $(ETC)/condor_config.$(ARCH).$(OPSYS), \
                        $(ETC)/$(HOSTNAME).local

Alternatively, when using AFS, an ``@sys`` link may be used to specify
the platform-specific configuration file, which lets AFS resolve this
link based on platform name. For example, consider a soft link named
``condor_config.platform`` that points to ``condor_config.@sys``. In
this case, the files might be named:

.. code-block:: console

      $ condor_config.i386_linux2
      condor_config.platform -> condor_config.@sys

and the :macro:`LOCAL_CONFIG_FILE` configuration variable would be set to

.. code-block:: condor-config

    LOCAL_CONFIG_FILE = $(ETC)/condor_config.platform, \
                        $(ETC)/$(HOSTNAME).local

Platform-Specific Configuration File Settings
'''''''''''''''''''''''''''''''''''''''''''''

The configuration variables that are truly platform-specific are:

:macro:`RELEASE_DIR`
    Full path to the installed HTCondor binaries. While the
    configuration files may be shared among different platforms, the
    binaries certainly cannot. Therefore, maintain separate release
    directories for each platform in the pool.

:macro:`MAIL`
    The full path to the mail program.

:macro:`CONSOLE_DEVICES`
    Which devices in ``/dev`` should be treated as console devices.

:macro:`DAEMON_LIST`
    Which daemons the *condor_master* should start up. The reason this
    setting is platform-specific is to distinguish the *condor_kbdd*.
    It is needed on many Linux and Windows machines, and it is not
    needed on other platforms.

Reasonable defaults for all of these configuration variables will be
found in the default configuration files inside a given platform's
binary distribution (except the :macro:`RELEASE_DIR`, since the location of
the HTCondor binaries and libraries is installation specific). With
multiple platforms, use one of the ``condor_config`` files from either
running *condor_configure* or from the
``$(RELEASE_DIR)``/etc/examples/condor_config.generic file, take these
settings out, save them into a platform-specific file, and install the
resulting platform-independent file as the global configuration file.
Then, find the same settings from the configuration files for any other
platforms to be set up, and put them in their own platform-specific
files. Finally, set the :macro:`LOCAL_CONFIG_FILE` configuration variable to
point to the appropriate platform-specific file, as described above.

Not even all of these configuration variables are necessarily going to
be different. For example, if an installed mail program understands the
**-s** option in ``/usr/local/bin/mail`` on all platforms, the :macro:`MAIL`
macro may be set to that in the global configuration file, and not
define it anywhere else. For a pool with only Linux or Windows machines,
the :macro:`DAEMON_LIST` will be the same for each, so there is no reason not
to put that in the global configuration file.

Other Uses for Platform-Specific Configuration Files
''''''''''''''''''''''''''''''''''''''''''''''''''''

An installation may want other configuration variables to be platform-specific.
Perhaps a different policy is desired for one of the platforms.  Perhaps
different people should get the e-mail about problems with the different
platforms. There is nothing hard-coded about any of this. What is shared and
what should not shared is entirely configurable.

Since the :macro:`LOCAL_CONFIG_FILE` macro
can be an arbitrary list of files, an installation can even break up the
global, platform-independent settings into separate files. In fact, the
global configuration file might only contain a definition for
:macro:`LOCAL_CONFIG_FILE`, and all other configuration variables would be
placed in separate files.

Different people may be given different permissions to change different
HTCondor settings. For example, if a user is to be able to change
certain settings, but nothing else, those settings may be placed in a
file which was early in the :macro:`LOCAL_CONFIG_FILE` list, to give that
user write permission on that file. Then, include all the other files
after that one. In this way, if the user was attempting to change
settings that the user should not be permitted to change, the settings
would be overridden.

This mechanism is quite flexible and powerful. For very specific
configuration needs, they can probably be met by using file permissions,
the :macro:`LOCAL_CONFIG_FILE` configuration variable, and imagination.

The *condor_kbdd*
------------------

:index:`condor_kbdd daemon`

The HTCondor keyboard daemon, *condor_kbdd*, monitors X events on
machines where the operating system does not provide a way of monitoring
the idle time of the keyboard or mouse. On Linux platforms, it is needed
to detect USB keyboard activity. Otherwise, it is not needed. On Windows
platforms, the *condor_kbdd* is the primary way of monitoring the idle
time of both the keyboard and mouse.

The *condor_kbdd* on Windows Platforms
'''''''''''''''''''''''''''''''''''''''

Windows platforms need to use the *condor_kbdd* to monitor the idle
time of both the keyboard and mouse. By adding ``KBDD`` to configuration
variable :macro:`DAEMON_LIST`, the *condor_master* daemon invokes the
*condor_kbdd*, which then does the right thing to monitor activity
given the version of Windows running.

With Windows Vista and more recent version of Windows, user sessions are
moved out of session 0. Therefore, the *condor_startd* service is no
longer able to listen to keyboard and mouse events. The *condor_kbdd*
will run in an invisible window and should not be noticeable by the
user, except for a listing in the task manager. When the user logs out,
the program is terminated by Windows. This implementation also appears
in versions of Windows that predate Vista, because it adds the
capability of monitoring keyboard activity from multiple users.

To achieve the auto-start with user login, the HTCondor installer adds a
*condor_kbdd* entry to the registry key at
HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Run. On 64-bit
versions of Vista and more recent Windows versions, the entry is
actually placed in
HKLM\\Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run.

In instances where the *condor_kbdd* is unable to connect to the
*condor_startd*, it is likely because an exception was not properly
added to the Windows firewall.

The *condor_kbdd* on Linux Platforms
'''''''''''''''''''''''''''''''''''''

On Linux platforms, great measures have been taken to make the
*condor_kbdd* as robust as possible, but the X window system was not
designed to facilitate such a need, and thus is not as efficient on
machines where many users frequently log in and out on the console.

In order to work with X authority, which is the system by which X
authorizes processes to connect to X servers, the *condor_kbdd* needs
to run with super user privileges. Currently, the *condor_kbdd* assumes
that X uses the ``HOME`` environment variable in order to locate a file
named ``.Xauthority``. This file contains keys necessary to connect to
an X server. The keyboard daemon attempts to set ``HOME`` to various
users' home directories in order to gain a connection to the X server
and monitor events. This may fail to work if the keyboard daemon is not
allowed to attach to the X server, and the state of a machine may be
incorrectly set to idle when a user is, in fact, using the machine.

In some environments, the *condor_kbdd* will not be able to connect to
the X server because the user currently logged into the system keeps
their authentication token for using the X server in a place that no
local user on the current machine can get to. This may be the case for
files on AFS, because the user's ``.Xauthority`` file is in an AFS home
directory.

There may also be cases where the *condor_kbdd* may not be run with
super user privileges because of political reasons, but it is still
desired to be able to monitor X activity. In these cases, change the XDM
configuration in order to start up the *condor_kbdd* with the
permissions of the logged in user. If running X11R6.3, the files to edit
will probably be in ``/usr/X11R6/lib/X11/xdm``. The ``.xsession`` file
should start up the *condor_kbdd* at the end, and the ``.Xreset`` file
should shut down the *condor_kbdd*. The **-l** option can be used to
write the daemon's log file to a place where the user running the daemon
has permission to write a file. The file's recommended location will be
similar to ``$HOME/.kbdd.log``, since this is a place where every user
can write, and the file will not get in the way. The **-pidfile** and
**-k** options allow for easy shut down of the *condor_kbdd* by storing
the process ID in a file. It will be necessary to add lines to the XDM
configuration similar to

.. code-block:: console

      $ condor_kbdd -l $HOME/.kbdd.log -pidfile $HOME/.kbdd.pid

This will start the *condor_kbdd* as the user who is currently logged
in and write the log to a file in the directory ``$HOME/.kbdd.log/``.
This will also save the process ID of the daemon to ``˜/.kbdd.pid``, so
that when the user logs out, XDM can do:

.. code-block:: console

      $ condor_kbdd -k $HOME/.kbdd.pid

This will shut down the process recorded in file ``˜/.kbdd.pid`` and
exit.

To see how well the keyboard daemon is working, review the log for the
daemon and look for successful connections to the X server. If there are
none, the *condor_kbdd* is unable to connect to the machine's X server.

Configuring HTCondor for Running Backfill Jobs
----------------------------------------------

:index:`Backfill`

HTCondor can be configured to run backfill jobs whenever the
*condor_startd* has no other work to perform. These jobs are considered
the lowest possible priority, but when machines would otherwise be idle,
the resources can be put to good use.

Currently, HTCondor only supports using the Berkeley Open Infrastructure
for Network Computing (BOINC) to provide the backfill jobs. More
information about BOINC is available at
`http://boinc.berkeley.edu <http://boinc.berkeley.edu>`_.

The rest of this section provides an overview of how backfill jobs work
in HTCondor, details for configuring the policy for when backfill jobs
are started or killed, and details on how to configure HTCondor to spawn
the BOINC client to perform the work.

Overview of Backfill jobs in HTCondor
'''''''''''''''''''''''''''''''''''''

:index:`Overview<single: Overview; Backfill>`

Whenever a resource controlled by HTCondor is in the Unclaimed/Idle
state, it is totally idle; neither the interactive user nor an HTCondor
job is performing any work. Machines in this state can be configured to
enter the Backfill state, which allows the resource to attempt a
background computation to keep itself busy until other work arrives
(either a user returning to use the machine interactively, or a normal
HTCondor job). Once a resource enters the Backfill state, the
*condor_startd* will attempt to spawn another program, called a
backfill client, to launch and manage the backfill computation. When
other work arrives, the *condor_startd* will kill the backfill client
and clean up any processes it has spawned, freeing the machine resources
for the new, higher priority task. More details about the different
states an HTCondor resource can enter and all of the possible
transitions between them are described in
:doc:`/admin-manual/ep-policy-configuration/`, especially the
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`
and
:ref:`admin-manual/ap-policy-configuration:*condor_schedd* policy configuration`
sections.

At this point, the only backfill system supported by HTCondor is BOINC.
The *condor_startd* has the ability to start and stop the BOINC client
program at the appropriate times, but otherwise provides no additional
services to configure the BOINC computations themselves. Future versions
of HTCondor might provide additional functionality to make it easier to
manage BOINC computations from within HTCondor. For now, the BOINC
client must be manually installed and configured outside of HTCondor on
each backfill-enabled machine.

Defining the Backfill Policy
''''''''''''''''''''''''''''

:index:`Defining HTCondor policy<single: Defining HTCondor policy; Backfill>`

There are a small set of policy expressions that determine if a
*condor_startd* will attempt to spawn a backfill client at all, and if
so, to control the transitions in to and out of the Backfill state. This
section briefly lists these expressions. More detail can be found in
:ref:`admin-manual/configuration-macros:condor_startd configuration file macros`.

:macro:`ENABLE_BACKFILL`
    A boolean value to determine if any backfill functionality should be
    used. The default value is ``False``.

:macro:`BACKFILL_SYSTEM`
    A string that defines what backfill system to use for spawning and
    managing backfill computations. Currently, the only supported string
    is ``"BOINC"``.

:macro:`START_BACKFILL`
    A boolean expression to control if an HTCondor resource should start
    a backfill client. This expression is only evaluated when the
    machine is in the Unclaimed/Idle state and the :macro:`ENABLE_BACKFILL`
    expression is ``True``.

:macro:`EVICT_BACKFILL`
    A boolean expression that is evaluated whenever an HTCondor resource
    is in the Backfill state. A value of ``True`` indicates the machine
    should immediately kill the currently running backfill client and
    any other spawned processes, and return to the Owner state.

The following example shows a possible configuration to enable backfill:

.. code-block:: text

    # Turn on backfill functionality, and use BOINC
    ENABLE_BACKFILL = TRUE
    BACKFILL_SYSTEM = BOINC

    # Spawn a backfill job if we've been Unclaimed for more than 5
    # minutes
    START_BACKFILL = $(StateTimer) > (5 * $(MINUTE))

    # Evict a backfill job if the machine is busy (based on keyboard
    # activity or cpu load)
    EVICT_BACKFILL = $(MachineBusy)

Overview of the BOINC system
''''''''''''''''''''''''''''

:index:`BOINC Overview<single: BOINC Overview; Backfill>`

The BOINC system is a distributed computing environment for solving
large scale scientific problems. A detailed explanation of this system
is beyond the scope of this manual. Thorough documentation about BOINC
is available at their website:
`http://boinc.berkeley.edu <http://boinc.berkeley.edu>`_. However, a
brief overview is provided here for sites interested in using BOINC with
HTCondor to manage backfill jobs.

BOINC grew out of the relatively famous SETI@home computation, where
volunteers installed special client software, in the form of a screen
saver, that contacted a centralized server to download work units. Each
work unit contained a set of radio telescope data and the computation
tried to find patterns in the data, a sign of intelligent life elsewhere
in the universe, hence the name: "Search for Extra Terrestrial
Intelligence at home". BOINC is developed by the Space Sciences Lab at
the University of California, Berkeley, by the same people who created
SETI@home. However, instead of being tied to the specific radio
telescope application, BOINC is a generic infrastructure by which many
different kinds of scientific computations can be solved. The current
generation of SETI@home now runs on top of BOINC, along with various
physics, biology, climatology, and other applications.

The basic computational model for BOINC and the original SETI@home is
the same: volunteers install BOINC client software, called the
*boinc_client*, which runs whenever the machine would otherwise be
idle. However, the BOINC installation on any given machine must be
configured so that it knows what computations to work for instead of
always working on a hard coded computation. The BOINC terminology for a
computation is a project. A given BOINC client can be configured to
donate all of its cycles to a single project, or to split the cycles
between projects so that, on average, the desired percentage of the
computational power is allocated to each project. Once the
*boinc_client* starts running, it attempts to contact a centralized
server for each project it has been configured to work for. The BOINC
software downloads the appropriate platform-specific application binary
and some work units from the central server for each project. Whenever
the client software completes a given work unit, it once again attempts
to connect to that project's central server to upload the results and
download more work.

BOINC participants must register at the centralized server for each
project they wish to donate cycles to. The process produces a unique
identifier so that the work performed by a given client can be credited
to a specific user. BOINC keeps track of the work units completed by
each user, so that users providing the most cycles get the highest
rankings, and therefore, bragging rights.

Because BOINC already handles the problems of distributing the
application binaries for each scientific computation, the work units,
and compiling the results, it is a perfect system for managing backfill
computations in HTCondor. Many of the applications that run on top of
BOINC produce their own application-specific checkpoints, so even if the
*boinc_client* is killed, for example, when an HTCondor job arrives at
a machine, or if the interactive user returns, an entire work unit will
not necessarily be lost.

Installing the BOINC client software
''''''''''''''''''''''''''''''''''''

:index:`BOINC Installation<single: BOINC Installation; Backfill>`

In HTCondor Version |release|, the *boinc_client* must be manually
downloaded, installed and configured outside of HTCondor. Download the
*boinc_client* executables at
`http://boinc.berkeley.edu/download.php <http://boinc.berkeley.edu/download.php>`_.

Once the BOINC client software has been downloaded, the *boinc_client*
binary should be placed in a location where the HTCondor daemons can use
it. The path will be specified with the HTCondor configuration variable
:macro:`BOINC_Executable`.

Additionally, a local directory on each machine should be created where
the BOINC system can write files it needs. This directory must not be
shared by multiple instances of the BOINC software. This is the same
restriction as placed on the ``spool`` or ``execute`` directories used
by HTCondor. The location of this directory is defined by
:macro:`BOINC_InitialDir`. The directory must
be writable by whatever user the *boinc_client* will run as. This user
is either the same as the user the HTCondor daemons are running as, if
HTCondor is not running as root, or a user defined via the
:macro:`BOINC_Owner` configuration variable.

Finally, HTCondor administrators wishing to use BOINC for backfill jobs
must create accounts at the various BOINC projects they want to donate
cycles to. The details of this process vary from project to project.
Beware that this step must be done manually, as the *boinc_client* can
not automatically register a user at a given project, unlike the more
fancy GUI version of the BOINC client software which many users run as a
screen saver. For example, to configure machines to perform work for the
Einstein@home project (a physics experiment run by the University of
Wisconsin at Milwaukee), HTCondor administrators should go to
`http://einstein.phys.uwm.edu/create_account_form.php <http://einstein.phys.uwm.edu/create_account_form.php>`_,
fill in the web form, and generate a new Einstein@home identity. This
identity takes the form of a project URL (such as
http://einstein.phys.uwm.edu) followed by an account key, which is a
long string of letters and numbers that is used as a unique identifier.
This URL and account key will be needed when configuring HTCondor to use
BOINC for backfill computations.

Configuring the BOINC client under HTCondor
'''''''''''''''''''''''''''''''''''''''''''

:index:`BOINC Configuration in HTCondor<single: BOINC Configuration in HTCondor; Backfill>`

After the *boinc_client* has been installed on a given machine, the
BOINC projects to join have been selected, and a unique project account
key has been created for each project, the HTCondor configuration needs
to be modified.

Whenever the *condor_startd* decides to spawn the *boinc_client* to
perform backfill computations, it will spawn a *condor_starter* to
directly launch and monitor the *boinc_client* program. This
*condor_starter* is just like the one used to invoke any other HTCondor
jobs.

This *condor_starter* reads values out of the HTCondor configuration
files to define the job it should run, as opposed to getting these
values from a job ClassAd in the case of a normal HTCondor job. All of
the configuration variables names for variables to control things such
as the path to the *boinc_client* binary to use, the command-line
arguments, and the initial working directory, are prefixed with the
string ``"BOINC_"``. Each of these variables is described as either a
required or an optional configuration variable.

Required configuration variables:

:macro:`BOINC_Executable`
    The full path and executable name of the *boinc_client* binary to
    use.

:macro:`BOINC_InitialDir`
    The full path to the local directory where BOINC should run.

:macro:`BOINC_Universe`
    The HTCondor universe used for running the *boinc_client* program.
    This must be set to ``vanilla`` for BOINC to work under HTCondor.

:macro:`BOINC_Owner`
    What user the *boinc_client* program should be run as. This
    variable is only used if the HTCondor daemons are running as root.
    In this case, the *condor_starter* must be told what user identity
    to switch to before invoking the *boinc_client*. This can be any
    valid user on the local system, but it must have write permission in
    whatever directory is specified by ``BOINC_InitialDir``.

Optional configuration variables:

:macro:`BOINC_Arguments`
    Command-line arguments that should be passed to the *boinc_client*
    program. For example, one way to specify the BOINC project to join
    is to use the **-attach_project** argument to specify a project URL
    and account key. For example:

    .. code-block:: text

        BOINC_Arguments = --attach_project http://einstein.phys.uwm.edu [account_key]

:macro:`BOINC_Environment`
    Environment variables that should be set for the *boinc_client*.

:macro:`BOINC_Output`
    Full path to the file where ``stdout`` from the *boinc_client*
    should be written. If this variable is not defined, ``stdout`` will
    be discarded.

:macro:`BOINC_Error`
    Full path to the file where ``stderr`` from the *boinc_client*
    should be written. If this macro is not defined, ``stderr`` will be
    discarded.

The following example shows one possible usage of these settings:

.. code-block:: text

    # Define a shared macro that can be used to define other settings.
    # This directory must be manually created before attempting to run
    # any backfill jobs.
    BOINC_HOME = $(LOCAL_DIR)/boinc

    # Path to the boinc_client to use, and required universe setting
    BOINC_Executable = /usr/local/bin/boinc_client
    BOINC_Universe = vanilla

    # What initial working directory should BOINC use?
    BOINC_InitialDir = $(BOINC_HOME)

    # Where to place stdout and stderr
    BOINC_Output = $(BOINC_HOME)/boinc.out
    BOINC_Error = $(BOINC_HOME)/boinc.err

If the HTCondor daemons reading this configuration are running as root,
an additional variable must be defined:

.. code-block:: text

    # Specify the user that the boinc_client should run as:
    BOINC_Owner = nobody

In this case, HTCondor would spawn the *boinc_client* as nobody, so the
directory specified in ``$(BOINC_HOME)`` would have to be writable by
the nobody user.

A better choice would probably be to create a separate user account just
for running BOINC jobs, so that the local BOINC installation is not
writable by other processes running as nobody. Alternatively, the
``BOINC_Owner`` could be set to daemon.

**Attaching to a specific BOINC project**

There are a few ways to attach an HTCondor/BOINC installation to a given
BOINC project:

-  Use the **-attach_project** argument to the *boinc_client* program,
   defined via the ``BOINC_Arguments`` variable. The *boinc_client*
   will only accept a single **-attach_project** argument, so this
   method can only be used to attach to one project.
-  The *boinc_cmd* command-line tool can perform various BOINC
   administrative tasks, including attaching to a BOINC project. Using
   *boinc_cmd*, the appropriate argument to use is called
   **-project_attach**. Unfortunately, the *boinc_client* must be
   running for *boinc_cmd* to work, so this method can only be used
   once the HTCondor resource has entered the Backfill state and has
   spawned the *boinc_client*.
-  Manually create account files in the local BOINC directory. Upon
   start up, the *boinc_client* will scan its local directory (the
   directory specified with ``BOINC_InitialDir``) for files of the form
   ``account_[URL].xml``, for example,
   ``account_einstein.phys.uwm.edu.xml``. Any files with a name that
   matches this convention will be read and processed. The contents of
   the file define the project URL and the authentication key. The
   format is:

   .. code-block:: text

       <account>
         <master_url>[URL]</master_url>
         <authenticator>[key]</authenticator>
       </account>

   For example:

   .. code-block:: text

       <account>
         <master_url>http://einstein.phys.uwm.edu</master_url>
         <authenticator>aaaa1111bbbb2222cccc3333</authenticator>
       </account>

   Of course, the <authenticator> tag would use the real authentication
   key returned when the account was created at a given project.

   These account files can be copied to the local BOINC directory on all
   machines in an HTCondor pool, so administrators can either distribute
   them manually, or use symbolic links to point to a shared file
   system.

In the two cases of using command-line arguments for *boinc_client* or
running the *boinc_cmd* tool, BOINC will write out the resulting
account file to the local BOINC directory on the machine, and then
future invocations of the *boinc_client* will already be attached to
the appropriate project(s).

BOINC on Windows
''''''''''''''''

The Windows version of BOINC has multiple installation methods. The
preferred method of installation for use with HTCondor is the Shared
Installation method. Using this method gives all users access to the
executables. During the installation process

#. Deselect the option which makes BOINC the default screen saver
#. Deselect the option which runs BOINC on start up.
#. Do not launch BOINC at the conclusion of the installation.

There are three major differences from the Unix version to keep in mind
when dealing with the Windows installation:

#. The Windows executables have different names from the Unix versions.
   The Windows client is called *boinc.exe*. Therefore, the
   configuration variable :macro:`BOINC_Executable` is written:

   .. code-block:: text

       BOINC_Executable = C:\PROGRA~1\BOINC\boinc.exe

   The Unix administrative tool *boinc_cmd* is called *boinccmd.exe* on
   Windows.

#. When using BOINC on Windows, the configuration variable
   :macro:`BOINC_InitialDir` will not be
   respected fully. To work around this difficulty, pass the BOINC home
   directory directly to the BOINC application via the
   :macro:`BOINC_Arguments` configuration
   variable. For Windows, rewrite the argument line as:

   .. code-block:: text

       BOINC_Arguments = --dir $(BOINC_HOME) \
                 --attach_project http://einstein.phys.uwm.edu [account_key]

   As a consequence of setting the BOINC home directory, some projects
   may fail with the authentication error:

   .. code-block:: text

       Scheduler request failed: Peer
       certificate cannot be authenticated
       with known CA certificates.

   To resolve this issue, copy the ``ca-bundle.crt`` file from the BOINC
   installation directory to ``$(BOINC_HOME)``. This file appears to be
   project and machine independent, and it can therefore be distributed
   as part of an automated HTCondor installation.

#. The :macro:`BOINC_Owner` configuration variable
   behaves differently on Windows than it does on Unix. Its value may
   take one of two forms:

   -  domain\\user
   -  user This form assumes that the user exists in the local domain
      (that is, on the computer itself).

   Setting this option causes the addition of the job attribute

   .. code-block:: text

       RunAsUser = True

   to the backfill client. This further implies that the configuration
   variable 
   :macro:`STARTER_ALLOW_RUNAS_OWNER` be set to ``True`` to insure
   that the local *condor_starter* be able to run jobs in this manner.
   For more information on the ``RunAsUser`` attribute, see
   :ref:`platform-specific/microsoft-windows:executing jobs as the submitting
   user`. For more information on the the ``STARTER_ALLOW_RUNAS_OWNER``
   configuration variable, see
   :ref:`admin-manual/configuration-macros:shared file system configuration
   file macros`.

Per Job PID Namespaces
----------------------

:index:`per job<single: per job; PID namespaces>`
:index:`per job PID namespaces<single: per job PID namespaces; namespaces>`
:index:`per job PID namespaces<single: per job PID namespaces; Linux kernel>`

Per job PID namespaces provide enhanced isolation of one process tree
from another through kernel level process ID namespaces. HTCondor may
enable the use of per job PID namespaces for Linux RHEL 6, Debian 6, and
more recent kernels.

Read about per job PID namespaces
`http://lwn.net/Articles/531419/ <http://lwn.net/Articles/531419/>`_.

The needed isolation of jobs from the same user that execute on the same
machine as each other is already provided by the implementation of slot
users as described in
:ref:`admin-manual/security:user accounts in htcondor on unix platforms`. This
is the recommended way to implement the prevention of interference between more
than one job submitted by a single user. However, the use of a shared
file system by slot users presents issues in the ownership of files
written by the jobs.

The per job PID namespace provides a way to handle the ownership of
files produced by jobs within a shared file system. It also isolates the
processes of a job within its PID namespace. As a side effect and
benefit, the clean up of processes for a job within a PID namespace is
enhanced. When the process with PID = 1 is killed, the operating system
takes care of killing all child processes.

To enable the use of per job PID namespaces, set the configuration to
include

.. code-block:: text

      USE_PID_NAMESPACES = True

This configuration variable defaults to ``False``, thus the use of per
job PID namespaces is disabled by default.

Group ID-Based Process Tracking
-------------------------------

One function that HTCondor often must perform is keeping track of all
processes created by a job. This is done so that HTCondor can provide
resource usage statistics about jobs, and also so that HTCondor can
properly clean up any processes that jobs leave behind when they exit.

In general, tracking process families is difficult to do reliably. By
default HTCondor uses a combination of process parent-child
relationships, process groups, and information that HTCondor places in a
job's environment to track process families on a best-effort basis. This
usually works well, but it can falter for certain applications or for
jobs that try to evade detection.

Jobs that run with a user account dedicated for HTCondor's use can be
reliably tracked, since all HTCondor needs to do is look for all
processes running using the given account. Administrators must specify
in HTCondor's configuration what accounts can be considered dedicated
via the :macro:`DEDICATED_EXECUTE_ACCOUNT_REGEXP` setting. See
:ref:`admin-manual/security:user accounts in htcondor on unix platforms` for
further details.

Ideally, jobs can be reliably tracked regardless of the user account
they execute under. This can be accomplished with group ID-based
tracking. This method of tracking requires that a range of dedicated
group IDs (GID) be set aside for HTCondor's use. The number of GIDs that
must be set aside for an execute machine is equal to its number of
execution slots. GID-based tracking is only available on Linux, and it
requires that HTCondor daemons run as root.

GID-based tracking works by placing a dedicated GID in the supplementary
group list of a job's initial process. Since modifying the supplementary
group ID list requires root privilege, the job will not be able to
create processes that go unnoticed by HTCondor.

Once a suitable GID range has been set aside for process tracking,
GID-based tracking can be enabled via the
:macro:`USE_GID_PROCESS_TRACKING` parameter. The minimum and
maximum GIDs included in the range are specified with the
:macro:`MIN_TRACKING_GID` and :macro:`MAX_TRACKING_GID` settings. For
example, the following would enable GID-based tracking for an execute
machine with 8 slots.

.. code-block:: text

    USE_GID_PROCESS_TRACKING = True
    MIN_TRACKING_GID = 750
    MAX_TRACKING_GID = 757

If the defined range is too small, such that there is not a GID
available when starting a job, then the *condor_starter* will fail as
it tries to start the job. An error message will be logged stating that
there are no more tracking GIDs.

GID-based process tracking requires use of the *condor_procd*. If
:macro:`USE_GID_PROCESS_TRACKING` is true, the *condor_procd* will be used
regardless of the :macro:`USE_PROCD` setting.
Changes to :macro:`MIN_TRACKING_GID` and :macro:`MAX_TRACKING_GID` require a full
restart of HTCondor.

.. _resource_limits_with_cgroups:

Cgroup-Based Process Tracking
-----------------------------

:index:`cgroup based process tracking`

A new feature in Linux version 2.6.24 allows HTCondor to more accurately
and safely manage jobs composed of sets of processes. This Linux feature
is called Control Groups, or cgroups for short, and it is available
starting with RHEL 6, Debian 6, and related distributions. Documentation
about Linux kernel support for cgroups can be found in the Documentation
directory in the kernel source code distribution. Another good reference
is
`http://docs.redhat.com/docs/en-US/Red_Hat_Enterprise_Linux/6/html/Resource_Management_Guide/index.html <http://docs.redhat.com/docs/en-US/Red_Hat_Enterprise_Linux/6/html/Resource_Management_Guide/index.html>`_

The interface between the kernel cgroup functionality is via a (virtual)
file system, usually mounted at ``/sys/fs/cgroup``.

If your Linux distribution uses *systemd*, it will mount the cgroup file
system, and the only remaining item is to set configuration variable
:macro:`BASE_CGROUP`, as described below.

When cgroups are correctly configured and running, the virtual file
system mounted on ``/sys/fs/cgroup`` should have several subdirectories under
it, and there should an ``htcondor`` subdirectory under the directory
``/sys/fs/cgroup/cpu``, ``/sys/fs/cgroup/memory`` and some others.

The *condor_starter* daemon uses cgroups by default on Linux systems to
accurately track all the processes started by a job, even when
quickly-exiting parent processes spawn many child processes. As with the
GID-based tracking, this is only implemented when a *condor_procd*
daemon is running.

Kernel cgroups are named in a virtual file system hierarchy. HTCondor
will put each running job on the execute node in a distinct cgroup. The
name of this cgroup is the name of the execute directory for that
*condor_starter*, with slashes replaced by underscores, followed by the
name and number of the slot. So, for the memory controller, a job
running on slot1 would have its cgroup located at
``/sys/fs/cgroup/memory/htcondor/condor_var_lib_condor_execute_slot1/``. The
``tasks`` file in this directory will contain a list of all the
processes in this cgroup, and many other files in this directory have
useful information about resource usage of this cgroup. See the kernel
documentation for full details.

Once cgroup-based tracking is configured, usage should be invisible to
the user and administrator. The *condor_procd* log, as defined by
configuration variable :macro:`PROCD_LOG`, will mention that it is using this
method, but no user visible changes should occur, other than the
impossibility of a quickly-forking process escaping from the control of
the *condor_starter*, and the more accurate reporting of memory usage.

A cgroup-enabled HTCondor will install and handle a per-job (not per-process)
Linux Out of Memory killer (OOM-Killer).  When a job exceeds the memory
provisioned by the *condor_startd*, the Linux kernel will send an OOM
message to the *condor_starter*, and HTCondor will evict the job, and
put it on hold.  Sometimes, even when the job's memory usage is below
the provisioned amount, if other, non-HTCondor processes, on the system
are using too much memory, the linux kernel may choose to OOM-kill the
job.  In this case, HTCondor will log a message and evict the job, mark
it as idle, so it can start again somewhere else.

Limiting Resource Usage Using Cgroups
-------------------------------------

:index:`resource limits with cgroups`
:index:`on resource usage with cgroup<single: on resource usage with cgroup; limits>`
:index:`resource limits<single: resource limits; cgroups>`

While the method described to limit a job's resource usage is portable,
and it should run on any Linux or BSD or Unix system, it suffers from
one large flaw. The flaw is that resource limits imposed are per
process, not per job. An HTCondor job is often composed of many Unix
processes. If the method of limiting resource usage with a user job
wrapper is used to impose a 2 Gigabyte memory limit, that limit applies
to each process in the job individually. If a job created 100 processes,
each using just under 2 Gigabytes, the job would continue without the
resource limits kicking in. Clearly, this is not what the machine owner
intends. Moreover, the memory limit only applies to the virtual memory
size, not the physical memory size, or the resident set size. This can
be a problem for jobs that use the ``mmap`` system call to map in a
large chunk of virtual memory, but only need a small amount of memory at
one time. Typically, the resource the administrator would like to
control is physical memory, because when that is in short supply, the
machine starts paging, and can become unresponsive very quickly.

The *condor_starter* can, using the Linux cgroup capability, apply
resource limits collectively to sets of jobs, and apply limits to the
physical memory used by a set of processes. The main downside of this
technique is that it is only available on relatively new Unix
distributions such as RHEL 6 and Debian 6. This technique also may
require editing of system configuration files.

To enable cgroup-based limits, first ensure that cgroup-based tracking
is enabled, as it is by default on supported systems, as described in
section  `3.14.13 <#x42-3790003.14.13>`_. Once set, the
*condor_starter* will create a cgroup for each job, and set
attributes in that cgroup to control memory and cpu usage. These
attributes are the cpu.shares attribute in the cpu controller, and
two attributes in the memory controller, both
memory.limit_in_bytes, and memory.soft_limit_in_bytes. The
configuration variable :macro:`CGROUP_MEMORY_LIMIT_POLICY` controls this.
If :macro:`CGROUP_MEMORY_LIMIT_POLICY` is set to the string ``hard``, the hard
limit will be set to the slot size, and the soft limit to 90% of the
slot size.. If set to ``soft``, the soft limit will be set to the slot
size and the hard limit will be set to the memory size of the whole startd.
By default, this whole size is the detected memory the size, minus
RESERVED_MEMORY.  Or, if :macro:`MEMORY` is defined, that value is used..

No limits will be set if the value is ``none``. The default is
``none``. If the hard limit is in force, then the total amount of
physical memory used by the sum of all processes in this job will not be
allowed to exceed the limit. If the process goes above the hard
limit, the job will be put on hold.

The memory size used in both cases is the machine ClassAd
attribute ``Memory``. Note that ``Memory`` is a static amount when using
static slots, but it is dynamic when partitionable slots are used. That
is, the limit is whatever the "Mem" column of condor_status reports for
that slot.

If :macro:`CGROUP_MEMORY_LIMIT_POLICY` is set, HTCondor will also also use
cgroups to limit the amount of swap space used by each job. By default,
the maximum amount of swap space used by each slot is the total amount
of Virtual Memory in the slot, minus the amount of physical memory. Note
that HTCondor measures virtual memory in kbytes, and physical memory in
megabytes. To prevent jobs with high memory usage from thrashing and
excessive paging, and force HTCondor to put them on hold instead, you
can tell condor that a job should never use swap, by setting
DISABLE_SWAP_FOR_JOB to true (the default is false).

In addition to memory, the *condor_starter* can also control the total
amount of CPU used by all processes within a job. To do this, it writes
a value to the cpu.shares attribute of the cgroup cpu controller. The
value it writes is copied from the ``Cpus`` attribute of the machine
slot ClassAd multiplied by 100. Again, like the ``Memory`` attribute,
this value is fixed for static slots, but dynamic under partitionable
slots. This tells the operating system to assign cpu usage
proportionally to the number of cpus in the slot. Unlike memory, there
is no concept of ``soft`` or ``hard``, so this limit only applies when
there is contention for the cpu. That is, on an eight core machine, with
only a single, one-core slot running, and otherwise idle, the job
running in the one slot could consume all eight cpus concurrently with
this limit in play, if it is the only thing running. If, however, all
eight slots where running jobs, with each configured for one cpu, the
cpu usage would be assigned equally to each job, regardless of the
number of processes or threads in each job.

Using HTCondor with AFS
-----------------------

:index:`AFS<single: AFS; file system>`

Configuration variables that allow machines to interact with and use a
shared file system are given at the 
:ref:`admin-manual/configuration-macros:shared file system configuration file
macros` section.

Limitations with AFS occur because HTCondor does not currently have a
way to authenticate itself to AFS. This is true of the HTCondor daemons
that would like to authenticate as the AFS user condor, and of the
*condor_shadow* which would like to authenticate as the user who
submitted the job it is serving. Since neither of these things can
happen yet, there are special things to do when interacting with AFS.
Some of this must be done by the administrator(s) installing HTCondor.
Other things must be done by HTCondor users who submit jobs.

AFS and HTCondor for Users
''''''''''''''''''''''''''

The *condor_shadow* daemon runs on the machine where jobs are
submitted. It performs all file system access on behalf of the jobs.
Because the *condor_shadow* daemon is not authenticated to AFS as the
user who submitted the job, the *condor_shadow* daemon will not
normally be able to write any output. Therefore the directories in which
the job will be creating output files will need to be world writable;
they need to be writable by non-authenticated AFS users. In addition,
the program's ``stdout``, ``stderr``, log file, and any file the program
explicitly opens will need to be in a directory that is world-writable.

An administrator may be able to set up special AFS groups that can make
unauthenticated access to the program's files less scary. For example,
there is supposed to be a way for AFS to grant access to any
unauthenticated process on a given host. If set up, write access need
only be granted to unauthenticated processes on the access point, as
opposed to any unauthenticated process on the Internet. Similarly,
unauthenticated read access could be granted only to processes running
on the access point.

A solution to this problem is to not use AFS for output files. If disk
space on the access point is available in a partition not on AFS,
submit the jobs from there. While the *condor_shadow* daemon is not
authenticated to AFS, it does run with the effective UID of the user who
submitted the jobs. So, on a local (or NFS) file system, the
*condor_shadow* daemon will be able to access the files, and no special
permissions need be granted to anyone other than the job submitter. If
the HTCondor daemons are not invoked as root however, the
*condor_shadow* daemon will not be able to run with the submitter's
effective UID, leading to a similar problem as with files on AFS.

AFS and HTCondor for Administrators
'''''''''''''''''''''''''''''''''''

The largest result from the lack of authentication with AFS is that the
directory defined by the configuration variable :macro:`LOCAL_DIR` and its
subdirectories ``log`` and ``spool`` on each machine must be either
writable to unauthenticated users, or must not be on AFS. Making these
directories writable a very bad security hole, so it is not a viable
solution. Placing :macro:`LOCAL_DIR` onto NFS is acceptable. To avoid AFS,
place the directory defined for :macro:`LOCAL_DIR` on a local partition on
each machine in the pool. This implies running *condor_configure* to
install the release directory and configure the pool, setting the
:macro:`LOCAL_DIR` variable to a local partition. When that is complete, log
into each machine in the pool, and run *condor_init* to set up the
local HTCondor directory.

The directory defined by :macro:`RELEASE_DIR`, which holds all the HTCondor
binaries, libraries, and scripts, can be on AFS. None of the HTCondor
daemons need to write to these files. They only need to read them. So,
the directory defined by :macro:`RELEASE_DIR` only needs to be world readable
in order to let HTCondor function. This makes it easier to upgrade the
binaries to a newer version at a later date, and means that users can
find the HTCondor tools in a consistent location on all the machines in
the pool. Also, the HTCondor configuration files may be placed in a
centralized location.

Finally, consider setting up some targeted AFS groups to help users deal
with HTCondor and AFS better. This is discussed in the following manual
subsection. In short, create an AFS group that contains all users,
authenticated or not, but which is restricted to a given host or subnet.
These should be made as host-based ACLs with AFS, but here at
UW-Madison, we have had some trouble getting that working. Instead, we
have a special group for all machines in our department. The users here
are required to make their output directories on AFS writable to any
process running on any of our machines, instead of any process on any
machine with AFS on the Internet.

