.. _htcondor_command:

*htcondor*
===============

Manage HTCondor jobs, job sets, and resources
:index:`htcondor<single: htcondor; HTCondor commands>`\ :index:`htcondor command`

Synopsis
--------

**htcondor** [ **-h** | **--help** ] [ **-v** | **-q** ]

**htcondor** **job** [ *submit* | *status* | *resources* ]
[**--resource** *resource-type*]
[**--runtime** *time-seconds*]
[**--email** *email-address*]
[**--skip-history**]

| **htcondor** **jobset** *submit* description-file
| **htcondor** **jobset** *list* [**--allusers**]
| **htcondor** **jobset** [ *status* | *remove* ] job-set-name [**--owner** *user-name*] [**--nobatch**] [**--skip-history**]

| **htcondor** **dag** *submit* dag-file
| **htcondor** **dag** *status* dagman-job-id

Description
-----------

*htcondor* is a tool for managing HTCondor jobs, job sets, and resources.

For **jobs**, the **--resource** option allows you to run jobs on resources other than your
local HTCondor pool. By specifying either ``EC2`` or ``Slurm`` here, the tool
provisions resources for the time described by the **--runtime** option (in seconds) 
and sends your HTCondor job to run on them. It assumes you have already 
completed the necessary setup tasks, such as creating an account for Slurm
submissions or making your AWS access keys available for EC2 submissions. 
submissions.

Global Options
--------------
 **-h**, **--help**
     Display the help message. Can also be specified after any
     subcommand to display the options available for each subcommand.
 **-q**
     Reduce verbosity of log messages.
 **-v**
     Increase verbosity of log messages.

Job Options
-----------

 **--resource**
    Resource type used to run this job. Currently supports ``Slurm`` and ``EC2``.
    Assumes the necessary setup is complete and security tokens available.
 **--runtime**
    Amount of time in seconds to allocate resources.
    Used in conjunction with the *--resource* flag.
 **--email**
    Email address to receive notification messages.
    Used in conjunction with the *--resource* flag.
 **--skip-history**
    Passed to the *status* subcommand to skip checking history
    if job not found in the active job queue.

Job Set Options
---------------

 **--allusers**
    Passed to the *list* subcommand to show job sets from all users
    rather than just the current user.
 **--nobatch**
    Passed to the *status* subcommand to display the status of
    individual job clusters within a job set
 **--owner=USERNAME**
    Passed to the *status* or *remove* subcommand to act on job sets
    submitted by the specified user instead of the current
    user. Using this option to *remove* job sets requires superuser
    permissions.
 **--skip-history**
    Passed to the *status* subcommand to skip checking history
    if job clusters are not found in the active job queue.


Exit Status
-----------

*htcondor* will exit with a non-zero status value if it fails and
zero status if it succeeds.
