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

from hashlib import sha1
from base64 import b64encode

import requests
import classad

TOKEN_DIR_ENV_NAME = '_CONDOR_CREDS'
TOKEN_FILE_EXT = '.use'

DEFAULT_TIMEOUT = 30

BOX_PLUGIN_VERSION = '1.1.0'

BOX_API_VERSION = '2.0'
BOX_API_BASE_URL = 'https://api.box.com/' + BOX_API_VERSION

# Two methods of uploading, depending on file size:
#   https://developer.box.com/guides/uploads/direct/
#   https://developer.box.com/guides/uploads/chunked/
DIRECT_UPLOAD_CUTOFF_MB = 35

def print_help(stream = sys.stderr):
    help_msg = '''Usage: {0} -infile <input-filename> -outfile <output-filename>
       {0} -classad

Options:
  -classad                    Print a ClassAd containing the capablities of this
                              file transfer plugin.
  -infile <input-filename>    Input ClassAd file
  -outfile <output-filename>  Output ClassAd file
  -upload
'''
    stream.write(help_msg.format(sys.argv[0]))

def print_capabilities():
    capabilities = {
         'MultipleFileSupport': True,
         'PluginType': 'FileTransfer',
         'SupportedMethods': 'box',
         'Version': BOX_PLUGIN_VERSION,
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

class BoxPlugin:

    def __init__(self, token_path):
        self.token_path = token_path
        self.token = self.get_token(self.token_path)
        self.headers = {'Authorization': 'Bearer {0}'.format(self.token)}
        self.path_ids = {'/': u'0'}

    def get_token(self, token_path):
        with open(token_path, 'r') as f:
            access_token = json.load(f)['access_token']
        return access_token

    def reload_token(self):
        self.token = self.get_token(self.token_path)
        self.headers['Authorization'] = 'Bearer {0}'.format(self.token)

    def parse_url(self, url):

        # Build the folder tree
        parsed_url = urlparse(url)
        folder_tree = []

        if parsed_url.netloc != '':
            folder_tree.append(parsed_url.netloc)
        if parsed_url.path != '':
            path = posixpath.split(parsed_url.path)
            while path[1] != '':
                folder_tree.insert(1, path[1])
                path = posixpath.split(path[0])

        # The file is the last item in the tree
        filename = folder_tree.pop()

        return (filename, folder_tree)

    def api_call(self, endpoint, method = 'GET', params = None, data = {}):
        self.reload_token()
        url = BOX_API_BASE_URL + endpoint

        kwargs = {
            'headers': self.headers,
            'timeout': DEFAULT_TIMEOUT,
        }

        if params is not None:
            kwargs['params'] = params

        if method in ['POST', 'PUT', 'PATCH']:
            kwargs['data'] = data

        response = requests.request(method, url, **kwargs)
        response.raise_for_status()

        return response.json()

    def create_folder(self, folder_name, parent_id):

        endpoint = '/folders'
        data = json.dumps({
            'name': folder_name,
            'parent': {'id': parent_id}
        })

        try:
            folder_info = self.api_call(endpoint, 'POST', data = data)
        except requests.exceptions.HTTPError as e:
            if e.response.status_code == 409: # folder already exists
                folder_info = e.response.json()['context_info']['conflicts'][0]
            else:
                raise e
        return folder_info['id']

    def get_object_id(self, object_name, object_type, object_parents):
        endpoint = '/folders/{0}/items'.format(object_parents[-1])
        fields = ['id', 'name', 'type', 'path_collection']
        limit = 1000

        params = {
            'fields': ','.join(fields),
            'limit': limit,
        }

        folder_items = self.api_call(endpoint, params = params)

        object_found = False
        while not object_found:
            for entry in folder_items['entries']:

                # First do a quick check against the name and type
                if (entry['name'] != object_name) or (entry['type'] != object_type):
                    continue

                # Then compare parents
                entry_parents = [p['id'] for p in entry['path_collection']['entries']]
                if set(entry_parents) == set(object_parents):
                    object_id = entry['id']
                    object_found = True
                    break

            # Go to next page if it exists and if haven't found object yet
            if object_found:
                pass
            elif (('next_marker' in folder_items) and
                    (folder_items['next_marker'] not in [None, ''])):
                params['next_marker'] = folder_items['next_marker']
                folder_items = self.api_call(endpoint, params = params)
            else:
                raise IOError(2, 'Object not found', object_name)

        return object_id

    def get_parent_folders_ids(self, folder_tree, create_if_missing = False):

        # Traverse the folder tree, starting at the root (id = 0)
        parent_ids = [u'0']
        searched_path = ''
        for folder_name in folder_tree:
            searched_path += '/{0}'.format(folder_name)
            if searched_path in self.path_ids: # Check the cached ids
                parent_id = self.path_ids[searched_path]
            else:
                try:
                    parent_id = self.get_object_id(folder_name, 'folder', parent_ids)
                except IOError:
                    if create_if_missing:
                        parent_id = self.create_folder(folder_name, parent_id = parent_ids[-1])
                    else:
                        raise IOError(2, 'Folder not found in Box', searched_path)
                self.path_ids[searched_path] = parent_id # Update the cached ids
            parent_ids.append(parent_id)

        return parent_ids

    def get_file_id(self, url):

        # Parse out the filename and folder_tree and get folder_ids
        (filename, folder_tree) = self.parse_url(url)
        parent_ids = self.get_parent_folders_ids(folder_tree)

        try:
            file_id = self.get_object_id(filename, 'file', parent_ids)
        except IOError:
            raise IOError(2, 'File not found in Box', '{0}/{1}'.format('/'.join(folder_tree), filename))

        return file_id

    def download_file(self, url, local_file_path):

        start_time = time.time()

        file_id = self.get_file_id(url)

        endpoint = '/files/{0}/content'.format(file_id)
        download_url = BOX_API_BASE_URL + endpoint

        # Stream the data to disk, chunk by chunk,
        # instead of loading it all into memory.
        self.reload_token()
        connection_start_time = time.time()
        response = requests.get(download_url, headers = self.headers, stream = True,
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

    def upload_file(self, url, local_file_path):

        # Determine file upload method
        local_file_size_mb = float(os.stat(local_file_path).st_size) / 1e6
        if local_file_size_mb > DIRECT_UPLOAD_CUTOFF_MB:
            transfer_stats = self.upload_file_chunked(url, local_file_path)
        else:
            transfer_stats = self.upload_file_direct(url, local_file_path)

        return transfer_stats

    def upload_file_chunked(self, url, local_file_path):

        file_size = os.stat(local_file_path).st_size
        start_time = time.time()

        # Check if file exists
        file_id = None
        try:
            file_id = self.get_file_id(url)
        except IOError:
            (filename, folder_tree) = self.parse_url(url)
            parent_ids = self.get_parent_folders_ids(folder_tree, create_if_missing = True)
            parent_id = parent_ids[-1]

        if file_id is None:

            # Upload a new file
            data = {
                "folder_id": parent_id,
                "file_size": file_size,
                "file_name": filename,
            }
            session_url = 'https://upload.box.com/api/2.0/files/upload_sessions'

        else:

            # Upload a new version of the file
            data = {"file_size": file_size}
            session_url = 'https://upload.box.com/api/2.0/files/{0}/upload_sessions'.format(file_id)

        # initialize the session
        self.reload_token()
        headers = self.headers.copy()
        connection_start_time = time.time()
        response = requests.post(session_url, headers = headers, json = data)
        response.raise_for_status()
        session = response.json()

        # upload the file in parts defined by the session
        session_id = session['id']
        upload_url = session['session_endpoints']['upload_part']
        part_size = session['part_size']
        parts = []
        with open(local_file_path, 'rb') as f:
            file_sha1 = sha1()
            while True:
                part = f.read(part_size)
                if not part:
                    break
                file_sha1.update(part)
                part_sha1 = sha1(part)

                digest = "sha={0}".format(b64encode(part_sha1.digest()).decode('utf-8'))
                content_range = "bytes {0}-{1}/{2}".format(len(parts) * part_size, len(parts) * part_size + len(part) - 1, file_size)
                content_type = "application/octet-stream"

                for part_tries in range(3):
                    self.reload_token()
                    headers = self.headers.copy()
                    headers['Digest'] = digest
                    headers['Content-Range'] = content_range
                    headers['Content-Type'] = content_type

                    # retry each part up to three times
                    try:
                        response = requests.put(upload_url, headers = headers, data = part)
                        response.raise_for_status()
                    except Exception as err:
                        if part_tries >= 2:
                            raise err
                        else:
                            pass
                    else:
                        try:
                            parts.append(response.json()['part'])
                        except Exception as err:
                            if part_tries >= 2:
                                raise err
                            else:
                                pass
                        else:
                            break

        # commit the session
        commit_url = session['session_endpoints']['commit']
        digest = "sha={0}".format(b64encode(file_sha1.digest()).decode('utf-8'))
        data = {'parts': parts}

        self.reload_token()
        headers = self.headers.copy()
        headers['Digest'] = digest

        response = requests.post(commit_url, headers = headers, json = data)
        response.raise_for_status()

        file_size = int(response.json()['entries'][0]['size'])

        end_time = time.time()

        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'https',
            'TransferType': 'upload',
            'TransferFileName': local_file_path,
            'TransferFileBytes': file_size,
            'TransferTotalBytes': file_size,
            'TransferStartTime': int(start_time),
            'TransferEndTime': int(end_time),
            'ConnectionTimeSeconds': end_time - connection_start_time,
            'TransferHostName': urlparse(str(upload_url)).netloc,
            'TransferLocalMachineName': socket.gethostname(),
            'TransferUrl': 'https://upload.box.com/api/2.0/files/upload_sessions',
        }

        return transfer_stats

    def upload_file_direct(self, url, local_file_path):

        start_time = time.time()

        # Check if file exists
        file_id = None
        try:
            file_id = self.get_file_id(url)
        except IOError:
            (filename, folder_tree) = self.parse_url(url)
            parent_ids = self.get_parent_folders_ids(folder_tree, create_if_missing = True)
            parent_id = parent_ids[-1]

        files = {
            'file': open(local_file_path, 'rb'),
        }

        if file_id is None:

            # Upload a new file
            data = {
            'attributes': json.dumps({
                'name': filename,
                'parent': {'id': parent_id},
                }),
            }

            upload_url = 'https://upload.box.com/api/2.0/files/content'
            self.reload_token()
            connection_start_time = time.time()
            response = requests.post(upload_url, headers = self.headers, data = data, files = files)

        else:

            # Upload a new version of the file
            upload_url = 'https://upload.box.com/api/2.0/files/{0}/content'.format(file_id)
            self.reload_token()
            connection_start_time = time.time()
            response = requests.post(upload_url, headers = self.headers, files = files)

        response.raise_for_status()

        try:
            content_length = int(response.request.headers['Content-Length'])
        except (ValueError, KeyError):
            content_length = False
        file_size = int(response.json()['entries'][0]['size'])

        end_time = time.time()

        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'https',
            'TransferType': 'upload',
            'TransferFileName': local_file_path,
            'TransferFileBytes': file_size,
            'TransferTotalBytes': content_length or file_size,
            'TransferStartTime': int(start_time),
            'TransferEndTime': int(end_time),
            'ConnectionTimeSeconds': end_time - connection_start_time,
            'TransferHostName': urlparse(str(upload_url)).netloc,
            'TransferLocalMachineName': socket.gethostname(),
            'TransferUrl': 'https://upload.box.com/api/2.0/files/content',
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
        del os.environ['HTTPS_PROXY']
    except Exception:
        pass

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
        running_plugins = {}
        with open(args['outfile'], 'w') as outfile:
            for ad in infile_ads:
                tries = 0
                try:
                    token_name = get_token_name(ad['Url'])
                    token_path = get_token_path(token_name)

                    # Use existing plugin objects if possible because they have
                    # cached object ids, which make path lookups much faster in
                    # the case of multiple file downloads/uploads.
                    if token_path in running_plugins:
                        box = running_plugins[token_path]
                    else:
                        box = BoxPlugin(token_path)
                        running_plugins[token_path] = box

                    while tries < 3:
                        tries += 1
                        try:
                            if not args['upload']:
                                outfile_dict = box.download_file(ad['Url'], ad['LocalFileName'])
                            else:
                                outfile_dict = box.upload_file(ad['Url'], ad['LocalFileName'])
                        except IOError as err:
                            # Retry on socket closed unexpectedly
                            if (err.errno == 32) and (tries < 3):
                                pass
                            else:
                                raise err
                        else:
                            break

                    outfile.write(str(classad.ClassAd(outfile_dict)))

                except Exception as err:
                    try:
                        outfile_dict = get_error_dict(err, url = ad['Url'])
                        outfile.write(str(classad.ClassAd(outfile_dict)))
                    except Exception:
                        pass

                    # Ask condor_starter to retry on 401
                    if (isinstance(err, requests.exceptions.HTTPError)
                        and err.response.status_code == 401):
                        sys.exit(1)
                    else:
                        sys.exit(-1)

    except Exception:
        sys.exit(-1)
