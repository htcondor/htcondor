      

*condor_findhost*
==================

find machine(s) in the pool that can be used with minimal impact on
currently running HTCondor jobs and best meet any specified constraints
:index:`condor_findhost<single: condor_findhost; HTCondor commands>`\ :index:`condor_findhost command`

Synopsis
--------

**condor_findhost** [**-help** ] [**-m** ] [**-n** *num*]
[**-c** *c_expr*] [**-r** *r_expr*]
[**-p** *centralmanagerhostname*]

Description
-----------

*condor_findhost* searches an HTCondor pool of machines for the best
machine or machines that will have the minimum impact on running
HTCondor jobs if the machine or machines are taken out of the pool. The
search may be limited to the machine or machines that match a set of
constraints and rank expression.

*condor_findhost* returns a fully-qualified domain name for each
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
 **-n** *num*
    Find and list up to *num* machines that fulfill the specification.
    *num* is an integer greater than zero.
 **-c** *c_expr*
    Constrain the search to only consider machines that result from the
    evaluation of *c_expr*. *c_expr* is a ClassAd expression.
 **-r** *r_expr*
    *r_expr* is the rank expression evaluated to use as a basis for
    machine selection. *r_expr* is a ClassAd expression.
 **-p** *centralmanagerhostname*
    Specify the pool to be searched by giving the central manager's host
    name. Without this option, the current pool is searched.

General Remarks
---------------

*condor_findhost* is used to locate a machine within a pool that can be
taken out of the pool with the least disturbance of the pool.

An administrator should set preemption requirements for the HTCondor
pool. The expression

.. code-block:: condor-classad-expr

    (Interactive =?= TRUE )

will let *condor_findhost* know that it can claim a machine even if
HTCondor would not normally preempt a job running on that machine.

Exit Status
-----------

The exit status of *condor_findhost* is zero on success. If not able to
identify as many machines as requested, it returns one more than the
number of machines identified. For example, if 8 machines are requested,
and *condor_findhost* only locates 6, the exit status will be 7. If not
able to locate any machines, or an error is encountered,
*condor_findhost* will return the value 1.

Examples
--------

To find and list four machines, preferring those with the highest mips
(on Drystone benchmark) rating:

.. code-block:: console

    $ condor_findhost -n 4 -r "mips"

To find and list 24 machines, considering only those where the
``kflops`` attribute is not defined:

.. code-block:: console

    $ condor_findhost -n 24 -c "kflops=?=undefined"

