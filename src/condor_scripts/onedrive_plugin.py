#!/usr/bin/env python3
'''OneDrive File Transfer Plugin

Known limitations:
--------------------

1. You can create directory structures deeper than you can recursively
download. As of 06/2026 the maximum path is 400 characters (as
determined by a HTTP 409 error message), so uploading something like
1/2/3/.../117/118/ is possible, but when trying to recursively
download you can't get deeper than 1/2/.../26. A Microsoft employee
commented on forum post that the number of '/' chracters seems to be
limited to around 30, and there are /'s before the API path begins
that count towards the 30.

2. Uploading a directory maintains the directory structure as if
preserve_relative_paths=True was set. The other cloud plugins also
seem to do this, and there currently isn't a way to tell the plugin if
it should or shouldn't collapse the paths.

'''


import os
import sys
import socket
import time
from urllib.parse import urlparse
import posixpath
import json
import mimetypes

import requests
import classad2 as classad

TOKEN_DIR_ENV_NAME = '_CONDOR_CREDS'
TOKEN_FILE_EXT = '.use'

DEFAULT_TIMEOUT = 30

ONEDRIVE_PLUGIN_VERSION = '1.1.0'

ONEDRIVE_API_VERSION = 'v1.0'

ONEDRIVE_API_BASE_URL = "https://graph.microsoft.com/" + \
    ONEDRIVE_API_VERSION + "/me/drive/"

# Two methods of uploading, depending on file size:
# Simple - One shot upload (limited size)
# Session - Create session, upload chunks of file
# Microsoft recommends files larger than 10MB be chunked
DIRECT_UPLOAD_CUTOFF_MB = 10

# (Chunk size must be multiple of 320KB according to docs)
# 60MB (2048 * 320KB)
CHUNK_SIZE = 60 * 1024 * 1024

# Define a number of times to re-attempt the transfer (upload & download)
MAX_RETRIES = 3


def print_help(stream=sys.stderr):
    '''Prints usage message to stderr'''

    help_msg = '''\
Usage: {0} [-upload] -infile <input-filename> -outfile <output-filename>
       {0} -classad

Options:
  -classad                    Print a ClassAd containing the capablities of
                              this file transfer plugin.
  -infile <input-filename>    Input ClassAd file
  -outfile <output-filename>  Output ClassAd file
  -upload                     Upload instead of download
'''
    stream.write(help_msg.format(sys.argv[0]))


def print_capabilities():
    '''Write a classad of the supported capabilities of this plugin'''

    capabilities = {
         'MultipleFileSupport': True,
         'PluginType': 'FileTransfer',
         'SupportedMethods': 'onedrive',
         'Version': ONEDRIVE_PLUGIN_VERSION,
    }
    sys.stdout.write(classad.ClassAd(capabilities).printOld())


def parse_args():
    '''The optparse library can't handle the types of arguments that
    the file transfer plugin sends, the argparse library can't be
    expected to be found on machines running EL 6 (Python 2.6), and a
    plugin should not reach outside the standard library, so the
    plugin must roll its own argument parser. The expected input is
    very rigid, so this isn't too awful.
    '''

    # The only argument lists that are acceptable are
    # <this> -classad
    # <this> -infile <input-filename> -outfile <output-filename>
    # <this> -outfile <output-filename> -infile <input-filename>
    if not len(sys.argv) in [2, 5, 6]:
        print_help()
        sys.exit(1)

    # If -classad, print the capabilities of the plugin and exit early
    elif (len(sys.argv) == 2) and (sys.argv[1] == '-classad'):
        print_capabilities()
        sys.exit(0)

    infile = None
    outfile = None

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
    try:
        for i, arg in enumerate(sys.argv):
            if i == 0:
                continue

            if arg == '-infile':
                infile = sys.argv[i+1]
            elif arg == '-outfile':
                outfile = sys.argv[i+1]
    except IndexError:
        print_help()
        sys.exit(1)

    return {'infile': infile, 'outfile': outfile, 'upload': is_upload}


