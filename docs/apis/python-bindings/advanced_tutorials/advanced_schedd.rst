
Advanced Schedd Interaction
===========================

The introductory tutorial only scratches the surface of what the Python bindings
can do with the ``condor_schedd``; this module focuses on covering a wider range
of functionality:

*  Job and history querying.
*  Advanced job submission.
*  Python-based negotiation with the Schedd.

Job and History Querying
------------------------

In :doc:`../intro_tutorials/htcondor`, we covered the :meth:`~htcondor.Schedd.xquery` method
and its two most important keywords:

*  ``requirements``: Filters the jobs the schedd should return.
*  ``projection``: Filters the attributes returned for each job.

For those familiar with SQL queries, ``requirements`` performs the equivalent
as the ``WHERE`` clause while ``projection`` performs the equivalent of the column
listing in ``SELECT``.

There are two other keywords worth mentioning:

*  ``limit``: Limits the number of returned ads; equivalent to SQL's ``LIMIT``.
*  ``opts``: Additional flags to send to the schedd to alter query behavior.
   The only flag currently defined is :attr:`~QueryOpts.AutoCluster`; this
   groups the returned results by the current set of "auto-cluster" attributes
   used by the pool.  It's analogous to ``GROUP BY`` in SQL, except the columns
   used for grouping are controlled by the schedd.

To illustrate these additional keywords, let's first submit a few jobs::

   >>> schedd = htcondor.Schedd()
   >>> sub = htcondor.Submit({
   ...                        "executable": "/bin/sleep",
   ...                        "arguments":  "5m",
   ...                        "hold":       "True",
   ...                       })
   >>> with schedd.transaction() as txn:
   ...     clusterId = sub.queue(txn, 10)

.. note:: In this example, we used the ``hold`` submit command to indicate that
   the jobs should start out in the ``condor_schedd`` in the *Hold* state; this
   is used simply to prevent the jobs from running to completion while you are
   running the tutorial.

We now have 10 jobs running under ``clusterId``; they should all be identical::

   >>> print sum(1 for _ in schedd.xquery(projection=["ProcID"], requirements="ClusterId==%d" % clusterId, limit=5))
   5
   >>> print list(schedd.xquery(projection=["ProcID"], requirements="ClusterId==%d" % clusterId, opts=htcondor.QueryOpts.AutoCluster))

The ``sum(1 for _ in ...)`` syntax is a simple way to count the number of items
produced by an iterator without buffering all the objects in memory.

Querying many Schedds
^^^^^^^^^^^^^^^^^^^^^

On larger pools, it's common to write Python scripts that interact with not one but many schedds.  For example,
if you want to implement a "global query" (equivalent to ``condor_q -g``; concatenates all jobs in all schedds),
it might be tempting to write code like this::

   >>> jobs = []
   >>> for schedd_ad in htcondor.Collector().locateAll(htcondor.DaemonTypes.Schedd):
   ...     schedd = htcondor.Schedd(schedd_ad)
   ...     jobs += schedd.xquery()
   >>> print len(jobs)

This is sub-optimal for two reasons:

*  ``xquery`` is not given any projection, meaning it will pull all attributes for all jobs -
   much more data than is needed for simply counting jobs.
*  The querying across all schedds is serialized: we may wait for painfully long on one or two
   "bad apples"

We can instead begin the query for all schedds simultaneously, then read the responses as
they are sent back.  First, we start all the queries without reading responses::

   >>> queries = []
   >>> coll_query = htcondor.Collector().locate(htcondor.AdTypes.Schedd)
   >>> for schedd_ad in coll_query:
   ...     schedd_obj = htcondor.Schedd(schedd_ad)
   ...     queries.append(schedd_obj.xquery())

The iterators will yield the matching jobs; to return the autoclusters instead of jobs, use
the ``AutoCluster`` option (``schedd_obj.xquery(opts=htcondor.QueryOpts.AutoCluster)``).  One
auto-cluster ad is returned for each set of jobs that have identical values for all significant
attributes.  A sample auto-cluster looks like::

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

We use the :func:`poll` function, which will return when a query has available results::

   >>> job_counts = {}
   >>> for query in htcondor.poll(queries):
   ...    schedd_name = query.tag()
   ...    job_counts.setdefault(schedd_name, 0)
   ...    count = len(query.nextAdsNonBlocking())
   ...    job_counts[schedd_name] += count
   ...    print "Got %d results from %s." % (count, schedd_name)
   >>> print job_counts

The :meth:`~htcondor.QueryIterator.tag` method is used to identify which query is returned; the
tag defaults to the Schedd's name but can be manually set through the ``tag`` keyword argument
to :meth:`~htcondor.Schedd.xquery`.

History Queries
^^^^^^^^^^^^^^^

After a job has finished in the Schedd, it moves from the queue to the history file.  The
history can be queried (locally or remotely) with the :meth:`~htcondor.Schedd.history` method::

   >>> schedd = htcondor.Schedd()
   >>> for ad in schedd.history('true', ['ProcId', 'ClusterId', 'JobStatus', 'WallDuration'], 2):
   ...     print ad

At the time of writing, unlike :meth:`~htcondor.Schedd.xquery`, :meth:`~htcondor.Schedd.history`
takes positional arguments and not keyword.  The first argument a job constraint; second is the
projection list; the third is the maximum number of jobs to return.

Advanced Job Submission
-----------------------

