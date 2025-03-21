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
    Condor,
    ClusterState,
)

import htcondor2 as htcondor
import classad2 as classad

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

#
# If a plug-in fails partway through uploading a checkpoint, it doesn't
# necessarily delete the partial upload, so HTCondor has to track all
# upload attempts, and delete those which fail.
#
# We therefore need to test:
#   1.  That the shadow cleans up after an upload failure.
#   2.  That condor_preen cleans up after an upload failure, in case the
#       the shadow didn't try to clean up (because it failed) or because
#       its attempt failed for some other reason.
#   3.  That deleting the failed attemt does not delete any other attempt.
#   4.  That the starter does not attempt to download any failed
#       checkpoint.
#   5.  That a failure to checkpoint does not increase CommittedTime.
#
# We assume that if the starter downloads only succesful checkpoints that
# the job resumes appropriately; this is explicitly test elsewhere.
#
# The upload failure cases should specifically include:
#   a.  The first attempt failing, then the rest succeeding.
#   b.  The second attempt failing, and the rest succeeding.
#   c.  Alternate attempts succeeding and failing.
#   d.  All attempts failing.  (All attempts succeeding is checked
#       in other tests.)
#   e.  Alternate attempts suceeding and failing in pairs.
#
# Test (1) and (2) are mutually exclusive, but (3), (4), and (5) should
# be checked in both cases.  However, since (1) and (2) are mutually
# exclusive, we can just examine the state of the disk after the job
# completes to determine if they did the right thing.
#
# It would be easiest to implement (1) and (2) as different HTCondor
# configurations, implying that test cases (a) - (e) could otherwise
# be shared.  (They should be executed concurrently, unless (3) - (5)
# imply otherwise.)  In both (1) and (2), it would be easiest to
# submit the job with `on_exit_remove = false`, so that the schedd
# doesn't try to clean up when the job exits the queue.  (It should
# be possible to configure the schedd to run a useless clean-up
# script instead, but it seems easier to not run it at all.)
#
#   * Let's ignore (3) for now; it's wrong to delete _all_ failed checkpoints
#     only for performance reasons.  If that becomes a problem, we can
#     write a test static test for this later (that is, construct a fake set
#     of MANIFEST and FAILURE files in the right place, run the clean-up
#     script and see what's left).
#   * We should be able to check (4) after the fact by looking at the
#     transfer log.  (The URLs will all be distinct.)
#     Test (4) doesn't actually make sense if we've passed test (1),
#     so we could skip it in the shadow tests, but it won't hurt to
#     double-check, and if (1) fails but (4) succeeds, that probably
#     means we have an interesting problem....
#   * We should be able to check (5) by looking at the epoch log, because
#     all jobs evict themselves after their first upload failure.  For
#     simplicity, we'll only look at the test cases whose first checkpoint
#     fails, because then we know the CommittedTime should be 0.
#

TEST_CONFIGS = {
    "shadow": {
        "config": {
            # Prevent condor_preen from running.
            'PREEN':                    '/bin/false',
        },
        "script":                       'cleanup_locally_mounted_checkpoint',
    },
    "preen": {
        "config": {
        },
        "script":                       '/bin/false',
    },
}


# For simplicity, (a)-(d) could all attempt four checkpoints.  The lists would
# have to be updated, but we could also attempt eight checkpoints in all cases,
# if that would easier.
ONE_TEST_CASES = {
    "five": {
        "expected_checkpoints":     ["0000", "0001", "0002", "0004"],
        "checkpoint_count":         5,
    },
}

TEST_CASES = {
    "a": {
        "expected_checkpoints":     ["0001", "0002"],
        "checkpoint_count":         3,
    },
    "b": {
        "expected_checkpoints":     ["0000", "0002"],
        "checkpoint_count":         3,
    },
    "c1": {
        "expected_checkpoints":     ["0001", "0003"],
        "checkpoint_count":         4,
    },
    "c2": {
        "expected_checkpoints":     ["0000", "0002"],
        "checkpoint_count":         4,
    },
    "d": {
        "expected_checkpoints":     [],
        "checkpoint_count":         3,
    },
    "e1": {
        "expected_checkpoints":     ["0000", "0001", "0004", "0005"],
        "checkpoint_count":         6,
    },
    "e2": {
        "expected_checkpoints":     ["0000", "0001", "0004", "0005"],
        "checkpoint_count":         8,
    },
}


