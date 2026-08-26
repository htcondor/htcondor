
*condor_qedit*
===============

modify job attributes

Synopsis
--------

**condor_qedit** [**-debug** ] [**-n** *schedd-name*]
[**-pool** *pool-name*] [**-forward** ] *{cluster | cluster.proc | owner |
-constraint constraint}* *edit-list*

Description
-----------

*condor_qedit* modifies job ClassAd attributes of queued HTCondor jobs.
The jobs are specified either by cluster number, job ID, owner, or by a
ClassAd constraint expression. The *edit-list* can take one of 3 forms

-  *attribute-name* *attribute-value* *...*
    This is the older form, which behaves the same as the format below.

-  *attribute-name=attribute-value* *...*
    The *attribute-value* may be any ClassAd
    expression. String expressions must be surrounded by double quotes.
    Multiple attribute value pairs may be listed on the same command line.

-  **-edits[:auto|long|xml|json|new]** *file-name*
    The file indicated by *file-name* is read as a classad of the given format.
    If no format is specified or ``auto`` is specified the format will be detected.
    if *file-name* is ``-`` standard input will be read.

To ensure security and correctness, *condor_qedit* will not allow
modification of the following ClassAd attributes:

-  :ad-attr:`Owner`
-  :ad-attr:`ClusterId`
-  :ad-attr:`ProcId`
-  :ad-attr:`MyType`
-  :ad-attr:`TargetType`
-  :ad-attr:`JobStatus`

Since :ad-attr:`JobStatus` may not be changed with *condor_qedit*, use
*condor_hold* to place a job in the hold state, and use
*condor_release* to release a held job, instead of attempting to modify
:ad-attr:`JobStatus` directly.

If a job is currently running, modified attributes for that job will not
affect the job until it restarts. As an example, for ``PeriodicRemove``
to affect when a currently running job will be removed from the queue,
that job must first be evicted from a machine and returned to the queue.
The same is true for other periodic expressions, such as
``PeriodicHold`` and ``PeriodicRelease``.

*condor_qedit* validates both attribute names and attribute values,
checking for correct ClassAd syntax. An error message is printed, and no
attribute is set or changed if any name or value is invalid.

Options
-------

 **-help**
    Display usage information and exit.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-dry-run**
    Do not modify any job attributes; instead, report whether the
    requested edit would be permitted. Useful for checking which
    attributes are protected or otherwise not editable before performing
    the actual update. Requires a *condor_schedd* that supports the
    **-dry-run** query.
 **-n** *schedd-name*
    Modify job attributes in the queue of the specified schedd
 **-pool** *pool-name*
    Modify job attributes in the queue of the schedd specified in the
    specified pool
 **-forward**
    Forward modifications to shadow/gridmanager
 **-constraint** *expression*
    Restrict the edit to jobs for which the ClassAd *expression*
    evaluates to *true*. May be combined with **-owner** and
    **-jobids**; only jobs that match all of the supplied restrictions
    will be edited. If only **-constraint** is given, jobs not owned by
    the current user are skipped unless the user is a queue
    superuser.
 **-owner** *owner*
    Restrict the edit to jobs owned by *owner*. May be combined with
    **-jobids** and **-constraint**.
 **-jobids** *id-list*
    Restrict the edit to the comma-separated list of job IDs in
    *id-list*. Each ID may be a *cluster* or a *cluster.proc*. May be
    combined with **-owner** and **-constraint**.
 **-edits[:auto|long|xml|json|new]** *file*
    Read the *attribute = value* edit list from *file*, in ClassAd
    format. The optional format qualifier selects the input format; the
    default ``auto`` detects the format from the input. If *file* is
    ``-``, edits are read from standard input.

Examples
--------

.. code-block:: console

    $ condor_qedit -name north.cs.wisc.edu -pool condor.cs.wisc.edu 249.0 answer 42 
    Set attribute "answer". 
    $ condor_qedit -name perdita 1849.0 In '"myinput"' 
    Set attribute "In". 
    % condor_qedit jbasney OnExitRemove=FALSE
    Set attribute "OnExitRemove".
    % condor_qedit -constraint 'JobUniverse == 1' 'Requirements=(Arch == "INTEL") && (OpSys == "SOLARIS26") && (Disk >= ExecutableSize) && (VirtualMemory >= ImageSize)'
    Set attribute "Requirements".

General Remarks
---------------

A job's ClassAd attributes may be viewed with

.. code-block:: console

      $ condor_q -long

Exit Status
-----------

0  -  Success

1  -  Failure

See Also
--------

:tool:`condor_q`, :tool:`condor_submit`, :tool:`condor_prio`, :tool:`condor_hold`, :tool:`condor_release`

Availability
------------

Linux, MacOS, Windows
