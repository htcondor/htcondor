      

Stable Release Series 8.8
=========================

This is the stable release series of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
8.8.x releases. New features will be added in the 8.9.x development
series.

The details of each version are described below.

Version 8.8.2
-------------

Release Notes:

-  HTCondor version 8.8.2 not yet released.

New Features:

-  None.

Bugs Fixed:

-  None.

Version 8.8.1
-------------

Release Notes:

-  HTCondor version 8.8.1 not yet released.

Known Issues:

-  GPU resource monitoring is no longer enabled by default after reports
   indicating excessive CPU usage. GPU resource monitoring may be enable
   by adding ‘use feature: GPUsMonitor’ to the HTCondor configuration.
   :ticket:`6857`

New Features:

-  None.

Bugs Fixed:

-  None.

Version 8.8.0
-------------

Release Notes:

-  HTCondor version 8.8.0 released on January 3, 2019.

New Features:

-  Provides a new package: ``minicondor`` on Red Hat based systems and
   ``minihtcondor`` on Debian and Ubuntu based systems. This
   mini-HTCondor package configures HTCondor to work on a single
   machine. :ticket:`6823`
-  Made the Python bindings’ ``JobEvent`` API more Pythonic by handling
   optional event attributes as if the ``JobEvent`` object were a
   dictionary, instead. See section `Python
   Bindings <../apis/python-bindings.html>`__ for details. :ticket:`6820`
-  Added job ad attribute ``BlockReadKbytes`` and ``BlockWriteKybtes``
   which describe the number of kbytes read and written by the job to
   the sandbox directory. These are only defined on Linux machines with
   cgroup support enabled for vanilla jobs. :ticket:`6826`
-  The new ``IOWait`` attribute gives the I/O Wait time recorded by the
   cgroup controller. :ticket:`6830`
-  *condor\_ssh\_to\_job* is now configured to be more secure. In
   particular, it will only use FIPS 140-2 approved algorithms. :ticket:`6822`
-  Added configuration parameter ``CRED_SUPER_USERS``, a list of users
   who are permitted to store credentials for any user when using the
   *condor\_store\_credd* command. Normally, users can only store
   credentials for themselves. :ticket:`6346`
-  For packaged HTCondor installations, the package version is now
   present in the HTCondor version string. :ticket:`6828`

Bugs Fixed:

-  Fixed a problem where a daemon would queue updates indefinitely when
   another daemon is offline. This is most noticeable as excess memory
   utilization when a *condor\_schedd* is trying to flock to an offline
   HTCondor pool. :ticket:`6837`
-  Fixed a bug where invoking the Python bindings as root could change
   the effective uid of the calling process. :ticket:`6817`
-  Jobs in REMOVED status now properly leave the queue when evaluation
   of their ``LeaveJobInQueue`` attribute changes from ``True`` to
   ``False``. :ticket:`6808`
-  Fixed a rarely occurring bug where the *condor\_schedd* would crash
   when jobs were submitted with a ``queue`` statement with multiple
   keys. The bug was introduced in the 8.7.10 release. :ticket:`6827`
-  Fixed a couple of bugs in the job event log reader code that were
   made visible by the new JobEventLog python object. The remote error
   and job terminated event did not read all of the available
   information from the job log correctly. :ticket:`6816`
   :ticket:`6836`
-  On Debian and Ubuntu systems, the templates for
   *condor\_ssh\_to\_job* and interactive submits are no longer
   installed in ``/etc/condor``. :ticket:`6770`

      