@action
def path_to_the_job_script(test_dir, the_condor):
    with the_condor.use_config():
        condor_libexec = htcondor.param["LIBEXEC"]
        condor_bin = htcondor.param["BIN"]

    script = f"""
    #!/usr/bin/python3

    import os
    import sys
    import time
    import subprocess
    from pathlib import Path

    os.environ['CONDOR_CONFIG'] = '{the_condor.config_file}'
    posix_test_dir = '{test_dir.as_posix()}'
    condor_libexec = '{condor_libexec}'
    condor_bin = '{condor_bin}'
    """ + """
    os.environ['PATH'] = condor_libexec + os.pathsep + os.environ.get('PATH', '') + os.pathsep + condor_bin

    count = 0

    print("Starting up...")
    if Path("saved-state-dir/FAILURE").exists():
        print("Found FAILURE file")

    try:
        with open("saved-state-dir/count", "r") as saved_state:
            count = int(saved_state.readline().strip())
            print(f"Starting from checkpoint at {count}.")
    except IOError:
        pass

    count +=1
    time.sleep(1)

    jobID = sys.argv[1]
    expected_checkpoints = sys.argv[2]
    num_checkpoints = int(sys.argv[3])

    if count > num_checkpoints:
        print("Completed counting.")
        sys.exit(0)

    path = Path("saved-state-dir")
    path.mkdir(exist_ok=True)

    with open(path / "count", "w") as saved_state:
        saved_state.write(f'{count}')

    fail_path = path / "FAILURE"
    checkpoint_number = count - 1
    the_count = f"{checkpoint_number:04}"
    if the_count in expected_checkpoints:
        # Sigh.  Python >= 3.8 only.
        # fail_path.unlink(missing_ok=True)
        try:
            fail_path.unlink()
        except FileNotFoundError:
            pass
    else:
        fail_path.touch()

    # Evict ourselves once just before the end.
    rv = subprocess.run(
        ["condor_q", "-jobads", os.environ['_CONDOR_JOB_AD'], "-af", "NumJobStarts"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20
    )
    if rv.returncode != 0:
        print(f'Failed to find NumJobStarts in .job.ad')
        print(f'{rv.stdout}')
        print(f'{rv.stderr}')
        sys.exit(1)
    epoch = int(rv.stdout.decode('utf-8').strip())

    print(f"epoch {epoch}")
    should_evict = False
    if epoch == 0 and not the_count in expected_checkpoints:
        should_evict = True
    the_previous_count = f"{(checkpoint_number-1):04}"
    if epoch == 1 and the_previous_count in expected_checkpoints:
        should_evict = True

    if should_evict:
        print("evicting myself...", flush=True)
        rv = subprocess.run(
            ["condor_vacate_job", jobID],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=20
        )
        if rv.returncode != 0:
            print(f'Failed to vacate myself after second checkpoint:')
            print(f'{rv.stdout}')
            print(f'{rv.stderr}')
            sys.exit(1)

        # Don't exit before we've been vacated.
        time.sleep(60)

        # Signal a failure if we weren't vacated.
        print("Failed to evict, aborting.")
        sys.exit(1)

    print("... checkpointing")
    sys.exit(85)
    """

    path = test_dir / "job_script.py"
    write_file(path, format_script(script))

    return path


@action(params={name: name for name in TEST_CONFIGS})
def the_config_tuple(request):
    return (request.param, TEST_CONFIGS[request.param])


@action
def the_config_name(the_config_tuple):
    return the_config_tuple[0]


@action
def the_config(the_config_tuple):
    return the_config_tuple[1]


