:mod:`htcondor` API Reference
=============================

.. module:: htcondor
   :platform: Unix, Windows, Mac OS X
   :synopsis: Interact with the HTCondor daemons
.. moduleauthor:: Brian Bockelman <bbockelm@cse.unl.edu>

This page is an exhaustive reference of the API exposed by the :mod:`htcondor`
module.  It is not meant to be a tutorial for new users but rather a helpful
guide for those who already understand the basic usage of the module.

Interacting with Collectors
---------------------------

.. autoclass:: Collector

   .. automethod:: locate
   .. automethod:: locateAll
   .. automethod:: query
   .. automethod:: directQuery
   .. automethod:: advertise

.. autoclass:: DaemonTypes

.. autoclass:: AdTypes


Interacting with Schedulers
---------------------------

.. autoclass:: Schedd

   .. automethod:: transaction
   .. automethod:: query
   .. automethod:: xquery
   .. automethod:: act
   .. automethod:: edit
   .. automethod:: history
   .. automethod:: jobEpochHistory
   .. automethod:: submit
   .. automethod:: submitMany
   .. automethod:: spool

   A fully-worked example follows.

   .. code-block:: python

        import htcondor

        coll = htcondor.Collector("cm.pool.tld")
        quoted_schedd_name = htcondor.classad.quote("remote-schedd.pool.tld")
        hostname_job = htcondor.Submit({
            "executable": "/bin/hostname",  # the program to run on the execute node
            "output": "hostname.out",       # anything the job prints to standard output will end up in this file
            "error": "hostname.err",        # anything the job prints to standard error will end up in this file
            "log": "hostname.log",          # this file will contain a record of what happened to the job
            "request_cpus": "1",            # how many CPU cores we want
            "request_memory": "128MB",      # how much memory we want
            "request_disk": "128MB",        # how much disk space we want
            "transfer_input_files": "hello.txt",
            })

        sad = coll.query(htcondor.AdTypes.Schedd, constraint=f"Name=?={quoted_schedd_name}",projection=["Name", "MyAddress"])[0]
        schedd = htcondor.Schedd(sad)
        res = schedd.submit(hostname_job, spool=True)
        schedd.spool(list(hostname_job.jobs(clusterid=res.cluster())))

   .. automethod:: retrieve
   .. automethod:: refreshGSIProxy
   .. automethod:: reschedule
   .. automethod:: export_jobs
   .. automethod:: import_exported_job_results
   .. automethod:: unexport_jobs

.. autoclass:: JobAction

.. autoclass:: Transaction

.. autoclass:: TransactionFlags

.. autoclass:: QueryOpts

.. autoclass:: BlockingMode

.. autoclass:: HistoryIterator

.. autoclass:: QueryIterator

   .. automethod:: nextAdsNonBlocking
   .. automethod:: tag
   .. automethod:: done
   .. automethod:: watch

.. autofunction:: poll

.. autoclass:: BulkQueryIterator

.. autoclass:: JobStatus

Submitting Jobs
---------------

.. autoclass:: Submit

   .. automethod:: queue
   .. automethod:: queue_with_itemdata
   .. automethod:: expand
   .. automethod:: jobs
   .. automethod:: procs
   .. automethod:: itemdata
   .. automethod:: getQArgs
   .. automethod:: setQArgs
   .. automethod:: from_dag
   .. automethod:: setSubmitMethod
   .. automethod:: getSubmitMethod

.. autoclass:: QueueItemsIterator

.. autoclass:: SubmitResult

   .. automethod:: cluster
   .. automethod:: clusterad
   .. automethod:: first_proc
   .. automethod:: num_procs


Interacting with Negotiators
----------------------------

.. autoclass:: Negotiator

   .. automethod:: deleteUser
   .. automethod:: getPriorities
   .. automethod:: getResourceUsage
   .. automethod:: resetAllUsage
   .. automethod:: resetUsage
   .. automethod:: setBeginUsage
   .. automethod:: setCeiling
   .. automethod:: setLastUsage
   .. automethod:: setFactor
   .. automethod:: setPriority
   .. automethod:: setUsage


