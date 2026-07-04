#!/usr/bin/env pytest
#
# Integration test for the native HTCondor <-> Pelican credmon integration.
#
# This is the HTCondor/ornithology counterpart to Pelican's Go integration test
# (cmd/plugin_htcondor_credmon_test.go).  It exercises the same end-to-end flow:
#
#   1. Stand up a self-contained Pelican federation (POSIXv2 origin with the
#      embedded OIDC issuer) using the `pelican-server` binary.
#   2. Create the credmon's OAuth2 client (with the token-exchange grant) via the
#      issuer's admin REST API.
#   3. Submit a job whose credentials are obtained by the merged
#      condor_credential_storer, which detects the Pelican service and runs the
#      Pelican client's device-code flow.  The test scrapes the verification URL
#      from condor_submit's output and approves it over HTTP.
#   4. The PelicanCredmon performs the RFC 8693 token exchange; the job stages in
#      an input file via `transfer_input_files = pelican://...` (the Pelican
#      binary acts as an HTCondor file-transfer plugin when named pelican_plugin)
#      using the exchanged token.
#   5. The access token is backdated and the credmon is SIGHUP'd to force a
#      refresh.
#
# Both the exchange and the refresh are verified by inspecting the token's `jti`
# claim: the exchanged token's jti differs from the subject token's, and the
# refreshed token's jti differs from the exchanged token's.
#
# This test ships as part of the credmon, so the PelicanCredmon and the merged
# condor_credential_storer are always present when it runs.  It SKIPS only when the
# external Pelican binaries it needs to stand up a federation are unavailable:
# `pelican` or `pelican-server` not in PATH (a POSIXv2 federation cannot be
# started), or the `requests` module / a bcrypt implementation is missing.

import base64
import json
import logging
import os
import re
import secrets
import shutil
import signal
import subprocess
import time
from getpass import getuser
from pathlib import Path
from urllib.parse import urlparse, parse_qs

import pytest

try:
    import requests

    requests.packages.urllib3.disable_warnings()
except Exception:
    requests = None

from ornithology import standup, action, Condor


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


USER = getuser()
SERVICE = "pelicantest"
FED_PREFIX = "/credmon-test"
PAYLOAD = "credmon integration test payload\n"

PELICAN = shutil.which("pelican")
PELICAN_SERVER = shutil.which("pelican-server")


# ---------------------------------------------------------------------------
# Locate the condor_credential_storer that ships in the credmon package.  (It
# was formerly named condor_vault_storer, still installed as a symlink, so
# accept either name.  The credmon daemon is found via the default CREDMON_OAUTH
# config knob and loads its Python package from LIBEXEC automatically -- no extra
# wiring needed.)
# ---------------------------------------------------------------------------
STORER_NAMES = ("condor_credential_storer", "condor_vault_storer")


def find_storer():
    for name in STORER_NAMES:
        storer = shutil.which(name)
        if storer:
            return storer
    try:
        out = subprocess.run(
            ["condor_config_val", "BIN"], capture_output=True, text=True
        )
        if out.returncode == 0:
            for name in STORER_NAMES:
                candidate = Path(out.stdout.strip()) / name
                if candidate.exists():
                    return str(candidate)
    except Exception:
        pass
    return None


# Module-level skips that do not need a live federation.
pytestmark = pytest.mark.skipif(
    PELICAN is None or PELICAN_SERVER is None or requests is None,
    reason="requires the `pelican` and `pelican-server` binaries in PATH and the python `requests` module",
)


# ---------------------------------------------------------------------------
# HTTP helpers (admin client creation + device-code approval)
# ---------------------------------------------------------------------------
def _new_session():
    s = requests.Session()
    s.verify = False
    return s


def create_credmon_client(session, server_url, admin_user, admin_password):
    """Create an OAuth2 client with the token-exchange + refresh_token grants."""
    r = session.post(
        server_url + "/api/v1.0/auth/login",
        data={"user": admin_user, "password": admin_password},
    )
    assert r.status_code == 200, f"admin login failed: {r.status_code} {r.text}"

    r = session.post(
        server_url + "/api/v1.0/issuer/admin/ns" + FED_PREFIX + "/clients",
        json={
            "client_name": "pelican-credmon",
            "grant_types": [
                "urn:ietf:params:oauth:grant-type:token-exchange",
                "refresh_token",
            ],
            "scopes": [
                "openid", "offline_access", "wlcg",
                "storage.read:/", "storage.modify:/", "storage.create:/",
            ],
        },
    )
    assert r.status_code == 201, f"admin client creation failed: {r.status_code} {r.text}"
    body = r.json()
    return body["client_id"], body["client_secret"]


