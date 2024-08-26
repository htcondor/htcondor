Examples of Using the ``htcondor2`` and/or ``classad2`` Modules
===============================================================

Although this documentation is not intended for new users, sometimes a
dozen lines of code is worth a thousand words.

Using :class:`htcondor2.JobEventLog`
------------------------------------

The following is a complete example of submitting a job and waiting (forever)
for it to finish.  The next example implements a time-out.

.. note::
    Both examples were originally written for the version 1 API.  Both
    examples ``import htcondor2 as htcondor`` to help show that the
    version 1 and version 2 APIs are mostly compatible, but are otherwise
    identical to the version 1 examples at the time of writing.

.. code-block:: python

    #!/usr/bin/env python3

    import htcondor2 as htcondor

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
    import htcondor2 as htcondor

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
