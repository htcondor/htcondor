Advanced Schedd Interaction
===========================

The introductory tutorial only scratches the surface of what the Python
bindings can do with the ``condor_schedd``; this module focuses on
covering a wider range of functionality:

-  Job and history querying.
-  Advanced job submission.
-  Python-based negotiation with the Schedd.

As usual, let's start by doing our usual imports:

.. code:: ipython3

    import htcondor
    import classad

Job and History Querying
------------------------

In `HTCondor
Introduction <../introductory/HTCondor-Introduction.ipynb>`__, we
covered the ``Schedd.xquery`` method and its two most important
keywords:

-  ``requirements``: Filters the jobs the schedd should return.
-  ``projection``: Filters the attributes returned for each job.

For those familiar with SQL queries, ``requirements`` performs the
equivalent as the ``WHERE`` clause while ``projection`` performs the
equivalent of the column listing in ``SELECT``.

There are two other keywords worth mentioning:

-  ``limit``: Limits the number of returned ads; equivalent to SQL's
   ``LIMIT``.
-  ``opts``: Additional flags to send to the schedd to alter query
   behavior. The only flag currently defined is
   ``QueryOpts.AutoCluster``; this groups the returned results by the
   current set of "auto-cluster" attributes used by the pool. It's
   analogous to ``GROUP BY`` in SQL, except the columns used for
   grouping are controlled by the schedd.

To illustrate these additional keywords, let's first submit a few jobs:

.. code:: ipython3

    schedd = htcondor.Schedd()
    sub = htcondor.Submit({
                           "executable": "/bin/sleep",
                           "arguments":  "5m",
                           "hold":       "True",
                          })
    with schedd.transaction() as txn:
        clusterId = sub.queue(txn, 10)
    print(clusterId)


.. parsed-literal::

    11


**Note:** In this example, we used the ``hold`` submit command to
indicate that the jobs should start out in the ``condor_schedd`` in the
*Hold* state; this is used simply to prevent the jobs from running to
completion while you are running the tutorial.

We now have 10 jobs running under ``clusterId``; they should all be
identical:

.. code:: ipython3

    print(sum(1 for _ in schedd.xquery(projection=["ProcID"], requirements="ClusterId==%d" % clusterId, limit=5)))


.. parsed-literal::

    5


The ``sum(1 for _ in ...)`` syntax is a simple way to count the number
of items produced by an iterator without buffering all the objects in
memory.

Querying many Schedds
---------------------

On larger pools, it's common to write Python scripts that interact with
not one but many schedds. For example, if you want to implement a
"global query" (equivalent to ``condor_q -g``; concatenates all jobs in
all schedds), it might be tempting to write code like this:

.. code:: ipython3

    jobs = []
    for schedd_ad in htcondor.Collector().locateAll(htcondor.DaemonTypes.Schedd):
        schedd = htcondor.Schedd(schedd_ad)
        jobs += schedd.xquery()
    print(len(jobs))


.. parsed-literal::

    58


This is sub-optimal for two reasons:

-  ``xquery`` is not given any projection, meaning it will pull all
   attributes for all jobs - much more data than is needed for simply
   counting jobs.
-  The querying across all schedds is serialized: we may wait for
   painfully long on one or two "bad apples."

We can instead begin the query for all schedds simultaneously, then read
the responses as they are sent back. First, we start all the queries
without reading responses:

.. code:: ipython3

    queries = []
    coll_query = htcondor.Collector().locateAll(htcondor.DaemonTypes.Schedd)
    for schedd_ad in coll_query:
        schedd_obj = htcondor.Schedd(schedd_ad)
        queries.append(schedd_obj.xquery())

The iterators will yield the matching jobs; to return the autoclusters
instead of jobs, use the ``AutoCluster`` option
(``schedd_obj.xquery(opts=htcondor.QueryOpts.AutoCluster)``). One
auto-cluster ad is returned for each set of jobs that have identical
values for all significant attributes. A sample auto-cluster looks like:

::

       [
        RequestDisk = DiskUsage;
        Rank = 0.0;
        FileSystemDomain = "hcc-briantest7.unl.edu";
        MemoryUsage = ( ( ResidentSetSize + 1023 ) / 1024 );
        ImageSize = 1000;
        JobUniverse = 5;
        DiskUsage = 1000;
        JobCount = 1;
        Requirements = ( TARGET.Arch == "X86_64" ) && ( TARGET.OpSys == "LINUX" ) && ( TARGET.Disk >= RequestDisk ) && ( TARGET.Memory >= RequestMemory ) && ( ( TARGET.HasFileTransfer ) || ( TARGET.FileSystemDomain == MY.FileSystemDomain ) );
        RequestMemory = ifthenelse(MemoryUsage isnt undefined,MemoryUsage,( ImageSize + 1023 ) / 1024);
        ResidentSetSize = 0;
        ServerTime = 1483758177;
        AutoClusterId = 2
       ]

We use the ``poll`` function, which will return when a query has
available results:

.. code:: ipython3

    job_counts = {}
    for query in htcondor.poll(queries):
        schedd_name = query.tag()
        job_counts.setdefault(schedd_name, 0)
        count = len(query.nextAdsNonBlocking())
        job_counts[schedd_name] += count
        print("Got {} results from {}.".format(count, schedd_name))
    print(job_counts)


.. parsed-literal::

    Got 58 results from jovyan@c096c3d74375.
    {'jovyan@c096c3d74375': 58}


The ``QueryIterator.tag`` method is used to identify which query is
returned; the tag defaults to the Schedd's name but can be manually set
through the ``tag`` keyword argument to ``Schedd.xquery``.

History Queries
---------------

After a job has finished in the Schedd, it moves from the queue to the
history file. The history can be queried (locally or remotely) with the
``Schedd.history`` method:

.. code:: ipython3

    schedd = htcondor.Schedd()
    for ad in schedd.history('true', ['ProcId', 'ClusterId', 'JobStatus', 'WallDuration'], 2):
        print(ad)


.. parsed-literal::

    
        [
            JobStatus = 4; 
            ProcId = 7; 
            ClusterId = 9
        ]
    
        [
            JobStatus = 4; 
            ProcId = 6; 
            ClusterId = 9
        ]


At the time of writing, unlike ``Schedd.xquery``, ``Schedd.history``
takes positional arguments and not keyword. The first argument a job
constraint; second is the projection list; the third is the maximum
number of jobs to return.

Advanced Job Submission
-----------------------

In `HTCondor
Introduction <../introductory/HTCondor-Introduction.ipynb>`__, we
introduced the ``Submit`` object. ``Submit`` allows jobs to be created
using the *submit file* language. This is the well-documented, familiar
means for submitting jobs via ``condor_submit``. This is the preferred
mechansim for submitting jobs from Python.

Internally, the submit files are converted to a job ClassAd. The older
``Schedd.submit`` method allows jobs to be submitted as ClassAds. For
example:

.. code:: ipython3

    import os
    
    schedd = htcondor.Schedd()
    job_ad = classad.ClassAd({
         'Cmd': '/bin/sh',
         'JobUniverse': 5,
         'Iwd': os.path.abspath("/tmp"),
         'Out': 'testclaim.out',
         'Err': 'testclaim.err',
         'Arguments': 'sleep 5m',
    })
    clusterId = schedd.submit(job_ad, count=2)
    print(clusterId)


.. parsed-literal::

    12


This will submit two copies of the job described by ``job_ad`` into a
single job cluster.

**Hint**: To generate an example ClassAd, take a sample submit
description file and invoke:

::

      condor_submit -dump <filename> [cmdfile]

Then, load the resulting contents of ``<filename>`` into Python.

Calling ``Schedd.submit`` standalone will automatically create and
commit a transaction. Multiple jobs can be submitted atomically and more
efficiently within a ``Schedd.transaction()`` context.

Each ``Schedd.submit`` invocation will create a new job cluster; all
attributes will be identical except for the ``ProcId`` attribute
(process IDs are assigned in monotonically increasing order, starting at
zero). If jobs in the same cluster need to differ on additional
attributes, one may use the ``Schedd.submitMany`` method:

.. code:: ipython3

    foo = classad.ClassAd({'myAttr': 'foo'})
    bar = classad.ClassAd({'myAttr': 'bar'})
    clusterId = schedd.submitMany(job_ad, [(foo, 2), (bar, 2)])
    print(clusterId)


.. parsed-literal::

    13


.. code:: ipython3

    query = schedd.xquery('ClusterId=={}'.format(clusterId), ['ProcId', 'myAttr'])
    for ad in query:
        print(ad)


.. parsed-literal::

    
        [
            ServerTime = 1574110214; 
            ProcId = 0; 
            myAttr = "foo"
        ]
    
        [
            ServerTime = 1574110214; 
            ProcId = 1; 
            myAttr = "foo"
        ]
    
        [
            ServerTime = 1574110214; 
            ProcId = 2; 
            myAttr = "bar"
        ]
    
        [
            ServerTime = 1574110214; 
            ProcId = 3; 
            myAttr = "bar"
        ]


``Schedd.submitMany`` takes a basic job ad (sometimes referred to as the
*cluster ad*), shared by all jobs in the cluster and a list of *process
ads*. The process ad list indicates the attributes that should be
overridden for individual jobs, as well as the number of such jobs that
should be submitted.

Job Spooling
------------

HTCondor file transfer will move output and input files to and from the
submit host; these files will move back to the original location on the
host. In some cases, this may be problematic; you may want to submit one
set of jobs to run ``/home/jovyan/a.out``, recompile the binary, then
submit a fresh set of jobs. By using the *spooling* feature, the
``condor_schedd`` will make a private copy of ``a.out`` after submit,
allowing the user to make new edits.

**Hint**: Although here we give an example of using ``Schedd.spool`` for
spooling on the local Schedd, with appropriate authoriation the same
methods can be used for submitting to remote hosts.

