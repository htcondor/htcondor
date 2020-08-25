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


Running Ornithology Tests Locally
---------------------------------

Running tests locally during development is a good way to quickly iterate on
the design of a test.

To run Ornithology tests locally, first follow these steps:

#. Install HTCondor (perhaps by building it). Ornithology will use the system
   HTCondor configuration to find the HTCondor binaries, but does not otherwise
   need a running HTCondor pool on the machine.
#. Install Python. Ornithology requires Python >=3.5, Python >=3.6 preferred.
   We recommend using `miniconda <https://docs.conda.io/en/latest/miniconda.html>`_.
#. Install Ornithology's dependencies by running
   ``pip install -r src/condor_tests/requirements.txt``.

Now that Ornithology is installed, you can run Ornithology tests using the
``pytest`` command, passing it the path to the test file.
For example, to the run the ``test_hold_and_release.py`` test shown above,
you would run ``pytest test_hold_and_release.py``:

.. code-block:: console

    $ pytest src/condor_tests/test_hold_and_release.py
    ================================================= test session starts ==================================================
    platform linux -- Python 3.6.8, pytest-6.0.1, py-1.9.0, pluggy-0.13.1 -- /usr/bin/python3
    cachedir: .pytest_cache
    rootdir: /home/build/htcondor/src/condor_tests, configfile: pytest.ini
    plugins: httpserver-0.3.5

    Base per-test directory: /tmp/condor-tests-1598380756-5052
    Platform:
    Linux-5.4.0-42-generic-x86_64-with-centos-7.8.2003-Core
    Python version:
    3.6.8 (default, Apr  2 2020, 13:34:55)
    [GCC 4.8.5 20150623 (Red Hat 4.8.5-39)]
    Python bindings version:
    $CondorVersion: 8.9.9 Aug 25 2020 PRE-RELEASE-UWCS $
    HTCondor version:
    $CondorVersion: 8.9.9 Aug 25 2020 PRE-RELEASE-UWCS $
    $CondorPlatform: X86_64-CentOS_7.8 $

    collected 2 items

    test_hold_and_release.py::TestCanHoldAndReleaseJob::test_job_queue_events_in_correct_order PASSED                [ 50%]
    test_hold_and_release.py::TestCanHoldAndReleaseJob::test_hold_reason_code_was_1 PASSED                           [100%]

    ================================================== 2 passed in 25.25s ==================================================

By default, Ornithology puts the temporary directories used for its tests
under the system temporary directory.
The local directory for personal pools will be under the test directory;
its specific location is often mentioned in
log messages, so they should be easy to find and inspect.
Ornithology does not remove these directories when the tests finish
(whether they succeeded or failed).


Running Ornithology Tests in the BaTLab
---------------------------------------

Ornithology tests can run as part of the standard BaTLab/Metronome test process.
Which tests are run is controlled by ``src/condor_tests/CMakeLists.txt``.
The line which runs the ``test_hold_and_release.py`` test above is

.. code-block:: text

    condor_pl_test(test_hold_and_release "Submit a job, hold it, release it, run it completion" "core;quick;full")

... which looks like any other test declaration.

When running on the BaTLab, Ornithology test directories are available
in the test results tarball under ``condor_tests/test-dirs``.
