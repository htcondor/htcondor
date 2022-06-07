#!/usr/bin/env pytest

#Job Ad attribute: TransferInputStats regression test
#
#The attribute 'TransferInputStats' nested attributes were
#overflowing due to big numbers being shoved into int variables.
#This test is to make sure that large files tranferred don't
#result in the nested attributes have byte transfer sizes less
#than 0.
#


from ornithology import *
import os

#-----------------------------------------------------------------------------------------
@action
def job_python_file(test_dir):
     job_python_file = test_dir / "xferstats.py"
     contents = format_script(
     """#!/usr/bin/env python3

import classad
import json
import os
import posixpath
import shutil
import socket
import sys
import time
from glob import glob as find

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
         # SupportedMethods indicates which URL methods/types this plugin supports
         'SupportedMethods': 'xferstats',
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

class XferStatsPlugin:

    # Extract whatever information we want from the url provided.
    def Xfile_path(self):
        # Get test dir path and remove the script name from the end
        path = os.path.realpath(__file__)
        path = path[1:]
        path_parts = path.split("/")
        new_path = ""
        for i in range(len(path_parts)-1):
            new_path += f"/{path_parts[i]}"
        # Find the test file and return it
        file_name = find(os.path.join(new_path,"*-xferIn.txt"))
        # There should only be 1 file per
        xfer_file = file_name[0][1:].split("/")
        return (new_path + f"/{xfer_file[(len(xfer_file) - 1)]}")

    def download_file(self, url, local_file_path):

        start_time = time.time()

        path = self.Xfile_path()
        file_size = os.stat(path).st_size
        print(f'\\nPath-: {path}')
        print(f'Bytes: {file_size}')

        end_time = time.time()

        # Get transfer statistics
        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'xferstats',
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

        path = self.Xfile_path()
        file_size = os.stat(path).st_size
        print(f'\\nPath-: {path}')
        print(f'Bytes: {file_size}')

        end_time = time.time()

        # Get transfer statistics
        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'xferstats',
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

    xferstats_plugin = XferStatsPlugin()

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
                        outfile_dict = xferstats_plugin.download_file(ad['Url'], ad['LocalFileName'])
                    else:
                        outfile_dict = xferstats_plugin.upload_file(ad['Url'], ad['LocalFileName'])

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
     """)
     write_file(job_python_file, contents)
     return job_python_file

#-----------------------------------------------------------------------------------------
@action
def job_shell_file(test_dir, job_python_file):
    job_shell_file = test_dir / "xferstats.sh"
    contents = format_script(
    f"""
        #!/bin/bash
        exec {job_python_file} $@ &>> {test_dir}/plugin.log
    """
    )
    write_file(job_shell_file, contents)
    return job_shell_file

#-----------------------------------------------------------------------------------------
#Fixture to run a simple job with varying sized transfer files
@action(params={
"0kb File":"0K",
"2gb File":"2G",
"4gb File":"4G",
"8gb File":"8G",
})
def run_file_xfer_job(default_condor,test_dir,request, job_shell_file):
     #Make File to be transferred based on size passed
     xfer_file = "{0}-xferIn.txt".format(request.param)
     os.system("truncate -s {0} {1}/{2}".format(request.param,test_dir,xfer_file))
     #Submit a list command job with output so we can varify file sizes
     job_handle = default_condor.submit(
          {
               "executable":"/bin/ls",
               "arguments":f"\"-l {test_dir}\"",
               "should_transfer_files":"Yes",
               "transfer_input_files":f"xferstats://{test_dir}/{xfer_file}",
               "transfer_plugins":"xferstats=xferstats.sh",
               "output":(test_dir / "{0}-file_sizes.txt".format(request.param)).as_posix(),
               "log":(test_dir / "{0}-test.log".format(request.param)).as_posix(),
          },
          count=1
     )
     #Wait for job to complete. Gave a hefty amount of time due to large files being transfered
     job_handle.wait(timeout=360)
     schedd = default_condor.get_local_schedd()
     #once job is done get job ad attributes needed for test verification
     job_ad = schedd.history(
          constraint=None,
          projection=["TransferInput","TransferInputStats","TransferOutputStats"],
          match=1,
     )
     #Remove transfer file so we don't have 8gb files lying around
     os.remove(xfer_file)
     #Return job ad for checking attributes
     return job_ad

#=========================================================================================
#JobSubmitMethod Tests
class TestTransferStatsOverflowRegression:

     #Test that a job submited with value < Minimum doestn't add JobSubmitMethod attr
     def test_transfer_stats_bytes_dont_overflow(self,run_file_xfer_job):
          passed = True
          is_0K = False
          #For every classad returned (should only be 1) check statistics
          for ad in run_file_xfer_job:
               #Get TransferInput line to use file size in outputs and checks
               _file = ad["TransferInput"]
               local = (_file.find("-xferIn.txt") - 2)
               print(f"\nFile: {_file[local:local+13]} is {_file[local:local+2]}b in size.")
               if "0K-xferIn.txt" in _file:
                    is_0K = True
               #For every statistic in TransferInputStats
               for stat in ad["TransferInputStats"]:
                    #Check if stat is a 'SizeBytes' statistic
                    if "SizeBytes" in stat:
                         #Get value
                         value = ad["TransferInputStats"][stat]
                         #If value is less than 0 overflow occured. Output error and fail test
                         if value < 0:
                              passed = False
                              print(f"Error: {stat} value is {value}. The value should be 0 or greater.")
                         #If it isn't the 0Kb file and value is 0 then overflow occured. Output error and fail test.
                         elif is_0K is False and value == 0:
                              passed = False
                              print(f"Error: {stat} value is {value} for a file larger than 0 bytes. The value should not be 0.")
               #For every statistic in TransferOutputStats
               for stat in ad["TransferOutputStats"]:
                    #Check if stat is a 'SizeBytes' statistic
                    if "SizeBytes" in stat:
                         #Get value
                         value = ad["TransferOutputStats"][stat]
                         #If value is less than 0 overflow occured. Output error and fail test
                         if value < 0:
                              passed = False
                              print(f"Error: {stat} value is {value}. The value should be 0 or greater.")
                         #If it isn't the 0Kb file and value is 0 then overflow occured. Output error and fail test.
                         elif is_0K is False and value == 0:
                              passed = False
                              print(f"Error: {stat} value is {value} for a file larger than 0 bytes. The value should not be 0.")
          #Assert Pass or Fail here
          assert passed

