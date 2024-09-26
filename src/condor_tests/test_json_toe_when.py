#!/usr/bin/env pytest

import os
import json
import time
import logging
import htcondor2 as htcondor

from ornithology import (
    config,
    standup,
    action,
    JobStatus,
    ClusterState,
    Condor,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_event_log(test_dir):
    return (test_dir / "event_log").as_posix()


@action
def the_condor_config(test_dir, the_event_log):
    return {
        "EVENT_LOG_FORMAT_OPTIONS": "JSON, ISO_DATE",
        "EVENT_LOG": the_event_log,
    }


@action
def the_condor(test_dir, the_condor_config):
    with Condor(test_dir / "condor", the_condor_config) as condor:
        yield condor


@config
def the_log():
    return "the.log"


@config
def the_job_parameters(path_to_sleep, the_log):
    return {
        "executable": path_to_sleep,
        "transfer_executable": "true",
        "should_transfer_files": "true",
        "universe": "vanilla",
        "arguments": "1",
        "log": the_log,
    }


@action
def the_job(test_dir, the_condor, the_job_parameters):
    return the_condor.submit(the_job_parameters, count=1);


@action
def the_job_log(test_dir, the_log, the_job):
    assert the_job.wait(
        condition=ClusterState.all_complete,
        timeout=60,
        fail_condition=ClusterState.any_held,
    )

    # This seems like something I should be able to get from the cluster handle.
    return htcondor.JobEventLog((test_dir / the_log).as_posix())


@action
def the_event(the_job_log):
    for event in the_job_log.events(0):
        if event.type == htcondor.JobEventType.JOB_TERMINATED:
            return event
    assert False


# This should probably be in a library.
def parse_json_event_log(f, file_size):
    start_pos = f.tell()

    while True:
        l = f.readline()

        if l == "}\n":
            end_pos = f.tell() - 1

            f.seek(start_pos)
            blob = f.read(end_pos - start_pos)
            obj = json.loads(blob)
            start_pos = end_pos
            f.seek(start_pos)

            yield obj

            if end_pos + 1 == file_size:
                return


class TestJSONToEWhen:

    # We don't actually use `the_event` for anything, but it does make sure
    # that the job has actually terminated.
    def test_toe_when_is_integer(self, the_event, the_event_log):
        found_integer_toe_when = False

        file_size = os.stat(the_event_log).st_size
        with open(the_event_log, "r") as f:
            for event in parse_json_event_log(f, file_size):
                if event["EventTypeNumber"] == htcondor.JobEventType.JOB_TERMINATED:
                    toe_tag = event.get("ToE")
                    assert toe_tag is not None
                    found_integer_toe_when = isinstance(toe_tag["When"], int)

        # Assume that if we didn't find it right away, that we need to
        # wait for the event log to be updated.
        if not found_integer_toe_when:
            print( "Did not find JOB_TERMINATED event in log, waiting 10 seconds for file size to change." )

            for i in range(1, 10):
                time.sleep(1)
                new_file_size = os.stat(the_event_log).st_size
                if file_size != new_file_size:
                    print( "Detected file size change, trying again." )
                    break
            else:
                print( "Did not detect file size change, giving up." )
                assert file_size != new_file_size

            file_size = os.stat(the_event_log).st_size
            with open(the_event_log, "r") as f:
                for event in parse_json_event_log(f, file_size):
                    print(event["EventTypeNumber"])
                    if event["EventTypeNumber"] == htcondor.JobEventType.JOB_TERMINATED:
                        toe_tag = event.get("ToE")
                        assert toe_tag is not None
                        found_integer_toe_when = isinstance(toe_tag["When"], int)

        assert found_integer_toe_when
