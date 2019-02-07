      

condor\_qedit
=============

modify job attributes

Synopsis
--------

**condor\_qedit** [**-debug**\ ] [**-n  **\ *schedd-name*]
[**-pool  **\ *pool-name*] *{cluster \| cluster.proc \| owner \|
-constraint constraint}* *attribute-name* *attribute-value* *…*

Description
-----------

*condor\_qedit* modifies job ClassAd attributes of queued HTCondor jobs.
The jobs are specified either by cluster number, job ID, owner, or by a
ClassAd constraint expression. The *attribute-value* may be any ClassAd
expression. String expressions must be surrounded by double quotes.
Multiple attribute value pairs may be listed on the same command line.

To ensure security and correctness, *condor\_qedit* will not allow
modification of the following ClassAd attributes:

-  ``Owner``
-  ``ClusterId``
-  ``ProcId``
-  ``MyType``
-  ``TargetType``
-  ``JobStatus``

Since ``JobStatus`` may not be changed with *condor\_qedit*, use
*condor\_hold* to place a job in the hold state, and use
*condor\_release* to release a held job, instead of attempting to modify
``JobStatus`` directly.

If a job is currently running, modified attributes for that job will not
affect the job until it restarts. As an example, for ``PeriodicRemove``
to affect when a currently running job will be removed from the queue,
that job must first be evicted from a machine and returned to the queue.
The same is true for other periodic expressions, such as
``PeriodicHold`` and ``PeriodicRelease``.

*condor\_qedit* validates both attribute names and attribute values,
checking for correct ClassAd syntax. An error message is printed, and no
attribute is set or changed if the name or value is invalid.

Options
-------

 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable ``TOOL_DEBUG``.
 **-n **\ *schedd-name*
    Modify job attributes in the queue of the specified schedd
 **-pool **\ *pool-name*
    Modify job attributes in the queue of the schedd specified in the
    specified pool

Examples
--------

::

    % condor_qedit -name north.cs.wisc.edu -pool condor.cs.wisc.edu 249.0 answer 42 
    Set attribute "answer". 
    % condor_qedit -name perdita 1849.0 In '"myinput"' 
    Set attribute "In". 
    % condor_qedit jbasney NiceUser TRUE 
    Set attribute "NiceUser". 
    % condor_qedit -constraint 'JobUniverse == 1' Requirements '(Arch == "INTEL") && (OpSys == "SOLARIS26") && (Disk >= ExecutableSize) && (VirtualMemory >= ImageSize)' 
    Set attribute "Requirements".

General Remarks
---------------

A job’s ClassAd attributes may be viewed with

::

      condor_q -long

Exit Status
-----------

*condor\_qedit* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
