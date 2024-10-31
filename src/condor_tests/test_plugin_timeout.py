#!/usr/bin/env pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
	# make sure that file transfer plugins are enabled (might be disabled by default)
	ENABLE_URL_TRANSFERS = true
	FILETRANSFER_PLUGINS = $(LIBEXEC)/curl_plugin $(LIBEXEC)/data_plugin
"""
#endtestreq


import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import os

from ornithology import (
    action,
    jobs,
    Condor,
    ClusterState,
    format_script,
    write_file,
)


TEST_CASES = {
    "short": (
        {
            "MAX_FILE_TRANSFER_PLUGIN_LIFETIME": "5",
            "FILETRANSFER_PLUGINS": "$(FILETRANSFER_PLUGINS), {single_shell_file}",
        },
        jobs.JobStatus.HELD,
    ),
    "long": (
        {
            "MAX_FILE_TRANSFER_PLUGIN_LIFETIME": "60",
            "FILETRANSFER_PLUGINS": "$(FILETRANSFER_PLUGINS), {single_shell_file}",
        },
        jobs.JobStatus.COMPLETED,
    ),
}


@action
def the_condors(test_dir, single_shell_file):
    condors = {}
    for test_name, test_case in TEST_CASES.items():
        test_config = test_case[0]
        test_config = {key: value.format(single_shell_file=single_shell_file) for key, value in test_config.items()}
        condors[test_name] = Condor(
            local_dir=test_dir / test_name,
            config={** test_config}
        )
        condors[test_name]._start()

    yield condors

    for test_name in TEST_CASES:
        condors[test_name]._cleanup()


@action(params={name: name for name in TEST_CASES})
def the_test_case(request, the_condors):
    return (request.param, the_condors[request.param], TEST_CASES[request.param][1])


@action
def the_test_name(the_test_case):
    return the_test_case[0]


@action
def the_condor(the_test_case):
    return the_test_case[1]


@action
def the_expected_result(the_test_case):
    return the_test_case[2]


@action
def another_job_handle(test_dir, the_test_name, the_condor, path_to_sleep, single_shell_file):
    another_job_handle = the_condor.submit(
        description={
            "LeaveJobInQueue":              "true",
            "executable":                   path_to_sleep,
            "arguments":                    "1",

            "transfer_executable":          "false",
            "transfer_input_files":         f"single://{path_to_sleep}",
            "should_transfer_files":        "true",
            "when_to_transfer_output":      "ON_EXIT",

            "log":                          test_dir / f"{the_test_name}_$(CLUSTER).log",
            "output":                       test_dir / f"{the_test_name}_$(CLUSTER).out",
            "error":                        test_dir / f"{the_test_name}_$(CLUSTER).err",
        },
        count=1,
    )

    return another_job_handle


@action
def the_job_handle(test_dir, the_test_name, the_condor, path_to_sleep, plugin_shell_file):
    the_job_handle = the_condor.submit(
        description={
            "LeaveJobInQueue":              "true",
            "executable":                   path_to_sleep,
            "arguments":                    "1",

            "transfer_executable":          "false",
            "transfer_input_files":         f"local://{path_to_sleep}",
            "transfer_plugins":             f"local={plugin_shell_file}",
            "should_transfer_files":        "true",
            "when_to_transfer_output":      "ON_EXIT",

            "log":                          test_dir / f"{the_test_name}_$(CLUSTER).log",
            "output":                       test_dir / f"{the_test_name}_$(CLUSTER).out",
            "error":                        test_dir / f"{the_test_name}_$(CLUSTER).err",
        },
        count=1,
    )

    return the_job_handle


@action
def the_completed_job(the_job_handle):
    the_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held
    )

    return the_job_handle


@action
def another_completed_job(another_job_handle):
    another_job_handle.wait(
        timeout=60,
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held
    )

    return another_job_handle


class TestPluginTimeOut:
    def test_got_expected_result(self, the_completed_job, another_completed_job, the_expected_result):
        assert the_completed_job.state.all_status(the_expected_result)
        assert another_completed_job.state.all_status(the_expected_result)


#
# Stolen from test_checkpoint_destination.py.
#

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
    """
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
                time.sleep(10)

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
                time.sleep(10)

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

            if not args['upload']:
                mutator = Path('mutate_checkpoint.py')
                if mutator.exists():
                    os.system(f"./{mutator}")
    """
    )
    write_file(plugin_python_file, contents)
    return plugin_python_file

#
# End thievery.
#


@action
def single_log_file(test_dir):
    return test_dir / "single.log"


@action
def single_shell_file(test_dir, single_python_file, single_log_file):
    pythonpath = os.environ["PYTHONPATH"]
    single_shell_file = test_dir / "single.sh"
    contents = format_script(
    f"""
        #!/bin/bash
        PYTHONPATH={pythonpath}
        exec {single_python_file} "$@"
    """
    )
    write_file(single_shell_file, contents)
    return single_shell_file


@action
def single_python_file(test_dir):
    single_python_file = test_dir / "single.py"
    contents = format_script(
    """
        #!/usr/bin/env python3

        import sys
        import time
        import shutil

        import classad


        def print_capabilities():
            capabilities = {
                'MultipleFileSupport':  False,
                'PluginType':           'FileTransfer',
                'SupportedMethods':     'single',
                'Version':              '0.1',
            }

            sys.stdout.write(classad.ClassAd(capabilities).printOld())


        if __name__ == '__main__':
            if len(sys.argv) == 2 and sys.argv[1] == '-classad':
                print_capabilities()
                sys.exit(0)

            source = sys.argv[1]
            destination = sys.argv[2]

            if source.startswith("single://"):
                source = source[9:]

            if destination.startswith("single://"):
                destination = destination[9:]

            time.sleep(10)
            shutil.copy(source, destination)
            sys.exit(0)
    """
    )
    write_file(single_python_file, contents)
    return single_python_file
