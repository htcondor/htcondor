"""
PelicanCredmon -- an HTCondor credential monitor for Pelican federations.

This credmon mirrors the Vault credmon's division of labor but speaks to a
Pelican federation's embedded OAuth2 issuer instead of HashiCorp Vault:

  * A SEC_CREDENTIAL_STORER script (pelican_credential_storer) runs the Pelican
    client's device-code flow at submit time and stores the resulting *subject*
    access token (which carries `offline_access` and storage scopes) into the
    credmon directory as `<service>.top`.

  * This credmon notices the `.top` file and performs an RFC 8693 OAuth2 token
    exchange using *its own* client credentials (configured by the admin),
    receiving a fresh access token and a refresh token bound to the credmon's
    client.  The access token is written to `<service>.use` (JSON) and the
    refresh token replaces the subject token in `<service>.top`.

  * On subsequent scans the credmon refreshes the access token with the
    refresh-token grant.  If the storer replaces `.top` with a newer subject
    token, the exchange is performed again.

Configuration knobs (read from the HTCondor config; SERVICE is the token name):

    PELICAN_CREDMON_PROVIDER_NAMES   space/comma separated list of services
    <SERVICE>_PELICAN_CLIENT_ID          credmon's OAuth2 client id
    <SERVICE>_PELICAN_CLIENT_SECRET_FILE  path to a file holding the credmon's
                                          client secret. NOTE: the secret must
                                          never be placed inline in the config,
                                          which is world-readable; only a file
                                          path (readable solely by the credmon)
                                          is supported.
    <SERVICE>_PELICAN_TOKEN_URL          OPTIONAL override of the issuer token
                                          endpoint.  Normally discovered via OIDC
                                          metadata from the token's issuer.
    PELICAN_CREDMON_CA_FILE              CA bundle to trust for the issuer (opt.)
    PELICAN_CREDMON_TLS_SKIP_VERIFY      "true" to disable TLS verification (opt.)
    CREDMON_OAUTH_TOKEN_MINIMUM          access token lifetime floor, seconds

Only the `requests` library is required (no requests_oauthlib dependency).
"""

from credmon.CredentialMonitors.AbstractCredentialMonitor import AbstractCredentialMonitor
from credmon.utils import atomic_rename
import os
import re
import time
import json
import glob
import base64
import tempfile

try:
    import requests
except ImportError:
    requests = None

try:
    import htcondor2 as htcondor
except ImportError:
    htcondor = None


TOKEN_EXCHANGE_GRANT = "urn:ietf:params:oauth:grant-type:token-exchange"
ACCESS_TOKEN_TYPE = "urn:ietf:params:oauth:token-type:access_token"