def approve_device_code(server_url, verify_url, user, password):
    """Log in as `user` and approve the device code named by verify_url."""
    parsed = urlparse(verify_url)
    qs = parse_qs(parsed.query)
    user_code = qs["user_code"][0]
    namespace = "/" + qs.get("namespace", [""])[0].lstrip("/")
    origin = f"{parsed.scheme}://{parsed.netloc}"
    ns_base = origin + "/api/v1.0/issuer/ns" + namespace

    session = _new_session()
    r = session.post(
        server_url + "/api/v1.0/auth/login", data={"user": user, "password": password}
    )
    assert r.status_code == 200, f"approver login failed: {r.status_code} {r.text}"

    r = session.get(ns_base + "/device", params={"user_code": user_code})
    assert r.status_code == 200, f"device GET failed: {r.status_code} {r.text}"
    csrf = r.json()["csrf_token"]

    r = session.post(
        ns_base + "/device",
        json={"user_code": user_code, "action": "approve", "csrf_token": csrf},
    )
    assert r.status_code == 200, f"device approval failed: {r.status_code} {r.text}"
    logger.info("Approved device code %s as %s", user_code, user)


# ---------------------------------------------------------------------------
# JWT / credential helpers
# ---------------------------------------------------------------------------
def jwt_claims(token):
    payload = token.split(".")[1]
    payload += "=" * (-len(payload) % 4)
    return json.loads(base64.urlsafe_b64decode(payload))


def access_token_from_use(text):
    text = text.strip()
    if text.startswith("{"):
        return json.loads(text).get("access_token", "")
    return text


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------
def _bcrypt_hash(pw):
    """Return a bcrypt ($2*$) hash that Pelican's go-htpasswd accepts, or None.

    Prefers the `bcrypt` module; falls back to the stdlib `crypt` blowfish method
    (present through Python 3.12).  Pelican parses the file with
    htpasswd.AcceptBcrypt, which accepts $2a$/$2b$/$2y$.
    """
    try:
        import bcrypt

        return bcrypt.hashpw(pw.encode(), bcrypt.gensalt()).decode()
    except Exception:
        pass
    try:
        import crypt

        if getattr(crypt, "METHOD_BLOWFISH", None):
            return crypt.crypt(pw, crypt.mksalt(crypt.METHOD_BLOWFISH))
    except Exception:
        pass
    return None


@standup
def credentials(test_dir):
    """An htpasswd file with an admin and a 'testuser' approver."""
    htpasswd = test_dir / "htpasswd"
    admin_pw = secrets.token_urlsafe(12)
    user_pw = secrets.token_urlsafe(12)

    admin_hash = _bcrypt_hash(admin_pw)
    user_hash = _bcrypt_hash(user_pw)
    if not admin_hash or not user_hash:
        pytest.skip("no bcrypt implementation available to build the htpasswd file")

    htpasswd.write_text(f"admin:{admin_hash}\ntestuser:{user_hash}\n")
    htpasswd.chmod(0o644)
    return {"file": htpasswd, "admin_pw": admin_pw, "user_pw": user_pw}