@action
def the_condor(test_dir, the_config, the_config_name):
    local_dir = test_dir / f"condor.{the_config_name}"

    with Condor(
        local_dir=local_dir,
        config={
            'ENABLE_URL_TRANSFERS':             'TRUE',
            'JOB_EPOCH_HISTORY':                '$(LOG)/EpochLog',
            'SCHEDD_INTERVAL':                  '2',
            'SHADOW.FILE_TRANSFER_STATS_LOG':   '$(LOG)/shadow_transfer_history',
            'SHADOW_DEBUG':                     'D_CATEGORY D_SUBSECOND D_STATUS D_TEST',
            ** the_config['config'],
        }
    ) as the_condor:
        (local_dir / "checkpoint-destination-mapfile").write_text(
            f"*   local://example.vo/example.fs   {the_config['script']},-prefix,\\0,-path,{test_dir.as_posix()}\n"
        )
        yield the_condor


@action
def the_job_description(test_dir, the_config_name, path_to_the_job_script, path_to_the_plugin):
    return {
        # Setting this means that all terminal job states leave the job
        # in the queue until it's removed, which is convenient.
        "LeaveJobInQueue":              "true",

        "executable":                   path_to_the_job_script.as_posix(),
        "transfer_executable":          "true",
        "should_transfer_files":        "true",
        "when_to_transfer_output":      "ON_EXIT",
        "stream_output":                "true",
        "stream_error":                 "true",

        "log":                          test_dir / f"{the_config_name}_test_job_$(CLUSTER).log",
        "output":                       test_dir / f"{the_config_name}_test_job_$(CLUSTER).out",
        "error":                        test_dir / f"{the_config_name}_test_job_$(CLUSTER).err",
        "want_io_proxy":                "True",

        "checkpoint_exit_code":         "85",
        "transfer_checkpoint_files":    "saved-state-dir",
        "transfer_output_files":        "",

        "checkpoint_destination":       'local://example.vo/example.fs',
        "transfer_plugins":             f'local={path_to_the_plugin}',
    }