def get_token_name(url):
    '''Return the name of the token from the given URL'''
    scheme = url.split('://')[0]
    if '+' in scheme:
        (handle, provider) = scheme.split('+')
        token_name = f'{provider}_{handle}'
    else:
        token_name = scheme

    return token_name


def get_token_path(token_name):
    '''Finds the token's path or raises an exception'''
    if TOKEN_DIR_ENV_NAME in os.environ:
        cred_dir = os.environ[TOKEN_DIR_ENV_NAME]
    else:
        raise KeyError(f"Required variable '{TOKEN_DIR_ENV_NAME}' was not " +
                       "found in job's environment")

    token_path = os.path.join(cred_dir, token_name + TOKEN_FILE_EXT)

    if not os.path.exists(token_path):
        raise IOError(2, 'Token file not found', token_path)

    return token_path


def format_error(error):
    '''Return a formatted string of the error message'''
    return f'{type(error).__name__}: {str(error)}'


def get_error_dict(error, url=''):
    '''Return a ditionary of the failure, including URL and formatted
    error message'''
    error_string = format_error(error)
    error_dict = {
        'TransferSuccess': False,
        'TransferError': error_string,
        'TransferUrl': url,
    }

    return error_dict


class OneDrivePlugin:
    '''The OneDrive specific features live here'''

    def __init__(self, token_path):
        self.token_path = token_path
        self.token = self.get_token(self.token_path)
        self.headers = {'Authorization': f'Bearer {self.token}'}
        self.path_ids = {'/': self.get_item('/')['id']}

    def get_token(self, token_path):
        '''Load the access token'''
        with open(token_path, 'r', encoding="utf8") as f:
            access_token = json.load(f)['access_token']
        return access_token

    def reload_token(self):
        '''Read the token from the disk and (re)set the header'''
        self.token = self.get_token(self.token_path)
        self.headers['Authorization'] = f'Bearer {self.token}'

    def parse_url(self, url):
        '''Transform the URL to a list of folders and a filename'''

        # Build the folder tree
        parsed_url = urlparse(url)
        folder_tree = []
        pop_index = 0

        if parsed_url.netloc != '':
            # No leading '/' character before directory or filename
            pop_index = -1
            folder_tree.append(parsed_url.netloc)
        if parsed_url.path != '':
            path = posixpath.split(parsed_url.path)
            while path[1] != '':
                folder_tree.insert(1, path[1])
                path = posixpath.split(path[0])

        # Separate the filename from the directory list
        filename = folder_tree.pop(pop_index)

        return (filename, folder_tree)

    def api_call(self, endpoint, method='GET', params=None, data=None):
        '''Make a generic API call and get the response'''
        # It's not safe to set a default argument to {}, reset it here instead
        data = {} if data is None else data
        url = ONEDRIVE_API_BASE_URL + endpoint
        self.reload_token()
        headers = self.headers.copy()
        headers['Content-Type'] = 'application/json'
        kwargs = {
            'headers': headers,
            'timeout': DEFAULT_TIMEOUT,
        }
        if method in ['GET'] and params is not None:
            kwargs['params'] = params
        if method in ['POST', 'PUT', 'PATCH']:
            kwargs['data'] = data
        response = requests.request(method, url, **kwargs)
        response.raise_for_status()
        return response.json()

    def create_folder(self, folder_name, parent_id):
        '''Create a new folder under the parent_id folder, returning
        the id of the new folder'''
        if parent_id == self.path_ids['/']:
            # It seems to be invalid to refer to root by its id?
            endpoint = 'root/children'
        else:
            endpoint = f'items/{parent_id}/children'

        data = json.dumps({
            'name': folder_name,
            'folder': {},
            '@microsoft.graph.conflictBehavior': 'fail',
        })

        try:
            folder_id = self.api_call(endpoint, 'POST', data=data)['id']
        except requests.exceptions.HTTPError as e:
            if e.response.status_code == 409:
                # folder already exists, using existing id if possible
                if 'id' in e.response.json():
                    folder_id = e.response.json()['id']
                else:
                    raise e
            else:
                raise e
        return folder_id

    def get_item(self, item_path):
        '''Return driveItem from full path'''
        endpoint = f'root:{item_path}' if item_path != '/' else 'root'

        try:
            drive_item = self.api_call(endpoint, 'GET')
        except requests.exceptions.HTTPError as e:
            if e.response.status_code == 404:  # Item not found
                raise IOError(2, 'File not found', f'{item_path}') from e
            raise e
        return drive_item

    def get_parent_folders_ids(self, folder_tree, create_if_missing=False):
        '''Build a list of all parent ids in the folder_tree, creating
        missing directories if requested'''

        # Traverse the folder tree, starting at the root
        parent_ids = [self.path_ids['/']]
        searched_path = ''
        for folder_name in folder_tree:
            searched_path += f'/{folder_name}'
            if searched_path in self.path_ids:  # Check the cached ids
                parent_id = self.path_ids[searched_path]
            else:
                try:
                    parent_id = self.get_item(searched_path)['id']
                except IOError as e:
                    if create_if_missing:
                        parent_id = \
                            self.create_folder(folder_name,
                                               parent_id=parent_ids[-1])
                    else:
                        raise IOError(2, 'Folder not found', searched_path) \
                            from e
                # Update the cached ids
                self.path_ids[searched_path] = parent_id
            parent_ids.append(parent_id)

        return parent_ids

    def get_children(self, item_id):
        '''Return a list of all driveItem resources for a given folder id'''

        children = []

        if item_id != self.path_ids['/']:
            endpoint = f'items/{item_id}/children'
        else:
            endpoint = 'root/children'

        response = self.api_call(endpoint)
        children.extend(response['value'])

        while '@odata.nextLink' in response:
            # More data in the folder, need to ask next URL for more
            self.reload_token()
            response = requests.get(response['@odata.nextLink'],
                                    headers=self.headers,
                                    timeout=DEFAULT_TIMEOUT)
            response.raise_for_status()
            response = response.json()
            children.extend(response['value'])

        return children

    def download_folder(self, item_id, local_file_path):
        '''Recursively download the contents of the folder specified
        at item_id, putting into the local_file_path'''

        os.mkdir(local_file_path)

        transfer_stats = {
            'TransferSuccess': True,
            'TransferProtocol': 'https',
            'TransferType': 'download',
            'TransferFileBytes': 0,
            'TransferTotalBytes': 0,
            'TransferFileName': local_file_path,
            'TransferLocalMachineName': socket.gethostname(),
        }

        connection_start_time = time.time()
        children = self.get_children(item_id)
        for c in children:
            local_child_path = os.path.join(local_file_path, c['name'])
            if 'folder' in c:
                tmp_stats = self.download_folder(c['id'], local_child_path)
            else:
                tmp_stats = self.download_file_helper(local_child_path,
                                                      item_id=c['id'])

            transfer_stats['TransferFileBytes'] \
                += tmp_stats['TransferFileBytes']
            transfer_stats['TransferTotalBytes'] \
                += tmp_stats['TransferTotalBytes']

        end_time = time.time()

        # Update final stats
        transfer_stats['TransferEndTime'] = int(end_time)
        transfer_stats['ConnectionTimeSeconds'] = end_time \
            - connection_start_time

        return transfer_stats

    def download_file_helper(self, local_path, item_id=None, file_path=None):
        '''The real download logic lives here. Download a file given
        its id or path, saving it to the local_path'''

        if item_id:
            endpoint = f'items/{item_id}/content'
        else:
            endpoint = f'root:/{file_path}:/content'

        url = ONEDRIVE_API_BASE_URL + endpoint

        # Stream the data to disk, chunk by chunk,
        # instead of loading it all into memory.
        connection_start_time = time.time()
        response = requests.get(url, headers=self.headers, stream=True,
                                timeout=DEFAULT_TIMEOUT)

        try:
            response.raise_for_status()

            try:
                content_length = int(response.headers['Content-Length'])
            except (ValueError, KeyError):
                content_length = False

            with open(local_path, 'wb') as f:
                file_size = 0
                for chunk in response.iter_content(chunk_size=8192):
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
            'TransferFileName': local_path,
            'TransferFileBytes': file_size,
            'TransferTotalBytes': content_length or file_size,
            'TransferEndTime': int(end_time),
            'ConnectionTimeSeconds': end_time - connection_start_time,
            'TransferHostName': urlparse(response.url.encode()).netloc,
            'TransferLocalMachineName': socket.gethostname(),
        }

        return transfer_stats

    def download_file(self, url, local_file_path):
        '''Download a file or directory from the given URL to the
        specified local path'''

        start_time = time.time()
        file_path = url[url.find('://')+3:]

        if not file_path.startswith('/'):
            file_path = '/' + file_path

        # Check if file_path is a directory
        drive_item = self.get_item(file_path)
        if 'folder' in drive_item:
            transfer_stats = self.download_folder(drive_item['id'],
                                                  local_file_path)
        else:
            transfer_stats = \
                self.download_file_helper(local_file_path,
                                          item_id=drive_item['id'])

        # Start time was set by this method, not one of the helper methods
        transfer_stats['TransferStartTime'] = start_time

        return transfer_stats

    def upload_file(self, url, local_file_path):
        '''Generic file upload interface, determines which method to
        use based on file size limits'''

        local_file_size_mb = float(os.stat(local_file_path).st_size) / 1e6
        if local_file_size_mb > DIRECT_UPLOAD_CUTOFF_MB:
            transfer_stats = self.upload_file_chunked(url, local_file_path)
        else:
            transfer_stats = self.upload_file_direct(url, local_file_path)

        return transfer_stats

    def get_upload_endpoint(self, url, direct=True):
        '''Get the end point for uploading a new file, or to replace
        an existing file'''

        command = 'content' if direct else 'createUploadSession'

        try:
            file_path = urlparse(url).path
            file_id = self.get_item(file_path)['id']
            # Upload a new version of the file
            endpoint = f'items/{file_id}/{command}'
        except IOError:
            # Upload a new file
            (filename, folder_tree) = self.parse_url(url)
            parent_ids = self.get_parent_folders_ids(folder_tree,
                                                     create_if_missing=True)
            parent_id = parent_ids[-1]
            # Can't refer to root's id?
            if parent_id == self.path_ids['/']:
                endpoint = f'root:/{filename}:/{command}'
            else:
                endpoint = f'items/{parent_id}:/{filename}:/{command}'

        return endpoint

    def upload_file_direct(self, url, local_file_path):
        '''Create or replace a file under the size limit'''

        start_time = time.time()

        upload_url = ONEDRIVE_API_BASE_URL + self.get_upload_endpoint(url)

        headers = self.headers.copy()
        headers['Content-Type'] = mimetypes.guess_type(local_file_path)[0] \
            or 'application/octet-stream'

        for retries in range(MAX_RETRIES):
            self.reload_token()

            connection_start_time = time.time()
            with open(local_file_path, 'rb') as f:
                response = requests.put(upload_url, headers=headers, data=f)

                try:
                    response.raise_for_status()
                except requests.exceptions.HTTPError as e:
                    # Give up after exceeding MAX_RETRIES
                    if retries == (MAX_RETRIES-1):
                        raise e

                    # On a server error, give the server some more time
                    # before trying again
                    if e.response.status_code >= 500:
                        time.sleep((retries + 1) * 10)

                    # Retry
                    continue

                try:
                    content_length = \
                        int(response.request.headers['Content-Length'])
                except (ValueError, KeyError):
                    content_length = False

                file_size = int(response.json()['size'])

                # Success, exit the retry loop
                break

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
            'TransferUrl': upload_url,
        }

        return transfer_stats

    def upload_file_chunked_helper(self, url, local_file_path):
        '''The man loop of the chunked file upload. Returns the final
        response or throws the HTTP exception'''

        file_size = os.stat(local_file_path).st_size
        parts = 0
        with open(local_file_path, 'rb') as f:
            while True:
                part = f.read(CHUNK_SIZE)
                if not part:
                    break

                len_part = len(part)
                start = parts * CHUNK_SIZE
                end = parts * CHUNK_SIZE + len_part - 1
                content_range = f'bytes {start}-{end}/{file_size}'

                for part_tries in range(MAX_RETRIES):
                    self.reload_token()
                    headers = self.headers.copy()
                    headers['Content-Type'] = 'application/octet-stream'
                    headers['Content-Length'] = str(len_part)
                    headers['Content-Range'] = content_range

                    # retry each part up to three times
                    try:
                        response = requests.put(url, headers=headers,
                                                data=part)
                        response.raise_for_status()
                    except requests.exceptions.HTTPError as err:
                        if part_tries >= (MAX_RETRIES-1):
                            raise err
                    else:
                        parts += 1
                        break
        return response

    def upload_file_chunked(self, url, local_file_path):
        '''Create or replace a file by uploading multiple requests'''

        start_time = time.time()

        endpoint = self.get_upload_endpoint(url, direct=False)

        data = json.dumps({
            'item': {
                '@microsoft.graph.conflictBehavior': 'replace',
                'name': self.parse_url(url)[0],
            },
            'deferCommit': False,
        })

        connection_start_time = time.time()

        # initialize the session
        try:
            session = self.api_call(endpoint, 'POST', data=data)
        except requests.exceptions.HTTPError as e:
            raise e

        # upload the file in parts defined by the session
        upload_url = session['uploadUrl']
        response = self.upload_file_chunked_helper(upload_url, local_file_path)
        file_size = int(response.json()['size'])
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
            'TransferUrl': ONEDRIVE_API_BASE_URL + endpoint,
        }

        return transfer_stats