@standup
def federation(test_dir, credentials):
    """Launch a self-contained Pelican federation and yield its coordinates."""
    storage_dir = test_dir / "fed-storage"
    storage_dir.mkdir(parents=True, exist_ok=True)
    config_dir = test_dir / "pelican-config"
    config_dir.mkdir(parents=True, exist_ok=True)
    runtime_dir = test_dir / "pelican-runtime"
    runtime_dir.mkdir(parents=True, exist_ok=True)

    # Pre-create the file the job will download.
    (storage_dir / "payload.txt").write_text(PAYLOAD)

    config_file = test_dir / "pelican.yaml"
    config_file.write_text(f"""
Server:
  EnableUI: true
  UIPasswordFile: {credentials['file']}
Origin:
  StorageType: posixv2
  EnableIssuer: true
  IssuerMode: embedded
  Exports:
    - FederationPrefix: {FED_PREFIX}
      StoragePrefix: {storage_dir}
      Capabilities: ["Reads", "Writes", "Listings"]
Issuer:
  AuthorizationTemplates:
    - prefix: /
      actions: ["read", "write", "create"]
""")

    env = dict(os.environ)
    env.update({
        "PELICAN_CONFIGDIR": str(config_dir),
        "PELICAN_RUNTIMEDIR": str(runtime_dir),
        "PELICAN_TLSSKIPVERIFY": "true",
        "PELICAN_SERVER_WEBPORT": "0",
        "PELICAN_SERVER_DBLOCATION": str(config_dir / "registry.sqlite"),
    })

    log_file = open(test_dir / "pelican-server.log", "w")
    proc = subprocess.Popen(
        [PELICAN_SERVER, "--config", str(config_file), "serve",
         "--module", "director", "--module", "registry", "--module", "origin",
         "--port", "0", "-d"],
        env=env, stdout=log_file, stderr=subprocess.STDOUT,
    )

    # Wait for the address file, then poll the health endpoint.
    addr_file = runtime_dir / "pelican.addresses"
    server_url = None
    deadline = time.time() + 60
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError("pelican-server exited early; see pelican-server.log")
        if addr_file.exists():
            for line in addr_file.read_text().splitlines():
                if line.startswith("SERVER_EXTERNAL_WEB_URL="):
                    server_url = line.split("=", 1)[1].strip().strip('"')
            if server_url:
                break
        time.sleep(0.5)
    assert server_url, "pelican-server did not publish SERVER_EXTERNAL_WEB_URL"

    deadline = time.time() + 60
    healthy = False
    while time.time() < deadline:
        try:
            r = requests.get(server_url + "/api/v1.0/health", verify=False, timeout=5)
            if r.status_code == 200:
                healthy = True
                break
        except Exception:
            pass
        time.sleep(0.5)
    assert healthy, "pelican federation did not become healthy"

    parsed = urlparse(server_url)
    info = {
        "url": server_url,
        "hostname": parsed.hostname,
        "port": parsed.port,
        "storage_dir": storage_dir,
    }
    logger.info("Federation ready at %s", server_url)
    yield info

    proc.send_signal(signal.SIGINT)
    try:
        proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
        proc.kill()
    log_file.close()


@standup
def credmon_client(test_dir, federation, credentials):
    session = _new_session()
    client_id, client_secret = create_credmon_client(
        session, federation["url"], "admin", credentials["admin_pw"]
    )
    secret_file = test_dir / "credmon_client_secret"
    secret_file.write_text(client_secret)
    secret_file.chmod(0o600)
    logger.info("Created credmon client %s", client_id)
    return {"client_id": client_id, "secret_file": secret_file}


@standup
def the_condor(test_dir, federation, credmon_client):
    cred_dir = test_dir / "oauth_credentials"
    cred_dir.mkdir(parents=True, exist_ok=True)
    cred_dir.chmod(0o2770)

    storer = find_storer()
    assert storer, "condor_credential_storer (shipped with the credmon) was not found"

    plugin_path = test_dir / "pelican_plugin"
    shutil.copyfile(PELICAN, plugin_path)
    plugin_path.chmod(0o755)

    token_url = federation["url"] + "/api/v1.0/issuer/ns" + FED_PREFIX + "/token"

    with Condor(
        local_dir=test_dir / "condor",
        config={
            "SEC_DEFAULT_AUTHENTICATION":        "REQUIRED",
            "SEC_DEFAULT_AUTHENTICATION_METHODS": "FS, PASSWORD",
            "SEC_DEFAULT_ENCRYPTION":            "REQUIRED",
            "SEC_DEFAULT_INTEGRITY":             "REQUIRED",

            "SEC_CREDENTIAL_DIRECTORY_OAUTH":    str(cred_dir),
            # CREDMON_OAUTH defaults to the installed condor_credmon_oauth, which
            # loads its `credmon` package (including PelicanCredmon) from LIBEXEC.
            "CREDMON_OAUTH_LOG":                 "$(LOG)/CredMonOAuthLog",
            "SEC_CREDENTIAL_MONITOR_OAUTH_LOG":  "$(LOG)/CredMonOAuthLog",
            "DAEMON_LIST":                       "$(DAEMON_LIST) CREDD CREDMON_OAUTH",
            "AUTO_INCLUDE_CREDD_IN_DAEMON_LIST": "True",
            "TRUST_CREDENTIAL_DIRECTORY":        "True",
            "CREDD_OAUTH_MODE":                  "True",
            "SEC_CREDENTIAL_STORER":             storer,
            "CREDMON_OAUTH_TOKEN_MINIMUM":       "600",

            "FILETRANSFER_PLUGINS":              str(plugin_path),

            # The Pelican token service.
            "PELICAN_CREDMON_PROVIDER_NAMES":            SERVICE,
            "PELICAN_CREDMON_TLS_SKIP_VERIFY":           "true",
            f"{SERVICE.upper()}_PELICAN_URL":            federation["url"],
            f"{SERVICE.upper()}_PELICAN_PREFIX":         FED_PREFIX,
            f"{SERVICE.upper()}_PELICAN_PERMISSIONS":    "read",
            f"{SERVICE.upper()}_PELICAN_TOKEN_URL":      token_url,
            f"{SERVICE.upper()}_PELICAN_CLIENT_ID":      credmon_client["client_id"],
            f"{SERVICE.upper()}_PELICAN_CLIENT_SECRET_FILE": str(credmon_client["secret_file"]),
        },
    ) as condor:
        yield condor