To spool, one must specify this at submit time and invoke the
``Schedd.spool`` method and provide an ``ad_results`` array:

.. code:: ipython3

    ads = []
    cluster = schedd.submit(job_ad, 1, spool=True, ad_results=ads)
    schedd.spool(ads)
    print(ads)


.. parsed-literal::

    [[ ClusterId = 14; LocalSysCpu = 0.0; RemoteSysCpu = 0.0; OnExitRemove = true; PeriodicRemove = false; LocalUserCpu = 0.0; RemoteUserCpu = 0.0; StreamErr = false; HoldReason = "Spooling input data files"; CommittedSuspensionTime = 0; ProcId = 0; PeriodicHold = false; CondorPlatform = "$CondorPlatform: X86_64-CentOS_5.11 $"; ExitStatus = 0; ShouldTransferFiles = "YES"; LastSuspensionTime = 0; NumJobStarts = 0; NumCkpts = 0; WhenToTransferOutput = "ON_EXIT"; TargetType = "Machine"; JobNotification = 0; BufferSize = 524288; ImageSize = 100; PeriodicRelease = false; CompletionDate = 0; RemoteWallClockTime = 0.0; Arguments = "sleep 5m"; WantCheckpoint = false; NumSystemHolds = 0; CumulativeSuspensionTime = 0; QDate = 1574110214; EnteredCurrentStatus = 1574110214; CondorVersion = "$CondorVersion: 8.9.3 Sep 17 2019 BuildID: UW_Python_Wheel_Build $"; MyType = "Job"; Owner = undefined; ExitBySignal = false; JobUniverse = 5; BufferBlockSize = 32768; Err = "testclaim.err"; NiceUser = false; CoreSize = -1; CumulativeSlotTime = 0; OnExitHold = false; WantRemoteSyscalls = false; CommittedTime = 0; Cmd = "/bin/sh"; WantRemoteIO = true; StreamOut = false; CommittedSlotTime = 0; TotalSuspensions = 0; JobPrio = 0; CurrentHosts = 0; RootDir = "/"; Out = "testclaim.out"; LeaveJobInQueue = JobStatus == 4 && (CompletionDate is UNDDEFINED || CompletionDate == 0 || ((time() - CompletionDate) < 864000)); RequestCpus = 1; RequestDisk = DiskUsage; MinHosts = 1; Requirements = true && TARGET.OPSYS == "LINUX" && TARGET.ARCH == "X86_64" && TARGET.HasFileTransfer && TARGET.Disk >= RequestDisk && TARGET.Memory >= RequestMemory && TARGET.Cpus >= RequestCpus; RequestMemory = ifthenelse(MemoryUsage isnt undefined,MemoryUsage,(ImageSize + 1023) / 1024); Args = ""; MaxHosts = 1; JobStatus = 5; DiskUsage = 1; In = "/dev/null"; HoldReasonCode = 16; Iwd = "/tmp"; NumJobCompletions = 0; NumRestarts = 0 ]]


This will copy the files into the Schedd's ``spool`` directory. After
the job completes, its output files will stay in the spool. One needs to
call ``Schedd.retrieve`` to move the outputs back to their final
destination:

.. code:: ipython3

    schedd.retrieve("ClusterId=={}".format(cluster))

Negotiation with the Schedd
---------------------------

The ``condor_negotiator`` daemon gathers job and machine ClassAds, tries
to match machines to available jobs, and sends these matches to the
``condor_schedd``.

In truth, the "match" is internally a *claim* on the resource; the
Schedd is allowed to execute one or more job on it.

The Python bindings can also send claims to the Schedds. First, we must
prepare the claim objects by taking the slot's public ClassAd and adding
a ``ClaimId`` attribute:

.. code:: ipython3

    coll = htcondor.Collector()
    private_ads = coll.query(htcondor.AdTypes.StartdPrivate)
    startd_ads = coll.query(htcondor.AdTypes.Startd)
    claim_ads = []
    for ad in startd_ads:
        if "Name" not in ad: continue
        found_private = False
        for pvt_ad in private_ads:
            if pvt_ad.get('Name') == ad['Name']:
                found_private = True
                ad['ClaimId'] = pvt_ad['Capability']
                claim_ads.append(ad)

Once the claims are prepared, we can send them to the schedd. Here's an
example of sending the claim to user ``jovyan@example.com``, for any
matching ad:

.. code:: ipython3

    with htcondor.Schedd().negotiate("bbockelm@unl.edu") as session:
        found_claim = False
        for resource_request in session:
            for claim_ad in claim_ads:
                if resource_request.symmetricMatch(claim_ad):
                    print("Sending claim for", claim_ad["Name"])
                    session.sendClaim(claim_ads[0])
                    found_claim = True
                    break
            if found_claim: break

This is far cry from what the ``condor_negotiator`` actually does (the
negotiator additionally enforces fairshare, for example).

**Note**: The Python bindings can send claims to the schedd immediately,
even without reading the resource request from the schedd. The schedd
will only utilize the claim if there's a matching job, however.
