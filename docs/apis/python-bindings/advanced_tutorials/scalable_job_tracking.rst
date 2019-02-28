Scalable Job Tracking
=====================

The Python bindings provide two scalable mechanisms for tracking jobs:

   * **Poll-based tracking**: The Schedd can be periodically polled
     through the use of :meth:`~htcondor.Schedd.xquery` to get job
     status information.
   * **Event-based tracking**: Using the job's *user log*, Python can
     see all job events and keep an in-memory representation of the
     job status.

Both poll- and event-based tracking have their strengths and weaknesses; the
intrepid user can even combine both methodologies to have extremely reliable,
low-latency job status tracking.

In this module, we outline the important design considerations behind each
approach and walk through examples.

Poll-based Tracking
^^^^^^^^^^^^^^^^^^^

Poll-based tracking involves periodically querying the schedd(s) for jobs of interest.
We have covered the technical aspects of querying the Schedd in prior tutorials.
Beside the technical means of polling, important aspects to consider are *how often*
the poll should be performed and *how much* data should be retrieved.

.. note:: When :meth:`~htcondor.Schedd.xquery` is used, the query will cause the schedd to fork
   up to ``SCHEDD_QUERY_WORKERS`` simultaneous workers.  Beyond that point, queries will
   be handled in a non-blocking manner inside the main ``condor_schedd`` process.  Thus, the
   memory used by many concurrent queries can be reduced by decreasing ``SCHEDD_QUERY_WORKERS``.

A job tracking system should not query the Schedd more than once a minute.  Aim to minimize the
data returned from the query through the use of the projection; minimize the number of jobs returned
by using a query constraint.  Better yet, use the ``AutoCluster`` flag to have :meth:`~htcondor.Schedd.xquery`
return a list of job summaries instead of individual jobs.

Advantages:
   *  A single entity can poll all ``condor_schedd`` instances in a pool; using :func:`~htcondor.poll`,
      multiple Schedds can be queried simultaneously.
   *  The tracking is resilient to bugs or crashes.  All tracked state is replaced at the next polling
      cycle.

Disadvantages:
   *  The amount of work to do is a function of the number of jobs in the schedd; may scale poorly
      once more than 100,000 simultaneous jobs are tracked.
   *  Each job state transition is not seen; only snapshots of the queue in time.
   *  If a job disappears from the Schedd, it may be difficult to determine why (Did it finish?  Was
      it removed?)
   *  Only useful for tracking jobs at the minute-level granularity.


Event-based Tracking
^^^^^^^^^^^^^^^^^^^^

Each job in the Schedd can specify the ``UserLog`` attribute; the Schedd will atomically append a
machine-parseable event to the specified file for every state transition the job goes through.
By keeping track of the events in the logs, we can build an in-memory representation of the job
queue state.

Advantages:
   *  No interaction with the ``condor_schedd`` process is needed to read the event logs; the job
      tracking effectively places no burden on the Schedd.
   *  In most cases, the Schedd writes to the log synchronously after the event occurs.  Hence, the
      latency of receiving an update can be sub-second.
   *  The job tracking scales as a function of the event rate, not the total number of jobs.
   *  Each job state is seen, even after the job has left the queue.

Disadvantages:
   *  Only the local ``condor_schedd`` can be tracked; there is no mechanism to receive the event
      log remotely.
   *  Log files must be processed from the beginning, with no rotations or truncations possible.
      Large files can take a large amount of CPU time to process.
   *  If every job writes to a separate log file, the job tracking software may have to keep an
      enormous number of open file descriptors.  If every job writes to the same log file, the
      log file may grow to many gigabytes.
   *  If the job tracking software misses an event (or an unknown bug causes the ``condor_schedd``
      to fail to write the event), then the job tracker may believe a job incorrectly is stuck
      in the wrong state.

At a technical level, event tracking is implemented with the :func:`~htcondor.read_events` function::

   >>> fp = open("/tmp/job_one.log")
   >>> events = htcondor.read_events(fp)
   >>> for event in events:
   ...     print event

The return value of :func:`~htcondor.read_events` is an :class:`~htcondor.EventIterator` object,
which yields a :class:`~classad.ClassAd`-based description of the job event.

If the job tracker is embedded inside a larger application framework, it may be worth noting
that:

   *  The :class:`~htcondor.EventIterator` can be set to non-blocking mode; if no event is available,
      it will return ``None`` instead of throwing a ``StopException``.
   *  The :class:`~htcondor.EventIterator` can return an ``inotify``-based file descriptor.  This
      file descriptor can be used with :func:`select.select()` to determine which log file has a
      new event ready to be read.

In particular, when using the ``inotify``-based polling, one can be informed of newly-available
events within milliseconds of them being written to the file.

