from credmon.CredentialMonitors.AbstractCredentialMonitor import AbstractCredentialMonitor
from credmon.utils import atomic_rename
import os
import ssl
import time
import json
import glob
import tempfile
import re
import urllib3
import socket
import http.client

try:
    import htcondor
except ImportError:
    htcondor = None

# This class supports one vault host, possibly round-robin, using a
# HTTPSConnectionPool to continue to use the same IP address of the
# host for up to 5 minutes.  If there is a connection timeout the IP
# address that caused it is removed from consideration until 5 minutes
# have passed.
class vaulthost:
    updatetime = 0
    ips = []
    pool = None
    host = ""
    port = 0
    log = None
    capath = ""
    cafile = ""

    def __init__(self, parsedurl, log, capath, cafile):
        self.host = parsedurl.host
        self.port = parsedurl.port
        self.log = log
        self.capath = capath
        self.cafile = cafile

    def getips(self):
        info = socket.getaddrinfo(self.host, 0, 0, 0, socket.IPPROTO_TCP)

        self.ips = []
        # Use only IPv4 addresses if there are any
        for tuple in info:
            if tuple[0] == socket.AF_INET:
                self.ips.append(tuple[4][0])
        if len(self.ips) == 0:
            # Otherwise use the IPv6 addresses
            for tuple in info:
                if tuple[0] == socket.AF_INET6:
                    self.ips.append(tuple[4][0])
        if len(self.ips) == 0:
            raise RuntimeError('no ip address found for', self.host)

    def newpool(self):
        if self.pool != None:
            self.pool.close()
        self.pool = urllib3.HTTPSConnectionPool(self.ips[0],
                            timeout=urllib3.util.Timeout(connect=2, read=10),
                            assert_hostname=self.host, port=self.port,
                            ca_cert_dir=self.capath, ca_certs=self.cafile)

    def request(self, path, headers, fields):
        now = time.time()
        if (now - self.updatetime) > (5 * 60):
            self.updatetime = now
            self.getips()
            self.newpool()
        while len(self.ips) > 0:
            try:
                resp = self.pool.request('GET', path, headers=headers, fields=fields, retries=0)
            except urllib3.exceptions.MaxRetryError as e:
                if type(e.reason) != urllib3.exceptions.ConnectTimeoutError:
                    raise e
                if len(self.ips) == 1:
                    raise e
                self.log.info('Connection timeout on %s ip %s, trying %s', self.host, self.ips[0], self.ips[1])
                del self.ips[0]
                self.newpool()
            else:
                if resp.status != 200:
                    jsondata = json.loads(resp.data.decode())
                    errormsg = http.client.responses[resp.status] + ":"
                    if 'errors' in jsondata:
                        for error in jsondata['errors']:
                            errormsg += ' ' + error.encode('ascii','ignore').decode('ascii')
                    raise urllib3.exceptions.HTTPError(errormsg)

                return resp


