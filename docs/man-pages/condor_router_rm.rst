      

*condor_router_rm*
====================

Remove jobs being managed by the HTCondor Job Router
:index:`condor_router_rm<single: condor_router_rm; HTCondor commands>`\ :index:`condor_router_rm command`

Synopsis
--------

**condor_router_rm** [*router_rm options* ] [*condor_rm options* ]

Description
-----------

*condor_router_rm* is a script that provides additional features above
those offered by *condor_rm*, for removing jobs being managed by the
HTCondor Job Router.

The options that may be supplied to *condor_router_rm* belong to two
groups:

-  **router_rm options** provide the additional features
-  **condor_rm options** are those options already offered by
   *condor_rm*. See the *condor_rm* manual page for specification of
   these options.

Options
-------

 **-constraint** *X*
    (router_rm option) Remove jobs matching the constraint specified by
    *X*
 **-held**
    (router_rm option) Remove only jobs in the hold state
 **-idle**
    (router_rm option) Remove only idle jobs
 **-route** *name*
    (router_rm option) Remove only jobs on specified route

Exit Status
-----------

*condor_router_rm* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

