Configuration Templates
=======================

:index:`configuration-templates<single: configuration-templates; HTCondor>`
:index:`configuration: templates`

Achieving certain behaviors in an HTCondor pool often requires setting
the values of a number of configuration macros in concert with each
other. We have added configuration templates as a way to do this more
easily, at a higher level, without having to explicitly set each
individual configuration macro.

Configuration templates are pre-defined; users cannot define their own
templates.

Note that the value of an individual configuration macro that is set by
a configuration template can be overridden by setting that configuration
macro later in the configuration.

Detailed information about configuration templates (such as the macros
they set) can be obtained using the *condor_config_val* ``use`` option
(see the :doc:`/man-pages/condor_config_val` manual page). (This
document does not contain such information because the
*condor_config_val* command is a better way to obtain it.)

Configuration Templates: Using Predefined Sets of Configuration
---------------------------------------------------------------

:index:`USE syntax<single: USE syntax; configuration>`
:index:`USE configuration syntax`

Predefined sets of configuration can be identified and incorporated into
the configuration using the syntax

.. code-block:: text

      use <category name> : <template name>

The ``use`` key word is case insensitive. There are no requirements for
white space characters surrounding the colon character. More than one
``<template name>`` identifier may be placed within a single ``use``
line. Separate the names by a space character. There is no mechanism by
which the administrator may define their own custom ``<category name>``
or ``<template name>``.

Each predefined ``<category name>`` has a fixed, case insensitive name
for the sets of configuration that are predefined. Placement of a
``use`` line in the configuration brings in the predefined configuration
it identifies.

As of version 8.5.6, some of the configuration templates take arguments
(as described below).

Available Configuration Templates
---------------------------------

There are four ``<category name>`` values. Within a category, a
predefined, case insensitive name identifies the set of configuration it
incorporates.

``ROLE category``
    Describes configuration for the various roles that a machine might
    play within an HTCondor pool. The configuration will identify which
    daemons are running on a machine.

    -  ``Personal``

       Settings needed for when a single machine is the entire pool.

    -  ``Submit``

       Settings needed to allow this machine to submit jobs to the pool.
       May be combined with ``Execute`` and ``CentralManager`` roles.

    -  ``Execute``

       Settings needed to allow this machine to execute jobs. May be
       combined with ``Submit`` and ``CentralManager`` roles.

    -  ``CentralManager``

       Settings needed to allow this machine to act as the central
       manager for the pool. May be combined with ``Submit`` and
       ``Execute`` roles.

