
import random
import time
import urllib

import htcondor
import urllib3

from credmon.CredentialMonitors.LocalCredmon import LocalCredmon, TokenInfo

class ClientCredmon(LocalCredmon):
    """
    A CredMon class that uses the OAuth2 Client Credentials flow for
    acquiring access tokens
    """

    @property
    def credmon_name(self):
        return "CLIENT"
    
    def __init__(self, *args, **kwargs):
        super(ClientCredmon, self).__init__(*args, **kwargs)
        self._token_endpoint = ""
        self._token_endpoint_resolved = 0
        self._token_endpoint_lifetime = int(self.get_credmon_config("TOKEN_ENDPOINT_LIFETIME", "3600")) + random.randint(0, 60)

        self.client_id = self.get_credmon_config("CLIENT_ID")
        if not self.client_id:
            raise Exception(f"{self.credmon_name}_CREDMON_{self.provider}_CLIENT_ID configuration parameter must be set to use the client credential credmon")
        secret_file = self.get_credmon_config("CLIENT_SECRET_FILE")
        if not secret_file:
            raise Exception(f"{self.credmon_name}_CREDMON_{self.provider}_CLIENT_SECRET_FILE configuration parameter must be set to use the client credential credmon")

        with open(secret_file) as fp:
            self.secret = fp.read().strip()

        if not self.secret:
            raise Exception(f"Client credentials flow credmon configured with an empty secret file ({self.credmon_name}_CREDMON_{self.provider}_CLIENT_SECRET_FILE)")


    @property
    def token_endpoint(self):
        """
        Determine the token endpoint from the static configuration or dynamically from
        OIDC metadata discovery
        """
        endpoint = self.get_credmon_config("TOKEN_URL")
        if endpoint:
            return endpoint
        
        if self._token_endpoint and (self._token_endpoint_resolved + self._token_endpoint_lifetime > time.time()):
            return self._token_endpoint
        
        resp = urllib3.request("GET", self.token_issuer + "/.well-known/openid-configuration")
        if resp.status != 200:
            msg = f"When performing OIDC metadata discovery, {self.token_issuer} responded with {resp.status}"
            self.log.error(msg)
            if self._token_endpoint:
                return self._token_endpoint
            raise Exception(msg)

        try:
            resp_json = resp.json()
        except Exception as exc:
            self.log.error(f"When performing OIDC metadata discovery, {self.token_issuer} had non-JSON response: {exc}")
            if self._token_endpoint:
                return self._token_endpoint
            raise

        if 'token_endpoint' not in resp_json:
            msg = f"When performing OIDC metadata discovery, {self.token_issuer} response lacked the `token_endpoint` key"
            self.log.error(msg)
            if self._token_endpoint:
                return self._token_endpoint
            raise Exception(msg)

        self._token_endpoint = resp_json["token_endpoint"]
        self._token_endpoint_resolved = time.time()
        self.log.info(f"Resolved token endpoint of '{self._token_endpoint}' for issuer {self.token_issuer}")
        return self._token_endpoint


    def refresh_access_token(self, username, token_name):
        """
        Execute the client credentials flow with the remote token endpoint.

        If successful, write the access token to disk.
        """

        info = self.generate_access_token_info(username, token_name)

        scopes = info.scopes
        if info.profile == "wlcg" or info.profile == "wlcg:1.0":
            scopes.append("wlcg")
        scopes = " ".join(scopes)
        if info.sub:
            scopes.append(f"condor.user:{urllib.parse.quote(info.sub)}")

        fields = {
            "client_id": self.client_id,
            "scopes": scopes,
            "grant_type": "client_credentials",
        }
        if info.audience:
            fields["audience"] = " ".join(info.audience)

        self.log.debug(f"Requesting token from {self.token_endpoint} with following info: {fields}")
        fields["client_secret"] = self.secret

        resp = urllib3.request("POST", self.token_endpoint, fields=fields)
        del fields["client_secret"]
        if resp.status != 200:
            self.log.error(f"HTTP status failure ({resp.status}) when requesting token from {self.token_endpoint} with parameters {fields}")
            return False

        try:
            resp_json = resp.json()
        except Exception as exc:
            self.log.error(f"Token issuer {self.token_issuer} failed to have a valid JSON response: {exc}")
            return False

        access_token = resp_json.get("access_token")
        if not access_token:
            self.log.error(f"Token issuer {self.token_issuer} failed to respond with an access token")
            return False

        lifetime = resp_json.get("expires_in")
        if not lifetime:
            self.log.warning(f"Token issuer {self.token_issuer} failed to indicate when access token will expire; assuming {self.token_lifetime}")

        return self.write_access_token(username, token_name, lifetime, access_token)

