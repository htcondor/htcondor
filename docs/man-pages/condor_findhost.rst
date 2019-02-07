      

condor\_findhost
================

find machine(s) in the pool that can be used with minimal impact on
currently running HTCondor jobs and best meet any specified constraints

Synopsis
--------

**condor\_findhost** [**-help**\ ] [**-m**\ ] [**-n  **\ *num*]
[**-c  **\ *c\_expr*] [**-r  **\ *r\_expr*]
[**-p  **\ *centralmanagerhostname*]

Description
-----------

*condor\_findhost* searches an HTCondor pool of machines for the best
machine or machines that will have the minimum impact on running
HTCondor jobs if the machine or machines are taken out of the pool. The
search may be limited to the machine or machines that match a set of
constraints and rank expression.

*condor\_findhost* returns a fully-qualified domain name for each
machine. The search is limited (constrained) to a specific set of
machines using the *-c* option. The search can use the *-r* option for
rank, the criterion used for selecting a machine or machines from the
constrained list.

Options
-------

 **-help**
    Display usage information and exit
 **-m**
    Only search for entire machines. Slots within an entire machine are
    not considered.
 **-n **\ *num*
    Find and list up to *num* machines that fulfill the specification.
    *num* is an integer greater than zero.
 **-c **\ *c\_expr*
    Constrain the search to only consider machines that result from the
    evaluation of *c\_expr*. *c\_expr* is a ClassAd expression.
 **-r **\ *r\_expr*
    *r\_expr* is the rank expression evaluated to use as a basis for
    machine selection. *r\_expr* is a ClassAd expression.
 **-p **\ *centralmanagerhostname*
    Specify the pool to be searched by giving the central manager’s host
    name. Without this option, the current pool is searched.

General Remarks
---------------

*condor\_findhost* is used to locate a machine within a pool that can be
taken out of the pool with the least disturbance of the pool.

An administrator should set preemption requirements for the HTCondor
pool. The expression

::

    (Interactive =?= TRUE )

will let *condor\_findhost* know that it can claim a machine even if
HTCondor would not normally preempt a job running on that machine.

Exit Status
-----------

The exit status of *condor\_findhost* is zero on success. If not able to
identify as many machines as requested, it returns one more than the
number of machines identified. For example, if 8 machines are requested,
and *condor\_findhost* only locates 6, the exit status will be 7. If not
able to locate any machines, or an error is encountered,
*condor\_findhost* will return the value 1.

Examples
--------

To find and list four machines, preferring those with the highest mips
(on Drystone benchmark) rating:

::

    condor_findhost -n 4 -r "mips"

To find and list 24 machines, considering only those where the
``kflops`` attribute is not defined:

::

    condor_findhost -n 24 -c "kflops=?=undefined"

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
