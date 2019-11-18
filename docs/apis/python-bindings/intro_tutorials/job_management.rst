Submitting and Managing Jobs
============================

The two most common HTCondor command line tools are ``condor_q`` and
``condor_submit``; in the previous module, we learning the ``xquery()``
method that corresponds to ``condor_q``. Here, we will learn the Python
binding equivalent of ``condor_submit``.

As usual, we start by importing the relevant modules:

.. code:: ipython3

    import htcondor

Submitting Jobs
---------------

We will submit jobs utilizing the dedicated ``Submit`` object.

**Note** the Submit object was introduced in 8.5.6, which might be newer
than your home cluster. The original API, using the ``Schedd.submit``
method, utilizes raw ClassAds and is not covered here.

``Submit`` objects consist of key-value pairs. Unlike ClassAds, the
values do not have an inherent type (such as strings, integers, or
booleans); they are evaluated with macro expansion at submit time. Where
reasonable, they behave like Python dictionaries:

.. code:: ipython3

    sub = htcondor.Submit({"foo": "1", "bar": "2", "baz": "$(foo)"})
    sub.setdefault("qux", "3")
    print("=== START ===\n{}\n=== END ===".format(sub))
    print(sub.expand("baz"))


.. parsed-literal::

    === START ===
    foo = 1
    bar = 2
    baz = $(foo)
    qux = 3
    
    === END ===
    1


The available attribuets - and their semantics - are relatively well
documented in the ``condor_submit`` `online
help <http://research.cs.wisc.edu/htcondor/manual/v8.5/condor_submit.html>`__;
we won’t repeat them here. A minimal, but realistic submit object may
look like the following:

.. code:: ipython3

    sub = htcondor.Submit({"executable": "/bin/sleep", "arguments": "5m"})

To go from a submit object to job in a schedd, one must do three things:

1. Create a new transaction in the schedd using ``transaction()``.
2. Call the ``queue()`` method, passing the transaction object.
3. Commit the transaction.

Since the transaction object is a Python context, (1) and (3) can be
achieved using Python's with statement:

.. code:: ipython3

    schedd = htcondor.Schedd()         # Create a schedd object using default settings.
    with schedd.transaction() as txn:  # txn will now represent the transaction.
       print(sub.queue(txn))            # Queues one job in the current transaction; returns job's cluster ID


.. parsed-literal::

    6


If the code block inside the ``with`` statement completes successfully,
the transaction is automatically committed. If an exception is thrown
(or Python abruptly exits), the transaction is aborted.

By default, each invocation of ``queue`` will submit a single job. A
more common use case is to submit many jobs at once - often identical.
Suppose we don't want to submit a single "sleep" job, but 10; instead of
writing a ``for``-loop around the ``queue`` method, we can use the
``count`` argument:

.. code:: ipython3

    schedd = htcondor.Schedd()                # Create a fresh Schedd object, pointint at the current schedd.
    with schedd.transaction() as txn:         # Start a new transaction
        cluster_id = sub.queue(txn, count=10) # Submit 10 identical jobs
    print(cluster_id)


.. parsed-literal::

    7


We can now query for all the jobs we have in the queue:

.. code:: ipython3

    schedd.query(constraint='ClusterId=?={}'.format(cluster_id),
                 attr_list=["ClusterId", "ProcId", "JobStatus", "EnteredCurrentStatus"])