def main():
    '''
    Per the design doc, all failures should result in exit code 1.
    This is true even if we cannot write a ClassAd to the outfile,
    so we catch all exceptions, try to write to the outfile if we can
    and always exit 1 on error.

    Exiting 1 without an outfile thus means one of two things:
    1. Couldn't parse arguments.
    2. Couldn't open outfile for writing.
    '''

    # parse_args() already calls sys.exit() on failure
    args = parse_args()

    try:
        with open(args['outfile'], 'w', encoding='utf8') as outfile:
            with open(args['infile'], 'r', encoding='utf8') as infile:
                infile_ads = classad.parseAds(infile)

                running_plugins = {}
                for ad in infile_ads:
                    token_name = get_token_name(ad['Url'])
                    token_path = get_token_path(token_name)

                    # Use existing plugin objects if possible because they have
                    # cached object ids, which make path lookups much faster in
                    # the case of multiple file downloads/uploads.
                    if token_path in running_plugins:
                        onedrive = running_plugins[token_path]
                    else:
                        onedrive = OneDrivePlugin(token_path)
                        running_plugins[token_path] = onedrive

                    if not args['upload']:
                        outfile_dict = \
                            onedrive.download_file(ad['Url'],
                                                   ad['LocalFileName'])
                    else:
                        outfile_dict = \
                            onedrive.upload_file(ad['Url'],
                                                 ad['LocalFileName'])

                    outfile.write(str(classad.ClassAd(outfile_dict)))

    except (OSError, requests.exceptions.RequestException) as err:
        try:
            with open(args['outfile'], 'a', encoding='utf8') as outfile:
                outfile_dict = get_error_dict(err, url=ad['Url'])
                outfile.write(str(classad.ClassAd(outfile_dict)))
        finally:
            sys.exit(1)


if __name__ == '__main__':
    main()