class VaultCredmon(AbstractCredentialMonitor):

    cafile = ssl.get_default_verify_paths().cafile
    capath = '/etc/grid-security/certificates'
    vaulthosts = {}

    def __init__(self, *args, **kw):
        if htcondor is not None:
            if 'AUTH_SSL_CLIENT_CAFILE' in htcondor.param:
                self.cafile = htcondor.param['AUTH_SSL_CLIENT_CAFILE']
            if 'AUTH_SSL_CLIENT_CADIR' in htcondor.param:
                self.capath = htcondor.param['AUTH_SSL_CLIENT_CADIR']
        super(VaultCredmon, self).__init__(*args, **kw)

    def request_url(self, url, headers, params):
        parsedurl = urllib3.util.parse_url(url)

        # These checks are to avoid someone abusing the free-form of the URL
        # stored with condor_store_cred
        if parsedurl.scheme != 'https':
            raise RuntimeError('bad url: %s; only https is allowed' % url)
        if parsedurl.port != 8200:
            raise RuntimeError('bad url: %s; only port 8200 is allowed' % url)
        if parsedurl.path[0:3] != '/v1':
            raise RuntimeError('bad url: %s; only paths starting /v1 are allowed' % url)

        if parsedurl.host in self.vaulthosts:
            vault = self.vaulthosts[parsedurl.host]
        else:
            vault = vaulthost(parsedurl, self.log, self.capath, self.cafile)
            self.vaulthosts[parsedurl.host] = vault

        return vault.request(parsedurl.path, headers, params)

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
        if htcondor is not None and 'CREDMON_OAUTH_TOKEN_REFRESH' in htcondor.param:
            refresh_seconds = int(htcondor.param['CREDMON_OAUTH_TOKEN_REFRESH'])
        else:
            refresh_seconds = self.get_minimum_seconds() * 0.5
        create_time = os.path.getctime(access_token_path)
        refresh_time = create_time + refresh_seconds

        # check if token is past its refresh time
        if time.time() > refresh_time:
            return True

        # check if top file is newer than access token
        if os.path.exists(top_path):
            top_time = os.path.getctime(top_path)
            if top_time > create_time:
                return True

        return False

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

        vault_token = ''
        if 'vault_token' not in top_data:
            # The vault_token may instead be in a '.top' file without the
            # last portion of the name following an underscore, the handle.
            # The handle is added for reduced scopes and/or audience but it
            # doesn't get its own vault token.
            underscore = token_name.rfind('_')
            if underscore > 0:
                parenttop_name = token_name[0:underscore]
                parenttop_path = os.path.join(self.cred_dir, username, parenttop_name + '.top')
                try:
                    with open(parenttop_path, 'r') as f:
                        parenttop_data = json.load(f)
                except IOError as ie:
                    self.log.warning("Could not open %s: %s", parenttop_path, str(ie))
                    # This can happen when the parent was deleted because of
                    # permission denied
                    self.log.info("Deleting %s token for user %s because parent .top not available", token_name, username)
                    self.delete_tokens(username, token_name)
                    return False
                except ValueError as ve:
                    self.log.warning("The file at %s is invalid; could not parse as JSON: %s", parenttop_path, str(ve))
                    return False

                if 'vault_token' not in parenttop_data:
                    self.log.error("vault_token missing from %s and %s", top_path, parenttop_path)
                    return False
                vault_token = parenttop_data['vault_token']
            else:
                self.log.error("vault_token missing from %s", top_path)
                return False
        else:
            vault_token = top_data['vault_token']

        if 'vault_url' not in top_data:
            self.log.error("vault_url missing from %s", top_path)
            return False

        url = top_data['vault_url']
        if not url.startswith('https://'):
            self.log.error("vault_url does not begin with https://")
            return False

        headers = {'X-Vault-Token' : vault_token}
        params = {'minimum_seconds' : self.get_minimum_seconds()}
        try:
            response = self.request_url(url, headers, params)
        except Exception as e:
            self.log.error("Read of access token from %s failed: %s", url, str(e))
            if 'permission denied' in str(e):
                # the vault token is expired, might as well delete it
                self.log.info("Deleting %s token for user %s because permission denied", token_name, username)
                self.delete_tokens(username, token_name)
            return False
        try:
            response = json.loads(response.data.decode())
        except Exception as e:
            self.log.error("Could not parse json response from %s: %s", url, str(e))
            return False

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

            try:
                response = self.request_url(url, headers, params)
            except Exception as e:
                self.log.error("Read of exchanged access token from %s failed: %s", url, str(e))
                return False
            try:
                response = json.loads(response.data.decode())
            except Exception as e:
                self.log.error("Could not parse json response from %s: %s", url, str(e))
                return False

            if 'data' not in response or 'access_token' not in response['data']:
                self.log.error("Exchanged access_token missing in read from %s", url)
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

        if self.should_renew(username, token_name):
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
