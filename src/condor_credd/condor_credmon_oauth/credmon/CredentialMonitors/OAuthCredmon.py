from credmon.CredentialMonitors.AbstractCredentialMonitor import AbstractCredentialMonitor
from credmon.utils import atomic_rename, api_endpoints
try:
    from requests_oauthlib import OAuth2Session
except ImportError:
    OAuth2Session = None
import os
import time
import json
import glob
import tempfile
import re

try:
    import htcondor
except ImportError:
    htcondor = None

class OAuthCredmon(AbstractCredentialMonitor):

    use_token_metadata = True

    @property
    def credmon_name(self):
        return "OAUTH"

    def __init__(self, *args, **kw):
        super(OAuthCredmon, self).__init__(*args, **kw)
        self.providers = kw.get("providers", set())

    def should_renew(self, username, token_name):

        access_token_path = os.path.join(self.cred_dir, username, token_name + '.use')
        metadata_path = os.path.join(self.cred_dir, username, token_name + '.meta')

        # check if access token exists
        if not os.path.exists(access_token_path):
            return True

        try:
            with open(access_token_path, 'r') as f:
                access_token = json.load(f)
        except IOError as ie:
            self.log.warning("Could not open access token %s: %s", access_token_path, str(ie))
            return True
        except ValueError as ve:
            self.log.warning("The access token file at %s is invalid; could not parse as JSON: %s", access_token_path, str(ve))
            return True

        # load metadata to check if access token uses a refresh token
        if self.use_token_metadata:
            try:
                with open(metadata_path, 'r') as f:
                    token_metadata = json.load(f)
            except IOError as ie:
                self.log.warning("Could not find metadata file %s: %s", metadata_path, ie)
            except ValueError as ve:
                self.log.warning("The metadata file at %s is invalid; could not parse as JSON: %s", metadata_path, str(ve))
            else:
                if 'use_refresh_token' in token_metadata:
                    if token_metadata['use_refresh_token'] == False:
                        return False
            lifetime_fraction = api_endpoints.token_lifetime_fraction(token_metadata['token_url'])
        else:
            lifetime_fraction = 0.5

        # compute token refresh time
        create_time = os.path.getctime(access_token_path)
        refresh_time = create_time + (float(access_token['expires_in']) * lifetime_fraction)

        # check if token is past its refresh time
        if time.time() > refresh_time:
            return True

        return False


    def refresh_access_token(self, username, token_name):
        if OAuth2Session is None:
            raise ImportError("No module named OAuth2Session")

        # load the refresh token
        refresh_token_path = os.path.join(self.cred_dir, username, token_name + '.top')
        try:
            with open(refresh_token_path, 'r') as f:
                refresh_token = json.load(f)
        except IOError as ie:
            self.log.error("Could not open refresh token %s: %s", refresh_token_path, str(ie))
            return False
        except ValueError as ve:
            self.log.error("The format of the refresh token file %s is invalid; could not parse as JSON: %s", refresh_token_path, str(ve))
            return False

        # load metadata
        metadata_path = os.path.join(self.cred_dir, username, token_name + '.meta')
        try:
            with open(metadata_path, 'r') as f:
                token_metadata = json.load(f)
        except IOError as ie:
            self.log.error("Could not open metadata file %s: %s", metadata_path, str(ie))
            return False
        except ValueError as ve:
            self.log.error("The metadata file at %s is invalid; could not parse as JSON: %s", metadata_path, str(ve))
            return False

        # refresh the token (provides both new refresh and access tokens)
        oauth_client = OAuth2Session(token_metadata['client_id'], token = refresh_token)
        new_token = oauth_client.refresh_token(token_metadata['token_url'],
                                                   client_id = token_metadata['client_id'],
                                                   client_secret = token_metadata['client_secret'])
        try:
            new_refresh_token = new_token.pop('refresh_token')
        except KeyError:
            self.log.error("No %s refresh token returned for %s", token_name, username)
            return False
        refresh_token[u'refresh_token'] = new_refresh_token

        # write tokens to tmp files
        (tmp_fd, tmp_refresh_token_path) = tempfile.mkstemp(dir = self.cred_dir)
        with os.fdopen(tmp_fd, 'w') as f:
            json.dump(refresh_token, f)
        (tmp_fd, tmp_access_token_path) = tempfile.mkstemp(dir = self.cred_dir)
        with os.fdopen(tmp_fd, 'w') as f:
            json.dump(new_token, f)

        # atomically move new tokens in place
        access_token_path = os.path.join(self.cred_dir, username, token_name + '.use')
        try:
            atomic_rename(tmp_access_token_path, access_token_path)
            atomic_rename(tmp_refresh_token_path, refresh_token_path)
        except OSError as e:
            self.log.error(e)
            return False
        else:
            return True


    def check_access_token(self, access_token_path):

        (basename, token_filename) = os.path.split(access_token_path)
        (cred_dir, username) = os.path.split(basename)
        token_name = os.path.splitext(token_filename)[0] # strip .use

        if self.providers and (token_name not in self.providers) and ('*' not in self.providers):
            if htcondor and (token_name + "CLIENT_ID") not in htcondor.param:
                self.log.warning(f"User requested token provider {token_name} but parameter {token_name}_CLIENT_ID not set in config; ignoring request")
            return
        elif htcondor and (token_name + "CLIENT_ID") not in htcondor.param:
            return

        # OAuthCredmon only handles OAuth access tokens, which must have metadata files
        metadata_path = os.path.join(self.cred_dir, username, token_name + '.meta')
        if not os.path.exists(metadata_path):
            self.log.debug('Skipping check of %s token files for user %s, no metadata found', token_name, username)
            return

        elif self.should_renew(username, token_name):
            self.log.info('Refreshing %s tokens for user %s', token_name, username)
            success = self.refresh_access_token(username, token_name)
            if success:
                self.log.info('Successfully refreshed %s tokens for user %s', token_name, username)
            else:
                self.log.error('Failed to refresh %s tokens for user %s', token_name, username)

    def scan_tokens(self):

        # loop over all access tokens in the cred_dir
        access_token_files = glob.glob(os.path.join(self.cred_dir, '*', '*.use'))
        self.log.debug(f"Found {len(access_token_files)} possible tokens to check")
        for access_token_file in access_token_files:
            self.log.debug(f"Checking {access_token_file}")
            self.check_access_token(access_token_file)

        # also cleanup any stale key files
        self.cleanup_key_files()

    def cleanup_key_files(self):

        # key filenames are hashes with str len 64
        key_file_re = re.compile(r'^[a-f0-9]{64}$')

        # loop over all possible key files in cred_dir
        key_files = glob.glob(os.path.join(self.cred_dir, '?'*64))
        for key_file in key_files:
            if ((not key_file_re.match(os.path.basename(key_file)))
                    or os.path.isdir(key_file)):
                continue

            try:
                ctime = os.stat(key_file).st_ctime
            except OSError as os_error:
                self.log.error('Could not stat key file %s: %s', key_file, os_error)
                continue

            # remove key files if over 12 hours old
            if time.time() - ctime > 12*3600:
                self.log.info('Removing stale key file %s', os.path.basename(key_file))
                try:
                    os.unlink(key_file)
                except OSError as os_error:
                    self.log.error('Could not remove key file %s: %s', key_file, os_error)
