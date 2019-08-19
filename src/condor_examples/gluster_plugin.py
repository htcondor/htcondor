#!/usr/bin/env python

import classad
import json
import os
import posixpath
import requests
import shutil
import socket
import sys
import time

try:
    from urllib.parse import urlparse # Python 3
except ImportError:
    from urlparse import urlparse # Python 2

DEFAULT_TIMEOUT = 30
GLUSTER_PLUGIN_VERSION = '1.0.0'

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
         'SupportedMethods': 'gluster',
         'Version': GLUSTER_PLUGIN_VERSION,
    }
    sys.stdout.write(classad.ClassAd(capabilities).printOld())

def parse_args():
    '''The optparse library can't handle the types of arguments that the file
    transfer plugin sends, the argparse library can't be expected to be
    found on machines running EL 6 (Python 2.6), and a plugin should not
    reach outside the standard library, so the plugin must roll its own argument
    parser. The expected input is very rigid, so this isn't too awful.'''

    # The only argument lists that are acceptable are
    # <this> -classad
    # <this> -infile <input-filename> -outfile <output-filename>
    # <this> -outfile <output-filename> -infile <input-filename>
    if not len(sys.argv) in [2, 5, 6]:
        print_help()
        sys.exit(-1)

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
        sys.exit(-1)
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
        sys.exit(-1)

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

class GlusterPlugin:

    def __init__(self):
        self.gluster_root = "/mnt/gluster"

    # Extract whatever information we want from the url provided.
    # In the case of Gluster, convert the gluster://path/to/file url to a 
    # path in the file system (ie. /path/to/file)
    def parse_url(self, url):
        url_path = url[(url.find("://") + 3):]
        return url_path

    def download_file(self, url, local_file_path):

        start_time = time.time()

        # Download transfer logic goes here
        gluster_file_path = self.gluster_root + "/" + self.parse_url(url)
        shutil.copy(gluster_file_path, local_file_path)
        file_size = os.stat(local_file_path).st_size

        end_time = time.time()

        # Get transfer statistics
        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'gluster',
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
        gluster_file_path = self.gluster_root + "/" + self.parse_url(url)
        shutil.copy(local_file_path, gluster_file_path)
        file_size = os.stat(local_file_path).st_size

        end_time = time.time()

        # Including TransferUrl makes little sense here, too.
        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'gluster',
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
        sys.exit(-1)

    gluster = GlusterPlugin()

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
        sys.exit(-1)

    # Now iterate over the list of classads and perform the transfers.
    try:
        with open(args['outfile'], 'w') as outfile:
            for ad in infile_ads:
                try:
                    if not args['upload']:
                        outfile_dict = gluster.download_file(ad['Url'], ad['LocalFileName'])
                    else:
                        outfile_dict = gluster.upload_file(ad['Url'], ad['LocalFileName'])

                    outfile.write(str(classad.ClassAd(outfile_dict)))

                except Exception as err:
                    try:
                        outfile_dict = get_error_dict(err, url = ad['Url'])
                        outfile.write(str(classad.ClassAd(outfile_dict)))
                    except Exception:
                        pass
                    sys.exit(-1)

    except Exception:
        sys.exit(-1)
