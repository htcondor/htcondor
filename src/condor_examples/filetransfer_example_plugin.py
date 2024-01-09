#!/usr/bin/env python3

"""
This is an example template for a custom HTCondor file transfer plugin.
In this example, it transfers files described by an example://path/to/file URL
by copying them from the path indicated to a job's working directory.
To make this a functional plugin, change everything in this file that comes
after a #CHANGE ME HERE comment.
"""

import classad
import json
import os
import posixpath
import requests
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
         # SupportedMethods indicates which URL methods/types this plugin supports
         #CHANGE ME HERE
         'SupportedMethods': 'example',
         #END CHANGE
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

#CHANGE ME HERE
class ExamplePlugin:
#END CHANGE

    # Extract whatever information we want from the url provided.
    # In this example, convert the example://path/to/file url to a 
    # path in the file system (ie. /path/to/file)
    def parse_url(self, url):
        url_path = url[(url.find("://") + 3):]
        return url_path

    def download_file(self, url, local_file_path):

        start_time = time.time()

        # Download transfer logic goes here
        # In this example, we simply copy the file from a path indicated in the
        # URL string to the current working directory.
        #CHANGE ME HERE
        example_file_path = self.parse_url(url)
        shutil.copy(example_file_path, local_file_path)
        file_size = os.stat(local_file_path).st_size
        #END CHANGE

        end_time = time.time()

        # Get transfer statistics
        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'example',
            'TransferType': 'download',
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
        # In this example, we simply copy the file to a path indicated in the
        # URL string from the current working directory.
        #CHANGE ME HERE
        example_file_path = self.parse_url(url)
        shutil.copy(local_file_path, example_file_path)
        file_size = os.stat(local_file_path).st_size
        #END CHANGE

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

    #CHANGE ME HERE
    example_plugin = ExamplePlugin()
    #END CHANGE

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
                        #CHANGE ME HERE
                        outfile_dict = example_plugin.download_file(ad['Url'], ad['LocalFileName'])
                        #END CHANGE
                    else:
                        #CHANGE ME HERE
                        outfile_dict = example_plugin.upload_file(ad['Url'], ad['LocalFileName'])
                        #END CHANGE

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
