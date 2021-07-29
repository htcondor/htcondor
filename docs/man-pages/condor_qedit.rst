      

*condor_qedit*
===============

modify job attributes
:index:`condor_qedit<single: condor_qedit; HTCondor commands>`\ :index:`condor_qedit command`

Synopsis
--------

**condor_qedit** [**-debug** ] [**-n** *schedd-name*]
[**-pool** *pool-name*] *{cluster | cluster.proc | owner |
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

-  ``Owner``
-  ``ClusterId``
-  ``ProcId``
-  ``MyType``
-  ``TargetType``
-  ``JobStatus``

Since ``JobStatus`` may not be changed with *condor_qedit*, use
*condor_hold* to place a job in the hold state, and use
*condor_release* to release a held job, instead of attempting to modify
``JobStatus`` directly.

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

 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable ``TOOL_DEBUG``.
 **-n** *schedd-name*
    Modify job attributes in the queue of the specified schedd
 **-pool** *pool-name*
    Modify job attributes in the queue of the schedd specified in the
    specified pool

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

*condor_qedit* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

