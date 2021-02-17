from credmon.CredentialMonitors.AbstractCredentialMonitor import AbstractCredentialMonitor
from credmon.utils import atomic_rename
import os
import time
import json
import glob
import tempfile
import re
from urllib import request as urllib_request
from urllib import parse as urllib_parse

try:
    import htcondor
except ImportError:
    htcondor = None

class VaultCredmon(AbstractCredentialMonitor):

    def __init__(self, *args, **kw):
        super(VaultCredmon, self).__init__(*args, **kw)

    def get_minimum_seconds(self):
        if htcondor is not None and 'CREDMON_OAUTH_TOKEN_MINIMUM' in htcondor.param:
            return int(htcondor.param['CREDMON_OAUTH_TOKEN_MINIMUM'])
        return 40*60

    def should_renew(self, username, token_name):
        access_token_path = os.path.join(self.cred_dir, username, token_name + '.use')
        top_path = os.path.join(self.cred_dir, username, token_name + '.top')

        # check if access token exists
        if not os.path.exists(access_token_path):
            return True

        # compute token refresh time
        lifetime_fraction = 0.5
        create_time = os.path.getctime(access_token_path)
        refresh_time = create_time + (self.get_minimum_seconds() * lifetime_fraction)

        # check if token is past its refresh time
        if time.time() > refresh_time:
            return True

        # check if top file is newer than access token
        if os.path.exists(top_path):
            top_time = os.path.getctime(top_path)
            if top_time > create_time:
                return True

        return False

    def should_delete(self, username, token_name):
        top_path = os.path.join(self.cred_dir, username, token_name + '.top')

        if not os.path.exists(top_path):
            return False

        try:
            mtime = os.stat(top_path).st_mtime
        except OSError as e:
            self.log.error('could not stat %s', top_path)
            return False

        # if top file is older than 7 days (or CREDMON_OAUTH_TOKEN_LIFETIME if defined), delete tokens
        age = int(time.time() - mtime)
        self.log.debug('top file is %d seconds old', age)
        if htcondor is not None and 'CREDMON_OAUTH_TOKEN_LIFETIME' in htcondor.param:
            token_lifetime = int(htcondor.param['CREDMON_OAUTH_TOKEN_LIFETIME'])
        else:
            token_lifetime = 7*24*60*60
        if age > token_lifetime:
            return True

    def refresh_access_token(self, username, token_name):
        # load the vault token plus vault bearer token URL from .top file
        top_path = os.path.join(self.cred_dir, username, token_name + '.top')
        try:
            with open(top_path, 'r') as f:
                top_data = json.load(f)
        except IOError as ie:
            self.log.warning("Could not open %s: %s", top_path, str(ie))
            return False
        except ValueError as ve:
            self.log.warning("The file at %s is invalid; could not parse as JSON: %s", top_path, str(ve))
            return False

        if 'vault_token' not in top_data:
            self.log.error("vault_token missing from %s", top_path)
            return False

        if 'vault_url' not in top_data:
            self.log.error("vault_url missing from %s", top_path)
            return False

        params = {'minimum_seconds' : self.get_minimum_seconds()}
        url = top_data['vault_url'] + '?' + urllib_parse.urlencode(params)
        headers = {'X-Vault-Token' : top_data['vault_token']}
        request = urllib_request.Request(url=url, headers=headers)
        capath = '/etc/grid-security/certificates'
        if htcondor is not None and 'AUTH_SSL_CLIENT_CADIR' in htcondor.param:
            capath = htcondor.param['AUTH_SSL_CLIENT_CADIR']
        try:
            handle = urllib_request.urlopen(request, capath=capath)
        except Exception as e:
            self.log.error("read of access token from %s failed: %s", url, str(e))
            return False
        try:
            response = json.load(handle)
        except Exception as e:
            self.log.error("could not parse json response from %s: %s", url, str(e))
            return False

        finally:
            handle.close()

        if 'data' not in response or 'access_token' not in response['data']:
            self.log.error("access_token missing in read from %s", url)
            return False

        if 'scopes' in top_data or 'audience' in top_data:
            # Re-request access token with restricted scopes and/or audience.
            # These were not included in the initial request because currently
            #  this uses a separate token exchange flow in Vault which does
            #  not renew the refresh token, but we want that to happen too.
            # Just ignore the original access token in this case.

            if 'scopes' in top_data:
                params['scopes'] = top_data['scopes']
            if 'audience' in top_data:
                params['audience'] = top_data['audience']

            url = top_data['vault_url'] + '?' + urllib_parse.urlencode(params)

            try:
                handle = urllib_request.urlopen(request, capath=capath)
            except Exception as e:
                self.log.error("read of exchanged access token from %s failed: %s", url, str(e))
                return False
            try:
                response = json.load(handle)
            except Exception as e:
                self.log.error("could not parse json response from %s: %s", url, str(e))
                return False

            finally:
                handle.close()

            if 'data' not in response or 'access_token' not in response['data']:
                self.log.error("exchanged access_token missing in read from %s", url)
                return False

        access_token = response['data']['access_token']

        # write data to tmp file
        (tmp_fd, tmp_access_token_path) = tempfile.mkstemp(dir = self.cred_dir)
        with os.fdopen(tmp_fd, 'w') as f:
            f.write(access_token+'\n')

        # atomically move new file into place
        access_token_path = os.path.join(self.cred_dir, username, token_name + '.use')
        try:
            atomic_rename(tmp_access_token_path, access_token_path)
        except OSError as e:
            self.log.error(e)
            return False
        else:
            return True

    def delete_tokens(self, username, token_name):
        exts = ['.top', '.use']
        base_path = os.path.join(self.cred_dir, username, token_name)

        success = True
        for ext in exts:
            if os.path.exists(base_path + ext):
                try:
                    os.unlink(base_path + ext)
                except OSError as e:
                    self.log.debug('Could not remove %s: %s', base_path + ext, e.strerror)
                    success = False
            else:
                self.log.debug('Could not find %s', base_path + ext)
        return success

    def check_access_token(self, access_token_path):
        (basename, token_filename) = os.path.split(access_token_path)
        (cred_dir, username) = os.path.split(basename)
        token_name = os.path.splitext(token_filename)[0] # strip extension

        if self.should_delete(username, token_name):
            self.log.info('%s tokens for user %s are marked for deletion', token_name, username)
            success = self.delete_tokens(username, token_name)
            if success:
                self.log.info('Successfully deleted %s token files for user %s', token_name, username)
            else:
                self.log.error('Failed to delete all %s token files for user %s', token_name, username)

        elif self.should_renew(username, token_name):
            self.log.info('Refreshing %s token for user %s', token_name, username)
            success = self.refresh_access_token(username, token_name)
            if success:
                self.log.info('Successfully refreshed %s token for user %s', token_name, username)
            else:
                self.log.error('Failed to refresh %s token for user %s', token_name, username)

    def scan_tokens(self):
        # loop over all .top files in the cred_dir
        top_files = glob.glob(os.path.join(self.cred_dir, '*', '*.top'))

        for top_file in top_files:
            self.check_access_token(top_file)