Managing Starters and Claims
----------------------------

.. autoclass:: Startd

   .. automethod:: drainJobs
   .. automethod:: cancelDrainJobs


.. autoclass:: DrainTypes

.. autoclass:: VacateTypes


Security Management
-------------------

.. autoclass:: Credd

    .. automethod:: add_password
    .. automethod:: delete_password
    .. automethod:: query_password
    .. automethod:: add_user_cred
    .. automethod:: delete_user_cred
    .. automethod:: query_user_cred
    .. automethod:: add_user_service_cred
    .. automethod:: delete_user_service_cred
    .. automethod:: query_user_service_cred
    .. automethod:: check_user_service_creds

.. autoclass:: CredTypes

.. autoclass:: CredCheck

.. autoclass:: CredStatus

.. autoclass:: SecMan

   .. automethod:: invalidateAllSessions
   .. automethod:: ping
   .. automethod:: getCommandString
   .. automethod:: setConfig
   .. automethod:: setPoolPassword
   .. automethod:: setTag
   .. automethod:: setToken

.. autoclass:: Token

   .. automethod:: write

.. autoclass:: TokenRequest
   :members:

Reading Job Events
------------------

The following is a complete example of submitting a job and waiting (forever)
for it to finish.  The next example implements a time-out.

.. code-block:: python

    #!/usr/bin/env python3

    import htcondor

    # Create a job description.  It _must_ set `log` to create a job event log.
    logFileName = "sleep.log"
    submit = htcondor.Submit(
        f"""
        executable = /bin/sleep
        transfer_executable = false
        arguments = 5

        log = {logFileName}
        """
    )

    # Submit the job description, creating the job.
    result = htcondor.Schedd().submit(submit, count=1)
    clusterID = result.cluster()

    # Wait (forever) for the job to finish.
    jel = htcondor.JobEventLog(logFileName)
    for event in jel.events(stop_after=None):
        # HTCondor appends to job event logs by default, so if you run
        # this example more than once, there will be more than one job
        # in the log.  Make sure we have the right one.
        if event.cluster != clusterID or event.proc != 0:
            continue

        if event.type == htcondor.JobEventType.JOB_TERMINATED:
            if(event["TerminatedNormally"]):
                print(f"Job terminated normally with return value {event['ReturnValue']}.")
            else:
                print(f"Job terminated on signal {event['TerminatedBySignal']}.");
            break

        if event.type in { htcondor.JobEventType.JOB_ABORTED,
                           htcondor.JobEventType.JOB_HELD,
                           htcondor.JobEventType.CLUSTER_REMOVE }:
            print("Job aborted, held, or removed.")
            break

        # We expect to see the first three events in this list, and allow
        # don't consider the others to be terminal.
        if event.type not in { htcondor.JobEventType.SUBMIT,
                               htcondor.JobEventType.EXECUTE,
                               htcondor.JobEventType.IMAGE_SIZE,
                               htcondor.JobEventType.JOB_EVICTED,
                               htcondor.JobEventType.JOB_SUSPENDED,
                               htcondor.JobEventType.JOB_UNSUSPENDED }:
            print(f"Unexpected job event: {event.type}!");
            break

The following example includes a deadline for the job to finish.  To
make it quick to run the example, the deadline is only ten seconds;
real jobs will almost always take considerably longer.  You can change
``arguments = 20`` to ``arguments = 5`` to verify that this example
correctly detects the job finishing.  For the same reason, we check
once a second to see if the deadline has expired.  In practice, you
should check much less frequently, depending on how quickly your
script needs to react and how long you expect the job to last.  In
most cases, even once a minute is more frequent than necessary or
appropriate on shared resources; every five minutes is better.

