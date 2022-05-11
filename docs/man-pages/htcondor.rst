.. _htcondor_command:

*htcondor*
===============

Manage HTCondor jobs, job sets, and resources
:index:`htcondor<single: htcondor; HTCondor commands>`\ :index:`htcondor command`

Synopsis
--------

**htcondor** [**-h** | **-\-help**] [**-v** | **-\-verbose**] [**-q** | **-\-quiet**]
| **htcondor** **job** **submit** [[**-\-resource** *resource-type* [**-\-runtime** *time-seconds*] [**-\-email** *email-address*]] | **-\-annex-name** *annex-name*] *submit-file*
| **htcondor** **job** [**resources** | **status**] *job-id*

| **htcondor** **jobset** **submit** *submit-file*
| **htcondor** **jobset** **list** [**-\-allusers**]
| **htcondor** **jobset** [**status** [**-\-nobatch**] | **remove**] [**-\-owner** *user-name*] *job-set-name*

| **htcondor** **dag** **submit** *dag-file*
| **htcondor** **dag** **status** *dagman-job-id*

| **htcondor** **annex** [**create** | **add**] [**-\-nodes** *nodes* | [[**-\-cpus** *cpus*] [**-\-mem_mb** *mem*]] [**-\-lifetime** *lifetime*] [**-\-pool** *collector-host*] [**-\-token_file** *token-file*] [**-\-tmp_dir** *tmp_dir*] [**-\-project** *project*] *annex-name* *queue@machine*
| **htcondor** **annex** [**status** | **shutdown**] *annex-name*

Description
-----------

*htcondor* is a tool for managing HTCondor jobs, job sets, resources, and annexes.

For **job**\s, the **-\-resource** option allows you to run jobs on resources other than your
local HTCondor pool.  By specifying either ``EC2`` or ``Slurm`` here, the tool
provisions resources for the time described by the **-\-runtime** option (in seconds)
and sends your HTCondor job to run on them. It assumes you have already
completed the necessary setup tasks, such as creating an account for Slurm
submissions or making your AWS access keys available for EC2 submissions.

You may instead use the **-\-annex-name** option to specify an annex on which to
your job.  The **annex** noun manages these named groups of resources
"leased" from certain HPC machines.  Presently, you must have an XSEDE log-in
to access these resources; you must also have an allocation, identified by a
"project."

Global Options
--------------
 **-h**, **-\-help**
     Display the help message. Can also be specified after any
     subcommand to display the options available for each subcommand.
 **-q**, **-\-quiet**
     Reduce verbosity of log messages.
 **-v**, **-\-verbose**
     Increase verbosity of log messages.

Job Options
-----------

 **-\-resource**
    Resource type used to run this job. Currently supports ``Slurm`` and ``EC2``.
    Assumes the necessary setup is complete and security tokens available.
 **-\-runtime**
    Amount of time in seconds to allocate resources.
    Used in conjunction with the *-\-resource* flag.
 **-\-email**
    Email address to receive notification messages.
 **--skip-history**
    Passed to the *status* subcommand to skip checking history
    if job not found in the active job queue.
 **-\-annex-name**
    Name the annex this job must run on.  Mutually exclusive with
    **-\-resource** and its options.

Job Set Options
---------------

 **-\-allusers**
    Passed to the *list* subcommand to show job sets from all users
    rather than just the current user.
 **-\-nobatch**
    Passed to the *status* subcommand to display the status of
    individual job clusters within a job set
 **-\-owner** *user-name*
    Passed to the *status* or *remove* subcommand to act on job sets
    submitted by the specified user instead of the current
    user. Using this option to *remove* job sets requires superuser
    permissions.
 **--skip-history**
    Passed to the *status* subcommand to skip checking history
    if job clusters are not found in the active job queue.

Annex Options
-------------

 **-\-project** *project*
    Charge *project* for the leased resources.  On some machines,
    if you're only a member of one project, you don't need to
    specify which project to charge.

 **-\-nodes** *nodes*
    How many nodes to request.  The size of a node varies from
    machine to machine.  Mutually exclusive with both **-\-cpus**
    and **-\-mem_mb**.  Defaults to two.
 **-\-cpus** *cpus*
    How many CPUs to request, for queues which allow you to
    request only a fraction of a node.  Mutually exclusive with
 **-\-nodes**.
 **-\-mem_mb** *mem*
    How many (megabytes of) RAM to request, for queues which
    allow you to request only a fraction of a node.  Mutually
    exclusive with **-\-nodes**.
 **-\-lifetime** *lifetime*
    How long a lease to request, in seconds.  Defaults to an hour.

 **-\-pool** *collector-host*
    For experts only.  Specify the collector for the annex.
 **-\-token_file** *token-file*
    For experts only.  Specify a token for the annex to use.
 **-\-tmp_dir** *tmp-dir*
    For experts only.  Specify a location for temporary files.

Exit Status
-----------

*htcondor* will exit with a non-zero status value if it fails and
zero status if it succeeds.
