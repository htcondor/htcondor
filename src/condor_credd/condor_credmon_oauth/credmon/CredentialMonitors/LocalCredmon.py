
import os
import glob
import json
import tempfile

import scitokens
import htcondor

from credmon.CredentialMonitors.OAuthCredmon import OAuthCredmon
from credmon.utils import atomic_rename

class LocalCredmon(OAuthCredmon):
    """
    This credential monitor class provides the ability to self-sign SciTokens
    without needing to access a remote service; only useful when the credd host has
    a copy of the private signing key.
    """

    use_token_metadata = False

    def __init__(self, *args, **kwargs):
        super(LocalCredmon, self).__init__(*args, **kwargs)
        self.provider = "scitokens"
        self.token_issuer = None
        self.authz_template = "read:/user/{username} write:/user/{username}"
        self.token_lifetime = 60*20
        self.token_use_json = True
        self.token_aud = ""
        self.token_ver = "scitokens:2.0"
        if htcondor != None:
            self._private_key_location = htcondor.param.get('LOCAL_CREDMON_PRIVATE_KEY', "/etc/condor/scitokens-private.pem")
            if self._private_key_location != None and os.path.exists(self._private_key_location):
                with open(self._private_key_location, 'r') as private_key:
                    self._private_key = private_key.read()
                self._private_key_id = htcondor.param.get('LOCAL_CREDMON_KEY_ID', "local")
            else:
                self.log.error("LOCAL_CREDMON_PRIVATE_KEY specified, but not key not found or not readable")
            self.provider = htcondor.param.get("LOCAL_CREDMON_PROVIDER_NAME", "scitokens")
            self.token_issuer = htcondor.param.get("LOCAL_CREDMON_ISSUER", self.token_issuer)
            self.authz_template = htcondor.param.get("LOCAL_CREDMON_AUTHZ_TEMPLATE", self.authz_template)
            self.token_lifetime = htcondor.param.get("LOCAL_CREDMON_TOKEN_LIFETIME", self.token_lifetime)
            self.token_use_json = htcondor.param.get("LOCAL_CREDMON_TOKEN_USE_JSON", self.token_use_json)
            self.token_aud = htcondor.param.get("LOCAL_CREDMON_TOKEN_AUDIENCE", self.token_aud)
            self.token_ver = htcondor.param.get("LOCAL_CREDMON_TOKEN_VERSION", self.token_ver)
        else:
            self._private_key_location = None
        if not self.token_issuer:
            self.token_issuer = 'https://{}'.format(htcondor.param["FULL_HOSTNAME"])
        # algorithm is hardcoded to ES256, warn if private key does not appear to use EC
        if (self._private_key_location is not None) and ("BEGIN EC PRIVATE KEY" not in self._private_key.split("\n")[0]):
            self.log.warning("LOCAL_CREDMON_PRIVATE_KEY must use elipitcal curve cryptograph algorithm")
            self.log.warning("`scitokens-admin-create-key --pem-private` should be used with `--ec` option")
            self.log.warning("Errors are likely to occur when attempting to serialize SciTokens")

    def refresh_access_token(self, username, token_name):
        """
        Create a SciToken at the specified path.
        """

        token = scitokens.SciToken(algorithm="ES256", key=self._private_key, key_id=self._private_key_id)
        token.update_claims({'sub': username})
        user_authz = self.authz_template.format(username=username)
        token.update_claims({'scope': user_authz})

        # Only set the version if we have one.  No version is valid, and implies scitokens:1.0
        if self.token_ver:
            token.update_claims({'ver': self.token_ver})

        # Convert the space separated list of audiences to a proper list
        # No aud is valid for scitokens:1.0 tokens.  Also, no resonable default.
        aud_list = self.token_aud.strip().split()
        if aud_list:
            token.update_claims({'aud': aud_list})
        elif self.token_ver.lower() == "scitokens:2.0":
            self.log.error('No "aud" claim, LOCAL_CREDMON_TOKEN_AUDIENCE must be set when requesting a scitokens:2.0 token')
            return False

        # Serialize the token and write it to a file
        try:
            serialized_token = token.serialize(issuer=self.token_issuer, lifetime=int(self.token_lifetime))
        except TypeError:
            self.log.exception("Failure when attempting to serialize a SciToken, likely due to algorithm mismatch")
            return False

        # copied from the Vault credmon
        (tmp_fd, tmp_access_token_path) = tempfile.mkstemp(dir = self.cred_dir)
        with os.fdopen(tmp_fd, 'w') as f:
            if self.token_use_json:
                # use JSON if configured to do so, i.e. when
                # LOCAL_CREDMON_TOKEN_USE_JSON = True (default)
                f.write(json.dumps({
                    "access_token": serialized_token.decode(),
                    "expires_in":   int(self.token_lifetime),
                }))
            else:
                # otherwise write a bare token string when
                # LOCAL_CREDMON_TOKEN_USE_JSON = False
                f.write(serialized_token.decode()+'\n')

        access_token_path = os.path.join(self.cred_dir, username, token_name + '.use')

        # atomically move new file into place
        try:
            atomic_rename(tmp_access_token_path, access_token_path)
        except OSError as e:
            self.log.exception("Failure when writing out new access token to {}: {}.".format(
                access_token_path, str(e)))
            return False
        else:
            return True

    def process_cred_file(self, cred_fname):
        """
        Split out the file path to get username and base.
        Pass that data to the SciToken acquiring function.

        Format of cred_path should be:
        <cred_dir> / <username> / <provider>.top
        """
        # Take the cred_dir out of the cred_path
        base, _ = os.path.split(cred_fname)
        username = os.path.basename(base)

        if self.should_renew(username, self.provider):
            self.log.info('Found %s, acquiring SciToken and .use file', cred_fname)
            success = self.refresh_access_token(username, self.provider)
            if success:
                self.log.info("Successfully renewed SciToken for user: %s", username)
            else:
                self.log.error("Failed to renew SciToken for user: %s", username)

    def scan_tokens(self):
        """
        Scan the credential directory for new credential requests.

        The LocalCredmon will look for files of the form `<username>/<provider>.top`
        and create the corresponding access token files, then invoke the parent OAuthCredmon
        method.
        """

        provider_glob = os.path.join(self.cred_dir, "*", "{}.top".format(self.provider))

        for file_name in glob.glob(provider_glob):
            self.process_cred_file(file_name)

        super(LocalCredmon, self).scan_tokens