.. parsed-literal::

    [[ ClusterId = 7; ProcId = 0; EnteredCurrentStatus = 1574110191; JobStatus = 1; ServerTime = 1574110191 ],
     [ ClusterId = 7; ProcId = 1; EnteredCurrentStatus = 1574110191; JobStatus = 1; ServerTime = 1574110191 ],
     [ ClusterId = 7; ProcId = 2; EnteredCurrentStatus = 1574110191; JobStatus = 1; ServerTime = 1574110191 ],
     [ ClusterId = 7; ProcId = 3; EnteredCurrentStatus = 1574110191; JobStatus = 1; ServerTime = 1574110191 ],
     [ ClusterId = 7; ProcId = 4; EnteredCurrentStatus = 1574110191; JobStatus = 1; ServerTime = 1574110191 ],
     [ ClusterId = 7; ProcId = 5; EnteredCurrentStatus = 1574110191; JobStatus = 1; ServerTime = 1574110191 ],
     [ ClusterId = 7; ProcId = 6; EnteredCurrentStatus = 1574110191; JobStatus = 1; ServerTime = 1574110191 ],
     [ ClusterId = 7; ProcId = 7; EnteredCurrentStatus = 1574110191; JobStatus = 1; ServerTime = 1574110191 ],
     [ ClusterId = 7; ProcId = 8; EnteredCurrentStatus = 1574110191; JobStatus = 1; ServerTime = 1574110191 ],
     [ ClusterId = 7; ProcId = 9; EnteredCurrentStatus = 1574110191; JobStatus = 1; ServerTime = 1574110191 ]]



It's not entirely useful to submit many identical jobs -- but rather
each one needs to vary slightly based on its ID (the "process ID")
within the job cluster. For this, the ``Submit`` object in Python
behaves similarly to submit files: references within the submit command
are evaluated as macros at submit time.

For example, suppose we want the argument to ``sleep`` to vary based on
the process ID:

.. code:: ipython3

    sub = htcondor.Submit({"executable": "/bin/sleep", "arguments": "$(Process)m"})

Here, the ``$(Process)`` string will be substituted with the process ID
at submit time.

.. code:: ipython3

    with schedd.transaction() as txn:         # Start a new transaction
        cluster_id = sub.queue(txn, count=10) # Submit 10 identical jobs
    print(cluster_id)
    schedd.query(constraint='ClusterId=?={}'.format(cluster_id),
                 attr_list=["ClusterId", "ProcId", "JobStatus", "Args"])


.. parsed-literal::

    8




.. parsed-literal::

    [[ Args = "0m"; ClusterId = 8; ProcId = 0; JobStatus = 1; ServerTime = 1574110191 ],
     [ Args = "1m"; ClusterId = 8; ProcId = 1; JobStatus = 1; ServerTime = 1574110191 ],
     [ Args = "2m"; ClusterId = 8; ProcId = 2; JobStatus = 1; ServerTime = 1574110191 ],
     [ Args = "3m"; ClusterId = 8; ProcId = 3; JobStatus = 1; ServerTime = 1574110191 ],
     [ Args = "4m"; ClusterId = 8; ProcId = 4; JobStatus = 1; ServerTime = 1574110191 ],
     [ Args = "5m"; ClusterId = 8; ProcId = 5; JobStatus = 1; ServerTime = 1574110191 ],
     [ Args = "6m"; ClusterId = 8; ProcId = 6; JobStatus = 1; ServerTime = 1574110191 ],
     [ Args = "7m"; ClusterId = 8; ProcId = 7; JobStatus = 1; ServerTime = 1574110191 ],
     [ Args = "8m"; ClusterId = 8; ProcId = 8; JobStatus = 1; ServerTime = 1574110191 ],
     [ Args = "9m"; ClusterId = 8; ProcId = 9; JobStatus = 1; ServerTime = 1574110191 ]]



The macro evaluation behavior (and the various usable tricks and
techniques) are identical between the python bindings and the
``condor_submit`` executable.

Submitting Jobs with Unique Inputs
----------------------------------

While it's useful to submit jobs which each differ by an integer, it is
sometimes difficult to make your jobs fit into this paradigm. A common
case is to process unique files in a directory. Let's start by creating
a directory with 10 input files:

.. code:: ipython3

    # generate 10 input files, each with unique content.
    import pathlib
    input_dir = pathlib.Path("input_directory")
    input_dir.mkdir(exist_ok=True)
    
    for idx in range(10):
        input_file = input_dir / "job_{}.txt".format(idx)
        input_file.write_text("Hello from job {}".format(idx))

