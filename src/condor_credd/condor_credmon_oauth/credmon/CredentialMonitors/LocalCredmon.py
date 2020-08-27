
import os
import glob

import scitokens
import htcondor

from credmon.CredentialMonitors.OAuthCredmon import OAuthCredmon
from credmon.utils import atomic_output_json

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
        else:
            self._private_key_location = None
        if not self.token_issuer:
            self.token_issuer = 'https://{}'.format(htcondor.param["FULL_HOSTNAME"])

    def refresh_access_token(self, username, token_name):
        """
        Create a SciToken at the specified path.
        """

        token = scitokens.SciToken(algorithm="ES256", key=self._private_key, key_id=self._private_key_id)
        token.update_claims({'sub': username})
        user_authz = self.authz_template.format(username=username)
        token.update_claims({'scope': user_authz})

        # Serialize the token and write it to a file
        serialized_token = token.serialize(issuer=self.token_issuer, lifetime=int(self.token_lifetime))

        oauth_response = {"access_token": serialized_token,
                          "expires_in":   int(self.token_lifetime)}

        access_token_path = os.path.join(self.cred_dir, username, token_name + '.use')

        try:
            atomic_output_json(oauth_response, access_token_path)
        except OSError as oe:
            self.log.exception("Failure when writing out new access token to {}: {}.".format(
                access_token_path, str(oe)))
            return False
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

        if self.should_renew(base, username):
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