class PelicanCredmon(AbstractCredentialMonitor):

    # Fraction of an access token's lifetime that must elapse before we refresh.
    refresh_fraction = 0.5

    @property
    def credmon_name(self):
        return "PELICAN"

    # Re-run OIDC discovery for an issuer at least this often (seconds), so a
    # token endpoint that moves is eventually picked up.
    discovery_ttl = 3600

    def __init__(self, *args, **kw):
        super(PelicanCredmon, self).__init__(*args, **kw)
        # The set of service/provider names this credmon is responsible for.
        self.providers = set(kw.get("providers", set()))
        # Cache of issuer URL -> (token_endpoint, fetched_at) for OIDC discovery.
        self._discovery_cache = {}

    # ------------------------------------------------------------------
    # Configuration helpers
    # ------------------------------------------------------------------
    def _param(self, key, default=None):
        if htcondor is None:
            return default
        return htcondor.param.get(key, default)

    def _provider_for(self, token_name):
        """Return the configured provider name that owns this token, or None.

        A token name may carry a `_<handle>` suffix added for reduced scopes;
        the credentials/config live under the base service name.
        """
        if token_name in self.providers:
            return token_name
        if "_" in token_name:
            base = token_name.rsplit("_", 1)[0]
            if base in self.providers:
                return base
        return None

    def _client_credentials(self, provider):
        client_id = self._param("%s_PELICAN_CLIENT_ID" % provider)
        client_secret = None
        secret_file = self._param("%s_PELICAN_CLIENT_SECRET_FILE" % provider)
        if secret_file:
            try:
                with open(secret_file, "r") as f:
                    client_secret = f.read().strip()
            except IOError as ie:
                self.log.error("Could not read client secret file %s: %s", secret_file, str(ie))
                return (None, None)
        return (client_id, client_secret)

    @staticmethod
    def _jwt_issuer(token):
        """Return the `iss` claim of a JWT without verifying it, or None."""
        try:
            payload = token.split(".")[1]
            payload += "=" * (-len(payload) % 4)
            claims = json.loads(base64.urlsafe_b64decode(payload.encode("ascii")))
            return claims.get("iss")
        except Exception:
            return None

    def _discover_token_endpoint(self, issuer, verify):
        """Discover an issuer's OAuth2 token endpoint via OIDC metadata.

        Results are cached per-issuer and refreshed every `discovery_ttl`
        seconds so the credmon tracks a token endpoint that moves.  On a
        discovery failure the most recent cached value (if any) is returned.
        """
        if not issuer:
            return None
        now = time.time()
        cached = self._discovery_cache.get(issuer)
        if cached and (now - cached[1]) < self.discovery_ttl:
            return cached[0]
        well_known = issuer.rstrip("/") + "/.well-known/openid-configuration"
        try:
            resp = requests.get(well_known, verify=verify, timeout=30,
                               headers={"Accept": "application/json"})
            if resp.status_code != 200:
                self.log.warning("OIDC discovery at %s returned HTTP %d", well_known, resp.status_code)
                return cached[0] if cached else None
            token_endpoint = resp.json().get("token_endpoint")
        except Exception as e:
            self.log.warning("OIDC discovery at %s failed: %s", well_known, str(e))
            return cached[0] if cached else None
        if token_endpoint:
            self._discovery_cache[issuer] = (token_endpoint, now)
        return token_endpoint

    def _resolve_token_url(self, provider, issuer, top_data, verify):
        """Determine the token endpoint for a service.

        Resolution order:
          1. the explicit `<SERVICE>_PELICAN_TOKEN_URL` override, if set;
          2. OIDC discovery from the issuer that minted the token we hold
             (the federation's director resolves the prefix to this issuer); and
          3. whatever token URL was previously recorded in the .top file.
        """
        url = self._param("%s_PELICAN_TOKEN_URL" % provider)
        if url:
            return url
        discovered = self._discover_token_endpoint(issuer, verify)
        if discovered:
            return discovered
        return top_data.get("token_url") if isinstance(top_data, dict) else None

    def _tls_verify(self):
        skip = (self._param("PELICAN_CREDMON_TLS_SKIP_VERIFY", "") or "").strip().lower()
        if skip in ("1", "true", "yes", "on"):
            return False
        ca_file = self._param("PELICAN_CREDMON_CA_FILE")
        if ca_file:
            return ca_file
        return True

    def _minimum_seconds(self):
        val = self._param("CREDMON_OAUTH_TOKEN_MINIMUM")
        if val:
            try:
                return int(val)
            except ValueError:
                pass
        return 20 * 60

    # ------------------------------------------------------------------
    # Renewal logic
    # ------------------------------------------------------------------
    def should_renew(self, username, token_name):
        access_token_path = os.path.join(self.cred_dir, username, token_name + ".use")
        top_path = os.path.join(self.cred_dir, username, token_name + ".top")

        # No access token yet: must mint one from the subject token.
        if not os.path.exists(access_token_path):
            return True

        # If the storer dropped a newer .top (e.g. a fresh subject token), the
        # access token must be regenerated.
        try:
            use_ctime = os.path.getctime(access_token_path)
            if os.path.exists(top_path) and os.path.getctime(top_path) > use_ctime:
                return True
        except OSError:
            return True

        # Refresh once the access token is past a fraction of its lifetime.
        try:
            with open(access_token_path, "r") as f:
                access_token = json.load(f)
        except (IOError, ValueError) as e:
            self.log.warning("Could not parse access token %s: %s", access_token_path, str(e))
            return True

        try:
            expires_in = float(access_token.get("expires_in", 0))
        except (TypeError, ValueError):
            expires_in = 0
        if expires_in <= 0:
            return True
        refresh_time = use_ctime + (expires_in * self.refresh_fraction)
        return time.time() > refresh_time

    # ------------------------------------------------------------------
    # The OAuth2 exchanges
    # ------------------------------------------------------------------
    def _post_token(self, token_url, data, verify):
        resp = requests.post(token_url, data=data, verify=verify, timeout=30,
                             headers={"Accept": "application/json"})
        if resp.status_code != 200:
            raise RuntimeError("token endpoint %s returned HTTP %d: %s"
                               % (token_url, resp.status_code, resp.text[:512]))
        return resp.json()

    def refresh_access_token(self, username, token_name):
        if requests is None:
            self.log.error("The 'requests' module is required by PelicanCredmon")
            return False

        provider = self._provider_for(token_name)
        if provider is None:
            return False

        top_path = os.path.join(self.cred_dir, username, token_name + ".top")
        try:
            with open(top_path, "r") as f:
                top_data = json.load(f)
        except (IOError, ValueError) as e:
            self.log.error("Could not read %s: %s", top_path, str(e))
            return False

        client_id, client_secret = self._client_credentials(provider)
        if not client_id:
            self.log.error("Provider %s is missing %s_PELICAN_CLIENT_ID", provider, provider)
            return False

        verify = self._tls_verify()

        # Decide whether to refresh an existing grant or to exchange a freshly
        # delivered subject token.
        refresh_token = top_data.get("refresh_token")
        subject_token = top_data.get("access_token") or top_data.get("subject_token")

        # The token endpoint is normally discovered from the issuer that minted
        # the token we hold (recorded in .top after the first exchange, or read
        # from the subject token before that), rather than configured by hand.
        issuer = top_data.get("iss")
        if not issuer and subject_token:
            issuer = self._jwt_issuer(subject_token)
        token_url = self._resolve_token_url(provider, issuer, top_data, verify)
        if not token_url:
            self.log.error("Provider %s: could not determine a token endpoint "
                           "(no %s_PELICAN_TOKEN_URL override, issuer discovery failed, "
                           "and no cached URL)", provider, provider)
            return False

        try:
            if refresh_token:
                self.log.debug("Refreshing %s for %s via refresh_token grant", token_name, username)
                new_token = self._post_token(token_url, {
                    "grant_type": "refresh_token",
                    "refresh_token": refresh_token,
                    "client_id": client_id,
                    "client_secret": client_secret or "",
                }, verify)
            elif subject_token:
                self.log.debug("Exchanging subject token for %s (%s) via RFC 8693", token_name, username)
                exchange_req = {
                    "grant_type": TOKEN_EXCHANGE_GRANT,
                    "subject_token": subject_token,
                    "subject_token_type": ACCESS_TOKEN_TYPE,
                    "client_id": client_id,
                    "client_secret": client_secret or "",
                }
                # Carry forward any scopes/audience the storer recorded so the
                # exchanged token keeps the capabilities of the subject token.
                if top_data.get("scope"):
                    exchange_req["scope"] = top_data["scope"]
                if top_data.get("audience"):
                    exchange_req["audience"] = top_data["audience"]
                new_token = self._post_token(token_url, exchange_req, verify)
            else:
                self.log.error("%s contains neither a refresh_token nor a subject access_token", top_path)
                return False
        except Exception as e:
            self.log.error("Token operation for %s (%s) failed: %s", token_name, username, str(e))
            return False

        access_token = new_token.get("access_token")
        if not access_token:
            self.log.error("No access_token in response for %s (%s)", token_name, username)
            return False

        # Persist the (possibly rotated) refresh token back into .top FIRST so
        # future scans use the refresh grant.  Writing .top before .use is
        # important: should_renew() treats a .top newer than .use as a signal
        # that the storer delivered a fresh subject token, so .use must end up
        # being the newest file after a successful renewal.
        new_refresh = new_token.get("refresh_token")
        if new_refresh:
            # Record the issuer so future refreshes can re-discover the token
            # endpoint, and the resolved token_url as a fallback if discovery is
            # ever unavailable.
            new_top = {"refresh_token": new_refresh, "token_url": token_url}
            if issuer:
                new_top["iss"] = issuer
            for key in ("scope", "audience"):
                if top_data.get(key):
                    new_top[key] = top_data[key]
            if not self._atomic_write_json(username, token_name + ".top", new_top):
                return False
        elif subject_token and not refresh_token:
            # We just consumed a subject token but the issuer returned no refresh
            # token.  Leave the subject token in place so a later scan retries,
            # but warn loudly because periodic refresh will not work.
            self.log.warning("Exchange for %s (%s) returned no refresh_token; "
                             "periodic refresh is disabled for this credential",
                             token_name, username)

        # Persist the new access token (.use) as JSON so both the Pelican client
        # and our own should_renew() can read expires_in.  This is written last
        # so that .use is the newest file in the steady state.
        use_obj = {
            "access_token": access_token,
            "token_type": new_token.get("token_type", "Bearer"),
            "expires_in": int(new_token.get("expires_in", self._minimum_seconds())),
        }
        if not self._atomic_write_json(username, token_name + ".use", use_obj):
            return False
        return True

    def _atomic_write_json(self, username, filename, obj):
        target = os.path.join(self.cred_dir, username, filename)
        try:
            (tmp_fd, tmp_path) = tempfile.mkstemp(dir=self.cred_dir)
            with os.fdopen(tmp_fd, "w") as f:
                json.dump(obj, f)
            atomic_rename(tmp_path, target)
        except OSError as e:
            self.log.error("Failed to write %s: %s", target, str(e))
            return False
        return True

    # ------------------------------------------------------------------
    # Scan loop
    # ------------------------------------------------------------------
    def check_access_token(self, top_path):
        (basename, top_filename) = os.path.split(top_path)
        (_cred_dir, username) = os.path.split(basename)
        token_name = os.path.splitext(top_filename)[0]  # strip .top

        if self._provider_for(token_name) is None:
            return

        if self.should_renew(username, token_name):
            self.log.info("Renewing %s token for user %s", token_name, username)
            if self.refresh_access_token(username, token_name):
                self.log.info("Successfully renewed %s token for user %s", token_name, username)
            else:
                self.log.error("Failed to renew %s token for user %s", token_name, username)

    def scan_tokens(self):
        top_files = glob.glob(os.path.join(self.cred_dir, "*", "*.top"))
        self.log.debug("Found %d possible tokens to check", len(top_files))
        for top_file in top_files:
            self.log.debug("Checking %s", top_file)
            try:
                self.check_access_token(top_file)
            except Exception:
                self.log.exception("Error while checking %s", top_file)
