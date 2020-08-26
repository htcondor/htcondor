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
import mimetypes

import requests
import classad

TOKEN_DIR_ENV_NAME = '_CONDOR_CREDS'
TOKEN_FILE_EXT = '.use'

DEFAULT_TIMEOUT = 30

GDRIVE_PLUGIN_VERSION = '1.0.0'

GDRIVE_API_VERSION = 'v3'
GDRIVE_API_BASE_URL = 'https://www.googleapis.com/drive/' + GDRIVE_API_VERSION

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
         'SupportedMethods': 'gdrive',
         'Version': GDRIVE_PLUGIN_VERSION,
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
    elif (len(sys.argv) == 2) and (sys.argv[1] == '-classad'):
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

class GDrivePlugin:

    def __init__(self, token_path):
        self.token_path = token_path        
        self.token = self.get_token(self.token_path)
        self.headers = {'Authorization': 'Bearer {0}'.format(self.token)}
        self.path_ids = {'/': u'root'}

    def get_token(self, token_path):
        with open(token_path, 'r') as f:
            access_token = json.load(f)['access_token']
        return access_token

    def reload_token(self):
        self.token = self.get_token(self.token_path)
        self.headers['Authorization'] = 'Bearer {0}'.format(self.token)
        
    def get_headers_copy(self):
        self.reload_token()
        headers = {'Authorization': 'Bearer {0}'.format(self.token)}
        return headers

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

    def api_call(self, endpoint, method = 'GET', params = None, data = {}, headers = None):
        url = GDRIVE_API_BASE_URL + endpoint
        if headers is None:
            headers = self.get_headers_copy()
        
        kwargs = {
            'headers': headers,
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

        endpoint = '/files'
        data = json.dumps({
            'name': folder_name,
            'parents': [parent_id],
            'mimeType': 'application/vnd.google-apps.folder',
        })
        headers = self.get_headers_copy()
        headers['Content-Type'] = 'application/json'
        
        folder_info = self.api_call(endpoint, 'POST', data = data, headers = headers)

        return folder_info['id']

    def format_query(self, q):
        folder_comparison = ['!=', '=']
        query_strings = ['trashed = false']

        if 'name' in q:
            query_strings.append("name = '{0}'".format(q['name']))

        if 'is_folder' in q:
            query_strings.append(
                "mimeType {0} 'application/vnd.google-apps.folder'".format(
                    folder_comparison[q['is_folder']]))
        else: # assume not a folder
            query_strings.append(
                "mimeType != 'application/vnd.google-apps.folder'")

        if 'parents' in q:
            query_strings.append("'{0}' in parents".format(q['parents']))

        query = " and ".join(query_strings)
        return query

    def get_object_id(self, object_name, object_type, object_parents):
        endpoint = '/files'
        fields = ['id', 'name']
        limit = 1000

        query = {
            'parents': object_parents,
            'name': object_name,
            'is_folder': object_type == 'folder',
        }
        
        params = {
            'q': self.format_query(query),
            'fields': 'files(' + ','.join(fields) + '),nextPageToken',
            'pageSize': limit,
        }

        file_items = self.api_call(endpoint, params = params)

        object_found = False
        while not object_found:
            for entry in file_items['files']:
                if entry['name'] == object_name:
                    object_id = entry['id']
                    object_found = True
                    break

            # Go to next page if it exists and if haven't found object yet
            if object_found:
                pass
            elif (('nextPageToken' in file_items) and
                    (file_items['nextPageToken'] not in [None, ''])):
                params['pageToken'] = file_items['nextPageToken']
                file_items = self.api_call(endpoint, params = params)
            else:
                raise IOError(2, 'Object not found', object_name)

        return object_id

    def get_parent_folders_ids(self, folder_tree, create_if_missing = False):

        # Traverse the folder tree, starting at the root (id = 0)
        parent_ids = [u'root']
        searched_path = ''
        for folder_name in folder_tree:
            searched_path += '/{0}'.format(folder_name)
            if searched_path in self.path_ids: # Check the cached ids
                parent_id = self.path_ids[searched_path]
            else:
                try:
                    parent_id = self.get_object_id(folder_name, 'folder', parent_ids[-1])
                except IOError:
                    if create_if_missing:
                        parent_id = self.create_folder(folder_name, parent_id = parent_ids[-1])
                    else:
                        raise IOError(2, 'Folder not found in Google Drive', searched_path)
                self.path_ids[searched_path] = parent_id # Update the cached ids
            parent_ids.append(parent_id)

        return parent_ids
    
    def get_file_id(self, url):
        
        # Parse out the filename and folder_tree and get folder_ids
        (filename, folder_tree) = self.parse_url(url)
        parent_ids = self.get_parent_folders_ids(folder_tree)
        
        try:
            file_id = self.get_object_id(filename, 'file', parent_ids[-1])
        except IOError:
            raise IOError(2, 'File not found in Google Drive', '{0}/{1}'.format('/'.join(folder_tree), filename))

        return file_id

    def download_file(self, url, local_file_path):

        start_time = time.time()
        
        file_id = self.get_file_id(url)
        
        endpoint = '/files/{0}'.format(file_id)
        url = GDRIVE_API_BASE_URL + endpoint
        params = {'alt': 'media'} # https://developers.google.com/drive/api/v3/manage-downloads

        # Stream the data to disk, chunk by chunk,
        # instead of loading it all into memory.
        self.reload_token()
        connection_start_time = time.time()
        response = requests.get(url, headers = self.headers, params = params,
                                    stream = True, timeout = DEFAULT_TIMEOUT)

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

        start_time = time.time()

        # Check if file exists
        file_id = None
        try:
            file_id = self.get_file_id(url)
            metadata = None
        except IOError:
            (filename, folder_tree) = self.parse_url(url)
            parent_ids = self.get_parent_folders_ids(folder_tree, create_if_missing = True)
            parent_id = parent_ids[-1]
            metadata = {
                'name': filename,
                'parents': [parent_id]
            }
                
        # Not actually "files" but requests will turn these tuples into
        # multipart/form-data in the correct order (metadata, then file).
        params = {'uploadType': 'multipart'}
        files = (
            ('metadata', (
                None,
                json.dumps(metadata),
                'application/json; charset=UTF-8')),
            ('file', (
                os.path.basename(local_file_path),
                open(local_file_path, 'rb'),
                mimetypes.guess_type(local_file_path)[0] or 'application/octet-stream'))
            )

        self.reload_token()
        if file_id is None:

            # Upload a new file
            upload_url = 'https://www.googleapis.com/upload/drive/v3/files'
            connection_start_time = time.time()
            response = requests.post(upload_url, headers = self.headers, params = params, files = files)

        else:

            # Upload a new version of the file
            upload_url = 'https://www.googleapis.com/upload/drive/v3/files/{0}'.format(file_id)
            connection_start_time = time.time()
            response = requests.patch(upload_url, headers = self.headers, params = params, files = files)

        response.raise_for_status()

        try:
            content_length = int(response.request.headers['Content-Length'])
        except (ValueError, KeyError):
            content_length = False

        try:
            file_id = response.json()['id']
            endpoint = '/files/{0}'.format(file_id)
            params = {'fields': 'size'}
            file_size = int(self.api_call(endpoint, params = params)['size'])
        except Exception:
            file_size = os.stat(local_file_path).st_size

        end_time = time.time()

        # Including TransferUrl makes little sense here, too.
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
            'TransferHostName': urlparse(upload_url).netloc,
            'TransferLocalMachineName': socket.gethostname(),
            'TransferUrl': 'https://www.googleapis.com/upload/drive/v3/files',
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
        running_plugins = {}
        with open(args['outfile'], 'w') as outfile:
            for ad in infile_ads:
                try:
                    token_name = get_token_name(ad['Url'])
                    token_path = get_token_path(token_name)

                    # Use existing plugin objects if possible because they have
                    # cached object ids, which make path lookups much faster in
                    # the case of multiple file downloads/uploads.
                    if token_path in running_plugins:
                        gdrive = running_plugins[token_path]
                    else:
                        gdrive = GDrivePlugin(token_path)
                        running_plugins[token_path] = gdrive

                    if not args['upload']:
                        outfile_dict = gdrive.download_file(ad['Url'], ad['LocalFileName'])
                    else:
                        outfile_dict = gdrive.upload_file(ad['Url'], ad['LocalFileName'])
                        
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
