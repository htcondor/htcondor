Microsoft Windows
=================

:index:`Windows<single: Windows; platform-specific information>`

Windows is a strategic platform for HTCondor, and therefore we have been
working toward a complete port to Windows. Our goal is to make HTCondor
every bit as capable on Windows as it is on Unix -- or even more capable.

Porting HTCondor from Unix to Windows is a formidable task, because many
components of HTCondor must interact closely with the underlying
operating system.

This section contains additional information specific to running
HTCondor on Windows. In order to effectively use HTCondor, first read
the :doc:`/overview/index` chapter and the :doc:`/users-manual/index`. If
administrating or customizing the policy and set up of HTCondor, also
read the :doc:`/admin-manual/index` chapter. After
reading these chapters, review the information in this chapter for
important information and differences when using and administrating
HTCondor on Windows.  For information on installing HTCondor for Windows,
see :doc:`../getting-htcondor/install-windows-as-administrator`.

Limitations under Windows
-------------------------

:index:`release notes<single: release notes; Windows>`

In general, this release for Windows works the same as the release of
HTCondor for Unix. However, the following items are not supported in
this version:

-  **grid** universe jobs may not be submitted from a Windows platform,
   unless the grid type is **condor**.
-  Accessing files via a network share that requires a Kerberos ticket
   (such as AFS) is not yet supported.

Supported Features under Windows
--------------------------------

Except for those items listed above, most everything works the same way
in HTCondor as it does in the Unix release. This release is based on the
HTCondor Version |release| source tree, and thus the feature set is the same
as HTCondor Version |release| for Unix. For instance, all of the following
work in HTCondor:

-  The ability to submit, run, and manage queues of jobs running on a
   cluster of Windows machines.
-  All tools such as :tool:`condor_q`, :tool:`condor_status`, :tool:`condor_userprio`,
   are included.
-  The ability to customize job policy using ClassAds. The machine
   ClassAds contain all the information included in the Unix version,
   including current load average, RAM and virtual memory sizes, integer
   and floating-point performance, keyboard/mouse idle time, etc.
   Likewise, job ClassAds contain a full complement of information,
   including system dependent entries such as dynamic updates of the
   job's image size and CPU usage.
-  Everything necessary to run an HTCondor central manager on Windows.
-  Security mechanisms.
-  HTCondor for Windows can run jobs at a lower operating system
   priority level. Jobs can be suspended, soft-killed by using a
   WM_CLOSE message, or hard-killed automatically based upon policy
   expressions. For example, HTCondor can automatically suspend a job
   whenever keyboard/mouse or non-HTCondor created CPU activity is
   detected, and continue the job after the machine has been idle for a
   specified amount of time.
-  HTCondor correctly manages jobs which create multiple processes. For
   instance, if an HTCondor job spawns multiple processes and HTCondor
   needs to kill the job, all processes created by the job will be
   terminated.
-  In addition to interactive tools, users and administrators can
   receive information from HTCondor by e-mail (standard SMTP) and/or by
   log files.
-  HTCondor includes a friendly GUI installation and set up program,
   which can perform a full install or deinstall of HTCondor.
   Information specified by the user in the set up program is stored in
   the system registry. The set up program can update a current
   installation with a new release using a minimal amount of effort.
-  HTCondor can give a job access to the running user's Registry hive.

Secure Password Storage
-----------------------

In order for HTCondor to operate properly, it must at times be able to
act on behalf of users who submit jobs. This is required on submit
machines, so that HTCondor can access a job's input files, create and
access the job's output files, and write to the job's log file from
within the appropriate security context. On Unix systems, arbitrarily
changing what user HTCondor performs its actions as is easily done when
HTCondor is started with root privileges. On Windows, however,
performing an action as a particular user or on behalf of a particular
user requires knowledge of that user's password, even when running at
the maximum privilege level. HTCondor provides secure password storage
through the use of the :tool:`condor_store_cred` tool. Passwords managed by
HTCondor are encrypted and stored in a secure location within the
Windows registry. When HTCondor needs to perform an action as or on
behalf of a particular user, it uses the securely stored password to do
so. This implies that a password is stored for every user that will
submit jobs from the Windows submit machine.
:index:`condor_credd daemon`

