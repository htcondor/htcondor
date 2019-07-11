Submitting and Managing Jobs
============================

The two most common HTCondor command line tools are ``condor_q`` and ``condor_submit``; in :doc:`htcondor`
we learned about the :meth:`~htcondor.Schedd.xquery` method that corresponds to ``condor_q``.
Here, we will learn the Python binding equivalent of ``condor_submit``.

As usual, we start by importing the relevant modules::

   >>> import htcondor

Submitting Jobs
---------------

We will submit jobs utilizing the dedicated :class:`~htcondor.Submit` object.

.. note:: The :class:`~htcondor.Submit` object was introduced in 8.5.6, which might be newer than your
   home cluster.  The original API, using the :meth:`htcondor.Schedd.submit` method, utilizes raw ClassAds
   and is not covered here.

:class:`~htcondor.Submit` objects consist of key-value pairs.  Unlike ClassAds, the values do not have an
inherent type (such as strings, integers, or booleans); they *are* evaluated with macro expansion at submit time.
Where reasonable, they behave like Python dictionaries::

   >>> sub = htcondor.Submit({"foo": "1", "bar": "2", "baz": "$(foo)"})
   >>> sub.setdefault("qux", "3")
   >>> print "=== START ===\n%s\n=== END ===" % sub
   === START ===
   baz = $(foo)
   foo = 1
   bar = 2
   qux = 3
   queue
   === END ===
   >>> print sub.expand("baz")
   1

The available attributes - and their semantics - are relatively well documented in the
:doc:`/man-pages/condor_submit` man page; we won't repeat them
here.  A minimal, but realistic submit object may look like the following::

   >>> sub = htcondor.Submit({"executable": "/bin/sleep", "arguments": "5m"})

To go from a submit object to job in a schedd, one must do three things:

1.  Create a new transaction in the schedd using :meth:`~htcondor.Schedd.transaction`.
2.  Call the :meth:`~htcondor.Submit.queue` method, passing the transaction object.
3.  Commit the transaction.

Since the transaction object is a Python context, (1) and (3) can be achieved using Python's ``with`` statement::

   >>> schedd = htcondor.Schedd()         # Create a schedd object using default settings.
   >>> with schedd.transaction() as txn:  # txn will now represent the transaction.
   ...    print sub.queue(txn)            # Queues one job in the current transaction; returns job's cluster ID
   3

If the code block inside the ``with`` statement completes successfully, the transaction is automatically committed.
If an exception is thrown (or Python abruptly exits), the transaction is aborted.

Managing Jobs
-------------

Once a job is in queue, the schedd will try its best to execute it to completion.  There are several cases where
a user may want to interrupt the normal flow of jobs.  Perhaps the results are no longer needed; perhaps the job
needs to be edited to correct a submission error.  These actions fall under the purview of *job management*.

There are two :class:`~htcondor.Schedd` methods dedicated to job management:

*  :meth:`~htcondor.Schedd.edit`: Change an attribute for a set of jobs to a given expression.  If invoked within
   a transaction, multiple calls to :meth:`~htcondor.Schedd.edit` are visible atomically.

   *  The set of jobs to change can be given as a ClassAd expression.  If no jobs match the filter, *then an exception is thrown*.
*  :meth:`~htcondor.Schedd.act`: Change the state of a job to a given state (remove, hold, suspend, etc).

Both methods take a *job specification*: either a ClassAd expression (such as ``Owner=?="janedoe"``)
or a list of job IDs (such as ``["1.1", "2.2", "2.3"]``).  The :meth:`~htcondor.Schedd.act` method takes an argument
from the :class:`~htcondor.JobAction` enum.  Commonly-used values include:

*  ``Hold``: put a job on hold, vacating a running job if necessary.  A job will stay in the hold
   state until explicitly acted upon by the admin or owner.
*  ``Release``: Release a job from the hold state, returning it to Idle.
*  ``Remove``: Remove a job from the Schedd's queue, cleaning it up first on the remote host (if running).
   This requires the remote host to acknowledge it has successfully vacated the job, meaning ``Remove`` may
   not be instantaneous.
*  ``Vacate``: Cause a running job to be killed on the remote resource and return to idle state.  With
   ``Vacate``, jobs may be given significant time to cleanly shut down.

Here's an example of job management in action::

   >>> with schedd.transaction() as txn:
   ...    clusterId = sub.queue(txn, 5)  # Queues 5 copies of this job.
   ...    schedd.edit(["%d.0" % clusterId, "%d.1" % clusterId], "foo", '"bar"') # Sets attribute foo to the string "bar".
   >>> for job in schedd.xquery(requirements="ClusterId == %d" % clusterId, projection=["ProcId", "foo", "JobStatus"]):
   ...    print "%d: foo=%s, job_status = %d" % (job.get("ProcId"), job.get("foo", "default_string"), job["JobStatus"])
   0: foo=bar, job_status = 1
   1: foo=bar, job_status = 1
   2: foo=default_string, job_status = 1
   3: foo=default_string, job_status = 1
   4: foo=default_string, job_status = 1
   >>> schedd.act(htcondor.JobAction.Hold, 'ClusterId==%d && ProcId >= 2' % clusterId)
   >>> for job in schedd.xquery(requirements="ClusterId == %d" % clusterId, projection=["ProcId", "foo", "JobStatus"]):
   ...    print "%d: foo=%s, job_status = %d" % (job.get("ProcId"), job.get("foo", "default_string"), job["JobStatus"])
   0: foo=bar, job_status = 1
   1: foo=bar, job_status = 1
   2: foo=default_string, job_status = 5
   3: foo=default_string, job_status = 5
   4: foo=default_string, job_status = 5

**That's it!**

You've made it through the very basics of the Python bindings.  While there are many other features the Python
module has to offer, we have covered enough to replace the command line tools of ``condor_q``, ``condor_submit``,
``condor_status``, ``condor_rm`` and others.

