#!/usr/bin/env pytest

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import htcondor2
from htcondor import (
    JobEventType,
)

from ornithology import (
    action,
    Condor,
    ClusterState,
    DaemonLog,
    JobStatus,
)


#
# Tests to see if the starter is actually correctly executing the guidance
# it's given.  We could add more-specific testing to see if the diagnostic
# tool(s) are working as necessary (probably as a lambda in the test case).
#


TEST_CASES = {
    "CarryOn": (
        '{ [ Command = "CarryOn"; ] }',
        JobStatus.HELD,
        "Carrying on according to guidance...",
    ),
    "StartJob": (
        '{ [ Command = "StartJob"; ] }',
        JobStatus.COMPLETED,
        "Starting job as guided...",
    ),
    "Abort": (
        '{ [ Command = "Abort"; ] }',
        JobStatus.HELD,
        "Aborting job as guided...",
    ),
    "RetryTransfer": (
        '{ [ Command = "RetryTransfer"; ], [ Command = "CarryOn"; ] }',
        JobStatus.HELD,
        "Retrying transfer as guided...",
    ),
    "RunDiagnostic": (
        '{ [ Command = "RunDiagnostic"; Diagnostic = "send_ep_logs"; ], [ Command = "CarryOn"; ] }',
        JobStatus.HELD,
        "Running diagnostic 'send_ep_logs' as guided...",
    ),
    # We check the starter side of this test case in test_starter_guidance;
    # the goal here is to verify that the shadow handle the reply correctly.
    "RunUnknownDiagnostic": (
        '{ [ Command = "RunDiagnostic"; Diagnostic = "unknown"; ], [ Command = "CarryOn"; ] }',
        JobStatus.HELD,
        "... diagnostic 'unknown' not registered in param table",
    ),
    "CarryOn w/ Extra": (
        '{ [ Command = "CarryOn"; Extraneous = True; ] }',
        JobStatus.HELD,
        "Carrying on according to guidance...",
    ),
    "StartJob w/ Extra": (
        '{ [ Command = "StartJob"; Extraneous = True; ] }',
        JobStatus.COMPLETED,
        "Starting job as guided...",
    ),
    "Abort w/ Extra": (
        '{ [ Command = "Abort"; Extraneous = True; ] }',
        JobStatus.HELD,
        "Aborting job as guided...",
    ),
    "RetryTransfer w/ Extra": (
        '{ [ Command = "RetryTransfer"; Extraneous = True; ], [ Command = "CarryOn"; Extraneous = True; ] }',
        JobStatus.HELD,
        "Retrying transfer as guided...",
    ),
    "RunDiagnostic w/ Extra": (
        '{ [ Command = "RunDiagnostic"; Diagnostic = "send_ep_logs"; Extraneous = True; ], [ Command = "CarryOn"; Extraneous = True; ] }',
        JobStatus.HELD,
        "Running diagnostic 'send_ep_logs' as guided...",
    ),
}


@action
def path_to_shadow_wrapper(test_dir):
    return test_dir / "shadow_wrapper"


@action
def the_condor(test_dir, path_to_shadow_wrapper):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            "SHADOW":                       path_to_shadow_wrapper.as_posix(),
            "SHADOW_DEBUG":                 "D_FULLDEBUG",

            # For simplicity, so that each test job gets its own starter log.
            "STARTER_LOG_NAME_APPEND":      "JobID",
        },
    ) as the_condor:
        SBIN = htcondor2.param["SBIN"]
        path_to_shadow_wrapper.write_text(
            "#!/bin/bash\n"
            f'exec {SBIN}/condor_shadow --use-guidance-in-job-ad "$@"' "\n"
        )
        path_to_shadow_wrapper.chmod(0o777)

        yield the_condor