# ---------------------------------------------------------------------------
# Job submission + device approval + refresh helpers
# ---------------------------------------------------------------------------
def submit_with_device_approval(condor, submit_file, env, server_url, user, password):
    """Run condor_submit, approving the device code that appears on its output."""
    with condor.use_config():
        proc = subprocess.Popen(
            ["condor_submit", str(submit_file)],
            env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, bufsize=1,
        )
        url_re = re.compile(r"https?://\S+")
        cluster = None
        approved = False
        captured = []
        for line in proc.stdout:
            line = line.rstrip("\n")
            captured.append(line)
            logger.info("[condor_submit] %s", line)
            if not approved:
                m = url_re.search(line)
                if m and ("user_code=" in m.group(0) or "/device" in m.group(0)):
                    verify_url = m.group(0).rstrip(".)\"'")
                    logger.info("Approving device code at %s", verify_url)
                    approve_device_code(server_url, verify_url, user, password)
                    approved = True
            m2 = re.search(r"submitted to cluster (\d+)", line)
            if m2:
                cluster = m2.group(1)
        proc.wait()
    assert proc.returncode == 0, "condor_submit failed:\n" + "\n".join(captured)
    assert approved, "no device verification URL appeared:\n" + "\n".join(captured)
    return cluster


def wait_for_job(condor, cluster, timeout=300):
    deadline = time.time() + timeout
    while time.time() < deadline:
        rv = condor.run_command(
            ["condor_q", cluster, "-af", "JobStatus"], echo=False, suppress=True
        )
        statuses = rv.stdout.split()
        if any(s == "5" for s in statuses):
            return False, "job held"
        if not statuses or all(s == "4" for s in statuses):
            return True, ""
        time.sleep(2)
    return False, "timed out"


def read_credmon_pid(cred_dir):
    return int((cred_dir / "pid").read_text().strip())


def sighup_credmon(cred_dir, timeout=30):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if (cred_dir / "pid").exists():
            break
        time.sleep(0.5)
    os.kill(read_credmon_pid(cred_dir), signal.SIGHUP)


