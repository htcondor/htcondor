*htcondor*
===============

Manage HTCondor jobs and resources
:index:`htcondor<single: htcondor; HTCondor commands>`\ :index:`htcondor command`

Synopsis
--------

**htcondor** [**-help**]

**htcondor** **job** [ *submit* | *status* | *resources* ] [**--resource** *resource-type*] 
[**--runtime** *time-seconds*]
[**--email** *email-address*]

Description
-----------

*htcondor* is a tool for managing HTCondor jobs and resources. 

At present it only supports simple job submission and monitoring.

The **--resource** option allows you to run jobs on resources other than your
local HTCondor pool. By specifying either ``EC2`` or ``Slurm`` here, the tool
provisions resources for the time described by the **--runtime** option (in seconds) 
and sends your HTCondor job to run on them. It assumes you have already 
completed the necessary setup tasks, such as creating an account for Slurm
submissions or making your AWS access keys available for EC2 submissions. 

Options
-------

 **--resource**
    Resource type used to run this job. Currently supports ``Slurm`` and ``EC2``.
    Assumes the necessary setup is complete and security tokens available.
 **--runtime**
    Amount of time in seconds to allocate resources.
    Used in conjunction with the *--resource* flag.
 **--email**
    Email address to receive notification messages.
    Used in conjunction with the *--resource* flag.

Exit Status
-----------

*htcondor* will exit with a non-zero status value if it fails and
zero status if it succeeds.