A further feature permits HTCondor to execute the job itself under the
security context of its submitting user, specifying the
:subcom:`run_as_owner[on Windows]`
command in the job's submit description file. With this feature, it is
necessary to configure and run a centralized *condor_credd* daemon to
manage the secure password storage. This makes each user's password
available, via an encrypted connection to the *condor_credd*, to any
execute machine that may need it.

By default, the secure password store for a submit machine when no
*condor_credd* is running is managed by the *condor_schedd*. This
approach works in environments where the user's password is only needed
on the submit machine.

Executing Jobs as the Submitting User
-------------------------------------

By default, HTCondor executes jobs on Windows using dedicated run
accounts that have minimal access rights and privileges, and which are
recreated for each new job. As an alternative, HTCondor can be
configured to allow users to run jobs using their Windows login
accounts. This may be useful if jobs need access to files on a network
share, or to other resources that are not available to the low-privilege
run account.

This feature requires use of a *condor_credd* daemon for secure
password storage and retrieval. With the *condor_credd* daemon running,
the user's password must be stored, using the :tool:`condor_store_cred`
tool. Then, a user that wants a job to run using their own account
places into the job's submit description file

.. code-block:: condor-submit

      run_as_owner = True

The condor_credd Daemon
------------------------

:index:`condor_credd daemon`

The *condor_credd* daemon manages secure password storage. A single
running instance of the *condor_credd* within an HTCondor pool is
necessary in order to provide the feature described in
:ref:`platform-specific/microsoft-windows:executing jobs as the submitting user`,
where a job runs as the submitting user, instead of as a temporary user that
has strictly limited access capabilities.

It is first necessary to select the single machine on which to run the
*condor_credd*. Often, the machine acting as the pool's central manager
is a good choice. An important restriction, however, is that the
*condor_credd* host must be a machine running Windows.

All configuration settings necessary to enable the *condor_credd* are
contained in the example file etc\\condor_config.local.credd from the
HTCondor distribution. Copy these settings into a local configuration
file for the machine that will run the *condor_credd*. Run
:tool:`condor_restart` for these new settings to take effect, then verify
(via Task Manager) that a *condor_credd* process is running.

A second set of configuration variables specify security for the
communication among HTCondor daemons. These variables must be set for
all machines in the pool. The following example settings are in the
comments contained in the etc\\condor_config.local.credd example file.
These sample settings rely on the ``PASSWORD`` method for authentication
among daemons, including communication with the *condor_credd* daemon.
The :macro:`CREDD_HOST` variable must be
customized to point to the machine hosting the *condor_credd* and the
:macro:`ALLOW_CONFIG` variable will be
customized, if needed, to refer to an administrative account that exists
on all HTCondor nodes.

.. code-block:: condor-config

    CREDD_HOST = credd.cs.wisc.edu
    CREDD_CACHE_LOCALLY = True

    STARTER_ALLOW_RUNAS_OWNER = True

    ALLOW_CONFIG = Administrator@*
    SEC_CLIENT_AUTHENTICATION_METHODS = NTSSPI, PASSWORD
    SEC_CONFIG_NEGOTIATION = REQUIRED
    SEC_CONFIG_AUTHENTICATION = REQUIRED
    SEC_CONFIG_ENCRYPTION = REQUIRED
    SEC_CONFIG_INTEGRITY = REQUIRED

The example above can be modified to meet the needs of your pool,
providing the following conditions are met:

#. The requesting client must use an authenticated connection
#. The requesting client must have an encrypted connection
#. The requesting client must be authorized for ``DAEMON`` level access.

Using a pool password on Windows
''''''''''''''''''''''''''''''''

In order for ``PASSWORD`` authenticated communication to work, a pool
password must be chosen and distributed. The chosen pool password must
be stored identically for each machine. The pool password first should
be stored on the *condor_credd* host, then on the other machines in the
pool.

To store the pool password on a Windows machine, run