In :doc:`../intro_tutorials/htcondor`, we introduced the :class:`~htcondor.Submit` object.  :class:`~htcondor.Submit`
allows jobs to be created using the *submit file* language.  This is the well-documented, familiar
means for submitting jobs via ``condor_submit``.  This is the preferred mechansim for submitting
jobs from Python.

Internally, the submit files are converted to a job ClassAd.  The older :meth:`~htcondor.Schedd.submit`
method allows jobs to be submitted as ClassAds.  For example::

   >>> import os.path
   >>> schedd = htcondor.Schedd()
   >>> job_ad = { \
   ...      'Cmd': '/bin/sh',
   ...      'JobUniverse': 5,
   ...      'Iwd': os.path.abspath("/tmp"),
   ...      'Out': 'testclaim.out',
   ...      'Err': 'testclaim.err',
   ...      'Arguments': 'sleep 5m',
   ...  }
   >>> clusterId = schedd.submit(job_ad, count=2)

This will submit two copies of the job described by ``job_ad`` into a single job cluster.

.. hint:: To generate an example ClassAd, take a sample submit description
   file and invoke::

      condor_submit -dump <filename> [cmdfile]

   Then, load the resulting contents of ``<filename>`` into Python.

Calling :meth:`~htcondor.Schedd.submit` standalone will automatically create and commit a transaction.
Multiple jobs can be submitted atomically and more efficiently within a :meth:`~htcondor.Schedd.transaction()`
context.

Each :meth:`~htcondor.Schedd.submit` invocation will create a new job cluster; all attributes will be
identical except for the ``ProcId`` attribute (process IDs are assigned in monotonically increasing order,
starting at zero).  If jobs in the same cluster need to differ on additional attributes, one may use the
:meth:`~htcondor.Schedd.submitMany` method::

   >>> foo = {'myAttr': 'foo'}
   >>> bar = {'myAttr': 'bar'}
   >>> clusterId = schedd.submitMany(job_ad, [(foo, 2), (bar, 2)])
   >>> print list(schedd.xquery('ClusterId==%d' % clusterId, ['ProcId', 'myAttr']))

:meth:`~htcondor.Schedd.submitMany` takes a basic job ad (sometimes referred to as the *cluster ad*),
shared by all jobs in the cluster and a list of *process ads*.  The process ad list indicates
the attributes that should be overridden for individual jobs, as well as the number of such jobs
that should be submitted.

Job Spooling
^^^^^^^^^^^^

HTCondor file transfer will move output and input files to and from the submit host; these files will
move back to the original location on the host.  In some cases, this may be problematic; you may want
to submit one set of jobs to run ``/home/jovyan/a.out``, recompile the binary, then submit a fresh
set of jobs.  By using the *spooling* feature, the ``condor_schedd`` will make a private copy of
``a.out`` after submit, allowing the user to make new edits.

.. hint:: Although here we give an example of using :meth:`~htcondor.Schedd.spool` for spooling on
   the local Schedd, with appropriate authoriation the same methods can be used for submitting to
   remote hosts.

To spool, one must specify this at submit time and invoke the :meth:`~htcondor.Schedd.spool` method
and provide an ``ad_results`` array::

   >>> ads = []
   >>> cluster = schedd.submit(job_ad, 1, spool=True, ad_results=ads)
   >>> schedd.spool(ads)

This will copy the files into the Schedd's ``spool`` directory.  After the job completes, its
output files will stay in the spool.  One needs to call :meth:`~htcondor.Schedd.retrieve` to
move the outputs back to their final destination::

   >>> schedd.retrieve("ClusterId == %d" % cluster)

Negotiation with the Schedd
---------------------------

The ``condor_negotiator`` daemon gathers job and machine ClassAds, tries to match machines
to available jobs, and sends these matches to the ``condor_schedd``.

In truth, the "match" is internally a *claim* on the resource; the Schedd is allowed to
execute one or more job on it.

The Python bindings can also send claims to the Schedds.  First, we must prepare the
claim objects by taking the slot's public ClassAd and adding a ``ClaimId`` attribute::

   >>> coll = htcondor.Collector()
   >>> private_ads = coll.query(htcondor.AdTypes.StartdPrivate)
   >>> startd_ads = coll.query(htcondor.AdTypes.Startd)
   >>> claim_ads = []
   >>> for ad in startd_ads:
   ...     if "Name" not in ad: continue
   ...     found_private = False
   ...     for pvt_ad in private_ads:
   ...         if pvt_ad.get('Name') == ad['Name']:
   ...             found_private = True
   ...             ad['ClaimId'] = pvt_ad['Capability']
   ...            claim_ads.append(ad)

Once the claims are prepared, we can send them to the schedd.  Here's an example of
sending the claim to user ``jovyan@example.com``, for any matching ad::

   >>> with htcondor.Schedd().negotiate("bbockelm@unl.edu") as session:
   >>>     found_claim = False
   >>>     for resource_request in session:
   >>>         for claim_ad in claim_ads:
   >>>             if resource_request.symmetricMatch(claim_ad):
   ...                 print "Sending claim for", claim_ad["Name"]
   ...                 session.sendClaim(claim_ads[0])
   ...                 found_claim = True
   ...                 break
   ...         if found_claim: break

This is far cry from what the ``condor_negotiator`` actually does (the negotiator
additionally enforces fairshare, for example).

.. note:: The Python bindings can send claims to the schedd immediately, even without
   reading the resource request from the schedd.  The schedd will only utilize the
   claim if there's a matching job, however.