Next, we want to create a python dictionary of all the filenames in the
``input_directory`` and pass the iterator to the
``queue_with_itemdata``.

.. code:: ipython3

    sub = htcondor.Submit({"executable": "/bin/cat"})
    sub["arguments"] = "$(filename)"
    sub["transfer_input_files"] = "input_directory/$(filename)"
    sub["output"] = "results.$(Process)"
    
    # filter to select only the the job files
    itemdata = [{"filename": path.name} for path in input_dir.iterdir() if 'job' in path.name]
    
    for item in itemdata:
        print(item)


.. parsed-literal::

    {'filename': 'job_7.txt'}
    {'filename': 'job_4.txt'}
    {'filename': 'job_2.txt'}
    {'filename': 'job_5.txt'}
    {'filename': 'job_0.txt'}
    {'filename': 'job_6.txt'}
    {'filename': 'job_8.txt'}
    {'filename': 'job_1.txt'}
    {'filename': 'job_9.txt'}
    {'filename': 'job_3.txt'}


.. code:: ipython3

    with schedd.transaction() as txn:
        # Submit one job per entry in the iterator.
        results = sub.queue_with_itemdata(txn, 1, iter(itemdata))
        
    print(results.cluster())


.. parsed-literal::

    9


*Warning*: As of the time of writing (HTCondor 8.9.2), this function
takes an *iterator* and not an *iterable*. Therefore, ``[1,2,3,4]`` is
not a valid third argument but ``iter([1,2,3,4])`` is; this restriction
is expected to be relaxed in the future.

Note that the results of the method is a ``SubmitResults`` object and
not a plain integer as before.

Next, we can make sure our arguments were applied correctly:

.. code:: ipython3

    schedd.query(constraint='ClusterId=?={}'.format(results.cluster()),
                 attr_list=["ClusterId", "ProcId", "JobStatus", "TransferInput", "Out", "Args"])




.. parsed-literal::

    [[ Out = "results.0"; JobStatus = 1; TransferInput = "input_directory/job_7.txt"; ServerTime = 1574110191; Args = "job_7.txt"; ClusterId = 9; ProcId = 0 ],
     [ Out = "results.1"; JobStatus = 1; TransferInput = "input_directory/job_4.txt"; ServerTime = 1574110191; Args = "job_4.txt"; ClusterId = 9; ProcId = 1 ],
     [ Out = "results.2"; JobStatus = 1; TransferInput = "input_directory/job_2.txt"; ServerTime = 1574110191; Args = "job_2.txt"; ClusterId = 9; ProcId = 2 ],
     [ Out = "results.3"; JobStatus = 1; TransferInput = "input_directory/job_5.txt"; ServerTime = 1574110191; Args = "job_5.txt"; ClusterId = 9; ProcId = 3 ],
     [ Out = "results.4"; JobStatus = 1; TransferInput = "input_directory/job_0.txt"; ServerTime = 1574110191; Args = "job_0.txt"; ClusterId = 9; ProcId = 4 ],
     [ Out = "results.5"; JobStatus = 1; TransferInput = "input_directory/job_6.txt"; ServerTime = 1574110191; Args = "job_6.txt"; ClusterId = 9; ProcId = 5 ],
     [ Out = "results.6"; JobStatus = 1; TransferInput = "input_directory/job_8.txt"; ServerTime = 1574110191; Args = "job_8.txt"; ClusterId = 9; ProcId = 6 ],
     [ Out = "results.7"; JobStatus = 1; TransferInput = "input_directory/job_1.txt"; ServerTime = 1574110191; Args = "job_1.txt"; ClusterId = 9; ProcId = 7 ],
     [ Out = "results.8"; JobStatus = 1; TransferInput = "input_directory/job_9.txt"; ServerTime = 1574110191; Args = "job_9.txt"; ClusterId = 9; ProcId = 8 ],
     [ Out = "results.9"; JobStatus = 1; TransferInput = "input_directory/job_3.txt"; ServerTime = 1574110191; Args = "job_3.txt"; ClusterId = 9; ProcId = 9 ]]



