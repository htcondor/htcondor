#!/usr/bin/env pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
	# make sure that file transfer plugins are enabled (might be disabled by default)
	ENABLE_URL_TRANSFERS = true
	FILETRANSFER_PLUGINS = $(LIBEXEC)/curl_plugin $(LIBEXEC)/data_plugin
"""
#endtestreq

import re

from ornithology import (
    action,
    write_file,
    format_script,
    ClusterState,
)


@action
def plugin_log_file(test_dir):
    return test_dir / "plugin.log"


@action
def job_python_file(test_dir):
    job_python_file = test_dir / "debug.py"
    contents = format_script(
    """
        #!/usr/bin/python3

        import classad
        import json
        import os
        import posixpath
        import shutil
        import socket
        import sys
        import time

        from urllib.parse import urlparse

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
                 'SupportedMethods': 'debug',
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

        class DebugPlugin:

            # Extract whatever information we want from the url provided.
            # In this example, convert the example://path/to/file url to a
            # path in the file system (ie. /path/to/file)
            def parse_url(self, url):
                url_path = url[(url.find("://") + 3):]
                return url_path

            def download_file(self, url, local_file_path):

                start_time = time.time()

                # Download transfer logic goes here
                print(f"DEBUG: download {url} -> {local_file_path}")
                file_size = 0

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

                # Upload transfer logic goes here
                print(f"DEBUG: upload {local_file_path} --> {url}")
                file_size = 0

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

            debug_plugin = DebugPlugin()

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
                                outfile_dict = debug_plugin.download_file(ad['Url'], ad['LocalFileName'])
                            else:
                                outfile_dict = debug_plugin.upload_file(ad['Url'], ad['LocalFileName'])

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
    write_file(job_python_file, contents)
    return job_python_file


@action
def job_shell_file(test_dir, job_python_file, plugin_log_file):
    job_shell_file = test_dir / "debug.sh"
    contents = format_script(
    f"""
        #!/bin/bash
        exec {job_python_file} $@ &> {plugin_log_file}
    """
    )
    write_file(job_shell_file, contents)
    return job_shell_file


@action
def job_handle(default_condor, path_to_sleep, job_shell_file):
    job_handle = default_condor.submit({
            "executable":               path_to_sleep,
            "arguments":                1,
            "transfer_executable":      "true",
            "should_transfer_files":    "true",

            "output_destination":       "testing://root",
            "output":                   "new-output-name",
            "error":                    "new-error-name",
            "transfer_output_files":    "output-file",
            "transfer_plugins":         f"testing={job_shell_file}",

            "log":                      "job.log",
        },
        count=1,
    )

    yield job_handle

    job_handle.remove()


@action
def completed_job(job_handle):
    assert job_handle.wait(
        condition=ClusterState.all_complete,
        timeout=60,
        fail_condition=ClusterState.any_held,
    )

    return job_handle


@action
def plugin_log(completed_job, plugin_log_file):
    with open(plugin_log_file) as file:
        return file.read()


class TestOutputDestination:
    def test_standard_streams_renamed(self, plugin_log):
        output_rename = '^DEBUG: upload .*/_condor_stdout --> testing://root/new-output-name$'
        assert re.search(output_rename, plugin_log, re.MULTILINE) is not None

        error_rename = '^DEBUG: upload .*/_condor_stderr --> testing://root/new-error-name$'
        assert re.search(error_rename, plugin_log, re.MULTILINE) is not None