@action
def the_job_handles(the_condor, the_job_description):
    job_handles = {}
    for name, test_case in TEST_CASES.items():
        expected_checkpoint_list = ",".join(test_case['expected_checkpoints'])
        arguments = f"$(CLUSTER).$(PROCESS) {expected_checkpoint_list} {test_case['checkpoint_count']}"
        complete_job_description = {
            'arguments': arguments,
            ** the_job_description,
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
def the_expected_checkpoints(the_job_name):
    return TEST_CASES[the_job_name]["expected_checkpoints"]


@action
def the_job_handle(the_job_tuple):
    return the_job_tuple[1]


@action
def the_completed_job(the_condor, the_job_handle):
    # We now set SCHEDD_INTERVAL to a small enough value in the test suite
    # that we don't have explicitly call for a reschedule to run the test
    # in a timely fashion.
    assert the_job_handle.wait(
        timeout=300,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
    )

    return the_job_handle


@action
def the_completed_job_ad(the_condor, the_completed_job):
    schedd = the_condor.get_local_schedd()
    constraint = f'ClusterID == {the_completed_job.clusterid} && ProcID == 0'
    result = schedd.query(
            constraint=constraint,
            projection=[
                "CheckpointDestination",
                "GlobalJobID",
                "ClusterID",
                "ProcID",
                "Owner",
            ],
    )

    return result[0]


@action
def the_target_directory(test_dir, the_completed_job_ad):
    globalJobID = the_completed_job_ad["GlobalJobID"].replace('#', '_')
    return test_dir / globalJobID


@action
def the_transfers(test_dir, the_completed_job_ad, the_condor):
    # Find the transfer history file.
    with the_condor.use_config():
        FILE_TRANSFER_STATS_LOG = htcondor.param["FILE_TRANSFER_STATS_LOG"]

    # Each ClassAd in the transfer history file should have 'JobClusterID'
    # and 'JobProcID' attribute.
    clusterID = the_completed_job_ad["ClusterID"]
    procID = the_completed_job_ad["ProcID"]

    # Find each transfer ad for this job.
    transfers = []
    globalJobID = the_completed_job_ad["GlobalJobID"].replace('#', '_')
    with open(FILE_TRANSFER_STATS_LOG, "r") as transfer_history:
            #
            # This is _amazingly_ stupid.  Why don't any of the condor tools
            # actually handle this?  Why is there no Python API that does?
            #

            # Eat the first "separator" line.
            line = transfer_history.readline()
            while True:
                buffer = ''

                while True:
                    line = transfer_history.readline()
                    if line == "":
                        # Because Python has no eof() function.
                        break
                    elif line != "***\n":
                        buffer += line
                    else:
                        ad = classad.parseOne(buffer)

                        # We only care about the job we're looking at.
                        if ad['JobClusterID'] != clusterID:
                            break
                        if ad['JobProcID'] != procID:
                            break

                        # What I want to generate here was a list of all
                        # the checkpoint numbers downloaded by the starter.
                        #
                        # The original idea was to look at the URLs downloaded
                        # by the starter and parse them to find the checkpoint
                        # numbers, and report the list of those.
                        #
                        # For some reason, the file transfer object doesn't
                        # actually log downloaded URLs.  It also doesn't log
                        # CEDAR uploads, and it logs uploads from the starter
                        # to a URL as CEDAR downloads.
                        #
                        # The upshot of this disaster is that even adding
                        # which subsystem logged to the transfer doesn't
                        # let me figure out what actually happened.
                        #
                        # Instead, I'm going to look for the starter's records
                        # of downloaded MANIFEST files, and declare those to be
                        # a true indication of what actually happened.
                        #

                        # However, we can set SHADOW.FILE_TRANSFER_STATS_LOG
                        # and keep the shadow events out of the default
                        # transfer history file without (further) hacks.
                        # if ad['Subsystem'] != "STARTER":
                        #   break
                        if ad['TransferProtocol'] != "cedar":
                            break
                        # Just in case we ever fix any of the above.
                        if ad['TransferType'] != 'download':
                            break

                        # The ./ seems redundant, but it's always there.
                        prefix = "./_condor_checkpoint_MANIFEST."
                        transferFileName = ad['TransferFileName']
                        if transferFileName.startswith(prefix):
                            # Only for Python >= 3.9 :(
                            # suffix = transferFileName.removeprefix(prefix)
                            suffix = transferFileName[len(prefix):]
                            transfers.append(suffix[0:4])

                        break

                if line == "" and buffer == "":
                    break

    # print(f"TRANSFERS: {transfers}")
    return transfers


@action
def the_removed_job(the_condor, the_completed_job):
    the_condor.run_command(['condor_qedit', the_completed_job.clusterid, 'LeaveJobInQueue', 'False'])
    the_completed_job.remove()

    while True:
        # This makes me sad.
        time.sleep(1)
        history_ads = the_condor.get_local_schedd().history(f"ClusterID == {the_completed_job.clusterid}", ["ClusterID"])
        if sum(1 for _ in history_ads) > 0:
            # Job has hit the history file, now we can continue
            return the_completed_job
        # but assert if the job is held, so we don't go into an infinite loop
        assert not the_completed_job.state.any_held()

    return False


@action
def the_preen_directory(the_removed_job, the_target_directory, the_condor, test_dir, the_config_name):
    # To aid in debugging, don't run condor_preen after a shadow test.
    if the_config_name != "preen":
        pytest.skip("preen-only test")
        return the_target_directory

    logger.debug("logging all ads")
    all_job_ads = the_condor.query(projection=["ClusterID", "ProcID"])
    for ad in all_job_ads:
        logger.debug(ad)

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
        rv = subprocess.run( ['condor_preen', '-d'],
            # Crass empiricism.
            env=preen_env, timeout=120,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            universal_newlines=True,
        )
        logger.debug(rv.stdout)
        assert(rv.returncode == 0)

    return the_target_directory


@action
def the_epoch_log(the_condor, the_completed_job_ad, the_removed_job):
    schedd = the_condor.get_local_schedd()
    globalJobID = the_completed_job_ad["GlobalJobID"]
    constraint = f'globalJobID == "{globalJobID}"'
    return [ad for ad in schedd.jobEpochHistory(constraint, [])]


class TestPartialUploads:

    # Test (1).
    def test_s_removes_failed_checkpoints(self,
        the_config_name,
        the_target_directory,
        the_expected_checkpoints
    ):
        if the_config_name != "shadow":
            pytest.skip("shadow-only test")
            return

        # ... and every expected checkpoint should be on disk.
        for checkpoint in the_expected_checkpoints[-2:]:
            path = the_target_directory / checkpoint
            assert(path.exists())

        # If we don't expect any checkpoints, then the test below
        # will fail because the_target_directory doesn't exist.
        if len(the_expected_checkpoints) == 0:
            assert not the_target_directory.exists()
            return

        # Everything on disk should be an expected checkpoint...
        for path in the_target_directory.iterdir():
            assert(path.is_dir())
            assert(path.name in the_expected_checkpoints)


    # Test (2).
    def test_p_removes_failed_checkpoints(self,
        the_condor,
        the_config_name,
        the_preen_directory,
        the_completed_job_ad
    ):
        if the_config_name != "preen":
            pytest.skip("preen-only test")
            return

        assert(not the_preen_directory.exists())

        # In addition to cleaning up the checkpoint destination, we should
        # make certain that we clean up our own SPOOL directory; this test
        # triggers a case the test_checkpoint_destination does not, so we
        # need to check here, too.  (Where condor_preen and the schedd both
        # try to clean up checkpoints whose MANIFEST files don't exist,
        # causing condor_manifest to think that the clean-up failed; the
        # tool now cleans up the .job.ad file (under checkpoint-cleanup/)
        # and its containing directory if it's the only file in there.
        with the_condor.use_config():
            SPOOL = htcondor.param["SPOOL"]
            cleanup_dir = Path(SPOOL) / "checkpoint-cleanup" / the_completed_job_ad["Owner"] / f"cluster{the_completed_job_ad['ClusterID']}.proc{the_completed_job_ad['ProcID']}.subproc0"
            assert(not cleanup_dir.exists())


    # Test (4).
    def test_download_only_successful_checkpoints(self,
        the_transfers,
        the_expected_checkpoints
    ):
        assert (len(the_expected_checkpoints) != 0) == (len(the_transfers) != 0)

        for transfer in the_transfers:
            assert(transfer in the_expected_checkpoints)


    # Test (5).
    def test_committed_time(self,
        the_job_name,
        the_epoch_log,
        the_expected_checkpoints
    ):
        if '0000' in the_expected_checkpoints:
            pytest.skip('for simplicitly, check only for 0 committed time')
            return

        first_epoch_ad = list(reversed(the_epoch_log))[0]
        assert first_epoch_ad['CommittedTime'] == 0

        # for ad in reversed(the_epoch_log):
        #   print(f"{ad['ClusterID']}.{ad['ProcID']}: {ad['CommittedTime']}")


# Copied from test_checkpoint_destination, and modified to fail to upload
# files named FAILURE.
@action
def path_to_the_plugin(test_dir):
    path_to_the_plugin = test_dir / "local.py"

    contents = format_script(
    f"""
        #!/usr/bin/python3

        import os
        import sys
        import time
        import shutil

        from urllib.parse import urlparse
        from pathlib import Path

        import classad2 as classad

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

                success = True
                if local_file_path.endswith("FAILURE"):
                    success = False
                if success:
                    shutil.copy(local_file_path, remote_file_path)

                end_time = time.time()

                # Get transfer statistics
                transfer_stats = {
                    'TransferSuccess': success,
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
                if not success:
                    transfer_stats["TransferError"] = "b/c you said so"

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

    write_file(path_to_the_plugin, contents)
    return path_to_the_plugin