.. code-block:: console

      $ condor_store_cred add -c

when logged in with the administrative account on that machine, and
enter the password when prompted. If the administrative account is
shared across all machines, that is if it is a domain account or has the
same password on all machines, logging in separately to each machine in
the pool can be avoided. Instead, the pool password can be securely
pushed out for each Windows machine using a command of the form

.. code-block:: console

      $ condor_store_cred add -c -n exec01.cs.wisc.edu

Once the pool password is distributed, but before submitting jobs, all
machines must reevaluate their configuration, so execute

.. code-block:: console

      $ condor_reconfig -all

from the central manager. This will cause each execute machine to test
its ability to authenticate with the *condor_credd*. To see whether
this test worked for each machine in the pool, run the command

.. code-block:: console

      $ condor_status -f "%s\t" Name -f "%s\n" ifThenElse(isUndefined(LocalCredd),\"UNDEF\",LocalCredd)

Any rows in the output with the ``UNDEF`` string indicate machines where
secure communication is not working properly. Verify that the pool
password is stored correctly on these machines.

Regardless of how Condor's authentication is configured, the pool
password can always be set locally by running the

.. code-block:: console

      $ condor_store_cred add -c

command as the local SYSTEM account. Third party tools such as PsExec
can be used to accomplish this. When condor_store_cred is run as the
local SYSTEM account, it bypasses the network authentication and writes
the pool password to the registry itself. This allows the other condor
daemons (also running under the SYSTEM account) to access the pool
password when authenticating against the pool's collector. In case the
pool is remote and no initial communication can be established due to
strong security, the pool password may have to be set using the above
method and following command:

.. code-block:: console

      $ condor_store_cred -u condor_pool@poolhost add

Executing Jobs with the User's Profile Loaded
---------------------------------------------

:index:`loading account profile<single: loading account profile; Windows>`

HTCondor can be configured when using dedicated run accounts, to load
the account's profile. A user's profile includes a set of personal
directories and a registry hive loaded under ``HKEY_CURRENT_USER``.

This may be useful if the job requires direct access to the user's
registry entries. It also may be useful when the job requires an
application, and the application requires registry access. This feature
is always enabled on the *condor_startd*, but it is limited to the
dedicated run account. For security reasons, the profile is cleaned
before a subsequent job which uses the dedicated run account begins.
This ensures that malicious jobs cannot discover what any previous job
has done, nor sabotage the registry for future jobs. It also ensures the
next job has a fresh registry hive.

A job that is to run with a profile uses the
:subcom:`load_profile[definition]` command
in the job's submit description file:

.. code-block:: condor-submit

    load_profile = True

This feature is currently not compatible with
:subcom:`run_as_owner[incompatibility with load_profile]`, and
will be ignored if both are specified.

Using Windows Scripts as Job Executables
----------------------------------------

HTCondor has added support for scripting jobs on Windows. Previously,
HTCondor jobs on Windows were limited to executables or batch files.
With this new support, HTCondor determines how to interpret the script
using the file name's extension. Without a file name extension, the file
will be treated as it has been in the past: as a Windows executable.

This feature may not require any modifications to HTCondor's
configuration. An example that does not require administrative
intervention are Perl scripts using *ActivePerl*.

*Windows Scripting Host* scripts do require configuration to work
correctly. The configuration variables set values to be used in registry
look up, which results in a command that invokes the correct
interpreter, with the correct command line arguments for the specific
scripting language. In Microsoft nomenclature, verbs are actions that
can be taken upon a given a file. The familiar examples of **Open**,
**Print**, and **Edit**, can be found on the context menu when a user
right clicks on a file. The command lines to be used for each of these
verbs are stored in the registry under the ``HKEY_CLASSES_ROOT`` hive.
In general, a registry look up uses the form:

.. code-block:: text

    HKEY_CLASSES_ROOT\<FileType>\Shell\<OpenVerb>\Command

Within this specification, <FileType> is the name of a file type (and
therefore a scripting language), and is obtained from the file name
extension. <OpenVerb> identifies the verb, and is obtained from the
HTCondor configuration.

The HTCondor configuration sets the selection of a verb, to aid in the
registry look up. The file name extension sets the name of the HTCondor
configuration variable. This variable name is of the form:

.. code-block:: text

    OPEN_VERB_FOR_<EXT>_FILES

<EXT> represents the file name extension. The following configuration
example uses the Open2 verb for a *Windows Scripting Host* registry look
up for several scripting languages:

.. code-block:: condor-config

    OPEN_VERB_FOR_JS_FILES  = Open2
    OPEN_VERB_FOR_VBS_FILES = Open2
    OPEN_VERB_FOR_VBE_FILES = Open2
    OPEN_VERB_FOR_JSE_FILES = Open2
    OPEN_VERB_FOR_WSF_FILES = Open2
    OPEN_VERB_FOR_WSH_FILES = Open2

In this example, HTCondor specifies the Open2 verb, instead of the
default Open verb, for a script with the file name extension of wsh. The
*Windows Scripting Host* 's Open2 verb allows standard input, standard
output, and standard error to be redirected as needed for HTCondor jobs.

A common difficulty is encountered when a script interpreter requires
access to the user's registry. Note that the user's registry is
different than the root registry. If not given access to the user's
registry, some scripts, such as *Windows Scripting Host* scripts, will
fail. The failure error message appears as:

.. code-block:: text

    CScript Error: Loading your settings failed. (Access is denied.)

The fix for this error is to give explicit access to the submitting
user's registry hive. This can be accomplished with the addition of the
:subcom:`load_profile[and scripts]` command in the job's submit description file:

.. code-block:: condor-submit

    load_profile = True

With this command, there should be no registry access errors. This
command should also work for other interpreters. Note that not all
interpreters will require access. For example, *ActivePerl* does not by
default require access to the user's registry hive.

How HTCondor for Windows Starts and Stops a Job
-----------------------------------------------

:index:`starting and stopping a job<single: starting and stopping a job; Windows>`

This section provides some details on how HTCondor starts and stops
jobs. This discussion is geared for the HTCondor administrator or
advanced user who is already familiar with the material in the
Administrator's Manual and wishes to know detailed information on what
HTCondor does when starting and stopping jobs.

When HTCondor is about to start a job, the *condor_startd* on the
execute machine spawns a *condor_starter* process. The
*condor_starter* then creates:

#. a run account on the machine with a login name of condor-slot<X>,
   where ``<X>`` is the slot number of the *condor_starter*. This
   account is added to group ``Users`` by default. The default group may
   be changed by setting configuration variable
   :macro:`DYNAMIC_RUN_ACCOUNT_LOCAL_GROUP`. This step is skipped
   if the job is to be run using the submitting user's account, as
   specified in :ref:`platform-specific/microsoft-windows:executing jobs as
   the submitting user`.
#. a new temporary working directory for the job on the execute machine.
   This directory is named ``dir_XXX``, where ``XXX`` is the process ID
   of the *condor_starter*. The directory is created in the
   ``$(EXECUTE)`` directory, as specified in HTCondor's configuration
   file. HTCondor then grants write permission to this directory for the
   user account newly created for the job.
#. a new, non-visible Window Station and Desktop for the job.
   Permissions are set so that only the account that will run the job
   has access rights to this Desktop. Any windows created by this job
   are not seen by anyone; the job is run in the background. Setting
   :macro:`USE_VISIBLE_DESKTOP` to ``True`` will allow the job to access
   the default desktop instead of a newly created one.

Next, the *condor_starter* daemon contacts the *condor_shadow* daemon,
which is running on the submitting machine, and the *condor_starter*
pulls over the job's executable and input files. These files are placed
into the temporary working directory for the job. After all files have
been received, the *condor_starter* spawns the user's executable. Its
current working directory set to the temporary working directory.

While the job is running, the *condor_starter* closely monitors the CPU
usage and image size of all processes started by the job. Every 20
minutes the *condor_starter* sends this information, along with the
total size of all files contained in the job's temporary working
directory, to the *condor_shadow*. The *condor_shadow* then inserts
this information into the job's ClassAd so that policy and scheduling
expressions can make use of this dynamic information.

If the job exits of its own accord (that is, the job completes), the
*condor_starter* first terminates any processes started by the job
which could still be around if the job did not clean up after itself.
The *condor_starter* examines the job's temporary working directory for
any files which have been created or modified and sends these files back
to the *condor_shadow* running on the submit machine. The
*condor_shadow* places these files into the
:subcom:`initialdir[eviction on Windows]` specified in
the submit description file; if no :subcom:`initialdir` was specified, the
files go into the directory where the user invoked :tool:`condor_submit`.
Once all the output files are safely transferred back, the job is
removed from the queue. If, however, the *condor_startd* forcibly kills
the job before all output files could be transferred, the job is not
removed from the queue but instead switches back to the Idle state.

If the *condor_startd* decides to vacate a job prematurely, the
*condor_starter* sends a WM_CLOSE message to the job. If the job
spawned multiple child processes, the WM_CLOSE message is only sent to
the parent process. This is the one started by the *condor_starter*.
The WM_CLOSE message is the preferred way to terminate a process on
Windows, since this method allows the job to clean up and free any
resources it may have allocated. When the job exits, the
*condor_starter* cleans up any processes left behind. At this point, if
:subcom:`when_to_transfer_output`
is set to ``ON_EXIT`` (the default) in the job's submit description
file, the job switches states, from Running to Idle, and no files are
transferred back. If :subcom:`when_to_transfer_output` is set to
``ON_EXIT_OR_EVICT``, then files in the job's temporary working
directory which were changed or modified are first sent back to the
submitting machine. If exactly which files to transfer is specified with
:subcom:`transfer_output_files`,
then this modifies the files transferred and can affect the state of the
job if the specified files do not exist. On an eviction, the
*condor_shadow* places these intermediate files into a subdirectory
created in the ``$(SPOOL)`` directory on the submitting machine. The job
is then switched back to the Idle state until HTCondor finds a different
machine on which to run. When the job is started again, HTCondor places
into the job's temporary working directory the executable and input
files as before, plus any files stored in the submit machine's
``$(SPOOL)`` directory for that job.


.. note::

    A Windows console process can intercept a WM_CLOSE message via
    the Win32 SetConsoleCtrlHandler() function, if it needs to do special
    cleanup work at vacate time; a WM_CLOSE message generates a
    CTRL_CLOSE_EVENT. See SetConsoleCtrlHandler() in the Win32
    documentation for more info.

.. note::

    The default handler in Windows for a WM_CLOSE message is for the
    process to exit. Of course, the job could be coded to ignore it and not
    exit, but eventually the *condor_startd* will become impatient and
    hard-kill the job, if that is the policy desired by the administrator.

Finally, after the job has left and any files transferred back, the
*condor_starter* deletes the temporary working directory, the temporary
account if one was created, the Window Station and the Desktop before
exiting. If the *condor_starter* should terminate abnormally, the
*condor_startd* attempts the clean up. If for some reason the
*condor_startd* should disappear as well (that is, if the entire
machine was power-cycled hard), the *condor_startd* will clean up when
HTCondor is restarted.

Security Considerations in HTCondor for Windows
-----------------------------------------------

On the execute machine (by default), the user job is run using the
access token of an account dynamically created by HTCondor which has
bare-bones access rights and privileges. For instance, if your machines
are configured so that only Administrators have write access to
C:\\WINNT, then certainly no HTCondor job run on that machine would be
able to write anything there. The only files the job should be able to
access on the execute machine are files accessible by the Users and
Everyone groups, and files in the job's temporary working directory. Of
course, if the job is configured to run using the account of the
submitting user (as described in
:ref:`platform-specific/microsoft-windows:executing jobs as the submitting user`),
it will be able to do anything that the user is able to do on the
execute machine it runs on.

On the submit machine, HTCondor impersonates the submitting user,
therefore the File Transfer mechanism has the same access rights as the
submitting user. For example, say only Administrators can write to
C:\\WINNT on the submit machine, and a user gives the following to
:tool:`condor_submit` :

.. code-block:: condor-submit

    executable = mytrojan.exe
    initialdir = c:\winnt
    output = explorer.exe
    queue

Unless that user is in group Administrators, HTCondor will not permit
``explorer.exe`` to be overwritten.

If for some reason the submitting user's account disappears between the
time :tool:`condor_submit` was run and when the job runs, HTCondor is not
able to check and see if the now-defunct submitting user has read/write
access to a given file. In this case, HTCondor will ensure that group
"Everyone" has read or write access to any file the job subsequently
tries to read or write. This is in consideration for some network
setups, where the user account only exists for as long as the user is
logged in.

HTCondor also provides protection to the job queue. It would be bad if
the integrity of the job queue is compromised, because a malicious user
could remove other user's jobs or even change what executable a user's
job will run. To guard against this, in HTCondor's default configuration
all connections to the *condor_schedd* (the process which manages the
job queue on a given machine) are authenticated using Windows' eSSPI
security layer. The user is then authenticated using the same
challenge-response protocol that Windows uses to authenticate users to
Windows file servers. Once authenticated, the only users allowed to edit
job entry in the queue are:

#. the user who originally submitted that job (i.e. HTCondor allows
   users to remove or edit their own jobs)