Managing Jobs
-------------

Once a job is in queue, the schedd will try its best to execute it to
completion. There are several cases where a user may want to interrupt
the normal flow of jobs. Perhaps the results are no longer needed;
perhaps the job needs to be edited to correct a submission error. These
actions fall under the purview of *job management*.

There are two ``Schedd`` methods dedicated to job management:

-  ``edit()``: Change an attribute for a set of jobs to a given
   expression. If invoked within a transaction, multiple calls to
   ``edit`` are visible atomically.
-  The set of jobs to change can be given as a ClassAd expression. If no
   jobs match the filter, *then an exception is thrown*.
-  ``act()``: Change the state of a job to a given state (remove, hold,
   suspend, etc).

Both methods take a *job specification*: either a ClassAd expression
(such as ``Owner=?="janedoe"``) or a list of job IDs (such as
``["1.1", "2.2", "2.3"]``). The ``act`` method takes an argument from
the ``JobAction`` enum. Commonly-used values include:

-  ``Hold``: put a job on hold, vacating a running job if necessary. A
   job will stay in the hold state until explicitly acted upon by the
   admin or owner.
-  ``Release``: Release a job from the hold state, returning it to Idle.
-  ``Remove``: Remove a job from the Schedd's queue, cleaning it up
   first on the remote host (if running). This requires the remote host
   to acknowledge it has successfully vacated the job, meaning
   ``Remove`` may not be instantaneous.
-  ``Vacate``: Cause a running job to be killed on the remote resource
   and return to idle state. With ``Vacate``, jobs may be given
   significant time to cleanly shut down.

Here's an example of job management in action:

.. code:: ipython3

    with schedd.transaction() as txn:
        clusterId = sub.queue(txn, 5)  # Queues 5 copies of this job.
        schedd.edit(["%d.0" % clusterId, "%d.1" % clusterId], "foo", '"bar"') # Sets attribute foo to the string "bar".
    print("=== START JOB STATUS ===")
    for job in schedd.xquery(requirements="ClusterId == %d" % clusterId, projection=["ProcId", "foo", "JobStatus"]):
        print("%d: foo=%s, job_status = %d" % (job.get("ProcId"), job.get("foo", "default_string"), job["JobStatus"]))
    print("=== END ===")
    
    schedd.act(htcondor.JobAction.Hold, 'ClusterId==%d && ProcId >= 2' % clusterId)
    print("=== START JOB STATUS ===")
    for job in schedd.xquery(requirements="ClusterId == %d" % clusterId, projection=["ProcId", "foo", "JobStatus"]):
        print("%d: foo=%s, job_status = %d" % (job.get("ProcId"), job.get("foo", "default_string"), job["JobStatus"]))
    print("=== END ===")


.. parsed-literal::

    === START JOB STATUS ===
    0: foo=bar, job_status = 1
    1: foo=bar, job_status = 1
    2: foo=default_string, job_status = 1
    3: foo=default_string, job_status = 1
    4: foo=default_string, job_status = 1
    === END ===
    === START JOB STATUS ===
    0: foo=bar, job_status = 1
    1: foo=bar, job_status = 1
    2: foo=default_string, job_status = 5
    3: foo=default_string, job_status = 5
    4: foo=default_string, job_status = 5
    === END ===


That's It!
----------

You've made it through the very basics of the Python bindings. While
there are many other features the Python module has to offer, we have
covered enough to replace the command line tools of ``condor_q``,
``condor_submit``, ``condor_status``, ``condor_rm`` and others.

Head back to the top-level notebook and try out one of our advanced
tutorials.
