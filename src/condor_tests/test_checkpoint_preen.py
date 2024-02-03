#!/usr/bin/env pytest

import os
import time
import pytest
import subprocess
from pathlib import Path

from ornithology import (
    action,
    write_file,
    format_script,
    ClusterState,
    Condor,
)

import htcondor2 as htcondor
from htcondor2 import (
    JobEventType,
    FileTransferEventType,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

#
# Largely copied from test_checkpoint_destination, for what I hope are
# obvious reasons.
#

@action
def the_condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            'NUM_CPUS': 40,
            'ENABLE_URL_TRANSFERS': 'TRUE',
            'LOCAL_CLEANUP_PLUGIN': '/bin/false',
            'MAX_CHECKPOINT_CLEANUP_PROCS': 2,
        }
    ) as the_condor:
        yield the_condor


@action
def plugin_log_file(test_dir):
    return test_dir / "plugin.log"


@action
def plugin_shell_file(test_dir, plugin_python_file, plugin_log_file):
    plugin_shell_file = test_dir / "local.sh"
    contents = format_script(
    f"""
        #!/bin/bash
        exec {plugin_python_file} "$@" 2>&1 >> {plugin_log_file}
    """
    )
    write_file(plugin_shell_file, contents)
    return plugin_shell_file


@action
def plugin_python_file(test_dir):
    plugin_python_file = test_dir / "local.py"
    contents = format_script(
    f"""
        #!/usr/bin/python3

        import os
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

        test_dir = '{test_dir.as_posix()}'

""" + """

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
                remote_file_path = remote_file_path.replace( "example.vo/example.fs", test_dir )

                # print(f"DEBUG: {start_time} download {remote_file_path} -> {local_file_path}")
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
                remote_file_path = remote_file_path.replace( "example.vo/example.fs", test_dir )

                # print(f"DEBUG: {start_time} upload {remote_file_path} <- {local_file_path}")
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

            if not args['upload']:
                mutator = Path('mutate_checkpoint.py')
                if mutator.exists():
                    os.system(f"./{mutator}")
    """
    )
    write_file(plugin_python_file, contents)
    return plugin_python_file


@action
def path_to_symlink_script(test_dir):
    script = f"""
    #!/usr/bin/python3

    import os
    import sys
    from pathlib import Path

    d = Path( "d" )
    d.mkdir(parents=True, exist_ok=True)
    (d / "a").write_text("a")
    Path( "s" ).symlink_to( d )
    sys.exit(17)
    """

    path = test_dir / "symlink.py"
    write_file(path, format_script(script))

    return path


