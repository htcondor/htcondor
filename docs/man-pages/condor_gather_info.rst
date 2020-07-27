      

*condor_gather_info*
======================

Gather information about an HTCondor installation and a queued job
:index:`condor_gather_info<single: condor_gather_info; HTCondor commands>`\ :index:`condor_gather_info command`

Synopsis
--------

**condor_gather_info** [--**jobid** *ClusterId.ProcId*] [--**scratch**
*/path/to/directory*]

Description
-----------

*condor_gather_info* is a Linux-only tool that will collect and output
information about the machine it is run upon, about the HTCondor
installation local to the machine, and optionally about a specified
HTCondor job. The information gathered by this tool is most often used
as a debugging aid for the developers of HTCondor.

Without the --**jobid** option, information about the local machine and
its HTCondor installation is gathered and placed into the file called
``condor-profile.txt``, in the current working directory. The
information gathered is under the category of Identity.

With the --**jobid** option, additional information is gathered about
the job given in the command line argument and identified by its
``ClusterId`` and ``ProcId`` ClassAd attributes. The information
includes both categories, Identity and Job information. As the quantity
of information can be extensive, this information is placed into a
compressed tar file. The file is placed into the current working
directory, and it is named using the format

.. code-block:: text

    cgi-<username>-jid<ClusterId>.<ProcId>-<year>-<month>-<day>-<hour>_<minute>_<second>-<TZ>.tar.gz

All values within <> are substituted with current values. The building
of this potentially large tar file can require a fair amount of
temporary space. If the --**scratch** option is specified, it identifies
a directory in which to build the tar file. If the --**scratch** option
is not specified, then the directory will be ``/tmp/cgi-<PID>``, where
the process ID is that of the *condor_gather_info* executable.

The information gathered by this tool:

#. Identity

   -  User name who generated the report
   -  Script location and machine name
   -  Date of report creation
   -  ``uname -a``
   -  Contents of ``/etc/issue``
   -  Contents of ``/etc/redhat-release``
   -  Contents of ``/etc/debian_version``
   -  Contents of ``$(LOG)/MasterLog``
   -  Contents of ``$(LOG)/ShadowLog``
   -  Contents of ``$(LOG)/SchedLog``
   -  Output of ``ps -auxww -forest``
   -  Output of ``df -h``
   -  Output of ``iptables -L``
   -  Output of ``ls 'condor_config_val LOG'``
   -  Output of ``ldd 'condor_config_val SBIN'/condor_schedd``
   -  Contents of ``/etc/hosts``
   -  Contents of ``/etc/nsswitch.conf``
   -  Output of ``ulimit -a``
   -  Output of ``uptime``
   -  Output of ``free``
   -  Network interface configuration (``ifconfig``)
   -  HTCondor version
   -  Location of HTCondor configuration files
   -  HTCondor configuration variables

      -  All variables and values
      -  Definition locations for each configuration variable

#. Job Information

   -  Output of ``condor_q jobid``
   -  Output of ``condor_q -l jobid``
   -  Output of ``condor_q -analyze jobid``
   -  Job event log, if it exists

      -  Only events pertaining to the job ID

   -  If *condor_gather_info* has the proper permissions, it runs
      *condor_fetchlog* on the machine where the job most recently ran,
      and includes the contents of the logs from the *condor_master*,
      *condor_startd*, and *condor_starter*.

Options
-------

 **-jobid** *<ClusterId.ProcId>*
    Data mine information about this HTCondor job from the local
    HTCondor installation and *condor_schedd*.
 **-scratch** */path/to/directory*
    A path to temporary space needed when building the output tar file.
    Defaults to ``/tmp/cgi-<PID>``, where ``<PID>`` is replaced by the
    process ID of *condor_gather_info*.

Files
-----

-  ``condor-profile.txt`` The Identity portion of the information
   gathered when *condor_gather_info* is run without arguments.
-  ``cgi-<username>-jid<cluster>.<proc>-<year>-<month>-<day>-<hour>_<minute>_<second>-<TZ>.tar.gz``
   The output file which contains all of the information produced by
   this tool.

Exit Status
-----------

*condor_gather_info* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