#. users listed in the ``condor_config`` file parameter
   :macro:`QUEUE_SUPER_USERS`. In the default configuration, only the
   "SYSTEM" (LocalSystem) account is listed here.

WARNING: Do not remove "SYSTEM" from :macro:`QUEUE_SUPER_USERS`, or HTCondor
itself will not be able to access the job queue when needed. If the
LocalSystem account on your machine is compromised, you have all sorts
of problems!

To protect the actual job queue files themselves, the HTCondor
installation program will automatically set permissions on the entire
HTCondor release directory so that only Administrators have write
access.

Finally, HTCondor has all the security mechanisms present in the
full-blown version of HTCondor. See
the :ref:`admin-manual/security:authorization` section for complete
information on how to allow/deny access to HTCondor.

Network files and HTCondor
--------------------------

HTCondor can work well with a network file server. The recommended
approach to having jobs access files on network shares is to configure
jobs to run using the security context of the submitting user (see
:ref:`platform-specific/microsoft-windows:executing jobs as the submitting user`).
If this is done, the job will be able to access resources on the network in
the same way as the user can when logged in interactively.

In some environments, running jobs as their submitting users is not a
feasible option. This section outlines some possible alternatives. The
heart of the difficulty in this case is that on the execute machine,
HTCondor creates a temporary user that will run the job. The file server
has never heard of this user before.

