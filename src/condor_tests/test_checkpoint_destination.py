#!/usr/bin/env pytest

import os
import time

from ornithology import (
    action,
    write_file,
    format_script,
    ClusterState,
)

from htcondor import (
    JobEventType,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def plugin_log_file(test_dir):
    return test_dir / "plugin.log"


@action
def plugin_shell_file(test_dir, plugin_python_file, plugin_log_file):
    plugin_shell_file = test_dir / "local.sh"
    contents = format_script(
    f"""
        #!/bin/bash
        exec {plugin_python_file} $@ 2>&1 >> {plugin_log_file}
    """
    )
    write_file(plugin_shell_file, contents)
    return plugin_shell_file


@action
def plugin_python_file(test_dir):
    plugin_python_file = test_dir / "local.py"
    contents = format_script(
    """
        #!/usr/bin/python3

        import sys
        import time
        import shutil

        from urllib.parse import urlparse
        from pathlib import Path

        import classad

        DEFAULT_TIMEOUT = 30
        PLUGIN_VERSION = '1.0.0'

        EXIT_SUCCESS = 0
        EXIT_FAILURE = 1
        EXIT_AUTHENTICATION_REFRESH = 2


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
                 'SupportedMethods': 'local',
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
                sys.exit(EXIT_FAILURE)

            # If -classad, print the capabilities of the plugin and exit early
            if (len(sys.argv) == 2) and (sys.argv[1] == '-classad'):
                print_capabilities()
                sys.exit(EXIT_SUCCESS)

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
                sys.exit(EXIT_FAILURE)

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

        class LocalPlugin:

            # Extract whatever information we want from the url provided.
            # In this example, convert the example://path/to/file url to a
            # path in the file system (ie. /path/to/file)
            def parse_url(self, url):
                url_path = url[(url.find("://") + 3):]
                return url_path

            def download_file(self, url, local_file_path):
                start_time = time.time()

                remote_file_path = self.parse_url(url)
                print(f"DEBUG: {start_time} download {remote_file_path} -> {local_file_path}")
                file_size = Path(remote_file_path).stat().st_size
                Path(local_file_path).parent.mkdir(parents=True,exist_ok=True)
                shutil.copy(remote_file_path, local_file_path)

                end_time = time.time()

                # Get transfer statistics
                transfer_stats = {
                    'TransferSuccess': True,
                    'TransferProtocol': 'example',
                    'TransferType': 'upload',
                    'TransferFileName': local_file_path,
                    'TransferFileBytes': file_size,
                    'TransferTotalBytes': file_size,
                    'TransferStartTime': int(start_time),
                    'TransferEndTime': int(end_time),
                    'ConnectionTimeSeconds': end_time - start_time,
                    'TransferUrl': url,
                }

                return transfer_stats

            def upload_file(self, url, local_file_path):

                start_time = time.time()

                remote_file_path = self.parse_url(url)
                print(f"DEBUG: {start_time} upload {remote_file_path} <- {local_file_path}")
                file_size = Path(local_file_path).stat().st_size
                Path(remote_file_path).parent.mkdir(parents=True,exist_ok=True)
                shutil.copy(local_file_path, remote_file_path)

                end_time = time.time()

                # Get transfer statistics
                transfer_stats = {
                    'TransferSuccess': True,
                    'TransferProtocol': 'example',
                    'TransferType': 'upload',
                    'TransferFileName': local_file_path,
                    'TransferFileBytes': file_size,
                    'TransferTotalBytes': file_size,
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
                sys.exit(EXIT_FAILURE)

            local_plugin = LocalPlugin()

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
                sys.exit(EXIT_FAILURE)

            # Now iterate over the list of classads and perform the transfers.
            try:
                with open(args['outfile'], 'w') as outfile:
                    for ad in infile_ads:
                        try:
                            if not args['upload']:
                                outfile_dict = local_plugin.download_file(ad['Url'], ad['LocalFileName'])
                            else:
                                outfile_dict = local_plugin.upload_file(ad['Url'], ad['LocalFileName'])

                            outfile.write(str(classad.ClassAd(outfile_dict)))

                        except Exception as err:
                            try:
                                outfile_dict = get_error_dict(err, url = ad['Url'])
                                outfile.write(str(classad.ClassAd(outfile_dict)))
                            except Exception:
                                pass
                            sys.exit(EXIT_FAILURE)

            except Exception:
                sys.exit(EXIT_FAILURE)
    """
    )
    write_file(plugin_python_file, contents)
    return plugin_python_file


@action
def path_to_the_job_script(default_condor, test_dir):
    # This script can't have embedded newlines for stupid reasons.
    script = f"""
    #!/usr/bin/python3

    import os
    import sys
    import time
    import getopt
    import subprocess

    from pathlib import Path

    os.environ['CONDOR_CONFIG'] = '{default_condor.config_file}'
""" + """
    condor_libexec = Path(os.environ['_CONDOR_BIN']) / ".." / "libexec"
    os.environ['PATH'] = condor_libexec.as_posix() + os.pathsep + os.environ.get('PATH', '') + os.pathsep + os.environ['_CONDOR_BIN']

    epoch_number = 0
    rv = subprocess.run(
        ["condor_chirp", "get_job_attr", "epoch_name"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20
    )
    if rv.returncode == 0 and b"UNDEFINED" not in rv.stdout:
        print("Found epoch number in the job ad at startup.")
        epoch_number = int(rv.stdout.strip())
    print(f"Starting epoch number {epoch_number}.")

    my_checkpoint_number = 0
    rv = subprocess.run(
        ["condor_chirp", "get_job_attr", "my_checkpoint_number"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20
    )
    if rv.returncode == 0 and b"UNDEFINED" not in rv.stdout:
        print("Found my checkpoint number in the job ad at startup.")
        my_checkpoint_number = int(rv.stdout.strip())
    print(f"Starting from my checkpoint number {my_checkpoint_number}")

    # Oddly enough, `condor_chirp` gets the wrong answer here.
    the_checkpoint_number = 0
    rv = subprocess.run(
        ["condor_q", sys.argv[1], "-af", "CheckpointNumber"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20
    )
    if rv.returncode == 0 and b"undefined" not in rv.stdout:
        the_checkpoint_number = int(rv.stdout.strip())
    print(f"Found the checkpoint number {the_checkpoint_number}")

    total_steps = 14
    num_completed_steps = 0
    try:
        with open("saved-state", "r") as saved_state:
            num_completed_steps = int(saved_state.readline().strip())
    except IOError:
        pass

    while num_completed_steps < total_steps:
        print(f"Starting step {num_completed_steps}.")

        time.sleep(3)
        num_completed_steps += 1

        if num_completed_steps == 7 and epoch_number == 0:
                epoch_number += 1
                rv = subprocess.run(
                    ["condor_chirp", "set_job_attr", "epoch_name", str(epoch_number)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=20
                )
                if rv.returncode != 0:
                    print(f'Failed to set epoch number before vacating:')
                    print(f'{rv.stdout}')
                    print(f'{rv.stderr}')

                rv = subprocess.run(
                    ["condor_vacate_job", sys.argv[1]],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=20
                )
                if rv.returncode != 0:
                    print(f'Failed to vacate myself after second checkpoint:')
                    print(f'{rv.stdout}')
                    print(f'{rv.stderr}')

                # Don't exit before we've been vacated.
                time.sleep(60)

                # Signal a failure if we weren't vacated.
                print("Failed to evict, aborting.")
                sys.exit(1)

        if num_completed_steps % 5 == 0:
            print(f"Checkpointing after {num_completed_steps}.")
            try:
                with open("saved-state", "w") as saved_state:
                    saved_state.write(f'{num_completed_steps}')

                my_checkpoint_number += 1
                rv = subprocess.run(
                    ["condor_chirp", "set_job_attr", "my_checkpoint_number", str(my_checkpoint_number)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=20
                )
                if rv.returncode != 0:
                    print(f'Failed to set checkpoint number before vacating:')
                    print(f'{rv.stdout}')
                    print(f'{rv.stderr}')

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
def the_job_description(test_dir, path_to_the_job_script):
    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "executable":                   path_to_the_job_script.as_posix(),
        "arguments":                    "$(CLUSTER).$(PROCESS)",
        "transfer_executable":          "true",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",

        "log":                          test_dir / "test_job_$(CLUSTER).log",
        "output":                       test_dir / "test_job_$(CLUSTER).out",
        "error":                        test_dir / "test_job_$(CLUSTER).err",
        "+WantIOProxy":                 "True",

        "checkpoint_exit_code":         "17",
        "transfer_checkpoint_files":    "saved-state",
        "transfer_output_files":        "",
    }


@action(params={
    "local": {
        "+CheckpointDestination":       '"local://{test_dir}/"',
        "transfer_plugins":             'local={plugin_shell_file}',
    },
    "spool": {},
})
def the_job_handle(request, test_dir, default_condor, the_job_description, plugin_shell_file):
    test_case = request.param
    test_case = {key: value.format(test_dir=test_dir, plugin_shell_file=plugin_shell_file) for key, value in test_case.items()}

    complete_job_description = {
        ** the_job_description,
        ** test_case,
    }
    the_job_handle = default_condor.submit(
        description=complete_job_description,
        count=1,
    )
    yield the_job_handle

    the_job_handle.remove()


@action
def the_completed_job(default_condor, the_job_handle):
    # The job will evict itself part of the way through.  To avoid a long
    # delay waiting for the schedd to reschedule it, call condor_reschedule.
    the_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_held,
    )

    the_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_idle,
        fail_condition=ClusterState.any_held,
    )

    default_condor.run_command(['condor_reschedule'])

    the_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    return the_job_handle


@action
def the_completed_job_stdout(test_dir, the_completed_job):
    cluster = the_completed_job.clusterid
    with open(test_dir / f"test_job_{cluster}.out" ) as output:
        return output.readlines()

#
# Assertion functions for the tests.  From test_allowed_execute_duration.py,
# which means we should probably drop them in Ornithology somewhere.
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


class TestCheckpointDestination:

    #
    # Test that the self-checkpointing job resumes from its checkpoint
    # after being vacated and rescheduled.  The test cases will vary by
    # the complexity of the checkpoint and if there were checkpointed
    # to SPOOL via CEDAR file transfer or to a URL via a plug-in.
    #
    # The test job will check for and write down an epoch number every
    # time it starts up, and update that epoch number before each time
    # that it vacates itself.  This allows us to validate that the job
    # wrote its output from two different executons, which means we can
    # tell if the resume actually and correctly happened without depending
    # on time information (because we'll see the correct output sequence
    # with a changing epoch number).
    #
    # Because we test the whole output here, we also implicitly check
    # that HTCondor's checkpoint number is equal to the number of times
    # the job thinks it has checkpointed.
    #
    def test_resume(self, the_completed_job, the_completed_job_stdout):
        # Did the job proceed as expected?

        assert not types_in_events(
            [JobEventType.JOB_HELD], the_completed_job.event_log.events
        )

        assert event_types_in_order(
            [
                JobEventType.SUBMIT,
                JobEventType.EXECUTE,
                JobEventType.FILE_TRANSFER,
                JobEventType.EXECUTE,
                JobEventType.FILE_TRANSFER,
                JobEventType.JOB_TERMINATED,
            ],
            the_completed_job.event_log.events
        )

        # Did the job resume and complete correctly?
        expected_stdout = [
            "Starting epoch number 0.\n",
            "Starting from my checkpoint number 0\n",
            "Found the checkpoint number 0\n",
            "Starting step 0.\n",
            "Starting step 1.\n",
            "Starting step 2.\n",
            "Starting step 3.\n",
            "Starting step 4.\n",
            "Checkpointing after 5.\n",
            "Found epoch number in the job ad at startup.\n",
            "Starting epoch number 1.\n",
            "Found my checkpoint number in the job ad at startup.\n",
            "Starting from my checkpoint number 1\n",
            "Found the checkpoint number 1\n",
            "Starting step 5.\n",
            "Starting step 6.\n",
            "Starting step 7.\n",
            "Starting step 8.\n",
            "Starting step 9.\n",
            "Checkpointing after 10.\n",
            "Found epoch number in the job ad at startup.\n",
            "Starting epoch number 1.\n",
            "Found my checkpoint number in the job ad at startup.\n",
            "Starting from my checkpoint number 2\n",
            "Found the checkpoint number 2\n",
            "Starting step 10.\n",
            "Starting step 11.\n",
            "Starting step 12.\n",
            "Starting step 13.\n",
            "Completed all 14 steps.\n"
        ]
        assert expected_stdout == the_completed_job_stdout
