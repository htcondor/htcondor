      

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
   `(Ticket
   #6857). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6857>`__

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
   machine. `(Ticket
   #6823). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6823>`__
-  Made the Python bindings’ ``JobEvent`` API more Pythonic by handling
   optional event attributes as if the ``JobEvent`` object were a
   dictionary, instead. See section
   `7.1.1 <PythonBindings.html#x69-5500007.1.1>`__ for details. `(Ticket
   #6820). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6820>`__
-  Added job ad attribute ``BlockReadKbytes`` and ``BlockWriteKybtes``
   which describe the number of kbytes read and written by the job to
   the sandbox directory. These are only defined on Linux machines with
   cgroup support enabled for vanilla jobs. `(Ticket
   #6826). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6826>`__
-  The new ``IOWait`` attribute gives the I/O Wait time recorded by the
   cgroup controller. `(Ticket
   #6830). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6830>`__
-  *condor\_ssh\_to\_job* is now configured to be more secure. In
   particular, it will only use FIPS 140-2 approved algorithms. `(Ticket
   #6822). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6822>`__
-  Added configuration parameter ``CRED_SUPER_USERS``, a list of users
   who are permitted to store credentials for any user when using the
   *condor\_store\_credd* command. Normally, users can only store
   credentials for themselves. `(Ticket
   #6346). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6346>`__
-  For packaged HTCondor installations, the package version is now
   present in the HTCondor version string. `(Ticket
   #6828). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6828>`__

Bugs Fixed:

-  Fixed a problem where a daemon would queue updates indefinitely when
   another daemon is offline. This is most noticeable as excess memory
   utilization when a *condor\_schedd* is trying to flock to an offline
   HTCondor pool. `(Ticket
   #6837). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6837>`__
-  Fixed a bug where invoking the Python bindings as root could change
   the effective uid of the calling process. `(Ticket
   #6817). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6817>`__
-  Jobs in REMOVED status now properly leave the queue when evaluation
   of their ``LeaveJobInQueue`` attribute changes from ``True`` to
   ``False``. `(Ticket
   #6808). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6808>`__
-  Fixed a rarely occurring bug where the *condor\_schedd* would crash
   when jobs were submitted with a ``queue`` statement with multiple
   keys. The bug was introduced in the 8.7.10 release. `(Ticket
   #6827). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6827>`__
-  Fixed a couple of bugs in the job event log reader code that were
   made visible by the new JobEventLog python object. The remote error
   and job terminated event did not read all of the available
   information from the job log correctly. `(Ticket
   #6816). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6816>`__
   `(Ticket
   #6836). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6836>`__
-  On Debian and Ubuntu systems, the templates for
   *condor\_ssh\_to\_job* and interactive submits are no longer
   installed in ``/etc/condor``. `(Ticket
   #6770). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6770>`__

      