@action
def path_to_the_job_script(the_condor, test_dir):
    with the_condor.use_config():
        condor_libexec = htcondor.param["LIBEXEC"]
        condor_bin = htcondor.param["BIN"]

    # This script can't have embedded newlines for stupid reasons.
    script = f"""
    #!/usr/bin/python3

    import os
    import sys
    import time
    import getopt
    import subprocess

    from pathlib import Path

    os.environ['CONDOR_CONFIG'] = '{the_condor.config_file}'
    posix_test_dir = '{test_dir.as_posix()}'
    condor_libexec = '{condor_libexec}'
    condor_bin = '{condor_bin}'
""" + """
    os.environ['PATH'] = condor_libexec + os.pathsep + os.environ.get('PATH', '') + os.pathsep + condor_bin

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

    my_checkpoint_number = -1
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
    #
    # The shadow updates the schedd on a timer (and not when the starter
    # sends an update), so there's a delay between the starter incrementing
    # the checkpoint number (before it restarts the job) and the update
    # becoming visible in the schedd.  By default, we test with that interval
    # set to two seconds, so sleep for three to make sure it passes before
    # we try again.
    for i in range(1,3):
        the_checkpoint_number = -1
        rv = subprocess.run(
            ["condor_q", sys.argv[1], "-af", "CheckpointNumber"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=20
        )
        if rv.returncode == 0 and b"undefined" not in rv.stdout:
            the_checkpoint_number = int(rv.stdout.strip())
        if the_checkpoint_number == my_checkpoint_number:
            break
        else:
            time.sleep(3)
    print(f"Found the checkpoint number {the_checkpoint_number}")

    # Dump the sandbox contents (when resuming after the first checkpoint)
    # for later inspection.
    if the_checkpoint_number == 0:
        # This is a hack.
        target = Path(posix_test_dir) / sys.argv[1]
        os.system(f'/bin/cp -a . {target.as_posix()} 2>/dev/null')

    total_steps = 14
    num_completed_steps = 0
    try:
        with open("saved-state", "r") as saved_state:
            num_completed_steps = int(saved_state.readline().strip())
    except IOError:
        pass

    while num_completed_steps < total_steps:
        print(f"Starting step {num_completed_steps}.")

        time.sleep(1)
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
        "want_io_proxy":                "True",

        "checkpoint_exit_code":         "17",
        "transfer_checkpoint_files":    "saved-state",
        "transfer_output_files":        "",
    }


# This construction allows us to run test jobs concurrently.  It's clumsy,
# in that the test cases should be directly parameterizing a fixture, but
# there's no native support for concurrency in pytest, so this will do.
TEST_CASES = {
    "spool": {},
    "local": {
        "checkpoint_destination":       'local://example.vo/example.fs',
        "transfer_plugins":             'local={plugin_shell_file}',
    },
    # See HTCONDOR-1220 and -1221 for some of the bugs found while hand-
    # checking the results.  For now, we're just going to ignore the
    # output_destination -related problems and only check to make sure
    # that output_destination doesn't screw up checkpoint_destination.
    "with_output_destination": {
        "checkpoint_destination":       'local://example.vo/example.fs',
        "transfer_plugins":             'local={plugin_shell_file}',
        "output_destination":           'local://example.vo/example.fs/od/',

        # Absolute paths and output_destination make no sense together,
        # so put these logs where the test logic expects to find them.
        "output":                       "test_job_$(CLUSTER).out",
        "error":                        "test_job_$(CLUSTER).err",
    },
    "check_files_local": {
        "checkpoint_destination":       'local://example.vo/example.fs',
        "transfer_plugins":             'local={plugin_shell_file}',
        "transfer_input_files":         '{test_dir}/check_files_local/',
        "transfer_checkpoint_files":    'saved-state, a, b, c, d, e',
    },
    # To get preserve_relative_paths to work right, we need `iwd` to be
    # the directory where we created the input tree.  But if we leave
    # `transfer_output_files` unset, we get all the changes back in that
    # directory... including the last checkpoint, which is NOT the one
    # we're comparing against.  So only transfer the files we know won't
    # change to the output directory.
    "HTCONDOR_1218_1_spool": {
        "iwd":                          '{test_dir}/HTCONDOR_1218_1_spool',
        "transfer_output_files":        "d1",

        "transfer_input_files":         'd1/d2',
        "transfer_checkpoint_files":    'saved-state, d1/d2/a',
        "preserve_relative_paths":      'true',
    },
    "HTCONDOR_1218_2_spool": {
        "iwd":                          '{test_dir}/HTCONDOR_1218_2_spool',
        "transfer_output_files":        "d1",

        "transfer_input_files":         'd1',
        "transfer_checkpoint_files":    'saved-state, d1/a',
        "preserve_relative_paths":      'true',
    },
    "HTCONDOR_1218_1": {
        "iwd":                          '{test_dir}/HTCONDOR_1218_1',
        "transfer_output_files":        "d1",

        "checkpoint_destination":       'local://example.vo/example.fs',
        "transfer_plugins":             'local={plugin_shell_file}',
        "transfer_input_files":         'd1/d2',
        "transfer_checkpoint_files":    'saved-state, d1/d2/a',
        "preserve_relative_paths":      'true',
    },
    "HTCONDOR_1218_2": {
        "iwd":                          '{test_dir}/HTCONDOR_1218_2',
        "transfer_output_files":        "d1",

        "checkpoint_destination":       'local://example.vo/example.fs',
        "transfer_plugins":             'local={plugin_shell_file}',
        "transfer_input_files":         'd1',
        "transfer_checkpoint_files":    'saved-state, d1/a',
        "preserve_relative_paths":      'true',
    },
}

@action
def the_job_handles(test_dir, the_condor, the_job_description, plugin_shell_file):
    job_handles = {}
    for name, test_case in TEST_CASES.items():
        test_case = {key: value.format(
            test_dir=test_dir,
            plugin_shell_file=plugin_shell_file,
        ) for key, value in test_case.items()}

        # This is clumsy, but less clumsy that duplicating the basic
        # functionality tests for more-complicated checkpoint structures.
        input_path = test_dir / name
        input_path.mkdir(parents=True, exist_ok=True)

        count = 1
        if "spool" not in name:
            count = 5

        # If I were feeling clever, I'd rewrite TEST_CASES to include these
        # sequences as lambdas, or maybe just function references.
        if name == "check_files_local":
            ( input_path / "a" ).write_text("a")
            ( input_path / "b" ).write_text("b")
            ( input_path / "c" ).write_text("c")
            ( input_path / "d" ).mkdir(parents=True, exist_ok=True)
            ( input_path / "d" / "a" ).write_text("aa")
            ( input_path / "d" / "b" ).mkdir(parents=True, exist_ok=True)
            ( input_path / "d" / "b" / "a" ).mkdir(parents=True, exist_ok=True)
            ( input_path / "d" / "b" / "a" / "a" ).write_text("aaa")
            ( input_path / "d" / "b" / "b" ).write_text("bb")
            ( input_path / "d" / "b" / "c" ).write_text("cc")
            ( input_path / "d" / "c" ).write_text("ccc")
            ( input_path / "d" / "d" ).mkdir(parents=True, exist_ok=True)
            ( input_path / "d" / "d" / "a" ).write_text("aaaa")
            ( input_path / "d" / "d" / "b" ).write_text("bbb")
            ( input_path / "e" ).mkdir(parents=True, exist_ok=True)
            ( input_path / "e" / "a" ).write_text("aaaaa")
            ( input_path / "e" / "b" ).write_text("bbbb")
            ( input_path / "e" / "c" ).mkdir(parents=True, exist_ok=True)
            ( input_path / "e" / "d" ).symlink_to(( input_path / "c" ))
        elif name.startswith("HTCONDOR_1218_1"):
            ( input_path / "d1" ).mkdir(parents=True, exist_ok=True)
            ( input_path / "d1" / "d2" ).mkdir(parents=True, exist_ok=True)
            ( input_path / "d1" / "d2" / "a" ).write_text("a")
            ( input_path / "d1" / "d2" / "b" ).write_text("b")
        elif name.startswith("HTCONDOR_1218_2"):
            ( input_path / "d1" ).mkdir(parents=True, exist_ok=True)
            ( input_path / "d1" / "a" ).write_text("a")
            ( input_path / "d1" / "b" ).write_text("b")

        complete_job_description = {
            ** the_job_description,
            ** test_case,
        }
        job_handle = the_condor.submit(
            description=complete_job_description,
            count=count,
        )

        job_handles[name] = job_handle

    yield job_handles


@action(params={name: name for name in TEST_CASES})
def the_job_pair(request, the_job_handles):
    return (request.param, the_job_handles[request.param])


@action
def the_job_name(the_job_pair):
    return the_job_pair[0]


@action
def the_job_handle(the_job_pair):
    return the_job_pair[1]


@action
def the_completed_job(the_condor, the_job_handle):
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

    the_condor.run_command(['condor_reschedule'])

    the_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    return the_job_handle


@action
def the_removed_job(the_condor, the_completed_job):
    the_condor.run_command(['condor_qedit', the_completed_job.clusterid, 'LeaveJobInQueue', 'False'])
    the_completed_job.remove()

    while True:
        time.sleep(1)
        history_iter = the_condor.get_local_schedd().history(f"ClusterID == {the_completed_job.clusterid}", ["ClusterID"])
        if sum(1 for _ in history_iter) > 0:
            # Job has hit the history file, now we can continue
            return the_completed_job
        # but assert if the job is held, so we don't go into an infinite loop 
        assert not the_completed_job.state.any_held()
    return False


class TestCheckpointDestination:

    def test_checkpoint_removed(self,
      test_dir, the_condor,
      the_job_name, the_removed_job
    ):
        schedd = the_condor.get_local_schedd()
        constraint = f'ClusterID == {the_removed_job.clusterid}'

        # Make sure the job has left the queue.
        result = schedd.query(
            constraint=constraint,
            projection=["CheckpointDestination", "GlobalJobID"],
        )
        assert(len(result) == 0)


        with the_condor.use_config():
            SPOOL = htcondor.param["SPOOL"]

            local_dir = Path(htcondor.param["LOCAL_DIR"])
            (local_dir / "checkpoint-destination-mapfile").write_text(
                f"*   local://example.vo/example.fs   cleanup_locally_mounted_checkpoint,-prefix,\\0,-path,{test_dir.as_posix()}\n"
            )

            preen_env = {
                ** os.environ,
                '_CONDOR_SPOOL': SPOOL,
                '_CONDOR_TOOL_DEBUG': 'D_TEST D_SUB_SECOND D_CATEGORY',
            }
            try:
                rv = subprocess.run( ['condor_preen', '-d'],
                    # Crass empiricism.
                    env=preen_env, timeout=120,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    universal_newlines=True,
                )
            except subprocess.TimeoutExpired as te:
                # WTAF is going on here?
                stdout = str(te.stdout).replace( "\\n", "\n" )
                if stdout.startswith("b'"):
                    stdout = stdout[2:-1]
                stdout = "\n" + stdout
                logger.debug(stdout)
                raise te
        logger.debug(rv.stdout)
        assert(rv.returncode == 0)


        # Get the CheckpointDestination and GlobalJobID from the history file.
        result = schedd.history(
            constraint=constraint,
            projection=["ProcId", "CheckpointDestination", "GlobalJobID", "CheckpointNumber"],
        )
        results = [r for r in result]
        if "spool" in the_job_name:
            assert(len(results) == 1)
        else:
            assert(len(results) == 5)

        for jobAd in results:
            checkpointDestination = jobAd.get("CheckpointDestination")
            proc = jobAd["ProcId"]

            # Did we clean up the SPOOL directory?
            spoolDirectory = Path(SPOOL) / str(the_removed_job.clusterid)
            assert(not spoolDirectory.exists())

            if not checkpointDestination is None:
                globalJobID = jobAd["GlobalJobID"].replace('#', '_')
                prefix = test_dir / globalJobID

                checkpointNumber = int(jobAd["CheckpointNumber"])
                for i in range(0, checkpointNumber):
                    path = prefix / f"{i:04}"

                    # Did we remove the checkpoint destination?
                    if path.exists():
                        # Crass empiricism.
                        time.sleep(20)
                    assert(not path.exists())

                    # Did we remove the manifest file?
                    manifest_file = test_dir / "condor" / "spool" / "checkpoint-cleanup" / f"cluster{the_removed_job.clusterid}.proc{proc}.subproc0" / f"_condor_checkpoint_MANIFEST.{checkpointNumber}"
                    assert(not manifest_file.exists())

                # Once we've removed all of the manifest files, we should also
                # remove the directory we used to store them.
                checkpoint_cleanup_subdir = test_dir / "condor" / "spool" / "checkpoint-cleanup" / f"cluster{the_removed_job.clusterid}.proc{proc}.subproc0"
                if checkpoint_cleanup_subdir.exists():
                    # Crass empiricism.
                    time.sleep(20)
                assert(not checkpoint_cleanup_subdir.exists())