``FEATURE category``
    Describes configuration for implemented features.

    -  ``Remote_Runtime_Config``

       Enables the use of *condor_config_val* **-rset** to the machine
       with this configuration. Note that there are security
       implications for use of this configuration, as it potentially
       permits the arbitrary modification of configuration. Variable
       ``SETTABLE_ATTRS_CONFIG`` :index:`SETTABLE_ATTRS_CONFIG`
       must also be defined.

    -  ``Remote_Config``

       Enables the use of *condor_config_val* **-set** to the machine
       with this configuration. Note that there are security
       implications for use of this configuration, as it potentially
       permits the arbitrary modification of configuration. Variable
       ``SETTABLE_ATTRS_CONFIG`` :index:`SETTABLE_ATTRS_CONFIG`
       must also be defined.

    -  ``VMware``

       Enables use of the vm universe with VMware virtual machines. Note
       that this feature depends on Perl.

    -  ``GPUs([discovery_args])``

       Sets configuration based on detection with the
       *condor_gpu_discovery* tool, and defines a custom resource
       using the name ``GPUs``. Supports both OpenCL and CUDA, if
       detected. Automatically includes the ``GPUsMonitor`` feature.
       Optional discovery_args are passed to *condor_gpu_discovery*

    -  ``GPUsMonitor``

       Also adds configuration to report the usage of NVidia GPUs.

    -  ``Monitor( resource_name, mode, period, executable, metric[, metric]+ )``

       Configures a custom machine resource monitor with the given name,
       mode, period, executable, and metrics. See
       :ref:`admin-manual/hooks:daemon classad hooks` for the definitions of
       these terms.

    -  ``PartitionableSlot( slot_type_num [, allocation] )``

       Sets up a partitionable slot of the specified slot type number
       and allocation (defaults for slot_type_num and allocation are 1
       and 100% respectively). See the 
       :ref:`admin-manual/policy-configuration:*condor_startd* policy
       configuration` for information on partitionalble slot policies.

    -  ``AssignAccountingGroup( map_filename [, check_request] )`` Sets up a
       *condor_schedd* job transform that assigns an accounting group
       to each job as it is submitted. The accounting group is determined by
       mapping the Owner attribute of the job using the given map file, which
       should specify the allowed accounting groups each Owner is permitted to use.
       If the submitted job has an accounting group, that is treated as a requested
       accounting group and validated against the map.  If the optional
       ``check_request`` argument is true or not present submission will
       fail if the requested accounting group is present and not valid.  If the argument
       is false, the requested accounting group will be ignored if it is not valid.

    -  ``ScheddUserMapFile( map_name, map_filename )`` Defines a
       *condor_schedd* usermap named map_name using the given map
       file.

    -  ``SetJobAttrFromUserMap( dst_attr, src_attr, map_name [, map_filename] )``
       Sets up a *condor_schedd* job transform that sets the dst_attr
       attribute of each job as it is submitted. The value of dst_attr
       is determined by mapping the src_attr of the job using the
       usermap named map_name. If the optional map_filename argument
       is specifed, then this metaknob also defines a *condor_schedd*
       usermap named map_Name using the given map file.

    -  ``StartdCronOneShot( job_name, exe [, hook_args] )``

       Create a one-shot *condor_startd* job hook.
       (See :ref:`admin-manual/hooks:daemon classad hooks` for more information
       about job hooks.)

    -  ``StartdCronPeriodic( job_name, period, exe [, hook_args] )``

       Create a periodic-shot *condor_startd* job hook.
       (See :ref:`admin-manual/hooks:daemon classad hooks` for more information
       about job hooks.)

    -  ``StartdCronContinuous( job_name, exe [, hook_args] )``

       Create a (nearly) continuous *condor_startd* job hook.
       (See :ref:`admin-manual/hooks:daemon classad hooks` for more information
       about job hooks.)

    -  ``ScheddCronOneShot( job_name, exe [, hook_args] )``

       Create a one-shot *condor_schedd* job hook.
       (See :ref:`admin-manual/hooks:daemon classad hooks` for more information
       about job hooks.)

    -  ``ScheddCronPeriodic( job_name, period, exe [, hook_args] )``

       Create a periodic-shot *condor_schedd* job hook.
       (See :ref:`admin-manual/hooks:daemon classad hooks` for more information
       about job hooks.)

    -  ``ScheddCronContinuous( job_name, exe [, hook_args] )``

       Create a (nearly) continuous *condor_schedd* job hook.
       (See :ref:`admin-manual/hooks:daemon classad hooks` for more information
       about job hooks.)

    -  ``OneShotCronHook( STARTD_CRON | SCHEDD_CRON, job_name, hook_exe [,hook_args] )``

       Create a one-shot job hook.
       (See :ref:`admin-manual/hooks:daemon classad hooks` for more information
       about job hooks.)

    -  ``PeriodicCronHook( STARTD_CRON | SCHEDD_CRON , job_name, period, hook_exe [,hook_args] )``

       Create a periodic job hook.
       (See :ref:`admin-manual/hooks:daemon classad hooks` for more information
       about job hooks.)

    -  ``ContinuousCronHook( STARTD_CRON | SCHEDD_CRON , job_name, hook_exe [,hook_args] )``

       Create a (nearly) continuous job hook.
       (See :ref:`admin-manual/hooks:daemon classad hooks` for more information
       about job hooks.)

    -  ``OAuth``

       Sets configuration that enables the *condor_credd* and *condor_credmon_oauth* daemons,
       which allow for the automatic renewal of user-supplied OAuth2 credentials.
       See section :ref:`enabling_oauth_credentials` for more information.

    -  ``Adstash``

       Sets configuration that enables *condor_adstash* to run as a daemon.
       *condor_adstash* polls job history ClassAds and pushes them to an
       Elasticsearch index, see section
       :ref:`admin-manual/monitoring:Elasticsearch` for more information.

    -  ``UWCS_Desktop_Policy_Values``

       Configuration values used in the ``UWCS_DESKTOP`` policy. (Note
       that these values were previously in the parameter table;
       configuration that uses these values will have to use the
       ``UWCS_Desktop_Policy_Values`` template. For example,
       ``POLICY : UWCS_Desktop`` uses the
       ``FEATURE : UWCS_Desktop_Policy_Values`` template.)

