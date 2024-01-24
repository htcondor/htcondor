#!/usr/bin/env pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
	# make sure that file transfer plugins are enabled (might be disabled by default)
	ENABLE_URL_TRANSFERS = true
"""
#endtestreq

import time
import logging

from htcondor2 import (
    JobEventType,
)

from ornithology import (
    config,
    standup,
    action,

    write_file,
    run_command,
    ClusterState,
    format_script,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


#
# 1) Does a self-checkpointing job that should complete do so when
#    allowed_execute_duration is set?
# 2) Does a self-checkpointing job that should complete do so when
#    allowed_execute_duration is set if it's vacated (after a checkpoint)?
# 3) Does a self-checkpointing job that should be held before its first
#    checkpoint become so?
# 4) Does a self-checkpointing job that should be held before its second
#    checkpoint become so?
# 5) Does a self-checkpointing job that should be held before its second
#    checkpoint become so if it's restarted after its first checkpoint?
#

#
# 6) Does a non-self-checkpointing job that should complete do so when
#    allowed_execute_duration is set (even if its transfer time exceeds
#    its allowed execute duration)?
# 7) Does a non-self-checkpointing job that should be held when
#    allowed_execute_duration is set become so?
#

#
# Please don't try to edit the constants to speed this test up.
#
# If you must, make sure, among other things, that this job takes
# longer to complete than twice the specified AllowedExecuteDuration.
#

@action
def path_to_the_job_script(test_dir):
    script = """
    #!/usr/bin/python3

    import sys
    import time
    import getopt

    opt_slow_step = -1
    (list, remainder) = getopt.getopt(sys.argv[1:], '', ['normal', 'slow='])
    for option, value in list:
        if option == "--slow":
            opt_slow_step = int(value)

    total_steps = 24
    num_completed_steps = 0
    try:
        with open("saved-state", "r") as saved_state:
            num_completed_steps = int(saved_state.readline().strip())
    except IOError:
        pass

    while num_completed_steps < total_steps:
        print(f"Starting step {num_completed_steps}.")

        if num_completed_steps == opt_slow_step:
            time.sleep(60)
        else:
            time.sleep(3)
        num_completed_steps += 1

        if num_completed_steps % 5 == 0:
            print(f"Checkpointing after {num_completed_steps}.")
            try:
                with open("saved-state", "w") as saved_state:
                    saved_state.write(f"{num_completed_steps}")
                sys.exit(17)
            except IOError:
                print("Failed to write checkpoint.", file=sys.stderr);
                sys.exit(1)

    print(f"Completed all {total_steps} steps.")
    sys.exit(0)
    """

    path = test_dir / "counting.py"
    write_file(path, format_script(script))

    return path


@action
def the_job_description(path_to_the_job_script):
    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "executable":                   path_to_the_job_script.as_posix(),
        "transfer_executable":          "true",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",

        "checkpoint_exit_code":         "17",
        "transfer_checkpoint_files":    "saved-state",

        "allowed_execute_duration":       "30",
    }


#
# We submit the jobs for tests #1, #3, #4, #6, and #7 at the same time,
# because we don't need to interact with them while they're running, just
# check their log when they're done.
#
# Tests #2 and #5 require that the job be vacated at specific times.  There's
# an environment variable in the job that points at the HTCondor binaries,
# but not for the host HTCondor's configuration or the location of the job's
# schedd, so we can't have the job vacate itself.
#

#
# The code for this test could be improved: job two is a clone of job
# job one, except for the special finalization code, which could be shared
# with job five, which is a clone of job four otherwise.
#

#
# FIXME: We should check at the end of every otherwise-successful test
# to see if the final the output log is what we expect.
#

#
# FIXME: For jobs which are (deliberately) interruped, we should check if
# they actually resumed from where they left off.  I don't know how to do
# so right at the moment.
#

#
# This fixture depends on all of the job_*_handle fixtures we want to run
# concurrently.  Since each of the final_job_*_handle fixtures depend
# on this fixture, each job will have been submitted before we start
# waiting on the first one, whichever one that might be.
#

@action
def all_job_handles(job_one_handle, job_three_handle, job_four_handle, job_six_handle, job_seven_handle):
    pass


@action
def job_one_handle(default_condor, test_dir, the_job_description):
    job_one_description = {
        ** the_job_description,
        ** {
            "arguments":    "--normal",
            "log":          test_dir / "job_one.log",
            "output":       test_dir / "job_one.out",
            "error":        test_dir / "job_one.err",
        }
    }

    job_one_handle = default_condor.submit(
        description = job_one_description,
        count = 1,
    )

    yield job_one_handle

    job_one_handle.remove()


@action
def final_job_one_handle(default_condor, job_one_handle, all_job_handles):
    job_one_handle.wait(
        verbose = True,
        timeout = 180,
        condition = ClusterState.all_complete,
        fail_condition = ClusterState.any_held,
    )

    return job_one_handle


@action
def job_one_events(final_job_one_handle):
    return final_job_one_handle.event_log.events


@action
def job_two_handle(default_condor, test_dir, the_job_description):
    job_two_description = {
        ** the_job_description,
        ** {
            "arguments":    "--normal",
            "log":          test_dir / "job_two.log",
            "output":       test_dir / "job_two.out",
            "error":        test_dir / "job_two.err",
        }
    }

    job_two_handle = default_condor.submit(
        description = job_two_description,
        count = 1,
    )

    yield job_two_handle

    job_two_handle.remove()


#
# Note that job_two can not simply run until it reaches a terminal state;
# the test requires that it be interrupted (vacated) before it writes out
# its last checkpoint (to verify the doing so does not cause it to wrongly
# go on hold).
#
@action
def final_job_two_handle(default_condor, job_two_handle):
    # We can determine if the job has checkpointed by waiting for the
    # second of a pair of file transfer events after an execute event.
    # No other events are allowed between the two file transfer events,
    # to prevent us from detecting a restart.

    #
    # This code is awful and super detail-oriented.
    #
    job_started = False
    checkpoint_started = False
    checkpoint_completed = False
    for delay in range(60):
        for event in job_two_handle.event_log.read_events():
            if not job_started:
                if event.type == JobEventType.EXECUTE:
                    logger.debug("Found EXECUTE event")
                    job_started = True
                continue

            if not checkpoint_started:
                if event.type == JobEventType.FILE_TRANSFER:
                    logger.debug("Found first FILE_TRANSFER event")
                    checkpoint_started = True
            else:
                if event.type == JobEventType.IMAGE_SIZE:
                    logger.debug("Found IMAGE_SIZE event")
                    continue
                assert event.type == JobEventType.FILE_TRANSFER
                logger.debug("Found second FILE_TRANSFER event")
                checkpoint_completed = True
                break
        if checkpoint_completed:
            break
        time.sleep(1)
    assert checkpoint_completed

    default_condor.run_command(
        ['condor_vacate_job', str(job_two_handle.job_ids[0])],
        timeout=5, echo=True)
    # In the wild, running condor_reschedule immediately after running
    # condor_vacate_job will sometimes not result in the job being
    # considered by the negotiator in its next cycle.  The basic problem --
    # whyever the schedd needs to be prompted -- is kind of dumb, so
    # if this sleep ever becomes a problem, I'd rather fix that problem
    # than see if there's anything I can poll to determine when it's safe
    # to call condor_reschedule.
    time.sleep(1)
    # This is amazingly stupid but nonetheless necessary for some reason.
    default_condor.run_command(
        ['condor_reschedule'],
        timeout=60, echo=True)

    # This actually reads events out of the event_log trace, so it won't
    # miss the events that we just read in the loop above.  That might be
    # problematic in other cases, but not this one...
    job_two_handle.wait(
        verbose = True,
        timeout = 180,
        condition = ClusterState.all_complete,
        fail_condition = ClusterState.any_held,
    )

    return job_two_handle


@action
def job_two_events(final_job_two_handle):
    return final_job_two_handle.event_log.events


@action
def job_three_handle(default_condor, test_dir, the_job_description):
    job_three_description = {
        ** the_job_description,
        ** {
            "arguments":    "--slow 0",
            "log":          test_dir / "job_three.log",
            "output":       test_dir / "job_three.out",
            "error":        test_dir / "job_three.err",
       }
    }

    job_three_handle = default_condor.submit(
        description = job_three_description,
        count = 1,
    )

    yield job_three_handle

    job_three_handle.remove()


@action
def final_job_three_handle(default_condor, job_three_handle, all_job_handles):
    job_three_handle.wait(
        verbose = True,
        timeout = 180,
        condition = ClusterState.any_held,
        fail_condition = ClusterState.all_complete,
    )

    return job_three_handle


@action
def job_three_events(final_job_three_handle):
    return final_job_three_handle.event_log.events


@action
def job_four_handle(default_condor, test_dir, the_job_description):
    job_four_description = {
        ** the_job_description,
        ** {
            "arguments":    "--slow 6",
            "log":          test_dir / "job_four.log",
            "output":       test_dir / "job_four.out",
            "error":        test_dir / "job_four.err",
       }
    }

    job_four_handle = default_condor.submit(
        description = job_four_description,
        count = 1,
    )

    yield job_four_handle

    job_four_handle.remove()


@action
def final_job_four_handle(default_condor, job_four_handle, all_job_handles):
    job_four_handle.wait(
        verbose = True,
        timeout = 180,
        condition = ClusterState.any_held,
        fail_condition = ClusterState.all_complete,
    )

    return job_four_handle


@action
def job_four_events(final_job_four_handle):
    return final_job_four_handle.event_log.events


@action
def job_five_handle(default_condor, test_dir, the_job_description):
    job_five_description = {
        ** the_job_description,
        ** {
            "arguments":    "--slow 6",
            "log":          test_dir / "job_five.log",
            "output":       test_dir / "job_five.out",
            "error":        test_dir / "job_five.err",
        }
    }

    job_five_handle = default_condor.submit(
        description = job_five_description,
        count = 1,
    )

    yield job_five_handle

    job_five_handle.remove()


@action
def final_job_five_handle(default_condor, job_five_handle):
    # We can determine if the job has checkpointed by waiting for the
    # second of a pair of file transfer events after an execute event.
    # No other events are allowed between the two file transfer events,
    # to prevent us from detecting a restart.

    #
    # This code is awful and super detail-oriented.
    #
    job_started = False
    checkpoint_started = False
    checkpoint_completed = False
    for delay in range(60):
        for event in job_five_handle.event_log.read_events():
            if not job_started:
                if event.type == JobEventType.EXECUTE:
                    logger.debug("Found EXECUTE event")
                    job_started = True
                continue

            if not checkpoint_started:
                if event.type == JobEventType.FILE_TRANSFER:
                    logger.debug("Found first FILE_TRANSFER event")
                    checkpoint_started = True
            else:
                if event.type == JobEventType.IMAGE_SIZE:
                    logger.debug("Found IMAGE_SIZE event")
                    continue
                assert event.type == JobEventType.FILE_TRANSFER
                logger.debug("Found second FILE_TRANSFER event")
                checkpoint_completed = True
                break
        if checkpoint_completed:
            break
        time.sleep(1)
    assert checkpoint_completed

    default_condor.run_command(
        ['condor_vacate_job', str(job_five_handle.job_ids[0])],
        timeout=5, echo=True)
    # See previous comment.
    time.sleep(1)
    default_condor.run_command(
        ['condor_reschedule'],
        timeout=60, echo=True)

    # This actually reads events out of the event_log trace, so it won't
    # miss the events that we just read in the loop above.  That might be
    # problematic in other cases, but not this one...
    job_five_handle.wait(
        verbose = True,
        timeout = 180,
        condition = ClusterState.any_held,
        fail_condition = ClusterState.all_complete,
    )

    return job_five_handle


@action
def job_five_events(final_job_five_handle):
    return final_job_five_handle.event_log.events


@action
def sleep_job_description(path_to_sleep):
    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "executable":                   path_to_sleep,
        "transfer_executable":          "false",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",

        "allowed_execute_duration":       "30",
    }


@action
def path_to_sleep_transfer_plugin(test_dir):
    script="""
    #!/usr/bin/python3

    import classad2 as classad
    import sys
    import time

    PLUGIN_VERSION = '1.0.0'

    def print_help(stream = sys.stderr):
        help_msg = '''Usage: {0} -infile <input-filename> -outfile <output-filename>
           {0} -classad

    Options:
      -classad                    Print a ClassAd containing the capablities of this
                                  file transfer plugin.
      -infile <input-filename>    Input ClassAd file
      -outfile <output-filename>  Output ClassAd file
      -upload                     Indicates this transfer is an upload (default is
                                  download)
    '''
        stream.write(help_msg.format(sys.argv[0]))

    def print_capabilities():
        capabilities = {
             'MultipleFileSupport': True,
             'PluginType': 'FileTransfer',
             'SupportedMethods': 'sleep',
             'Version': PLUGIN_VERSION,
        }
        sys.stdout.write(classad.ClassAd(capabilities).printOld())

    def parse_args():

        # The only argument lists that are acceptable are
        # <this> -classad
        # <this> -infile <input-filename> -outfile <output-filename>
        # <this> -outfile <output-filename> -infile <input-filename>
        if not len(sys.argv) in [2, 5, 6]:
            print_help()
            sys.exit(1)

        # If -classad, print the capabilities of the plugin and exit early
        if (len(sys.argv) == 2) and (sys.argv[1] == '-classad'):
            print_capabilities()
            sys.exit(0)

        # If -upload, set is_upload to True and remove it from the args list
        is_upload = False
        if '-upload' in sys.argv[1:]:
            is_upload = True
            sys.argv.remove('-upload')

        # -infile and -outfile must be in the first and third position
        if not (
                ('-infile' in sys.argv[1:]) and
                ('-outfile' in sys.argv[1:]) and
                (sys.argv[1] in ['-infile', '-outfile']) and
                (sys.argv[3] in ['-infile', '-outfile']) and
                (len(sys.argv) == 5)):
            print_help()
            sys.exit(1)
        infile = None
        outfile = None
        try:
            for i, arg in enumerate(sys.argv):
                if i == 0:
                    continue
                elif arg == '-infile':
                    infile = sys.argv[i+1]
                elif arg == '-outfile':
                    outfile = sys.argv[i+1]
        except IndexError:
            print_help()
            sys.exit(1)

        return {'infile': infile, 'outfile': outfile, 'upload': is_upload}

    def format_error(error):
        return '{0}: {1}'.format(type(error).__name__, str(error))

    def get_error_dict(error, url = ''):
        error_string = format_error(error)
        error_dict = {
            'TransferSuccess': False,
            'TransferError': error_string,
            'TransferUrl': url,
        }

        return error_dict

    class SleepPlugin:

        def parse_url(self, url):
            duration = url[(url.find("://") + 3):]
            return int(duration)

        def download_file(self, url, local_file_path):

            start_time = time.time()

            duration = self.parse_url(url)
            time.sleep(duration)

            end_time = time.time()

            # Get transfer statistics
            transfer_stats = {
                'TransferSuccess': True,
                'TransferProtocol': 'sleep',
                'TransferType': 'upload',
                'TransferFileName': local_file_path,
                'TransferFileBytes': 0,
                'TransferTotalBytes': 0,
                'TransferStartTime': int(start_time),
                'TransferEndTime': int(end_time),
                'ConnectionTimeSeconds': end_time - start_time,
                'TransferUrl': url,
            }

            return transfer_stats

        def upload_file(self, url, local_file_path):

            start_time = time.time()

            duration = self.parse_url(url)
            time.sleep(duration)

            end_time = time.time()

            # Get transfer statistics
            transfer_stats = {
                'TransferSuccess': True,
                'TransferProtocol': 'sleep',
                'TransferType': 'upload',
                'TransferFileName': local_file_path,
                'TransferFileBytes': 0,
                'TransferTotalBytes': 0,
                'TransferStartTime': int(start_time),
                'TransferEndTime': int(end_time),
                'ConnectionTimeSeconds': end_time - start_time,
                'TransferUrl': url,
            }

            return transfer_stats


    if __name__ == '__main__':

        # Start by parsing input arguments
        try:
            args = parse_args()
        except Exception:
            sys.exit(1)

        sleep_plugin = SleepPlugin()

        # Parse in the classads stored in the input file.
        # Each ad represents a single file to be transferred.
        try:
            infile_ads = classad.parseAds(open(args['infile'], 'r'))
        except Exception as err:
            try:
                with open(args['outfile'], 'w') as outfile:
                    outfile_dict = get_error_dict(err)
                    outfile.write(str(classad.ClassAd(outfile_dict)))
            except Exception:
                pass
            sys.exit(1)

        # Now iterate over the list of classads and perform the transfers.
        try:
            with open(args['outfile'], 'w') as outfile:
                for ad in infile_ads:
                    try:
                        if not args['upload']:
                            outfile_dict = sleep_plugin.download_file(ad['Url'], ad['LocalFileName'])
                        else:
                            outfile_dict = sleep_plugin.upload_file(ad['Url'], ad['LocalFileName'])

                        outfile.write(str(classad.ClassAd(outfile_dict)))

                    except Exception as err:
                        try:
                            outfile_dict = get_error_dict(err, url = ad['Url'])
                            outfile.write(str(classad.ClassAd(outfile_dict)))
                        except Exception:
                            pass
                        sys.exit(1)

        except Exception:
            sys.exit(1)
    """

    path = test_dir / "sleep-transfer.py"
    write_file(path, format_script(script))

    return path

@action
def job_six_handle(default_condor, test_dir, sleep_job_description, path_to_sleep_transfer_plugin):
    job_six_description = {
        ** sleep_job_description,
        ** {
            "arguments":        "15",
            "log":              test_dir / "job_six.log",
            "output":           test_dir / "job_six.out",
            "error":            test_dir / "job_six.err",
            "transfer_plugins": f"sleep={path_to_sleep_transfer_plugin.as_posix()}",
            "transfer_input_files": "sleep://30",
        }
    }

    job_six_handle = default_condor.submit(
        description = job_six_description,
        count = 1,
    )

    yield job_six_handle

    job_six_handle.remove()


@action
def final_job_six_handle(default_condor, job_six_handle, all_job_handles):
    job_six_handle.wait(
        verbose = True,
        timeout = 180,
        condition = ClusterState.all_complete,
        fail_condition = ClusterState.any_held,
    )

    return job_six_handle


@action
def job_six_events(final_job_six_handle):
    return final_job_six_handle.event_log.events


@action
def job_seven_handle(default_condor, test_dir, sleep_job_description):
    job_seven_description = {
        ** sleep_job_description,
        ** {
            "arguments":    "45",
            "log":          test_dir / "job_seven.log",
            "output":       test_dir / "job_seven.out",
            "error":        test_dir / "job_seven.err",
        }
    }

    job_seven_handle = default_condor.submit(
        description = job_seven_description,
        count = 1,
    )

    yield job_seven_handle

    job_seven_handle.remove()


@action
def final_job_seven_handle(default_condor, job_seven_handle, all_job_handles):
    job_seven_handle.wait(
        verbose = True,
        timeout = 180,
        condition = ClusterState.any_held,
        fail_condition = ClusterState.all_complete,
    )

    return job_seven_handle


@action
def job_seven_events(final_job_seven_handle):
    return final_job_seven_handle.event_log.events



#
# Utility functions for the tests.
#

def types_in_events(types, events):
    return any(t in [e.type for e in events] for t in types)


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


class TestAllowedExecuteDuration:
    def test_job_one_completed(self, job_one_events):
        assert not types_in_events(
            [JobEventType.JOB_HELD], job_one_events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.FILE_TRANSFER,
                JobEventType.JOB_TERMINATED,
            ],
            job_one_events
        )


    def test_job_two_completed(self, job_two_events):
        assert not types_in_events(
            [JobEventType.JOB_HELD], job_two_events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.FILE_TRANSFER,
                JobEventType.EXECUTE,
                JobEventType.JOB_TERMINATED,
            ],
            job_two_events
        )


    # We can't easily assert the absence of a checkpoint for this job
    # because the event type is used the same used for initial transfer in.
    def test_job_three_held(self, job_three_events):
        assert not types_in_events(
            [JobEventType.JOB_TERMINATED], job_three_events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.JOB_HELD,
            ],
            job_three_events
        )


    def test_job_four_held(self, job_four_events):
        assert not types_in_events(
            [JobEventType.JOB_TERMINATED], job_four_events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.FILE_TRANSFER,
                JobEventType.JOB_HELD,
            ],
            job_four_events
        )


    def test_job_five_held(self, job_five_events):
        assert not types_in_events(
            [JobEventType.JOB_TERMINATED], job_five_events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.FILE_TRANSFER,
                JobEventType.EXECUTE,
                JobEventType.JOB_HELD,
            ],
            job_five_events
        )


    def test_job_six_completed(self, job_six_events):
        assert not types_in_events(
            [JobEventType.JOB_HELD], job_six_events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.JOB_TERMINATED,
            ],
            job_six_events
        )


    def test_job_seven_held(self, job_seven_events):
        assert not types_in_events(
            [JobEventType.JOB_TERMINATED], job_seven_events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.JOB_HELD,
            ],
            job_seven_events
        )
