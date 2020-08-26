#!/usr/bin/env python3

import os
import sys
import socket
import time
try:
    from urllib.parse import urlparse # Python 3
except ImportError:
    from urlparse import urlparse # Python 2
import posixpath
import json

import requests
import classad

TOKEN_DIR_ENV_NAME = '_CONDOR_CREDS'
TOKEN_FILE_EXT = '.use'

DEFAULT_TIMEOUT = 30

ONEDRIVE_PLUGIN_VERSION = '1.0.0'

ONEDRIVE_API_VERSION = 'v1.0'

ONEDRIVE_API_BASE_URL = "https://graph.microsoft.com/" + ONEDRIVE_API_VERSION + "/me/drive/root"

def print_help(stream = sys.stderr):
    help_msg = '''Usage: {0} -infile <input-filename> -outfile <output-filename>
       {0} -classad

Options:
  -classad                    Print a ClassAd containing the capablities of this
                              file transfer plugin.
  -infile <input-filename>    Input ClassAd file
  -outfile <output-filename>  Output ClassAd file
'''
    stream.write(help_msg.format(sys.argv[0]))

def print_capabilities():
    capabilities = {
         'MultipleFileSupport': True,
         'PluginType': 'FileTransfer',
         'SupportedMethods': 'onedrive',
         'Version': ONEDRIVE_PLUGIN_VERSION,
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
    if not len(sys.argv) in [2, 5]:
        print_help()
        sys.exit(-1)

    # If -classad, print the capabilities of the plugin and exit early
    elif (len(sys.argv) == 2) and (sys.argv[1] == '-classad'):
        print_capabilities()
        sys.exit(0)

    # -infile and -outfile must be in the first and third position
    else:
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

    return {'infile': infile, 'outfile': outfile}

def get_token_name(url):
    scheme = url.split('://')[0]
    if '+' in scheme:
        (handle, provider) = scheme.split('+')
        token_name = '{0}_{1}'.format(provider, handle)
    else:
        token_name = scheme

    return token_name

def get_token_path(token_name):
    if TOKEN_DIR_ENV_NAME in os.environ:
        cred_dir = os.environ[TOKEN_DIR_ENV_NAME]
    else:
        raise KeyError("Required variable '{0}' was not found in job's environment".format(TOKEN_DIR_ENV_NAME))

    token_path = os.path.join(cred_dir, token_name + TOKEN_FILE_EXT)

    if not os.path.exists(token_path):
        raise IOError(2, 'Token file not found', token_path)

    return token_path

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

class OneDrivePlugin:

    def __init__(self, token_path):
        self.token = self.get_token(token_path)
        self.headers = {'Authorization': 'Bearer {0}'.format(self.token)}

    def get_token(self, token_path):
        with open(token_path, 'r') as f:
            access_token = json.load(f)['access_token']
        return access_token

    def api_call(self, endpoint, method = 'GET', params = None, data = {}):
        url = ONEDRIVE_API_BASE_URL + endpoint
        kwargs = {
            'headers': self.headers,
            'timeout': DEFAULT_TIMEOUT,
        }
        if method in ['GET'] and params is not None:
            kwargs['params'] = params
        if method in ['POST', 'PUT', 'PATCH']:
            kwargs['data'] = data
        response = requests.request(method, url, **kwargs)
        response.raise_for_status()
        return response.json()

    def get_file_info(self, url):
        
        # Build the folder tree
        parsed_url = urlparse(url)
        file_info = {}
        file_info['path'] = parsed_url.path
        return file_info

    def download_file(self, url, local_file_path):

        start_time = time.time()
        
        file_info = self.get_file_info(url)
        file_path = file_info['path']

        endpoint = ':/{0}:/content'.format(file_path)
        url = ONEDRIVE_API_BASE_URL + endpoint

        # Stream the data to disk, chunk by chunk,
        # instead of loading it all into memory.
        connection_start_time = time.time()
        response = requests.get(url, headers = self.headers, stream = True,
                                    timeout = DEFAULT_TIMEOUT)

        try:
            response.raise_for_status()
            
            try:
                content_length = int(response.headers['Content-Length'])
            except (ValueError, KeyError):
                content_length = False
                
            with open(local_file_path, 'wb') as f:
                file_size = 0
                for chunk in response.iter_content(chunk_size = 8192):
                    file_size += len(chunk)
                    f.write(chunk)
                    
        except Exception as err:
            # Since we're streaming, we should
            # free the connection before raising the exception.
            response.close()
            raise err

        end_time = time.time()

        # Note that we *don't* include the TransferUrl:
        # 'TransferUrl': response.url.encode()
        # This would leak a short-lived URL into the transfer_history log
        # that anyone could use to access the requested content.
        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'https',
            'TransferType': 'download',
            'TransferFileName': local_file_path,
            'TransferFileBytes': file_size,
            'TransferTotalBytes': content_length or file_size,
            'TransferStartTime': int(start_time),
            'TransferEndTime': int(end_time),
            'ConnectionTimeSeconds': end_time - connection_start_time,
            'TransferHostName': urlparse(response.url.encode()).netloc,
            'TransferLocalMachineName': socket.gethostname(),
        }

        return transfer_stats
            

if __name__ == '__main__':
    # Per the design doc, all failures should result in exit code -1.
    # This is true even if we cannot write a ClassAd to the outfile,
    # so we catch all exceptions, try to write to the outfile if we can
    # and always exit -1 on error.
    #
    # Exiting -1 without an outfile thus means one of two things:
    # 1. Couldn't parse arguments.
    # 2. Couldn't open outfile for writing.
    
    try:
        args = parse_args()
    except Exception:
        sys.exit(-1)

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

    try:
        with open(args['outfile'], 'w') as outfile:
            for ad in infile_ads:
                try:
                    token_name = get_token_name(ad['Url'])
                    token_path = get_token_path(token_name)
                    onedrive = OneDrivePlugin(token_path)

                    outfile_dict = onedrive.download_file(ad['Url'], ad['LocalFileName'])
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