Choose one of these methods to make it work:

-  METHOD A: access the file server as a different user via a net use
   command with a login and password
-  METHOD B: access the file server as guest
-  METHOD C: access the file server with a "NULL" descriptor
-  METHOD D: create and have HTCondor use a special account

All of these methods have advantages and disadvantages.

Here are the methods in more detail:

METHOD A - access the file server as a different user via a net use
command with a login and password

Example: you want to copy a file off of a server before running it....

.. code-block:: bat

    @echo off
    net use \\myserver\someshare MYPASSWORD /USER:MYLOGIN
    copy \\myserver\someshare\my-program.exe
    my-program.exe

The idea here is to simply authenticate to the file server with a
different login than the temporary HTCondor login. This is easy with the
"net use" command as shown above. Of course, the obvious disadvantage is
this user's password is stored and transferred as clear text.

METHOD B - access the file server as guest

Example: you want to copy a file off of a server before running it as
GUEST

.. code-block:: bat

       @echo off
       net use \\myserver\someshare
       copy \\myserver\someshare\my-program.exe
       my-program.exe

In this example, you'd contact the server MYSERVER as the HTCondor
temporary user. However, if you have the GUEST account enabled on
MYSERVER, you will be authenticated to the server as user "GUEST". If
your file permissions (ACLs) are setup so that either user GUEST (or
group EVERYONE) has access the share "someshare" and the
directories/files that live there, you can use this method. The downside
of this method is you need to enable the GUEST account on your file
server. WARNING: This should be done \*with extreme caution\* and only
if your file server is well protected behind a firewall that blocks SMB
traffic.

