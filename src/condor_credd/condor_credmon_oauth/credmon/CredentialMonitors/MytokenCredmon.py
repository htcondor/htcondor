#!/usr/bin/python3

from credmon.CredentialMonitors.AbstractCredentialMonitor import AbstractCredentialMonitor
from credmon.utils import atomic_rename

import os
import subprocess
import time
import glob
import tempfile
import re

try:
    import htcondor
except ImportError:
    htcondor = None

try:
    import requests
except ImportError:
    requests = None

import jwt
from cryptography.fernet import Fernet

class MytokenCredmon(AbstractCredentialMonitor):

    def __init__(self, *args, **kw):
        super(MytokenCredmon, self).__init__(*args, **kw)
        self.encryption_key = None
        self.encryption_key_file = None
        self.access_token_time = 0
        self.access_token_lifetime = 0

    def should_renew(self, user_name, access_token_name):

        # initialize time for each user
        self.access_token_time = 0
        self.access_token_lifetime = 0

        access_token_path = os.path.join(self.cred_dir, user_name, access_token_name + '.use')
        self.log.debug(' Access token credential file: %s \n', access_token_path)

        # renew access token if credential file does not exist
        if not os.path.exists(access_token_path):
            return True

        # renew access token if credential information is absent or can not be retrieved
        if self.check_credential_access(access_token_path):
            return True

        # retrieve access token life time
        self.get_access_token_time(access_token_path)

        # retrieve period at which the credd is checking the credentials
        if (htcondor is not None) and ('CRED_CHECK_INTERVAL' in htcondor.param):
            credd_check_period = int(htcondor.param['CRED_CHECK_INTERVAL'])
            self.log.debug(' Period at which the credd is checking the access token remaining life time: %d seconds \n', credd_check_period)
        else:
            raise RuntimeError(' The parameter CRED_CHECK_INTERVAL is not defined in the configuration \n')

        # determine threshold for renewal
        threshold_renewal = int(1.2*credd_check_period)

        self.log.debug(' Access token life time: %d seconds \n', self.access_token_lifetime)
        self.log.debug(' Access token remaining life time: %d seconds \n', self.access_token_time)
        self.log.debug(' Access token threshold for renewal: %d seconds \n', threshold_renewal)

        # renew access token if remaining life time is smaller than threshold for renewal
        return (self.access_token_time < threshold_renewal)

    def should_delete(self, user_name, token_name):

        mytoken_path = os.path.join(self.cred_dir, user_name, token_name + '.top')

        try:
            with open(mytoken_path, "rb") as file:
                crypto = Fernet(self.encryption_key)
                mytoken_encrypted = file.read()
                mytoken_decrypted = crypto.decrypt(mytoken_encrypted)
                mytoken_claims = jwt.decode(mytoken_decrypted.decode('utf-8'), options={"verify_signature": False, "verify_aud": False})
                
                if mytoken_claims is None:
                    self.log.error(' Mytoken credential information is absent \n')
                    raise SystemExit(' Mytoken credential information is absent \n')
                else:
                    self.log.debug(' Information retrieved from Mytoken credential file: %s \n', mytoken_claims)

        except BaseException as error:
            self.log.error(' Could not retrieve Mytoken credential information: %s \n', error)
            raise SystemExit(' Could not retrieve Mytoken credential information: %s \n', error)       

        mytoken_time = int(mytoken_claims['exp'] - time.time())
        mytoken_lifetime =  int(mytoken_claims['exp'] - mytoken_claims['iat'])

        # retrieve period at which the credd is checking the credentials
        if (htcondor is not None) and ('CRED_CHECK_INTERVAL' in htcondor.param):
            credd_check_period = int(htcondor.param['CRED_CHECK_INTERVAL'])
            self.log.debug(' Period at which the credd is checking the access token remaining life time: %d seconds \n', credd_check_period)
        else:
            raise RuntimeError(' The parameter CRED_CHECK_INTERVAL is not defined in the configuration \n')
        
        # determine threshold for credential deletion
        threshold_deletion = int(1.2*credd_check_period)

        self.log.debug(' Mytoken life time: %d seconds \n', mytoken_lifetime)
        self.log.debug(' Mytoken remaining life time: %d seconds \n', mytoken_time)
        self.log.debug(' Threshold for credential deletion: %d seconds \n', threshold_deletion)

        # delete user credential directory if remaining life time is smaller than threshold for credential deletion
        return (mytoken_time < threshold_deletion)

    def refresh_access_token(self, user_name, token_name):

        # renew access token
        mytoken_path = os.path.join(self.cred_dir, user_name, token_name + '.top')

        try:
            with open(mytoken_path, "rb") as file:
                crypto = Fernet(self.encryption_key)
                mytoken_encrypted = file.read()
                mytoken_decrypted = crypto.decrypt(mytoken_encrypted)
                self.log.debug(' Mytoken credential has been decrypted: %s\n', mytoken_decrypted.decode('utf-8'))

                access_token_cmd = 'mytoken AT --MT ' + mytoken_decrypted.decode('utf-8')
                new_access_token = subprocess.run(access_token_cmd.split(), stdout=subprocess.PIPE).stdout.decode('ascii').strip('\n')                
        except BaseException as error:
            self.log.error(' Could not renew access token credential: %s \n', error)
            raise SystemExit(' Could not renew access token credential: %s \n', error)

        # write new access token to tmp file
        try:
            (tmp_fd, tmp_access_token_path) = tempfile.mkstemp(dir = self.cred_dir)
            with os.fdopen(tmp_fd, 'w') as tmp_file:
                tmp_file.write(new_access_token)
                self.log.debug(' New access token credential has been written to tmp file \n')
        except BaseException as error:
            self.log.error(' Could not write new access token credential to tmp file: %s \n', error)

        # atomically move new access token to dedicated directory
        access_token_path = os.path.join(self.cred_dir, user_name, token_name + '.use')
        try:
            atomic_rename(tmp_access_token_path, access_token_path)
            self.log.info(' Access token credential file has been successfully renewed for user %s \n', user_name)
            self.log.info(' Old access token remaining life time: %s seconds \n', self.access_token_time)
            self.get_access_token_time(access_token_path)
            self.log.info(' New access token remaining life time: %s seconds \n', self.access_token_time)
        except OSError as error:
            self.log.error(' Access token credential file could not be renewed: %s \n', error.strerror)

    def delete_mark_files(self):

        for file in os.listdir(self.cred_dir):
            if re.search(".mark",file):
                file_path = os.path.join(self.cred_dir,file)
                try:
                    os.unlink(file_path)
                    self.log.debug(' Mark file %s has been successfully removed \n', file_path)
                except OSError as error:
                    self.log.error(' Mark file %s could not be removed: %s \n', file_path, error.strerror)

    def delete_user_credentials(self, user_name, access_token_name):

        # delete mytoken and access token
        extensions = ['.top', '.use']
        base_path = os.path.join(self.cred_dir, user_name, access_token_name)

        for ext in extensions:            
            file_path = base_path + ext

            if os.path.exists(file_path):
                try:
                    os.unlink(file_path)
                    self.log.info(' Credential file %s has been successfully removed \n', file_path)
                except OSError as error:
                    self.log.error(' Credential file %s could not be removed: %s \n', file_path, error.strerror)
            else:
                self.log.error(' Credential file %s could not be found \n', file_path)

        # delete user credential directory
        user_cred_dir_path = os.path.join(self.cred_dir, user_name)
        if os.path.isdir(user_cred_dir_path):
            try:
                os.rmdir(user_cred_dir_path)
                self.log.info(' User credential directory %s has been successfully removed \n', user_cred_dir_path)
            except OSError as error:
                self.log.error(' User credential directory %s could not be removed: %s \n', user_cred_dir_path, error.strerror)
        else:
            self.log.error(' User credential directory %s could not be found \n', user_cred_dir_path)

    def check_access_token(self, access_token_path):

        # retrieve access token, user and provider
        basename, filename = os.path.split(access_token_path)
        user_name = os.path.split(basename)[1] # strip SEC_CREDENTIAL_DIRECTORY_OAUTH
        access_token_name = os.path.splitext(filename)[0] # strip .use

        self.log.debug(' ### User name: %s ### \n', user_name)
        self.log.debug(' Credential directory: %s \n', self.cred_dir)
        self.log.debug(' Access token credential name: %s \n', access_token_name)

        # delete user credential directory and revoke Mytoken if Mytoken is not valid
        if not self.mytoken_valid(user_name, access_token_name):
            self.revoke_mytoken(user_name, access_token_name)
            self.delete_user_credentials(user_name, access_token_name)

        # delete user credential directory and revoke Mytoken if Mytoken is about to expire
        elif self.should_delete(user_name, access_token_name):
            self.revoke_mytoken(user_name, access_token_name)
            self.delete_user_credentials(user_name, access_token_name)

        # renew access token if needed
        elif self.should_renew(user_name, access_token_name):
            self.refresh_access_token(user_name, access_token_name)

        # delete mark file if present
        self.delete_mark_files()

    def scan_tokens(self):

        self.log.debug(' Scanning the access token credential directory: %s \n', self.cred_dir)
        self.log.debug(' Looking for access token credentials: %s \n', os.path.join(self.cred_dir, '*', '*.use'))

        # loop over all access tokens in the cred_dir
        access_token_files = glob.glob(os.path.join(self.cred_dir, '*', '*.use'))
        self.log.debug(' The following access token credentials have been found: %s \n', access_token_files)

        for access_token_path in access_token_files:
            self.check_access_token(access_token_path)

    def get_encryption_key(self):
        if (htcondor is not None) and ('SEC_ENCRYPTION_KEY_DIRECTORY' in htcondor.param):
            self.encryption_key_file = htcondor.param.get('SEC_ENCRYPTION_KEY_DIRECTORY')
            try:
                with open(self.encryption_key_file, "r") as file:
                    self.encryption_key = str.encode(file.read())
                    self.log.debug(' Encryption key for Fernet algorithm has been retrieved \n')
            except BaseException as error:
                self.log.error(' Could not retrieve encryption key for Fernet algorithm: %s \n', error)
                raise SystemExit(' Could not retrieve encryption key for Fernet algorithm: %s \n', error)
        else:
            raise RuntimeError(' The encryption key for Fernet algorithm is not defined in the configuration \n')

    def check_credential_access(self, access_token_path):

        try:
            with open(access_token_path, "r") as file:
                token_data = file.read()
                token_data_trimmed = token_data.rstrip().lstrip()
                access_token_claims = jwt.decode(token_data_trimmed, options={"verify_signature": False, "verify_aud": False})

                # renew credential if credential information is absent
                if access_token_claims is None:
                    return True

        # renew credential if credential file can not be accessed
        except:
            return True

        return False

    def get_access_token_time(self, access_token_path):

        try:
            with open(access_token_path, "r") as file:
                token_data = file.read()
                token_data_trimmed = token_data.rstrip().lstrip()
                access_token_claims = jwt.decode(token_data_trimmed, options={"verify_signature": False, "verify_aud": False})

                if access_token_claims is None:
                    self.log.error(' Access token credential information is absent \n')
                else:
                    self.log.debug(' Information retrieved from access token credential file: %s \n', access_token_claims)

        except BaseException as error:
            self.log.error(' Access token credential information could not be retrieved: %s \n', error)

        self.access_token_time = int(access_token_claims['exp'] - time.time())
        self.access_token_lifetime = int(access_token_claims['exp'] - os.path.getmtime(access_token_path))

    def revoke_mytoken(self, user_name, token_name):

        mytoken_path = os.path.join(self.cred_dir, user_name, token_name + '.top')

        try:
            with open(mytoken_path, "rb") as file:
                crypto = Fernet(self.encryption_key)
                mytoken_encrypted = file.read()
                mytoken_decrypted = crypto.decrypt(mytoken_encrypted)
                self.log.debug(' Mytoken credential has been decrypted: %s\n', mytoken_decrypted.decode('utf-8'))

                revoke_cmd = 'mytoken revoke --MT ' + mytoken_decrypted.decode('utf-8')
                revoke_response = subprocess.run(revoke_cmd.split(), stdout=subprocess.PIPE).stdout.decode('ascii').strip('\n')

                if "revoked" in revoke_response:
                    self.log.debug(' Mytoken credential has been successfully revoked for user %s \n', user_name)
                else:
                    self.log.error(' Mytoken credential could not be revoked for user %s \n', user_name)

        except BaseException as error:
            self.log.error(' Could not revoke Mytoken credential: %s \n', error)
            raise SystemExit(' Could not revoke Mytoken credential: %s \n', error)

    def mytoken_valid(self, user_name, token_name):

        mytoken_path = os.path.join(self.cred_dir, user_name, token_name + '.top')

        try:
            with open(mytoken_path, "rb") as file:
                crypto = Fernet(self.encryption_key)
                mytoken_encrypted = file.read()
                mytoken_decrypted = crypto.decrypt(mytoken_encrypted)
                self.log.debug(' Mytoken credential has been decrypted: %s\n', mytoken_decrypted.decode('utf-8'))

                introspect_cmd = 'mytoken tokeninfo introspect --MT ' + mytoken_decrypted.decode('utf-8')
                introspect_response = subprocess.run(introspect_cmd.split(), stdout=subprocess.PIPE).stdout.decode('ascii').strip('\n')

                if introspect_response:
                    self.log.debug(' Mytoken credential is valid for user %s \n', user_name)
                else:
                    self.log.debug(' Mytoken credential is not valid for user %s \n', user_name)

                return(introspect_response)

        except BaseException as error:
            self.log.debug(' Could not introspect Mytoken credential: %s \n', error)
            raise SystemExit(' Could not introspect Mytoken credential: %s \n', error)

        return False
