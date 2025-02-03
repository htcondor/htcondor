
import os
import glob
import json
import tempfile

from typing import List

import scitokens
import htcondor

from credmon.CredentialMonitors.OAuthCredmon import OAuthCredmon
from credmon.utils import atomic_rename


class TokenInfo:
    def __init__(
            self,
            sub: str,
            scopes: List[str],
            audience: List[str],
            profile: str,
            ):
        self.sub = sub
        self.scopes = scopes
        self.audience = audience
        self.profile = profile


class LocalCredmon(OAuthCredmon):
    """
    This credential monitor class provides the ability to self-sign SciTokens
    without needing to access a remote service; only useful when the credd host has
    a copy of the private signing key.
    """

    use_token_metadata = False

    @property
    def credmon_name(self):
        return "LOCAL"

    def __init__(self, provider, *args, **kwargs):
        super(LocalCredmon, self).__init__(*args, **kwargs)
        self.provider = provider
        self.token_issuer = None
        self.authz_template = "read:/user/{username} write:/user/{username}"
        self.authz_group_template = None
        self.authz_group_mapfile = None
        self.authz_group_requirement = None
        self.token_lifetime = 60*20
        self.token_use_json = True
        self.token_aud = ""
        self.token_ver = "scitoken:2.0"
        if htcondor != None:
            self._private_key_location = self.get_credmon_config("PRIVATE_KEY", "/etc/condor/scitokens-private.pem")
            if self._private_key_location != None and os.path.exists(self._private_key_location):
                with open(self._private_key_location, 'r') as private_key:
                    self._private_key = private_key.read()
                self.private_key_id = self.get_credmon_config("KEY_ID", "local")
            else:
                self.log.error(f"{self.credmon_name}_CREDMON_PRIVATE_KEY specified at {self._private_key_location}, but key not found or not readable")
            self.token_issuer = self.get_credmon_config("ISSUER", self.token_issuer)
            self.authz_template = self.get_credmon_config("AUTHZ_TEMPLATE", self.authz_template)
            self.authz_group_mapfile = self.get_credmon_config("AUTHZ_GROUP_MAPFILE", self.authz_group_mapfile)
            self.authz_group_template = self.get_credmon_config("AUTHZ_GROUP_TEMPLATE", self.authz_group_template)
            self.authz_group_requirement  = self.get_credmon_config("AUTHZ_GROUP_REQUIREMENT", self.authz_group_requirement)
            self.token_lifetime = self.get_credmon_config("TOKEN_LIFETIME", self.token_lifetime)
            self.token_use_json = self.get_credmon_config("TOKEN_USE_JSON", self.token_use_json)
            self.token_aud = self.get_credmon_config("TOKEN_AUDIENCE", self.token_aud)
            self.token_ver = self.get_credmon_config("TOKEN_VERSION", self.token_ver)
        else:
            self._private_key_location = None
        if not self.token_issuer and htcondor:
            self.token_issuer = 'https://{}'.format(htcondor.param["FULL_HOSTNAME"])
        # algorithm is hardcoded to ES256, warn if private key does not appear to use EC
        if (self._private_key_location is not None) and ("BEGIN EC PRIVATE KEY" not in self._private_key.split("\n")[0]):
            self.log.warning(f"{self.credmon_name}_CREDMON_PRIVATE_KEY must use elipitcal curve cryptograph algorithm")
            self.log.warning("`scitokens-admin-create-key --pem-private` should be used with `--ec` option")
            self.log.warning("Errors are likely to occur when attempting to serialize SciTokens")


    def get_credmon_config(self, config: str, default: str="") -> str:
        """
        Given a local credmon config variable `FOO`, provider `BAR`, and default `BAZ`,
        returns the first matching of:
        - Value of `LOCAL_CREDMON_BAR_FOO` in the condor config
        - Value of `LOCAL_CREDMON_FOO` in the condor config
        - Default value

        (where `LOCAL` is substituted with the current class's `credmon_name` attribute)
        """
        return htcondor.param.get(f"{self.credmon_name}_CREDMON_{self.provider}_{config}", htcondor.param.get(f"{self.credmon_name}_CREDMON_{config}", default))


    def should_renew(self, username, token_name):
        if not super().should_renew(username, token_name):
            return False

        # Refuse to provide a token to users outside a specific group (if configured)
        if self.authz_group_requirement:
            if not self.authz_group_mapfile:
                self.log.error(f"Local credmon {self.provider} is configured to filter on group membership {self.authz_group_requirement} but {self.credmon_name}_CREDMON_{self.provider}_AUTHZ_GROUP_MAPFILE is undefined")
                return False
            try:
                groups = get_user_groups(username, self.authz_group_mapfile)
            except IOError:
                self.log.exception(f"Could not open {self.authz_group_mapfile}, cannot filter based on group membership")
                return False
            if self.authz_group_requirement not in groups:
                self.log.error(f"User {username} request for a token from provider {self.provider} is denied as {username} is not a member of group {self.authz_group_requirement}")
                return False

        return True


    def generate_access_token_info(self, username: str, token_name: str) -> TokenInfo:
        """
        Determines what information should be in the token for a given username / provider
        """
        # Create scopes from user and group templates
        scopes = [self.authz_template.format(username=username)]
        if self.authz_group_template:
            if self.authz_group_mapfile is None:
                self.log.warning(f"{self.credmon_name}_CREDMON_AUTHZ_GROUP_MAPFILE is undefined, cannot add group authorizations")
            else:
                try:
                    groups = get_user_groups(username, self.authz_group_mapfile)
                    self.log.info("Found {n} groups for user {username}".format(n=len(groups), username=username))
                    for groupname in groups:
                        scopes.append(self.authz_group_template.format(groupname=groupname))
                except IOError:
                    self.log.exception("Could not open {mapfile}, cannot add group authorizations".format(mapfile=self.authz_group_mapfile))

        if self.token_ver:
            profile = self.token_ver

        # Convert the space separated list of audiences to a proper list
        # No aud is valid for scitokens:1.0 tokens.  Also, no resonable default.
        aud_list = [aud for aud in self.token_aud.strip().split() if aud]

        return TokenInfo(sub=username, scopes=scopes, profile=profile, audience=aud_list)

    def write_access_token(self, username: str, token_name: str, token_lifetime: str, serialized_token: str) -> bool:
        """
        Write a serialized access token to the credential directory.
        """
        (tmp_fd, tmp_access_token_path) = tempfile.mkstemp(dir = self.cred_dir)
        with os.fdopen(tmp_fd, 'w') as f:
            if self.token_use_json:
                # use JSON if configured to do so, i.e. when
                # LOCAL_CREDMON_TOKEN_USE_JSON = True (default)
                f.write(json.dumps({
                    "access_token": serialized_token.decode(),
                    "expires_in":   int(token_lifetime),
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


    def refresh_access_token(self, username, token_name):
        """
        Create a SciToken at the specified path.
        """

        info = self.generate_access_token_info(username, token_name)

        token = scitokens.SciToken(algorithm="ES256", key=self._private_key, key_id=self.private_key_id)
        token.update_claims({'sub': info.sub})
        token.update_claims({'scope': " ".join(info.scopes)})

        # Only set the version if we have one.  No version is valid, and implies scitokens:1.0
        if info.profile == "wlcg:1.0" or info.profile == "wlcg":
            token.update_claims({"wlcg.ver", "1.0"})
        elif info.profile:
            token.update_claims({'ver': info.profile})

        if info.audience:
            token.update_claims({'aud': info.audience})
        elif self.token_ver.lower() == "scitoken:2.0":
            self.log.error(f'No "aud" claim, {self.credmon_name}_CREDMON_TOKEN_AUDIENCE must be set when requesting a scitoken:2.0 token')
            return False

        # Serialize the token and write it to a file
        try:
            serialized_token = token.serialize(issuer=self.token_issuer, lifetime=int(self.token_lifetime))
        except TypeError:
            self.log.exception("Failure when attempting to serialize a SciToken, likely due to algorithm mismatch")
            return False
        return self.write_access_token(username, token_name, self.token_lifetime, serialized_token)


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
            try:
                success = self.refresh_access_token(username, self.provider)
            except Exception as exc:
                success = False
                self.log.exception(f"Exception occurred when refreshing access token: {exc}")
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

        provider_glob = glob.glob(os.path.join(self.cred_dir, "*", f"{self.provider}.top"))
        self.log.debug(f"Found {len(provider_glob)} {self.provider} tokens to process")

        for file_name in provider_glob:
            self.log.debug(f"Processing {file_name}")
            self.process_cred_file(file_name)

        super(LocalCredmon, self).scan_tokens


def get_user_groups(username, mapfile):
    """Scan mapfile to find groups for username,
    returns a list of groups.

    Note that every call will read the current file,
    regardless of if HTCondor or the credmon have
    been reconfigured.
    
    Mapfiles have the format:
    * <userA> <group1>,<group2>,...,<groupN>
    * <userB> <group1>,<group2>,...,<groupN>
    """
    groups = []
    with open(mapfile) as f:
        for line in f:
            tokens = line.split()
            if len(tokens) < 3:
                continue
            if tokens[0] != "*":
                continue
            if tokens[1] == username:
                groups = tokens[2].split(",")
                break
    return groups
