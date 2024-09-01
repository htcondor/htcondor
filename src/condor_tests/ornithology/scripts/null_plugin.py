#!/usr/bin/env python3

# Custom Ornithology transfer plugin that transfers files to /dev/null
# This allows the mimicry of transfer without actually transferring

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
from pathlib import Path

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
         'SupportedMethods': 'null',
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

class DevNullPlugin:

    def parse_url(self, url):
        url_path = url[(url.find("://") + 3):]
        return url_path

    def nullify_in(self, xfer_file):
        Path(xfer_file).touch()
        total_bytes = 0
        xfer_bytes = 0
        return (total_bytes, xfer_bytes)

    def download_file(self, url, local_file_path):

        start_time = time.time()
        stats = self.nullify_in(self.parse_url(url))
        end_time = time.time()

        # Get transfer statistics
        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'xferstats',
            'TransferType': 'upload',
            'TransferFileName': local_file_path,
            'TransferFileBytes': stats[1],
            'TransferTotalBytes': stats[0],
            'TransferStartTime': int(start_time),
            'TransferEndTime': int(end_time),
            'ConnectionTimeSeconds': end_time - start_time,
            'TransferUrl': url,
        }

        return transfer_stats

    def nullify_out(self, xfer_file):
        total_bytes = os.stat(xfer_file).st_size
        xfer_bytes = 0
        with open(xfer_file, "r") as xfer:
            with open(os.devnull, "w") as null:
                for line in xfer:
                    xfer_bytes += len(line.encode('utf-8'))
                    null.write(line)
        return (total_bytes, xfer_bytes)

    def upload_file(self, url, local_file_path):

        start_time = time.time()
        stats = self.nullify_out(local_file_path)
        end_time = time.time()

        # Get transfer statistics
        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'xferstats',
            'TransferType': 'upload',
            'TransferFileName': local_file_path,
            'TransferFileBytes': stats[1],
            'TransferTotalBytes': stats[0],
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

    plugin = DevNullPlugin()

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
                        outfile_dict = plugin.download_file(ad['Url'], ad['LocalFileName'])
                    else:
                        outfile_dict = plugin.upload_file(ad['Url'], ad['LocalFileName'])

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