METHOD C - access the file server with a "NULL" descriptor

One more option is to use NULL Security Descriptors. In this way, you
can specify which shares are accessible by NULL Descriptor by adding
them to your registry. You can then use the batch file wrapper like:

.. code-block:: bat

    net use z: \\myserver\someshare /USER:""
    z:\my-program.exe

so long as 'someshare' is in the list of allowed NULL session shares. To
edit this list, run regedit.exe and navigate to the key:

.. code-block:: text

    HKEY_LOCAL_MACHINE\
       SYSTEM\
         CurrentControlSet\
           Services\
             LanmanServer\
               Parameters\
                 NullSessionShares

and edit it. Unfortunately it is a binary value, so you'll then need to
type in the hex ASCII codes to spell out your share. Each share is
separated by a null (0x00) and the last in the list is terminated with
two nulls.

Although a little more difficult to set up, this method of sharing is a
relatively safe way to have one quasi-public share without opening the
whole guest account. You can control specifically which shares can be
accessed or not via the registry value mentioned above.

METHOD D - create and have HTCondor use a special account

Create a permanent account (called condor-guest in this description)
under which HTCondor will run jobs. On all Windows machines, and on the
file server, create the condor-guest account.

On the network file server, give the condor-guest user permissions to
access files needed to run HTCondor jobs.