.. _CommonCloudAttributesConfiguration:

    - ``CommonCloudAttributesAWS``
    - ``CommonCloudAttributesGoogle``

       Sets configuration that will put some common cloud-related attributes
       in the slot ads.  Use the version which specifies the cloud you're
       using.  See :ref:`CommonCloudAttributes` for details.

    - ``JobsHaveInstanceIDs``

       Sets configuration that will cause job ads to track the instance IDs
       of slots that they ran on (if available).

``POLICY category``
    Describes configuration for the circumstances under which machines
    choose to run jobs.

    -  ``Always_Run_Jobs``

       Always start jobs and run them to completion, without
       consideration of *condor_negotiator* generated preemption or
       suspension. This is the default policy, and it is intended to be
       used with dedicated resources. If this policy is used together
       with the ``Limit_Job_Runtimes`` policy, order the specification
       by placing this ``Always_Run_Jobs`` policy first.

    -  ``UWCS_Desktop``

       This was the default policy before HTCondor version 8.1.6. It is
       intended to be used with desktop machines not exclusively running
       HTCondor jobs. It injects ``UWCS`` into the name of some
       configuration variables.

    -  ``Desktop``

       An updated and reimplementation of the ``UWCS_Desktop`` policy,
       but without the ``UWCS`` naming of some configuration variables.

    -  ``Limit_Job_Runtimes( limit_in_seconds )``

       Limits running jobs to a maximum of the specified time using
       preemption. (The default limit is 24 hours.) This policy does not
       work while the machine is draining; use the following policy
       instead.

       If this policy is used together with the ``Always_Run_Jobs``
       policy, order the specification by placing this
       ``Limit_Job_Runtimes`` policy second.

    -  ``Preempt_if_Runtime_Exceeds( limit_in_seconds )``

       Limits running jobs to a maximum of the specified time using
       preemption. (The default limit is 24 hours).

    -  ``Hold_if_Runtime_Exceeds( limit_in_seconds )``

       Limits running jobs to a maximum of the specified time by placing
       them on hold immediately (ignoring any job retirement time). (The
       default limit is 24 hours).

    -  ``Preempt_If_Cpus_Exceeded``

       If the startd observes the number of CPU cores used by the job
       exceed the number of cores in the slot by more than 0.8 on
       average over the past minute, preempt the job immediately
       ignoring any job retirement time.

    -  ``Hold_If_Cpus_Exceeded``

       If the startd observes the number of CPU cores used by the job
       exceed the number of cores in the slot by more than 0.8 on
       average over the past minute, immediately place the job on hold
       ignoring any job retirement time. The job will go on hold with a
       reasonable hold reason in job attribute ``HoldReason`` and a
       value of 101 in job attribute ``HoldReasonCode``. The hold reason
       and code can be customized by specifying
       ``HOLD_REASON_CPU_EXCEEDED`` and ``HOLD_SUBCODE_CPU_EXCEEDED``
       respectively.

    -  ``Preempt_If_Disk_Exceeded``

       If the startd observes the amount of disk space used by the job
       exceed the disk in the slot, preempt the job immediately
       ignoring any job retirement time.

    -  ``Hold_If_Disk_Exceeded``

       If the startd observes the amount of disk space used by the job
       exceed the disk in the slot, immediately place the job on hold
       ignoring any job retirement time. The job will go on hold with a
       reasonable hold reason in job attribute ``HoldReason`` and a
       value of 104 in job attribute ``HoldReasonCode``. The hold reason
       and code can be customized by specifying
       ``HOLD_REASON_DISK_EXCEEDED`` and ``HOLD_SUBCODE_DISK_EXCEEDED``
       respectively.

    -  ``Preempt_If_Memory_Exceeded``

       If the startd observes the memory usage of the job exceed the
       memory provisioned in the slot, preempt the job immediately
       ignoring any job retirement time.

    -  ``Hold_If_Memory_Exceeded``

       If the startd observes the memory usage of the job exceed the
       memory provisioned in the slot, immediately place the job on hold
       ignoring any job retirement time. The job will go on hold with a
       reasonable hold reason in job attribute ``HoldReason`` and a
       value of 102 in job attribute ``HoldReasonCode``. The hold reason
       and code can be customized by specifying
       ``HOLD_REASON_MEMORY_EXCEEDED`` and
       ``HOLD_SUBCODE_MEMORY_EXCEEDED`` respectively.

    -  ``Preempt_If( policy_variable )``

       Preempt jobs according to the specified policy.
       ``policy_variable`` must be the name of a configuration macro
       containing an expression that evaluates to ``True`` if the job
       should be preempted.

       See an example here:
       :ref:`admin-manual/configuration-templates:configuration template examples`.

    -  ``Want_Hold_If( policy_variable, subcode, reason_text )``

       Add the given policy to the ``WANT_HOLD`` expression; if the
       ``WANT_HOLD`` expression is defined, ``policy_variable`` is
       prepended to the existing expression; otherwise ``WANT_HOLD`` is
       simply set to the value of the textttpolicy_variable macro.

       See an example here:
       :ref:`admin-manual/configuration-templates:configuration template examples`.

    -  ``Startd_Publish_CpusUsage``

       Publish the number of CPU cores being used by the job into to
       slot ad as attribute ``CpusUsage``. This value will be the
       average number of cores used by the job over the past minute,
       sampling every 5 seconds.

