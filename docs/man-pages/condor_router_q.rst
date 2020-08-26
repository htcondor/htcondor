      

*condor_router_q*
===================

Display information about routed jobs in the queue
:index:`condor_router_q<single: condor_router_q; Job Router commands>`
:index:`condor_router_q`

Synopsis
--------

**condor_router_q** [**-S** ] [**-R** ] [**-I** ] [**-H** ]
[**-route** *name*] [**-idle** ] [**-held** ]
[**-constraint** *X*] [**condor_q options** ]

Description
-----------

*condor_router_q* displays information about jobs managed by the
*condor_job_router* that are in the HTCondor job queue. The
functionality of this tool is that of *condor_q*, with additional
options specialized for routed jobs. Therefore, any of the options for
*condor_q* may also be used with *condor_router_q*.

Options
-------

 **-S**
    Summarize the state of the jobs on each route.
 **-R**
    Summarize the running jobs on each route.
 **-I**
    Summarize the idle jobs on each route.
 **-H**
    Summarize the held jobs on each route.
 **-route** *name*
    Display only the jobs on the route identified by *name*.
 **-idle**
    Display only the idle jobs.
 **-held**
    Display only the held jobs.
 **-constraint** *X*
    Display only the jobs matching constraint *X*.

Exit Status
-----------

*condor_router_q* will exit with a status of 0 (zero) upon success,
and non-zero otherwise.