Securely store the password of the condor-guest user in the Windows
registry using :tool:`condor_store_cred` on all Windows machines.

Tell HTCondor to use the condor-guest user as the owner of jobs, when
required. Details for this are in
the :doc:`/admin-manual/security` section.

The *condor_kbdd* on Windows Platforms
'''''''''''''''''''''''''''''''''''''''

Windows platforms need to use the *condor_kbdd* to monitor the idle
time of both the keyboard and mouse. By adding ``KBDD`` to configuration
variable :macro:`DAEMON_LIST`, the :tool:`condor_master` daemon invokes the
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

Interoperability between HTCondor for Unix and HTCondor for Windows
-------------------------------------------------------------------

Unix machines and Windows machines running HTCondor can happily co-exist
in the same HTCondor pool without any problems. Jobs submitted on
Windows can run on Windows or Unix, and jobs submitted on Unix can run
on Unix or Windows. Without any specification using the
:subcom:`requirements[interop with Windows and Unix]` command
in the submit description file, the default behavior will be to require
the execute machine to be of the same architecture and operating system
as the submit machine.

There is absolutely no need to run more than one HTCondor central
manager, even if there are both Unix and Windows machines in the pool.
The HTCondor central manager itself can run on either Unix or Windows;
there is no advantage to choosing one over the other.

Some differences between HTCondor for Unix -vs- HTCondor for Windows
--------------------------------------------------------------------

-  On Unix, we recommend the creation of a condor account when
   installing HTCondor. On Windows, this is not necessary, as HTCondor
   is designed to run as a system service as user LocalSystem.
-  On Unix, HTCondor finds the ``condor_config`` main configuration file
   by looking in ˜condor, in ``/etc``, or via an environment variable.
   On Windows, the location of ``condor_config`` file is determined via
   the registry key ``HKEY_LOCAL_MACHINE/Software/Condor``. Override
   this value by setting an environment variable named
   ``CONDOR_CONFIG``.
-  On Unix, in the vanilla universe at job vacate time, HTCondor sends
   the job a softkill signal defined in the submit description file,
   which defaults to SIGTERM. On Windows, HTCondor sends a WM_CLOSE
   message to the job at vacate time.
-  On Unix, if one of the HTCondor daemons has a fault, a core file will
   be created in the ``$(Log)`` directory. On Windows, a core file will
   also be created, but instead of a memory dump of the process, it will
   be a very short ASCII text file which describes what fault occurred
   and where it happened. This information can be used by the HTCondor
   developers to fix the problem.

:index:`Windows<single: Windows; platform-specific information>`