``SECURITY category``
    Describes configuration for an implemented security model.

    -  ``Host_Based``

       The default security model (based on IPs and DNS names). Do not
       combine with ``User_Based`` security.

    -  ``User_Based``

       Grants permissions to an administrator and uses
       ``With_Authentication``. Do not combine with ``Host_Based``
       security.

    -  ``With_Authentication``

       Requires both authentication and integrity checks.

    -  ``Strong``

       Requires authentication, encryption, and integrity checks.

Configuration Template Transition Syntax
----------------------------------------

For pools that are transitioning to using this new syntax in
configuration, while still having some tools and daemons with HTCondor
versions earlier than 8.1.6, special syntax in the configuration will
cause those daemons to fail upon start up, rather than use the new, but
misinterpreted, syntax. Newer daemons will ignore the extra syntax.
Placing the @ character before the ``use`` key word causes the older
daemons to fail when they attempt to parse this syntax.

As an example, consider the *condor_startd* as it starts up. A
*condor_startd* previous to HTCondor version 8.1.6 fails to start when
it sees:

.. code-block:: condor-config

    @use feature : GPUs

Running an older *condor_config_val* also identifies the ``@use`` line
as being bad. A *condor_startd* of HTCondor version 8.1.6 or more
recent sees

.. code-block:: condor-config

    use feature : GPUs

Configuration Template Examples
-------------------------------

-  Preempt a job if its memory usage exceeds the requested memory:

   .. code-block:: condor-config

        MEMORY_EXCEEDED = (isDefined(MemoryUsage) && MemoryUsage > RequestMemory)
        use POLICY : PREEMPT_IF(MEMORY_EXCEEDED) 

-  Put a job on hold if its memory usage exceeds the requested memory:

   .. code-block:: condor-config

        MEMORY_EXCEEDED = (isDefined(MemoryUsage) && MemoryUsage > RequestMemory)
        use POLICY : WANT_HOLD_IF(MEMORY_EXCEEDED, 102, memory usage exceeded request_memory) 

-  Update dynamic GPU information every 15 minutes:

   .. code-block:: condor-config

        use FEATURE : StartdCronPeriodic(DYNGPU, 15*60, $(LOCAL_DIR)\dynamic_gpu_info.pl, $(LIBEXEC)\condor_gpu_discovery -dynamic)

   where ``dynamic_gpu_info.pl`` is a simple perl script that strips off
   the DetectedGPUs line from *condor_gpu_discovery*:

   .. code-block:: perl

        #!/usr/bin/env perl
        my @attrs = `@ARGV`; 
        for (@attrs) { 
            next if ($_ =~ /^Detected/i); 
            print $_; 
        } 
