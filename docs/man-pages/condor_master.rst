*condor_master*
================

The master HTCondor Daemon
:index:`condor_master<single: condor_master; HTCondor commands>`
:index:`condor_master daemon`

Synopsis
--------

**condor_master**

Description
-----------

This daemon is responsible for keeping all the rest of the HTCondor
daemons running on each machine in your pool. It spawns the other
daemons, and periodically checks to see if there are new binaries
installed for any of them. If there are, the *condor_master* will
restart the affected daemons. In addition, if any daemon crashes, the
*condor_master* will send e-mail to the HTCondor Administrator of your
pool and restart the daemon. The *condor_master* also supports various
administrative commands that let you start, stop or reconfigure daemons
remotely. The *condor_master* will run on every machine in your
HTCondor pool, regardless of what functions each machine are performing.
Additionally, on Linux platforms, if you start the *condor_master* as
root, it will tune (but never decrease) certain kernel parameters
important to HTCondor's performance.

The :macro:`DAEMON_LIST` configuration macro is
used by the *condor_master* to provide a per-machine list of daemons
that should be started and kept running. For daemons that are specified
in the :macro:`DC_DAEMON_LIST` configuration macro, the *condor_master*
daemon will spawn them automatically appending a *-f* argument. For
those listed in :macro:`DAEMON_LIST`, but not in :macro:`DC_DAEMON_LIST`, there
will be no *-f* argument.

The *condor_master* creates certain directories necessary for its proper
functioning on start-up if they don't already exist, using the values of
the configuration settings
:macro:`EXECUTE`,
:macro:`LOCAL_DIR`,
``LOCAL_DISK_LOCK_DIR``,
:macro:`LOCAL_UNIV_EXECUTE`,
:macro:`LOCK`,
:macro:`LOG`,
:macro:`RUN`,
:macro:`SEC_CREDENTIAL_DIRECTORY_KRB`,
:macro:`SEC_CREDENTIAL_DIRECTORY_OAUTH`,
:macro:`SEC_PASSWORD_DIRECTORY`,
:macro:`SEC_TOKEN_SYSTEM_DIRECTORY`,
and
:macro:`SPOOL`.

Options
-------

 **-n** *name*
    Provides an alternate name for the *condor_master* to override that
    given by the :macro:`MASTER_NAME` configuration variable.