@action
def scenario(test_dir, the_condor, federation, credentials):
    """Run the full submit -> exchange -> job -> refresh scenario once."""
    cred_dir = test_dir / "oauth_credentials"
    top_path = cred_dir / USER / f"{SERVICE}.top"
    use_path = cred_dir / USER / f"{SERVICE}.use"

    # ----- Job that prints the delivered token and the staged-in payload -----
    script = test_dir / "job.sh"
    script.write_text(
        "#!/bin/bash\n"
        'echo "=== TOKEN BEGIN ==="\n'
        f'cat "$_CONDOR_CREDS/{SERVICE}.use"\n'
        'echo ""\n'
        'echo "=== TOKEN END ==="\n'
        'echo "=== PAYLOAD BEGIN ==="\n'
        "cat payload.txt || echo MISSING\n"
        'echo "=== PAYLOAD END ==="\n'
    )
    script.chmod(0o755)

    out_file = test_dir / "job.out"
    fed_url = f"pelican://{federation['hostname']}:{federation['port']}"
    submit_file = test_dir / "job.sub"
    submit_file.write_text(f"""
executable = {script}
output = {out_file}
error = {test_dir / "job.err"}
log = {test_dir / "job.log"}
use_oauth_services = {SERVICE}
transfer_input_files = {fed_url}{FED_PREFIX}/payload.txt
should_transfer_files = YES
when_to_transfer_output = ON_EXIT
+PelicanCfg_TLSSkipVerify = true
queue
""")

    env = dict(os.environ)
    env.update({
        "CONDOR_CONFIG": str(the_condor.config_file),
        "PELICAN_BIN": PELICAN,
        "PELICAN_SKIP_TERMINAL_CHECK": "1",
        "PELICAN_TLSSKIPVERIFY": "true",
        "PELICAN_FEDERATION_DISCOVERYURL": federation["url"],
    })

    cluster = submit_with_device_approval(
        the_condor, submit_file, env, federation["url"], "testuser", credentials["user_pw"]
    )

    # Capture the storer's subject token (still in .top) before the credmon runs.
    subject_jti = None
    try:
        top = json.loads(top_path.read_text())
        if "access_token" in top:
            subject_jti = jwt_claims(top["access_token"]).get("jti")
    except Exception:
        pass

    # Kick the credmon so the exchange happens promptly.
    sighup_credmon(cred_dir)

    ok, why = wait_for_job(the_condor, cluster)
    assert ok, f"job did not complete: {why}"

    job_out = out_file.read_text()
    use_after_exchange = json.loads(use_path.read_text())
    exchanged_token = use_after_exchange["access_token"]
    exchanged_jti = jwt_claims(exchanged_token).get("jti")
    top_after_exchange = json.loads(top_path.read_text())

    # ----- Force a refresh by backdating the access token -----
    use_path.write_text(json.dumps({
        "access_token": exchanged_token,
        "token_type": "Bearer",
        "expires_in": 1,
    }))
    time.sleep(2)
    sighup_credmon(cred_dir)

    refreshed_jti = None
    deadline = time.time() + 90
    while time.time() < deadline:
        cur = json.loads(use_path.read_text())
        cur_token = cur.get("access_token", "")
        if cur_token and cur_token != exchanged_token:
            refreshed_jti = jwt_claims(cur_token).get("jti")
            break
        time.sleep(3)

    return {
        "job_out": job_out,
        "subject_jti": subject_jti,
        "exchanged_token": exchanged_token,
        "exchanged_jti": exchanged_jti,
        "exchanged_claims": jwt_claims(exchanged_token),
        "top_after_exchange": top_after_exchange,
        "refreshed_jti": refreshed_jti,
    }


# ---------------------------------------------------------------------------
# Assertions
# ---------------------------------------------------------------------------
class TestPelicanCredmon:

    def test_job_received_token_and_payload(self, scenario):
        assert PAYLOAD in scenario["job_out"], "job should have staged in the federation payload"
        token = access_token_from_use(
            re.search(r"=== TOKEN BEGIN ===\n(.*?)\n=== TOKEN END ===",
                      scenario["job_out"], re.S).group(1)
        )
        assert token, "job should have received a token in $_CONDOR_CREDS"

    def test_token_is_exchanged_wlcg_token(self, scenario):
        claims = scenario["exchanged_claims"]
        assert claims.get("sub") == "testuser"
        scope = claims.get("scope", "")
        if isinstance(scope, list):
            scope = " ".join(scope)
        assert "storage.read:/" in scope
        assert "offline_access" in scope

    def test_exchange_replaced_subject_token(self, scenario):
        # The credmon consumes the storer's subject token and stores a refresh
        # token in its place -- direct evidence the RFC 8693 exchange happened.
        top = scenario["top_after_exchange"]
        assert "refresh_token" in top, f"expected a refresh_token in .top, got keys {list(top)}"
        assert "access_token" not in top

    def test_exchanged_jti_differs_from_subject(self, scenario):
        # The exchanged token is a distinct token from the subject token.
        if scenario["subject_jti"] is None:
            pytest.skip("could not capture the subject token's jti before the exchange")
        assert scenario["exchanged_jti"]
        assert scenario["exchanged_jti"] != scenario["subject_jti"]

    def test_refresh_changed_jti(self, scenario):
        assert scenario["refreshed_jti"], "credmon did not refresh the access token after SIGHUP"
        assert scenario["refreshed_jti"] != scenario["exchanged_jti"], \
            "refreshed token should have a new jti"