@action
def the_job_description(test_dir, path_to_sleep):
    return {
        "executable":               path_to_sleep,
        "transfer_executable":      False,
        "should_transfer_files":    True,
        "universe":                 "vanilla",
        "arguments":                5,
        "log":                      f'{(test_dir / "job.log").as_posix()}.$(CLUSTER)',
        "starter_debug":            "D_FULLDEBUG",
        "request_cpus":             1,
        "request_memory":           1,
        "transfer_input_files":     "http://no-such.tld/example",
    }


@action
def the_job_handles(the_condor, the_job_description):
    job_handles = {}
    for name, test_case in TEST_CASES.items():
        (the_guidance, the_expected, _) = test_case

        complete_job_description = {
            ** the_job_description,
            "+_condor_guidance_test_case": the_guidance,
        }
        job_handle = the_condor.submit(
            description=complete_job_description,
            count=1,
        )

        job_handles[name] = job_handle

    yield job_handles


@action(params={name: name for name in TEST_CASES})
def the_job_tuple(request, the_job_handles):
    return (request.param, the_job_handles[request.param])


@action
def the_job_name(the_job_tuple):
    return the_job_tuple[0]


@action
def the_job_handle(the_job_tuple):
    return the_job_tuple[1]


@action
def the_expected_state(the_job_name):
    return TEST_CASES[the_job_name][1]


@action
def the_expected_log_line(the_job_name):
    return TEST_CASES[the_job_name][2]


@action
def the_completed_job(the_condor, the_job_handle):
    assert the_job_handle.wait(
        timeout=40,
        condition=ClusterState.all_terminal
    )

    return the_job_handle


@action
def the_starter_log(test_dir, the_completed_job):
    starter_log_path = (test_dir / "condor" / "log" / f"StarterLog.{the_completed_job.clusterid}.0")
    starter_log = DaemonLog(starter_log_path)
    return starter_log.open()


# From test_allowed_execute_duration.py
def event_types_in_order(types, events):
    t = 0
    for i in range(0, len(events)):
        event = events[i]

        if event.type == types[t]:
            t += 1
            if t == len(types):
                return True
    else:
        return False


class TestGuidanceCommands:

    def test_guidance_command(self,
        test_dir, the_job_name, the_condor,
        the_completed_job, the_expected_state,
        the_starter_log, the_expected_log_line
    ):
        # Did the job carry through to the expected state?
        assert the_completed_job.state.status_exactly( 1, the_expected_state )

        # Did the start actually execute the guidance it was given?
        assert the_starter_log.wait(
            timeout=1,
            condition=lambda line: the_expected_log_line in line.message,
        )

        # Gross hacks.
        if the_job_name == "RunUnknownDiagnostic":
            # Validate that the shadow handled the error correctly.
            expected_shadow_log_line = "Diagnostic 'unknown' did not complete: 'Error - Unregistered'"
            the_shadow_log = the_condor.shadow_log.open()
            assert the_shadow_log.wait(
                timeout=1,
                condition=lambda line: expected_shadow_log_line in line.message and f"{the_completed_job.clusterid}.0" in line.tags
            )

        if the_job_name.startswith("RetryTransfer"):
            # Validate that the transfer was retried.  (Note the lack of
            # an execute event, indicating that both begin/end pairs of
            # file transfer were input.)
            assert event_types_in_order(
                [
                    JobEventType.SUBMIT,
                    JobEventType.FILE_TRANSFER,
                    JobEventType.FILE_TRANSFER,
                    JobEventType.FILE_TRANSFER,
                    JobEventType.FILE_TRANSFER,
                    JobEventType.REMOTE_ERROR,
                    JobEventType.JOB_EVICTED,
                    JobEventType.JOB_HELD,
                ],
                the_completed_job.event_log.events
            )


        if the_job_name.startswith("RunDiagnostic"):
            # Validate that the diagnostic was run.
            diagnostic_log_path = (test_dir / ".diagnostic" / f"send_ep_logs.{the_completed_job.clusterid}.0.0")
            assert diagnostic_log_path.exists()
