#!/usr/bin/env pytest

#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
	# make sure that file transfer plugins are enabled (might be disabled by default)
	ENABLE_URL_TRANSFERS = true
	FILETRANSFER_PLUGINS = $(LIBEXEC)/curl_plugin $(LIBEXEC)/data_plugin
"""
#endtestreq


#Job Ad attribute: TransferInputStats regression test
#
#The attribute 'TransferInputStats' nested attributes were
#overflowing due to big numbers being shoved into int variables.
#This test is to make sure that large files transferred don't
#result in the nested attributes have byte transfer sizes less
#than 0.
#


from ornithology import *
from time import sleep

#-----------------------------------------------------------------------------------------
@action
def job_python_file(test_dir):
     job_python_file = test_dir / "xferstats.py"
     contents = format_script(
     """#!/usr/bin/env python3

import classad2 as classad
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
        # Split path and end filename
        parts = path.rsplit("/",1)
        # Return path / xferIn.txt
        return (parts[0] + "/xferIn.txt")

    def download_file(self, url, local_file_path):

        start_time = time.time()

        file_size = 0
        test = "-"
        f = open(self.Xfile_path(), "r")
        test = f.readline().strip()
        file_size = int(test[:-1]) * 1024 * 1024 * 1024
        f.close()
        print(f'\\nTest-: {test} file')
        print(f'Size-: {file_size} bytes')

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

        file_size = 0
        test = "-"
        f = open(self.Xfile_path(), "r")
        test = f.readline().strip()
        file_size = int(test[:-1]) * 1024 * 1024 * 1024
        f.close()
        print(f'\\nTest-: {test} file')
        print(f'Size-: {file_size} bytes')

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
        exec {job_python_file} $@ >> {test_dir}/plugin.log 2>&1
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
"16gb File":"16G",
})
def run_file_xfer_job(default_condor,test_dir,request, job_shell_file, path_to_sleep):
     #Write Test File size to input file
     xfer_file = "xferIn.txt"
     _file = open(xfer_file,"w")
     _file.write(request.param)
     _file.close()
     #Submit a list command job with output so we can varify file sizes
     job_handle = default_condor.submit(
          {
               "executable":path_to_sleep,
               "arguments":"0",
               "should_transfer_files":"Yes",
               "transfer_input_files":f"xferstats://{test_dir}/{xfer_file}",
               "transfer_plugins":f"xferstats={job_shell_file}",
               "log":(test_dir / "{0}-test.log".format(request.param)).as_posix(),
          },
          count=1
     )
     #Wait for job to complete. Gave a hefty amount of time due to large files being transfered
     clustId = job_handle.clusterid
     assert job_handle.wait(condition=ClusterState.all_complete,timeout=30)
     schedd = default_condor.get_local_schedd()
     #once job is done get job ad attributes needed for test verification
     job_ad = []
     for i in range(0,5):
          hist_itr = schedd.history(
               constraint=f"ClusterId=={clustId}",
               projection=["TransferInput","TransferInputStats","TransferOutputStats"],
               match=1,
          )
          job_ad = list(hist_itr)
          if len(job_ad) > 0:
              break
          sleep(1)
     assert len(job_ad) > 0
     #Return job ad for checking attributes
     return (request.param,job_ad)

#=========================================================================================
#JobSubmitMethod Tests
class TestTransferStatsOverflowRegression:

     #Test that a job submited with value < Minimum doesn't add JobSubmitMethod attr
     def test_transfer_stats_bytes_dont_overflow(self,run_file_xfer_job):
          passed = True
          is_0K = False
          count = 0
          #For every classad returned (should only be 1) check statistics
          for ad in run_file_xfer_job[1]:
               count += 1
               if "0K" in run_file_xfer_job[0]:
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
          if count != 1:
              passed = False
              print(f"Error: history() returned {count} job ads. Expected 1 ad.")
          #Assert Pass or Fail here
          assert passed