.. code-block:: python

    #!/usr/bin/env python3

    import time
    import htcondor

    # Create a job description.  It _must_ set `log` to create a job event log.
    logFileName = "sleep.log"
    submit = htcondor.Submit(
        f"""
        executable = /bin/sleep
        transfer_executable = false
        arguments = 20

        log = {logFileName}
        """
    )

    # Submit the job description, creating the job.
    result = htcondor.Schedd().submit(submit, count=1)
    clusterID = result.cluster()

    def waitForJob(deadline):
        jel = htcondor.JobEventLog(logFileName)
        while time.time() < deadline:
            # In real code, this should be more like stop_after=300; see above.
            for event in jel.events(stop_after=1):
                # HTCondor appends to job event logs by default, so if you run
                # this example more than once, there will be more than one job
                # in the log.  Make sure we have the right one.
                if event.cluster != clusterID or event.proc != 0:
                    continue
                if event.type == htcondor.JobEventType.JOB_TERMINATED:
                    if(event["TerminatedNormally"]):
                        print(f"Job terminated normally with return value {event['ReturnValue']}.")
                    else:
                        print(f"Job terminated on signal {event['TerminatedBySignal']}.");
                    return True
                if event.type in { htcondor.JobEventType.JOB_ABORTED,
                                   htcondor.JobEventType.JOB_HELD,
                                   htcondor.JobEventType.CLUSTER_REMOVE }:
                    print("Job aborted, held, or removed.")
                    return True
                # We expect to see the first three events in this list, and allow
                # don't consider the others to be terminal.
                if event.type not in { htcondor.JobEventType.SUBMIT,
                                       htcondor.JobEventType.EXECUTE,
                                       htcondor.JobEventType.IMAGE_SIZE,
                                       htcondor.JobEventType.JOB_EVICTED,
                                       htcondor.JobEventType.JOB_SUSPENDED,
                                       htcondor.JobEventType.JOB_UNSUSPENDED }:
                    print(f"Unexpected job event: {event.type}!");
                    return True
        else:
            print("Deadline expired.")
            return False

    # Wait no more than 10 seconds for the job finish.
    waitForJob(time.time() + 10);

Note that which job events are terminal, expected, or allowed may vary
somewhat from job to job; for instance, it's possible to submit a job
which releases itself from certain hold conditions.

.. autoclass:: JobEventLog

   .. automethod:: events
   .. automethod:: close

.. autoclass:: JobEvent

   .. autoattribute:: type
   .. autoattribute:: cluster
   .. autoattribute:: proc
   .. autoattribute:: timestamp
   .. automethod:: get
   .. automethod:: keys
   .. automethod:: values
   .. automethod:: items

.. autoclass:: JobEventType
.. autoclass:: FileTransferEventType

HTCondor Configuration
----------------------

.. autodata:: param
.. autofunction:: reload_config
.. autoclass:: _Param

.. autoclass:: RemoteParam

   .. automethod:: refresh

.. autofunction:: platform
.. autofunction:: version


HTCondor Logging
----------------

.. autofunction:: enable_debug
.. autofunction:: enable_log

.. autofunction:: log
.. autoclass:: LogLevel


Esoteric Functionality
----------------------

.. autofunction:: send_command
.. autoclass:: DaemonCommands

.. autofunction:: send_alive

.. autofunction:: set_subsystem
.. autoclass:: SubsystemType


Exceptions
----------

For backwards-compatibility, the exceptions in this module inherit
from the built-in exceptions raised in earlier (pre-v8.9.9) versions.

.. autoclass:: HTCondorException

.. autoclass:: HTCondorEnumError
.. autoclass:: HTCondorInternalError
.. autoclass:: HTCondorIOError
.. autoclass:: HTCondorLocateError
.. autoclass:: HTCondorReplyError
.. autoclass:: HTCondorTypeError
.. autoclass:: HTCondorValueError


.. _python-bindings-thread-safety:

Thread Safety
-------------

Most of the ``htcondor`` module is protected by a lock that prevents multiple
threads from executing locked functions at the same time.
When two threads both want to call locked functions or methods, they will wait
in line to execute them one at a time
(the ordering between threads is not guaranteed beyond "first come first serve").
Examples of locked functions include:
:meth:`Schedd.query`, :meth:`Submit.queue`, and :meth:`Schedd.edit`.

Threads that are not trying to execute locked ``htcondor`` functions will
be allowed to proceed normally.

This locking may cause unexpected slowdowns when using ``htcondor`` from
multiple threads simultaneously.

