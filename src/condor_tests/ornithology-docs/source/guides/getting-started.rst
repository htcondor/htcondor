Getting Started
===============

.. py:currentmodule:: ornithology

What is ``ornithology``?
------------------------

Ornithology is a Python-based testing framework for HTCondor.
It uses ``pytest`` as its test execution engine, and provides a wide variety
of tools for inspecting the state of an HTCondor pool.

Ornithology is designed to be flexible and extensible.
If you find yourself needing a new tool that might be useful in other
contexts, feel free to add it the ``ornithology`` module itself.

An annotated version of a test file written using Ornithology,
``test_hold_and_release.py``, is shown below. The goal of the test is to walk
through the process of submitting a job, letting it start running, then
holding it, then releasing it, then allowing it to finish. Ornithology is used
to submit the job, react to events, and confirm that the expected order of
events occurred.

.. code-block:: python

    #!/usr/bin/env pytest

    from ornithology import (
        config,
        standup,
        action,
        SetAttribute,
        SetJobStatus,
        JobStatus,
        in_order,
    )

    # This is a pytest "fixture" - a re-usable blob of code to run.
    # Its return value can be made available to tests by putting the name of the
    # fixture in the test function arguments.
    @action
    def job_queue_events_for_sleep_job(default_condor, path_to_sleep):
        # Submit a single job that sleeps for 10 seconds.
        # "path_to_sleep" is a fixture provided by Ornithology itself
        # (the path to a cross-platform sleep script).
        handle = default_condor.submit(
            description={"executable": path_to_sleep, "arguments": "10"},
            count=1,
        )

        # The id (cluster.proc) of the first (and only) job in the submit.
        jobid = handle.job_ids[0]

        # Wait for (and react to) events in the job queue log.
        default_condor.job_queue.wait_for_events(
            {  # this dictionary maps job ids to (event, reaction) tuples, or just to events to wait for, in order
                jobid: [
                    (  # when the job starts running, hold it
                        SetJobStatus(JobStatus.RUNNING),
                        lambda jobid, event: default_condor.run_command(
                            ["condor_hold", jobid]
                        ),
                    ),
                    (  # once the job is held, release it
                        SetJobStatus(JobStatus.HELD),
                        lambda jobid, event: default_condor.run_command(
                            ["condor_release", jobid]
                        ),
                    ),
                    # finally, wait for the job to complete
                    SetJobStatus(JobStatus.COMPLETED),
                ]
            },
            # All waits must have a timeout.
            timeout=120,
        )

        # This returns all of the job events in the job queue log for the jobid,
        # making that information available to downstream fixtures/tests.
        return default_condor.job_queue.by_jobid[jobid]


    # All tests must be methods of classes; the name of the class must be Test*
    class TestCanHoldAndReleaseJob:
        # Methods that begin with test_* are tests.

        # The "self" argument is required by Python, but unnecessary for Ornithology.
        # The "job_queue_events_for_sleep_job" brings in the return value of the fixture defined above.
        def test_job_queue_events_in_correct_order(self, job_queue_events_for_sleep_job):
            # "in_order" is an assertion helper provided by Ornithology.
            # It tests whether the items in the first argument appear in the order given
            # by the second argument (allowing extra items between the expected ones).
            assert in_order(
                job_queue_events_for_sleep_job,
                [
                    SetJobStatus(JobStatus.IDLE),
                    SetJobStatus(JobStatus.RUNNING),
                    SetJobStatus(JobStatus.HELD),
                    SetJobStatus(JobStatus.IDLE),
                    SetJobStatus(JobStatus.RUNNING),
                    SetJobStatus(JobStatus.COMPLETED),
                ],
            )

        # Another test, to check that the hold reason code was what we expected.
        def test_hold_reason_code_was_1(self, job_queue_events_for_sleep_job):
            assert SetAttribute("HoldReasonCode", "1") in job_queue_events_for_sleep_job


We recommend reading through :doc:`test_curl_plugin` to get a feel for how to
build an Ornithology test from the ground up.


Running Ornithology Tests
-------------------------

To run the Ornithology tests, you must be using Python 3 and have the
Python 3 HTCondor/ClassAd libraries in your ``PYTHONPATH``.  Assuming you set
``CMAKE_INSTALL_PREFIX`` to ``~/install``, after ``make install`` finishes,
you'll set ``PYTHONPATH`` to ``~/install/lib/python3``.  Make sure that your
installed ``bin`` directory is in your ``PATH`` (before any system version of
HTCondor), and that ``CONDOR_CONFIG`` points to HTCondor under test.
[#which_condor]_

You'll need ``pytest`` and one extension to run the Ornithology tests.  The
easiest way to handle this is with Pip, although you can use your system
``pytest`` (and ``pytest-httpserver``), or Miniconda, if you desire:

.. code-block:: console

  $ cd condor_tests
  $ pip3 install --user -r requirements.txt

This may install a ``pytest`` executable early enough in your ``PATH`` to be
useful, but assuming it doesn't, you can start a specific Ornithology test
in the following way:

.. code-block:: console

  $ python3 -m pytest test_run_sleep_job.py

One of the lines early in the output will look like the following:

.. code-block:: text

  Base per-test directory: /tmp/condor-tests-1598380839-15666

which will not be cleaned up after the test runs, for your debugging
convenience.

Ornithology tests can also be run under ``ctest``.
In that case, the test directory will end up under
``src/condor_tests/${TEST_NAME}_ctest/tests-dirs``.
Note that ``ctest`` will use your system Python, so you must install
``pytest`` to your system Python.


Running Ornithology Tests in the BaTLab
---------------------------------------

Ornithology tests can run as part of the standard BaTLab/Metronome test process.

Which tests are run is controlled by ``src/condor_tests/CMakeLists.txt``.

The line which runs the ``test_hold_and_release.py`` test above is

.. code-block:: text

    condor_pl_test(test_hold_and_release "Submit a job, hold it, release it, run it completion" "core;quick;full" CTEST DEPENDS "src/condor_tests/ornithology;src/condor_tests/conftest.py")

... which looks like any other test declaration.

When running on the BaTLab, Ornithology test directories are available
in the test results tarball under ``condor_tests/test-dirs``.

.. [#which_condor] This may require as little as setting ``RELEASE_DIR`` properly.

