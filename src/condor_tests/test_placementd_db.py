#!/usr/bin/env pytest

# Test the condor_placementd's database: the map file reconcile, the
# PLACEMENT_QUERY and PLACEMENT_EDIT commands and their audit log, and what
# PLACEMENT_USER_LOGIN will mint, through the Python bindings.

import base64
import getpass
import json
import logging
import os
import sqlite3
import subprocess
import sys
import time
import unicodedata
import uuid

import pytest

import classad2 as classad
import htcondor2 as htcondor

from ornithology import (
    standup,
    action,
    Condor,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

UID_DOMAIN = "example.org"

# alice reaches one account through two IdPs; bob is alone.  INSTRUCTOR is
# not in the authorizations file.
BASE_USERS = {
    "alice@idp": "alice@idp *     READ,WRITE,INSTRUCTOR 0 ProjA,ProjB",
    "alice@idp2": "alice@idp2 alice READ,WRITE,INSTRUCTOR 0 ProjA,ProjB",
    "bob@idp": "bob@idp bob READ 0 ProjA",
}

BASE_AUTHZ = [
    "READ|Read|normal|Read jobs",
    "WRITE|Write|warning|Submit jobs",
]

FAR_FUTURE = 4102444800     # 2100-01-01

# Configured queue super users, which may never be minted.
SUPER_USER = "superjoe"
OCU_SUPER_USER = "ocuann@other.org"


def write_users(path, lines):
    path.write_text("# test map file\n" + "\n".join(lines) + "\n")


def write_authz(path, lines):
    path.write_text("\n".join(lines) + "\n")


def unique(prefix):
    """A name no other test uses."""
    return f"{prefix}{uuid.uuid4().hex[:8]}"


def wait_for_placementd():
    deadline = time.time() + 60
    while True:
        try:
            return htcondor.Placementd(placementd_ad())
        except Exception:
            if time.time() > deadline:
                raise
            time.sleep(0.5)


def placementd_ad():
    ads = htcondor.Collector().query(htcondor.AdType.Any, constraint='MyType == "PlacementD"')
    assert len(ads) == 1
    return ads[0]


def status(pd):
    ads = pd.query("MapfileStatus")
    assert len(ads) == 1
    return ads[0]


def reconcile(condor, pd, hold_lock=None, lock_times=None):
    """Trigger a reconcile with condor_reconfig and wait for it.

    With hold_lock (seconds), another connection holds a write lock on the
    database from before the reconfig until that long after it; the times
    it took and released the lock are appended to lock_times."""
    before = status(pd).get("PlacementdMapfileLoadAttemptTime", 0)
    # The attempt time has one-second resolution.
    while int(time.time()) <= before:
        time.sleep(0.05)
    db = None
    if hold_lock:
        db = sqlite3.connect(tokens_database(condor), isolation_level=None, timeout=10)
        db.execute("BEGIN IMMEDIATE")
        if lock_times is not None:
            lock_times.append(time.time())
    try:
        rv = condor.run_command(["condor_reconfig"])
        assert rv.returncode == 0
        if db:
            time.sleep(hold_lock)
    finally:
        if db:
            db.execute("COMMIT")
            db.close()
            if lock_times is not None:
                lock_times.append(time.time())
    deadline = time.time() + 60
    while time.time() < deadline:
        st = status(pd)
        if st.get("PlacementdMapfileLoadAttemptTime", 0) > before:
            return st
        time.sleep(0.2)
    raise AssertionError("reconcile did not run")


def one(pd, target_type, name):
    ads = pd.query(target_type, name=name)
    assert len(ads) == 1, f"{target_type} {name}: {ads}"
    return ads[0]


def snapshot(pd):
    snap = {}
    for tt in ("Identity", "Account", "Project", "Authorization"):
        for ad in pd.query(tt):
            snap[(tt, ad["Id"])] = (ad["Version"], ad["UpdatedAt"], ad["Source"])
    return snap


def modifications(pd, entity_id=None):
    return pd.query("Modification", entity_id=entity_id)


def latest_modification_id(pd):
    ads = pd.query("Modification", limit=1)
    return ads[0]["Id"] if ads else 0


def changes(mod):
    """The Changes of a modification as {attr: (old, new)}, None for absent."""
    def plain(v):
        if v is classad.Value.Undefined:
            return None
        if isinstance(v, (list, tuple)) or type(v).__name__ == "list":
            return list(v)
        return v
    return {k: (plain(v[0]), plain(v[1])) for k, v in dict(mod["Changes"]).items()}


def expect_error(code, func, *args, **kwargs):
    with pytest.raises(htcondor.HTCondorException) as exc:
        func(*args, **kwargs)
    assert f"error {code}," in str(exc.value), str(exc.value)
    return str(exc.value)


def problems(st, kind):
    return [p for p in st["Problems"] if p["Kind"] == kind]


def token_claims(token):
    payload = token.split(".")[1]
    payload += "=" * (-len(payload) % 4)
    return json.loads(base64.urlsafe_b64decode(payload))


def qusers(condor, *args):
    """Run condor_qusers against the test's schedd."""
    rv = condor.run_command(["condor_qusers"] + list(args))
    assert rv.returncode == 0, rv.stdout + rv.stderr
    return rv.stdout


def schedd_enabled(condor, name, project=False):
    """Enabled for each schedd record named name (any case), keyed by name."""
    attr = "Name" if project else "User"
    args = (["-projects"] if project else []) + [
        "-constraint", f'{attr} == "{name}"', "-af", attr, "Enabled"]
    out = {}
    for line in qusers(condor, *args).splitlines():
        if line.strip():
            key, enabled = line.split()
            out[key] = enabled.lower() in ("true", "1")
    return out


def tokens_database(condor):
    return condor.run_command(["condor_config_val", "TOKENS_DATABASE"]).stdout.strip()


def token_jti(token):
    return token_claims(token)["jti"]


def token_subject(condor, token):
    # A placement token is a capability token: its subject is recorded in
    # the auth layer's table in TOKENS_DATABASE, not in the JWT.
    db = sqlite3.connect(f"file:{tokens_database(condor)}?mode=ro", uri=True)
    try:
        rows = db.execute("SELECT token_sub FROM idtokens_minting WHERE token_jti = ?",
            (token_jti(token),)).fetchall()
    finally:
        db.close()
    assert len(rows) == 1
    return rows[0][0]


def token_scope(condor, token):
    """The scope the auth layer recorded for a capability token."""
    db = sqlite3.connect(f"file:{tokens_database(condor)}?mode=ro", uri=True)
    try:
        rows = db.execute("SELECT token_scope FROM idtokens_minting WHERE token_jti = ?",
            (token_jti(token),)).fetchall()
    finally:
        db.close()
    assert len(rows) == 1
    return rows[0][0] or ""


def write_database(condor, sql, params=()):
    """Change TOKENS_DATABASE behind the daemon's back."""
    db = sqlite3.connect(tokens_database(condor), timeout=10)
    try:
        db.execute(sql, params)
        db.commit()
    finally:
        db.close()


def onboard(pd, account, identity, authorizations=("READ",), projects=("ProjA",), expires=None):
    ident = {"Operation": "Create", "TargetType": "Identity", "Identity": identity, "Account": account}
    if expires is not None:
        ident["ExpiresAt"] = expires
    results = pd.edit([
        {"Operation": "Create", "TargetType": "Account", "Name": account,
            "Authorizations": list(authorizations), "Projects": list(projects)},
        ident,
    ])
    return results[0]["Id"], results[1]["Id"]


def run_client(condor, test_dir, env, body):
    """Run a Placementd call in a fresh process with its own security environment."""
    script = test_dir / "placementd_client.py"
    with condor.use_config():
        address = placementd_ad()["MyAddress"]
    script.write_text(
        "import classad2 as classad\n"
        "import htcondor2 as htcondor\n"
        f"pd = htcondor.Placementd(classad.ClassAd({{'MyAddress': {address!r}}}))\n"
        "try:\n"
        f"    {body}\n"
        "    print('RESULT OK')\n"
        "except Exception as e:\n"
        "    print('RESULT ERROR', e)\n"
    )
    rv = condor.run_command(["env"] + [f"{k}={v}" for k, v in env.items()] + [sys.executable, script.as_posix()])
    out = [line for line in rv.stdout.splitlines() if line.startswith("RESULT")]
    assert len(out) == 1, rv.stdout + rv.stderr
    return out[0]


def ping(condor, env, daemon, level):
    rv = condor.run_command(["env", "_condor_SEC_CLIENT_AUTHENTICATION=REQUIRED"] +
        [f"{k}={v}" for k, v in env.items()] + ["condor_ping", "-type", daemon, "-verbose", level])
    logger.info("condor_ping %s %s: %s %s", daemon, level, rv.stdout, rv.stderr)
    return rv


def token_only_env(token_dir):
    """Authenticate with nothing but the tokens in token_dir."""
    return {
        "_condor_SEC_CLIENT_AUTHENTICATION_METHODS": "IDTOKENS",
        "_condor_SEC_PASSWORD_DIRECTORY": "/nothere",
        "_condor_SEC_TOKEN_SYSTEM_DIRECTORY": "/nothere",
        "_condor_SEC_TOKEN_DIRECTORY": token_dir.as_posix(),
    }


def token_accepted(condor, test_dir, token):
    """Whether the schedd accepts token (at READ, which every test token carries)."""
    token_dir = test_dir / ("tok" + token_jti(token))
    token_dir.mkdir(exist_ok=True)
    (token_dir / "token").write_text(token + "\n")
    (token_dir / "token").chmod(0o600)     # tokens must be private
    return ping(condor, token_only_env(token_dir), "schedd", "READ").returncode == 0


def minted(condor, token):
    """Whether the auth layer still has the token's record."""
    db = sqlite3.connect(f"file:{tokens_database(condor)}?mode=ro", uri=True)
    try:
        return db.execute("SELECT COUNT(*) FROM idtokens_minting WHERE token_jti = ?",
            (token_jti(token),)).fetchone()[0] == 1
    finally:
        db.close()


def revoked_tokens(pd, entity_id):
    """RevokedTokens of the entity's latest modification, or None."""
    return changes(modifications(pd, entity_id)[0]).get("RevokedTokens", (None, None))[1]


@standup
def mapfiles(test_dir):
    users = test_dir / "placementd_mapfile"
    authz = test_dir / "placementd_authorizations_mapfile"
    write_users(users, BASE_USERS.values())
    write_authz(authz, BASE_AUTHZ)
    return {"users": users, "authz": authz}


@standup
def condor(test_dir, mapfiles):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "DAEMON_LIST": "MASTER COLLECTOR SCHEDD PLACEMENTD",
            "PLACEMENTD_MAPFILE": mapfiles["users"].as_posix(),
            "PLACEMENTD_AUTHORIZATIONS_MAPFILE": mapfiles["authz"].as_posix(),
            "PLACEMENTD_UID_DOMAIN": UID_DOMAIN,
            "PLACEMENTD_DEBUG": "D_FULLDEBUG D_AUDIT D_SECURITY",
            # Let a client turn encryption off, so that the daemon's own
            # check is what refuses an unencrypted edit.
            "PLACEMENTD.SEC_DEFAULT_ENCRYPTION": "OPTIONAL",
            # An AES session is always encrypted, so also allow a cipher
            # that a client can negotiate without encryption.
            "PLACEMENTD.SEC_DEFAULT_INTEGRITY": "OPTIONAL",
            "PLACEMENTD.SEC_DEFAULT_CRYPTO_METHODS": "AES,BLOWFISH",
            "SEC_CLIENT_ENCRYPTION": "REQUIRED",
            # The placementd mints capability tokens, which are recorded in
            # TOKENS_DATABASE and honored as capabilities.
            "SEC_USE_TOKENS_DATABASE": "true",
            "SEC_ACCEPT_CAPABILITY_TOKENS": "true",
            # ...except at the schedd, which treats them as plain tokens.
            "SCHEDD.SEC_ACCEPT_CAPABILITY_TOKENS": "false",
            # Project records, as a placement access point keeps them.
            "PERSISTENT_PROJECT_RECORDS": "true",
            "QUEUE_SUPER_USERS": f"root, condor, {SUPER_USER}",
            "OCU_SUPER_USERS": OCU_SUPER_USER,
        },
    ) as condor:
        yield condor


@action
def pd(condor):
    with condor.use_config():
        htcondor.reload_config()
        return wait_for_placementd()


@pytest.fixture
def restore_mapfiles(condor, pd, mapfiles):
    yield
    if mapfiles["authz"].exists():
        mapfiles["authz"].chmod(0o644)
    write_users(mapfiles["users"], BASE_USERS.values())
    write_authz(mapfiles["authz"], BASE_AUTHZ)
    reconcile(condor, pd)


class TestPlacementdDb:

    # ------------------------------------------------------------ import

    def test_import_round_trip(self, pd):
        alice = one(pd, "Account", "alice@" + UID_DOMAIN)
        assert alice["Source"] == "mapfile"
        assert sorted(alice["Identities"]) == ["alice@idp", "alice@idp2"]
        assert sorted(alice["Authorizations"]) == ["INSTRUCTOR", "READ", "WRITE"]
        assert list(alice["Projects"]) == ["ProjA", "ProjB"]
        assert alice["DefaultProject"] == "ProjA"
        # Account names are canonicalized when looked up, too.
        assert one(pd, "Account", "alice")["Id"] == alice["Id"]

        ident = one(pd, "Identity", "alice@idp")
        assert ident["Identity"] == "alice@idp"
        assert "Name" not in ident
        assert ident["AccountId"] == alice["Id"]
        assert ident["Account"] == "alice@" + UID_DOMAIN
        assert ident["Expired"] is False
        assert "ExpiresAt" not in ident
        assert one(pd, "Identity", "alice@idp2")["AccountId"] == alice["Id"]

        instructor = one(pd, "Authorization", "INSTRUCTOR")
        assert instructor["Requestable"] is False
        assert instructor["Label"] == "INSTRUCTOR"
        assert one(pd, "Authorization", "write")["Color"] == "warning"
        assert {"alice@" + UID_DOMAIN, "bob@" + UID_DOMAIN} <= set(one(pd, "Project", "proja")["Accounts"])

        assert status(pd)["PlacementdApiVersion"] == 2

        # Constraint, projection and limit.
        ads = pd.query("Identity", constraint='Account == "bob@example.org"', projection=["Identity"])
        assert len(ads) == 1
        assert ads[0]["Identity"] == "bob@idp"
        assert "Account" not in ads[0]
        assert "Id" in ads[0]
        assert len(pd.query("Identity", limit=1)) == 1
        expect_error(19, pd.query, "Bogus")

    def test_legacy_commands(self, pd):
        users = {ad["UserName"]: ad for ad in pd.queryUsers()}
        alice = users["alice@idp"]
        assert alice["ApUserId"] == "alice@" + UID_DOMAIN
        assert alice["Projects"] == "ProjA,ProjB"
        assert sorted(alice["Authorizations"].split(",")) == ["INSTRUCTOR", "READ", "WRITE"]
        assert alice["MappingExpration"] == 0
        assert alice["MappingExpiration"] == 0
        assert alice["Authorized"] is True
        assert [ad["UserName"] for ad in pd.queryUsers("bob@idp")] == ["bob@idp"]

        # Only requestable authorizations, intersected with the user's grants.
        assert {"READ", "WRITE"} <= {ad["Name"] for ad in pd.queryAuthorizations()}
        assert [ad["Name"] for ad in pd.queryAuthorizations("bob@idp")] == ["READ"]
        expect_error(3, pd.queryAuthorizations, "nobody@idp")

    def test_daemon_ad(self, condor, pd):
        with condor.use_config():
            ad = placementd_ad()
        assert ad["PlacementdApiVersion"] == 2
        assert "PlacementdMapfileOK" in ad
        assert "PlacementdMapfileLoadTime" in ad

    # ------------------------------------------------------------ reconcile

    def test_stat_skip_and_touched_reload(self, condor, pd, mapfiles):
        # Settle both files in the past, then load them once.
        old = time.time() - 60
        for path in mapfiles.values():
            os.utime(path, (old, old))
        st = reconcile(condor, pd)
        before = snapshot(pd)
        load_time = st["PlacementdMapfileLoadTime"]
        last_modification = latest_modification_id(pd)
        assert last_modification > 0

        # Unchanged files: the stat-skip means nothing is applied.
        st = reconcile(condor, pd)
        assert st["PlacementdMapfileLoadTime"] == load_time
        assert snapshot(pd) == before

        # Touched file: a full pass that changes nothing.
        os.utime(mapfiles["users"], (old + 1, old + 1))
        st = reconcile(condor, pd)
        assert st["PlacementdMapfileLoadTime"] > load_time
        assert snapshot(pd) == before
        # Neither reload wrote audit records.
        assert latest_modification_id(pd) == last_modification

    def test_malformed_line_keeps_state(self, condor, pd, mapfiles, restore_mapfiles):
        bob = one(pd, "Identity", "bob@idp")
        lines = dict(BASE_USERS)
        lines["bob@idp"] = "bob@idp bob READ notanumber ProjA"
        lines["new@idp"] = "new@idp new READ"      # too few fields
        write_users(mapfiles["users"], lines.values())
        st = reconcile(condor, pd)

        assert st["PlacementdMapfileOK"] is False
        assert st["PlacementdMapfileQuarantined"] == 2
        assert {p["Key"] for p in st["Problems"] if p["Kind"] == "Malformed"} == {"bob@idp", "new@idp"}
        # bob keeps his row (and so his access); a new key gets no row.
        after = one(pd, "Identity", "bob@idp")
        assert (after["Id"], after["Version"]) == (bob["Id"], bob["Version"])
        assert pd.query("Identity", name="new@idp") == []

    def test_malformed_line_expiry_tightens(self, condor, pd, mapfiles, restore_mapfiles):
        acct = unique("mexp")
        reserved, spaced = f"{acct}@one", f"{acct}@two"
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [
            f"{reserved} {acct} READ {FAR_FUTURE} ProjA",
            f"{spaced} {acct} READ 0 ProjA",
        ])
        reconcile(condor, pd)
        pd.userLogin(reserved, "READ")

        past, soon = int(time.time()) - 60, int(time.time()) + 3600
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [
            # Malformed (a reserved account), with an expiry in the past.
            f"{reserved} root READ {past} ProjA",
            # Malformed (a stray space: six fields), with a sooner expiry.
            f"{spaced} {acct} READ {soon} ProjA ProjB",
        ])
        st = reconcile(condor, pd)
        assert {p["Key"] for p in problems(st, "Malformed")} >= {reserved, spaced}
        # Neither line is applied, but each tightens its identity's expiry.
        assert one(pd, "Identity", reserved)["ExpiresAt"] == past
        assert one(pd, "Identity", reserved)["Account"] == f"{acct}@{UID_DOMAIN}"
        assert one(pd, "Identity", spaced)["ExpiresAt"] == soon
        expect_error(6, pd.userLogin, reserved, "READ")

    def test_duplicate_identity_lines(self, condor, pd, mapfiles, restore_mapfiles):
        same, split_id = unique("dup") + "@idp", unique("split") + "@idp"
        acct = unique("dupacct")
        lines = list(BASE_USERS.values()) + [
            f"{split_id} {acct} READ {FAR_FUTURE} ProjA",
        ]
        write_users(mapfiles["users"], lines)
        reconcile(condor, pd)
        split_row = one(pd, "Identity", split_id)

        soon = int(time.time()) + 3600
        lines = list(BASE_USERS.values()) + [
            # Same account twice: the earliest expiry applies.
            f"{same} {acct} READ {FAR_FUTURE} ProjA",
            f"{same} {acct} READ {soon} ProjA",
            # Different accounts: quarantined, but the earlier expiry
            # still tightens the existing row.
            f"{split_id} {acct} READ {soon} ProjA",
            f"{split_id} other{acct} READ {FAR_FUTURE} ProjA",
        ]
        write_users(mapfiles["users"], lines)
        st = reconcile(condor, pd)
        assert one(pd, "Identity", same)["ExpiresAt"] == soon
        after = one(pd, "Identity", split_id)
        assert (after["Id"], after["Account"], after["ExpiresAt"]) == \
            (split_row["Id"], f"{acct}@{UID_DOMAIN}", soon)
        dups = [p for p in st["Problems"] if p["Kind"] == "Duplicate"]
        assert {p["Key"] for p in dups} == {same, split_id}
        assert st["PlacementdMapfileQuarantined"] == 1

    def test_account_conflict_intersects_and_unions(self, condor, pd, mapfiles, restore_mapfiles):
        acct = unique("conf")
        lines = list(BASE_USERS.values()) + [
            f"{acct}@a {acct} READ,WRITE 0 ProjA,ProjB",
            f"{acct}@b {acct} READ 0 ProjC",
        ]
        write_users(mapfiles["users"], lines)
        st = reconcile(condor, pd)
        account = one(pd, "Account", acct)
        # The authorizations both lines grant; the projects of either; the
        # first line's default.
        assert list(account["Authorizations"]) == ["READ"]
        assert sorted(account["Projects"]) == ["ProjA", "ProjB", "ProjC"]
        assert account["DefaultProject"] == "ProjA"
        assert sorted(account["Identities"]) == [f"{acct}@a", f"{acct}@b"]
        conflicts = [p for p in st["Problems"] if p["Kind"] == "AccountConflict"]
        assert len(conflicts) == 2
        # The narrowing is reported on its own, naming what was dropped.
        narrowed = problems(st, "GrantsNarrowed")
        assert [p["Key"] for p in narrowed] == [f"{acct}@{UID_DOMAIN}"]
        assert "WRITE" in narrowed[0]["Message"] and "READ" not in narrowed[0]["Message"]
        assert problems(st, "NoAuthorizations") == []
        assert st["PlacementdMapfileOK"] is False

    def test_conflict_leaving_no_authorizations(self, condor, pd, mapfiles, restore_mapfiles):
        acct = unique("none")
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [
            f"{acct}@a {acct} READ 0 ProjA",
            f"{acct}@b {acct} WRITE 0 ProjA",
        ])
        st = reconcile(condor, pd)
        assert list(one(pd, "Account", acct)["Authorizations"]) == []
        assert [p["Key"] for p in problems(st, "NoAuthorizations")] == [f"{acct}@{UID_DOMAIN}"]
        narrowed = problems(st, "GrantsNarrowed")
        assert len(narrowed) == 1 and "READ,WRITE" in narrowed[0]["Message"]
        # No grants, no token.
        expect_error(4, pd.userLogin, f"{acct}@a", "")

    def test_case_only_differences_agree(self, condor, pd, mapfiles, restore_mapfiles):
        acct = unique("case")
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [
            f"{acct}@a {acct} READ,write 0 ProjA,ProjB",
            f"{acct}@b {acct} read,WRITE 0 proja,projb",
        ])
        st = reconcile(condor, pd)
        assert sorted(a.upper() for a in one(pd, "Account", acct)["Authorizations"]) == ["READ", "WRITE"]
        keys = {p["Key"] for p in st["Problems"]}
        assert f"{acct}@{UID_DOMAIN}" not in keys, st["Problems"]
        assert st["PlacementdMapfileOK"] is True

    def test_revocation_applies_under_quarantine(self, condor, pd, mapfiles, restore_mapfiles):
        acct = unique("rev")
        kept, removed = f"{acct}@one", f"{acct}@two"
        lines = list(BASE_USERS.values()) + [
            f"{kept} {acct} READ {FAR_FUTURE} ProjA",
            f"{removed} {acct} READ {FAR_FUTURE} ProjA",
        ]
        write_users(mapfiles["users"], lines)
        reconcile(condor, pd)

        # kept's line turns bad (two accounts) with an earlier expiry, and
        # removed's line is gone.
        soon = int(time.time()) + 600
        lines = list(BASE_USERS.values()) + [
            f"{kept} {acct} READ {soon} ProjA",
            f"{kept} x{acct} READ {FAR_FUTURE} ProjA",
        ]
        write_users(mapfiles["users"], lines)
        reconcile(condor, pd)
        assert pd.query("Identity", name=removed) == []
        assert one(pd, "Identity", kept)["ExpiresAt"] == soon

    def test_mapfile_rejects_what_may_not_be_minted(self, condor, pd, mapfiles, restore_mapfiles):
        acct = unique("rej")
        lines = list(BASE_USERS.values()) + [
            f"{acct}@idp {acct} READ,ADMINISTRATOR 0 ProjA",
            f"{acct}@evil condor READ 0 ProjA",
        ]
        write_users(mapfiles["users"], lines)
        write_authz(mapfiles["authz"], BASE_AUTHZ + ["DAEMON|Daemon|normal|no", "BAD NAME|Bad|normal|no"])
        st = reconcile(condor, pd)
        assert list(one(pd, "Account", acct)["Authorizations"]) == ["READ"]
        assert pd.query("Authorization", name="ADMINISTRATOR") == []
        assert pd.query("Authorization", name="DAEMON") == []
        assert pd.query("Identity", name=f"{acct}@evil") == []
        assert pd.query("Account", name="condor") == []
        rejected = {p["Key"] for p in st["Problems"] if p["Kind"] == "Rejected"}
        assert {"ADMINISTRATOR", "DAEMON", "BAD NAME"} <= rejected
        assert any(p["Kind"] == "Malformed" and p["Key"] == f"{acct}@evil" for p in st["Problems"])

    def test_placeholder_for_quarantined_authorization(self, condor, pd, mapfiles, restore_mapfiles):
        acct = unique("qa")
        name = unique("QAUTH").upper()
        write_authz(mapfiles["authz"], BASE_AUTHZ + [f"{name}|Missing fields"])
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{acct}@idp {acct} READ,{name} 0 ProjA"])
        st = reconcile(condor, pd)
        # The account is unaffected; the authorization exists as a placeholder.
        assert sorted(one(pd, "Account", acct)["Authorizations"]) == sorted(["READ", name])
        placeholder = one(pd, "Authorization", name)
        assert (placeholder["Requestable"], placeholder["Label"]) == (False, name)
        assert any(p["Kind"] == "Malformed" and p["Key"] == name for p in st["Problems"])

    def test_missing_or_unreadable_authz_file(self, condor, pd, mapfiles, restore_mapfiles):
        acct = unique("noauthz")
        kept, removed = f"{acct}@kept", f"{acct}@removed"
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [
            f"{kept} {acct} READ {FAR_FUTURE} ProjA",
            f"{removed} {acct} READ {FAR_FUTURE} ProjA",
        ])
        reconcile(condor, pd)
        read_before = one(pd, "Authorization", "READ")

        # Without the authorizations file, the user file still applies: a
        # removed line revokes and an earlier expiry tightens.  Only the
        # authorizations' metadata stays as it was.
        mapfiles["authz"].unlink()
        soon = int(time.time()) + 3600
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{kept} {acct} READ {soon} ProjA"])
        st = reconcile(condor, pd)
        assert [p["Kind"] for p in st["Problems"]] == ["Missing"]
        assert st["Problems"][0]["File"] == mapfiles["authz"].as_posix()
        assert st["PlacementdMapfileOK"] is False
        assert pd.query("Identity", name=removed) == []
        assert one(pd, "Identity", kept)["ExpiresAt"] == soon
        read_after = one(pd, "Authorization", "READ")
        assert (read_after["Version"], read_after["Label"], read_after["Requestable"]) == \
            (read_before["Version"], "Read", True)
        # WRITE is granted by no line now, but is not deleted.
        assert one(pd, "Authorization", "WRITE")["Requestable"] is True

        if os.geteuid() != 0:
            write_authz(mapfiles["authz"], BASE_AUTHZ)
            mapfiles["authz"].chmod(0)
            write_users(mapfiles["users"], list(BASE_USERS.values()))
            st = reconcile(condor, pd)
            assert [p["Kind"] for p in st["Problems"]] == ["Unreadable"]
            assert pd.query("Identity", name=kept) == []
            # A chmod does not change what the stat-skip sees: the next pass
            # must still read the file.
            mapfiles["authz"].chmod(0o644)
            st = reconcile(condor, pd)
            assert st["Problems"] == []

    def test_reconcile_waits_for_a_locked_database(self, condor, pd, mapfiles, restore_mapfiles):
        # With the authorizations file missing, and another connection
        # holding the write lock, the reconcile waits for the lock instead
        # of failing.
        acct = unique("locked")
        mapfiles["authz"].unlink()
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{acct}@idp {acct} READ 0 ProjA"])
        lock_times = []
        st = reconcile(condor, pd, hold_lock=3, lock_times=lock_times)
        # The reconcile started (to the second) while the lock was held...
        assert st["PlacementdMapfileLoadAttemptTime"] < lock_times[1] - 1, lock_times
        # ...and applied once it was released.
        assert problems(st, "DatabaseError") == [], st["Problems"]
        assert [p["Kind"] for p in st["Problems"]] == ["Missing"]
        assert one(pd, "Identity", f"{acct}@idp")["Account"] == f"{acct}@{UID_DOMAIN}"

    # ------------------------------------------------------------ ownership

    def test_mapfile_rows_are_read_only(self, pd):
        expect_error(15, pd.edit, [{"Operation": "Update", "TargetType": "Identity",
            "Identity": "alice@idp", "Comment": "hi"}])
        expect_error(15, pd.edit, [{"Operation": "Delete", "TargetType": "Account", "Name": "alice"}])
        expect_error(15, pd.edit, [{"Operation": "Update", "TargetType": "Authorization",
            "Name": "READ", "Label": "x"}])

    def test_adopt_reclaimed_while_in_file(self, condor, pd):
        bob = one(pd, "Identity", "bob@idp")
        results = pd.edit([{"Operation": "Adopt", "TargetType": "Identity", "Identity": "bob@idp"}])
        assert "Warning" in results[0]
        adopted = one(pd, "Identity", "bob@idp")
        assert adopted["Source"] == "internal"
        assert adopted["Version"] > bob["Version"]
        mod = modifications(pd, bob["Id"])[0]
        assert (mod["Operation"], mod["Channel"], mod["Id"]) == ("adopt", "cedar", adopted["Version"])
        assert changes(mod) == {}

        st = reconcile(condor, pd)
        reclaimed = one(pd, "Identity", "bob@idp")
        assert reclaimed["Source"] == "mapfile"
        assert reclaimed["Id"] == bob["Id"]
        assert reclaimed["Version"] > adopted["Version"]
        mod = modifications(pd, bob["Id"])[0]
        assert (mod["Operation"], mod["Channel"], mod["Actor"]) == ("reclaim", "mapfile", "mapfile")
        assert mod["Id"] == reclaimed["Version"]
        assert reclaimed["ModifiedBy"] == "mapfile"
        assert st["PlacementdMapfileConflicts"] == 1
        assert any(p["Kind"] == "Reclaimed" and p["Key"] == "bob@idp" for p in st["Problems"])

    def test_adopt_sticks_once_line_removed(self, condor, pd, mapfiles, restore_mapfiles):
        acct, project = unique("carol"), unique("ProjC")
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{acct}@idp {acct} READ 0 {project}"])
        reconcile(condor, pd)
        account = one(pd, "Account", acct)
        assert account["Source"] == "mapfile"

        results = pd.edit([{"Operation": "Adopt", "TargetType": "Account", "Name": acct}])
        assert "Warning" in results[0]
        assert one(pd, "Identity", f"{acct}@idp")["Source"] == "internal"

        write_users(mapfiles["users"], BASE_USERS.values())
        st = reconcile(condor, pd)
        assert st["PlacementdMapfileConflicts"] == 0
        after = one(pd, "Account", acct)
        assert (after["Id"], after["Source"]) == (account["Id"], "internal")
        assert one(pd, "Identity", f"{acct}@idp")["Source"] == "internal"
        # The project is still granted to the (now internal) account.
        assert one(pd, "Project", project)["Source"] == "internal"

    def test_update_after_adopt_warns(self, condor, pd, mapfiles, restore_mapfiles):
        acct = unique("adupd")
        ident = f"{acct}@idp"
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{ident} {acct} READ 0 ProjA"])
        reconcile(condor, pd)
        pd.edit([{"Operation": "Adopt", "TargetType": "Account", "Name": acct}])

        # The map file still defines both keys, so the next reconcile will
        # undo these edits.
        results = pd.edit([
            {"Operation": "Update", "TargetType": "Identity", "Identity": ident, "ExpiresAt": FAR_FUTURE},
            {"Operation": "Update", "TargetType": "Account", "Name": acct, "Projects": ["ProjA", "ProjB"]},
        ])
        for result, key in zip(results, (ident, f"{acct}@{UID_DOMAIN}")):
            assert "reclaim" in result["Warning"] and key in result["Warning"], result

        # Once the line is gone, the edits stick, and nothing is said.
        write_users(mapfiles["users"], BASE_USERS.values())
        reconcile(condor, pd)
        results = pd.edit([{"Operation": "Update", "TargetType": "Identity", "Identity": ident,
            "ExpiresAt": FAR_FUTURE - 1}])
        assert "Warning" not in results[0]

    def test_adopt_all_then_reclaim(self, condor, pd, mapfiles, restore_mapfiles):
        acct, project, authz = unique("adoptall"), unique("ProjAll"), unique("REFALL").upper()
        write_users(mapfiles["users"], list(BASE_USERS.values()) +
            [f"{acct}@idp {acct} READ,{authz} 0 {project}"])
        reconcile(condor, pd)
        results = pd.edit([{"Operation": "Adopt", "TargetType": "All"}])
        assert results[0]["Count"] > 0
        assert "Warning" in results[0]
        for tt, name in (("Identity", f"{acct}@idp"), ("Authorization", "READ"), ("Project", project),
                         ("Authorization", authz)):
            assert one(pd, tt, name)["Source"] == "internal"

        # What the files define is reclaimed: identities, accounts (with
        # their grants) and the authorizations file's lines...
        st = reconcile(condor, pd)
        for tt, name in (("Identity", f"{acct}@idp"), ("Account", acct), ("Identity", "alice@idp"),
                         ("Account", "alice"), ("Authorization", "READ")):
            assert one(pd, tt, name)["Source"] == "mapfile"
        assert st["PlacementdMapfileConflicts"] >= 7
        # ...but a project or authorization a user line only names is not.
        assert one(pd, "Project", project)["Source"] == "internal"
        assert one(pd, "Authorization", authz)["Source"] == "internal"
        assert sorted(one(pd, "Account", acct)["Authorizations"]) == sorted(["READ", authz])

    def test_references_do_not_reclaim(self, condor, pd, mapfiles, restore_mapfiles):
        acct, project, authz = unique("refs"), unique("RefProj"), unique("REFAUTH").upper()
        results = pd.edit([
            {"Operation": "Create", "TargetType": "Project", "Name": project, "Description": "kept"},
            {"Operation": "Create", "TargetType": "Authorization", "Name": authz, "Label": "Fancy",
                "Color": "warning", "Requestable": True},
        ])
        project_id, authz_id = results[0]["Id"], results[1]["Id"]
        versions = (results[0]["Version"], results[1]["Version"])

        # A user line names both (in another case): it uses the internal
        # rows as they are.
        write_users(mapfiles["users"], list(BASE_USERS.values()) +
            [f"{acct}@idp {acct} READ,{authz.lower()} 0 {project.upper()}"])
        st = reconcile(condor, pd)
        proj = pd.query("Project", id=project_id)[0]
        assert (proj["Source"], proj["Name"], proj["Description"], proj["Version"]) == \
            ("internal", project, "kept", versions[0])
        auth = pd.query("Authorization", id=authz_id)[0]
        assert (auth["Source"], auth["Name"], auth["Label"], auth["Color"], auth["Requestable"], auth["Version"]) == \
            ("internal", authz, "Fancy", "warning", True, versions[1])
        assert st["PlacementdMapfileConflicts"] == 0
        assert problems(st, "Reclaimed") == []
        account = one(pd, "Account", acct)
        assert (account["Source"], list(account["Projects"])) == ("mapfile", [project])
        assert sorted(account["Authorizations"]) == sorted(["READ", authz])
        ad = pd.userLogin(f"{acct}@idp", authz, project.lower())
        assert token_subject(condor, ad["Token"]) == f"{acct}@{UID_DOMAIN}"

        # The internal rows stay editable, but a mapfile account holds them.
        pd.edit([{"Operation": "Update", "TargetType": "Project", "Id": project_id, "Description": "edited"}])
        expect_error(17, pd.edit, [{"Operation": "Delete", "TargetType": "Project", "Id": project_id}])
        expect_error(15, pd.edit, [{"Operation": "Delete", "TargetType": "Project", "Id": project_id,
            "Cascade": True}])

        # Without the line, the internal rows are left alone.
        write_users(mapfiles["users"], BASE_USERS.values())
        reconcile(condor, pd)
        assert pd.query("Account", name=acct) == []
        assert pd.query("Project", id=project_id)[0]["Description"] == "edited"
        assert pd.query("Authorization", id=authz_id)[0]["Source"] == "internal"

    def test_handoff_of_referenced_rows(self, condor, pd, mapfiles, restore_mapfiles):
        acct, project = unique("erin"), unique("ProjE")
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{acct}@idp {acct} READ 0 {project}"])
        reconcile(condor, pd)
        account = one(pd, "Account", acct)
        proj = one(pd, "Project", project)

        # Internal rows that reference mapfile rows.
        pd.edit([
            {"Operation": "Create", "TargetType": "Identity", "Identity": f"{acct}@other", "Account": account["Id"]},
            {"Operation": "Create", "TargetType": "Account", "Name": unique("frank"), "Projects": [project]},
        ])

        write_users(mapfiles["users"], BASE_USERS.values())
        reconcile(condor, pd)
        assert pd.query("Identity", name=f"{acct}@idp") == []
        retained = one(pd, "Account", acct)
        assert (retained["Id"], retained["Source"]) == (account["Id"], "internal")
        mod = modifications(pd, account["Id"])[0]
        assert (mod["Operation"], mod["Channel"]) == ("handoff", "mapfile")
        assert changes(mod) == {}
        retained = one(pd, "Project", project)
        assert (retained["Id"], retained["Source"]) == (proj["Id"], "internal")
        assert one(pd, "Identity", f"{acct}@other")["AccountId"] == account["Id"]

    def test_mapfile_deletes(self, condor, pd, mapfiles, restore_mapfiles):
        acct, project = unique("gone"), unique("ProjGone")
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{acct}@idp {acct} READ 0 {project}"])
        reconcile(condor, pd)
        identity_id = one(pd, "Identity", f"{acct}@idp")["Id"]
        write_users(mapfiles["users"], BASE_USERS.values())
        reconcile(condor, pd)
        assert pd.query("Identity", name=f"{acct}@idp") == []
        assert pd.query("Account", name=acct) == []
        assert pd.query("Project", name=project) == []
        assert [m["Operation"] for m in modifications(pd, identity_id)] == ["delete", "create"]

    # ------------------------------------------------------------ edits

    def test_batch_rolls_back(self, pd):
        last_modification = latest_modification_id(pd)
        project, acct = unique("RollbackProj"), unique("rollback")
        msg = expect_error(13, pd.edit, [
            {"Operation": "Create", "TargetType": "Project", "Name": project},
            {"Operation": "Create", "TargetType": "Account", "Name": acct, "Projects": [project]},
            {"Operation": "Create", "TargetType": "Identity", "Identity": f"{acct}@idp", "Account": "nobody"},
        ])
        assert "operation 2" in msg
        assert pd.query("Project", name=project) == []
        assert pd.query("Account", name=acct) == []
        # No change set either.
        assert latest_modification_id(pd) == last_modification

    def test_expected_version(self, pd):
        project = unique("VersionProj")
        results = pd.edit([{"Operation": "Create", "TargetType": "Project", "Name": project}])
        v1 = results[0]["Version"]
        assert v1 == latest_modification_id(pd)
        # Without an Id, Name is only the selector (and matches any case).
        results = pd.edit([{"Operation": "Update", "TargetType": "Project", "Name": project.lower(),
            "Description": "first", "ExpectedVersion": v1}])
        v2 = results[0]["Version"]
        assert v2 > v1
        assert one(pd, "Project", project)["Name"] == project
        expect_error(14, pd.edit, [{"Operation": "Update", "TargetType": "Project",
            "Name": project, "Description": "stale", "ExpectedVersion": v1}])
        assert one(pd, "Project", project)["Description"] == "first"

        # Versions are global: a later change elsewhere gets a larger one.
        acct = pd.edit([{"Operation": "Create", "TargetType": "Account", "Name": unique("versioned")}])[0]
        assert acct["Version"] > v2
        # An account's version follows its grants.
        results = pd.edit([{"Operation": "Update", "TargetType": "Account", "Id": acct["Id"],
            "Projects": [project], "ExpectedVersion": acct["Version"]}])
        after = pd.query("Account", id=acct["Id"])[0]
        assert after["Version"] == results[0]["Version"] > acct["Version"]
        assert after["ModificationId"] == after["Version"]
        assert changes(modifications(pd, acct["Id"])[0]) == {"Projects": ([], [project])}

    def test_noop_update_writes_nothing(self, pd):
        project = unique("NoopProj")
        version = pd.edit([{"Operation": "Create", "TargetType": "Project", "Name": project,
            "Description": "same"}])[0]["Version"]
        last_modification = latest_modification_id(pd)
        results = pd.edit([{"Operation": "Update", "TargetType": "Project", "Name": project,
            "Description": "same"}])
        assert results[0]["Version"] == version
        assert latest_modification_id(pd) == last_modification
        assert one(pd, "Project", project)["Version"] == version

    def test_audit_log(self, pd):
        project, acct = unique("AuditProj"), unique("audrey")
        results = pd.edit([
            {"Operation": "Create", "TargetType": "Project", "Name": project, "Description": "one"},
            {"Operation": "Create", "TargetType": "Account", "Name": acct, "Projects": [project]},
            {"Operation": "Create", "TargetType": "Identity", "Identity": f"{acct}@idp", "Account": acct},
        ], on_behalf_of="webadmin@example.org", comment="onboarding")
        project_id, account_id, identity_id = (r["Id"] for r in results)

        # One change set for the batch, one modification per entity.
        created = [modifications(pd, i)[0] for i in (project_id, account_id, identity_id)]
        assert len({m["ChangesetId"] for m in created}) == 1
        for mod, result in zip(created, results):
            assert mod["Operation"] == "create"
            assert mod["Channel"] == "cedar"
            assert mod["OnBehalfOf"] == "webadmin@example.org"
            assert mod["Comment"] == "onboarding"
            assert mod["Id"] == result["Version"]
            assert "PeerAddress" in mod
        actor = created[0]["Actor"]
        assert changes(created[0]) == {"Name": (None, project), "Description": (None, "one")}
        assert changes(created[1]) == {"Name": (None, f"{acct}@{UID_DOMAIN}"),
            "Authorizations": (None, []), "Projects": (None, [project])}
        assert changes(created[2]) == {"Identity": (None, f"{acct}@idp"), "Account": (None, f"{acct}@{UID_DOMAIN}")}

        proj = one(pd, "Project", project)
        assert proj["ModifiedBy"] == actor + " on behalf of webadmin@example.org"
        assert proj["CreatedAt"] == proj["UpdatedAt"] == created[0]["Time"]

        # Update: only what changed, old and new.
        pd.edit([{"Operation": "Update", "TargetType": "Project", "Id": project_id, "Description": "two"}])
        pd.edit([{"Operation": "Update", "TargetType": "Project", "Id": project_id, "Description": "three"}])
        mod = modifications(pd, project_id)[0]
        assert mod["Operation"] == "update"
        assert "OnBehalfOf" not in mod
        assert changes(mod) == {"Description": ("two", "three")}
        assert one(pd, "Project", project)["ModifiedBy"] == actor

        # Delete with Cascade: the identity and the account are each recorded.
        pd.edit([{"Operation": "Delete", "TargetType": "Account", "Id": account_id, "Cascade": True}])
        mod = modifications(pd, identity_id)[0]
        assert mod["Operation"] == "delete"
        assert changes(mod) == {"Identity": (f"{acct}@idp", None), "Account": (f"{acct}@{UID_DOMAIN}", None)}
        assert modifications(pd, account_id)[0]["Operation"] == "delete"

        # The deleted entities' history is still there, newest first.
        history = modifications(pd, identity_id)
        assert [m["Operation"] for m in history] == ["delete", "create"]
        assert history[0]["Id"] > history[1]["Id"]
        assert pd.query("Identity", id=identity_id) == []

        # Requirements reach past the head of the log, and Limit applies
        # after them.
        creates = pd.query("Modification", constraint=f'Operation == "create" && EntityId == "{project_id}"')
        assert len(creates) == 1
        assert creates[0]["Id"] == created[0]["Id"]
        updates = pd.query("Modification", constraint=f'EntityId == "{project_id}" && Operation == "update"', limit=1)
        assert len(updates) == 1
        assert changes(updates[0]) == {"Description": ("two", "three")}

    def test_on_behalf_of_is_sanitized(self, pd):
        project = unique("SanitizeProj")
        results = pd.edit([{"Operation": "Create", "TargetType": "Project", "Name": project}],
            on_behalf_of="mallory\nPERMISSION GRANTED\x07", comment="c" * 5000)
        mod = modifications(pd, results[0]["Id"])[0]
        assert mod["OnBehalfOf"] == "malloryPERMISSION GRANTED"
        assert len(mod["Comment"]) == 4096

    def test_rename_updates_holders(self, pd):
        project, acct = unique("HeldProj"), unique("holder")
        results = pd.edit([
            {"Operation": "Create", "TargetType": "Project", "Name": project},
            {"Operation": "Create", "TargetType": "Account", "Name": acct, "Projects": [project]},
        ])
        project_id, account_id = results[0]["Id"], results[1]["Id"]
        before = pd.query("Account", id=account_id)[0]["Version"]
        pd.edit([{"Operation": "Update", "TargetType": "Project", "Id": project_id, "Name": project + "X"}])
        holder = pd.query("Account", id=account_id)[0]
        assert holder["Version"] > before
        assert list(holder["Projects"]) == [project + "X"]
        assert changes(modifications(pd, account_id)[0]) == {"Projects": ([project], [project + "X"])}

    def test_delete_cascade(self, pd):
        acct = unique("gina")
        pd.edit([
            {"Operation": "Create", "TargetType": "Account", "Name": acct},
            {"Operation": "Create", "TargetType": "Identity", "Identity": f"{acct}@idp", "Account": acct},
        ])
        expect_error(17, pd.edit, [{"Operation": "Delete", "TargetType": "Account", "Name": acct}])
        assert len(pd.query("Identity", name=f"{acct}@idp")) == 1
        pd.edit([{"Operation": "Delete", "TargetType": "Account", "Name": acct, "Cascade": True}])
        assert pd.query("Account", name=acct) == []
        assert pd.query("Identity", name=f"{acct}@idp") == []

    def test_rename_of_a_name_the_mapfile_lists(self, condor, pd, mapfiles, restore_mapfiles):
        acct = unique("listed")
        project, other, authz = unique("ListedProj"), unique("UnlistedProj"), unique("LISTED").upper()
        results = pd.edit([
            {"Operation": "Create", "TargetType": "Project", "Name": project},
            {"Operation": "Create", "TargetType": "Project", "Name": other},
            {"Operation": "Create", "TargetType": "Authorization", "Name": authz},
        ])
        project_id, other_id, authz_id = (r["Id"] for r in results)
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{acct}@idp {acct} READ,{authz} 0 {project}"])
        reconcile(condor, pd)
        assert list(one(pd, "Account", acct)["Projects"]) == [project]

        # Renaming an internal row that a map file line names is allowed,
        # with a warning naming the accounts that will keep the old name.
        results = pd.edit([{"Operation": "Update", "TargetType": "Project", "Id": project_id,
            "Name": project + "New"}])
        warning = results[0]["Warning"]
        assert "future tokens" in warning
        assert f"map file still lists {project} for 1 account(s) ({acct}@{UID_DOMAIN})" in warning
        results = pd.edit([{"Operation": "Update", "TargetType": "Authorization", "Id": authz_id,
            "Name": authz + "NEW"}])
        assert f"map file still lists {authz} for 1 account(s) ({acct}@{UID_DOMAIN})" in results[0]["Warning"]

        # As warned: the next reload grants the account placeholders under
        # the old names.
        reconcile(condor, pd)
        account = one(pd, "Account", acct)
        assert list(account["Projects"]) == [project]
        assert sorted(account["Authorizations"]) == sorted(["READ", authz])
        assert one(pd, "Project", project)["Source"] == "mapfile"
        assert one(pd, "Authorization", authz)["Source"] == "mapfile"
        assert acct + "@" + UID_DOMAIN not in pd.query("Project", id=project_id)[0]["Accounts"]

        # A name no map file line lists gets no such warning.
        results = pd.edit([{"Operation": "Update", "TargetType": "Project", "Id": other_id,
            "Name": other + "New"}])
        assert "future tokens" in results[0]["Warning"]
        assert "map file" not in results[0]["Warning"]

    def test_cascade_delete_of_project_and_authorization(self, pd):
        project, authz, acct = unique("CascProj"), unique("CASCAUTH").upper(), unique("casc")
        results = pd.edit([
            {"Operation": "Create", "TargetType": "Project", "Name": project},
            {"Operation": "Create", "TargetType": "Authorization", "Name": authz},
            {"Operation": "Create", "TargetType": "Account", "Name": acct,
                "Projects": [project], "Authorizations": [authz]},
        ])
        account_id = results[2]["Id"]
        for tt, name, attr in (("Project", project, "Projects"), ("Authorization", authz, "Authorizations")):
            expect_error(17, pd.edit, [{"Operation": "Delete", "TargetType": tt, "Name": name}])
            before = pd.query("Account", id=account_id)[0]["Version"]
            pd.edit([{"Operation": "Delete", "TargetType": tt, "Name": name, "Cascade": True}])
            assert pd.query(tt, name=name) == []
            account = pd.query("Account", id=account_id)[0]
            assert account["Version"] > before
            assert list(account[attr]) == []
            assert changes(modifications(pd, account_id)[0]) == {attr: ([name], [])}

    def test_rename_to_permission_level_refused(self, condor, pd):
        # Grants refer to the authorization row, so renaming one to a
        # permission level would grant that level to every holder.
        acct, custom = unique("lvl"), unique("CUSTOM").upper()
        authz_id = pd.edit([{"Operation": "Create", "TargetType": "Authorization", "Name": custom}])[0]["Id"]
        onboard(pd, acct, f"{acct}@idp", authorizations=("READ", custom))
        # The WRITE row would make the rename a duplicate; move it aside
        # behind the daemon's back.
        hidden = unique("HIDDENWRITE").upper()
        write_database(condor, "UPDATE placementd_authorizations SET name = ? WHERE name = 'WRITE'", (hidden,))
        outcomes = []
        try:
            for level in ("WRITE", "write"):
                try:
                    pd.edit([{"Operation": "Update", "TargetType": "Authorization", "Id": authz_id, "Name": level}])
                    outcomes.append("renamed")
                except htcondor.HTCondorException as e:
                    outcomes.append(str(e))
            token = pd.userLogin(f"{acct}@idp", "")["Token"]
            scope = token_scope(condor, token)
        finally:
            write_database(condor, "UPDATE placementd_authorizations SET name = ? WHERE uuid = ?", (custom, authz_id))
            write_database(condor, "UPDATE placementd_authorizations SET name = 'WRITE' WHERE name = ?", (hidden,))
        assert all("error 16," in outcome for outcome in outcomes), outcomes
        assert "condor:/WRITE" not in scope.upper().split(), scope
        assert one(pd, "Authorization", custom)["Id"] == authz_id
        # Any other rename is still allowed.
        pd.edit([{"Operation": "Update", "TargetType": "Authorization", "Id": authz_id, "Name": custom + "2"}])
        assert one(pd, "Authorization", custom + "2")["Id"] == authz_id

    def test_authorization_update_restores_defaults(self, pd):
        name = unique("DEFAULTS").upper()
        auth_id = pd.edit([{"Operation": "Create", "TargetType": "Authorization", "Name": name,
            "Label": "Fancy", "Color": "warning", "Requestable": False}])[0]["Id"]
        pd.edit([{"Operation": "Update", "TargetType": "Authorization", "Id": auth_id,
            "Label": classad.Value.Undefined, "Color": classad.Value.Undefined,
            "Requestable": classad.Value.Undefined}])
        auth = one(pd, "Authorization", name)
        assert (auth["Label"], auth["Color"], auth["Requestable"]) == (name, "normal", True)

    def test_identity_rename(self, pd):
        acct = unique("ivan")
        identity_id = onboard(pd, acct, f"{acct}@idp")[1]
        # With an Id, Identity is the new value...
        pd.edit([{"Operation": "Update", "TargetType": "Identity", "Id": identity_id, "Identity": f"{acct}@idp2"}])
        assert pd.query("Identity", name=f"{acct}@idp") == []
        assert one(pd, "Identity", f"{acct}@idp2")["Id"] == identity_id
        # An identity has no Name.
        expect_error(16, pd.edit, [{"Operation": "Update", "TargetType": "Identity", "Id": identity_id,
            "Name": f"{acct}@idp3"}])
        # ...and without an Id, Identity is only the selector.
        pd.edit([{"Operation": "Update", "TargetType": "Identity", "Identity": f"{acct}@idp2", "Comment": "hi"}])
        ident = one(pd, "Identity", f"{acct}@idp2")
        assert (ident["Id"], ident["Comment"]) == (identity_id, "hi")

    def test_create_validation(self, pd):
        def create(tt, **fields):
            return [dict(Operation="Create", TargetType=tt, **fields)]
        expect_error(18, pd.edit, create("Project", Name="proja"))
        expect_error(16, pd.edit, create("Account", Name="bad name"))
        expect_error(16, pd.edit, create("Account", Name="a,b"))
        expect_error(16, pd.edit, create("Account", Name="x", Projects=["ProjA"], DefaultProject="ProjB"))
        expect_error(13, pd.edit, create("Account", Name="x", Authorizations=["NOPE"]))
        expect_error(19, pd.edit, [{"Operation": "Frobnicate", "TargetType": "Project", "Name": "x"}])
        # What may not be minted.
        for reserved in ("condor", "root@here", "Condor_Pool@example.org", "someone@password", "x@CHILD"):
            expect_error(16, pd.edit, create("Account", Name=reserved))
        for privileged in ("ADMINISTRATOR", "daemon", "CONFIG", "ADVERTISE_STARTD", "ALLOW"):
            expect_error(16, pd.edit, create("Authorization", Name=privileged))
        for bad in ('x" || true || "', "back\\slash", "tab\there", "nl\nhere"):
            expect_error(16, pd.edit, create("Project", Name=bad))
            expect_error(16, pd.edit, create("Identity", Identity=bad, Account="alice"))
        # Names are checked on rename too.
        project_id = pd.edit(create("Project", Name=unique("Renamed")))[0]["Id"]
        expect_error(16, pd.edit, [{"Operation": "Update", "TargetType": "Project", "Id": project_id,
            "Name": 'a"b'}])

    def test_free_text_is_cleaned(self, condor, pd, mapfiles, restore_mapfiles):
        # Control and format (bidi, zero-width) characters are stripped from
        # free text, which is capped at 256 bytes for a label, color or
        # display name and 4096 for a description or comment.
        name, acct = unique("TEXT").upper(), unique("text")
        evil = "line\nbreak\u202egnp.exe\x07\u200b"
        pd.edit([
            {"Operation": "Create", "TargetType": "Authorization", "Name": name, "Label": evil, "Color": evil,
                "Description": "\x1b[31m" + "d" * 10240},
            {"Operation": "Create", "TargetType": "Project", "Name": name, "Description": evil + "\u00e9" * 5000},
            {"Operation": "Create", "TargetType": "Account", "Name": acct},
            {"Operation": "Create", "TargetType": "Identity", "Identity": f"{acct}@idp", "Account": acct,
                "DisplayName": evil + "n" * 300, "Comment": evil},
        ])
        auth = one(pd, "Authorization", name)
        assert (auth["Label"], auth["Color"]) == ("linebreakgnp.exe", "linebreakgnp.exe")
        assert auth["Description"] == "[31m" + "d" * 4092
        project = one(pd, "Project", name)
        # Capped on a character boundary.
        assert len(project["Description"].encode()) <= 4096
        assert project["Description"] == "linebreakgnp.exe" + "\u00e9" * 2040
        ident = one(pd, "Identity", f"{acct}@idp")
        assert ident["Comment"] == "linebreakgnp.exe"
        assert len(ident["DisplayName"].encode()) == 256
        pd.edit([{"Operation": "Update", "TargetType": "Authorization", "Name": name, "Label": "a\u200bb\rc"}])
        assert one(pd, "Authorization", name)["Label"] == "abc"

        # So is the authorizations file's.
        write_authz(mapfiles["authz"], BASE_AUTHZ + [f"{name}FILE|Fi\u202ele|nor\u200bmal|de\u0085sc"])
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{acct}@file {acct}file {name}FILE 0 ProjA"])
        reconcile(condor, pd)
        from_file = one(pd, "Authorization", name + "FILE")
        assert (from_file["Label"], from_file["Color"], from_file["Description"]) == ("File", "normal", "desc")

        def dirty(value):
            return isinstance(value, str) and any(unicodedata.category(c) in ("Cc", "Cf") for c in value)
        for tt in ("Identity", "Account", "Project", "Authorization"):
            for ad in pd.query(tt):
                assert not any(dirty(v) for v in ad.values()), ad
        for ad in pd.queryAuthorizations():
            assert not any(dirty(v) for v in ad.values()), ad

    def test_name_rules(self, condor, pd, mapfiles, restore_mapfiles):
        def create(tt, **fields):
            return [dict(Operation="Create", TargetType=tt, **fields)]
        acct = unique("names")
        pd.edit(create("Account", Name=acct, Authorizations=["READ"]))
        expect_error(16, pd.edit, create("Account", Name=f"*@{UID_DOMAIN}"))
        expect_error(16, pd.edit, create("Project", Name="p" * 257))
        expect_error(16, pd.edit, create("Account", Name="a" * 250 + "@" + UID_DOMAIN))
        expect_error(16, pd.edit, create("Identity", Identity="i" * 1025, Account=acct))
        for bad in ("bidi\u202e@idp", "zero\u200bwidth@idp", "c1\u0085@idp"):
            expect_error(16, pd.edit, create("Identity", Identity=bad, Account=acct))
        pd.edit(create("Project", Name=(acct * 30)[:256]))
        pd.edit(create("Identity", Identity=acct + "i" * 300 + "@idp", Account=acct))

        # A map file line whose identity is not UTF-8 is malformed.
        acct = unique("utf")
        mapfiles["users"].write_bytes(("\n".join(BASE_USERS.values()) + "\n").encode() +
            f"{acct}\xff@idp {acct} READ 0 ProjA\n{acct}@idp {acct} READ 0 ProjA\n".encode("latin-1"))
        st = reconcile(condor, pd)
        malformed = problems(st, "Malformed")
        assert any(p["Key"].startswith(acct) and "UTF-8" in p["Message"] for p in malformed), malformed
        idents = pd.query("Identity", constraint=f'Account == "{acct}@{UID_DOMAIN}"')
        assert [ad["Identity"] for ad in idents] == [f"{acct}@idp"]

    def test_dry_run_writes_nothing(self, pd):
        last_modification = latest_modification_id(pd)
        project = unique("DryRunProj")
        results = pd.edit([{"Operation": "Create", "TargetType": "Project", "Name": project}],
            dry_run=True, on_behalf_of="dry", comment="dry run")
        # The ids a dry run would use are reused by the next real write.
        assert "Version" not in results[0]
        assert "Id" in results[0]
        assert pd.query("Project", name=project) == []
        assert latest_modification_id(pd) == last_modification
        assert modifications(pd, results[0]["Id"]) == []

    def test_failed_batch_reports_no_ids(self, pd):
        project = unique("FailedProj")
        existing = pd.edit([{"Operation": "Create", "TargetType": "Project", "Name": unique("Existing")}])[0]
        with pytest.raises(htcondor.HTCondorException) as exc:
            pd.edit([
                {"Operation": "Create", "TargetType": "Project", "Name": project},
                {"Operation": "Update", "TargetType": "Project", "Id": existing["Id"], "Description": "x"},
                {"Operation": "Create", "TargetType": "Project", "Name": project},
                {"Operation": "Create", "TargetType": "Project", "Name": unique("NotTried")},
            ])
        assert "error 18," in str(exc.value)
        results = exc.value.results
        assert [r["Index"] for r in results] == [0, 1, 2, 3]
        # Nothing was written, so no result names a row or a version.
        for r in results:
            assert "Id" not in r and "Version" not in r, r
        assert results[2]["ErrorCode"] == 18
        assert "ErrorCode" not in results[0]
        assert pd.query("Project", name=project) == []

    def test_edit_updates(self, condor, pd):
        acct, other = unique("upd"), unique("upd2")
        account_id, identity_id = onboard(pd, acct, f"{acct}@idp", authorizations=("READ",),
            projects=("ProjA", "ProjB"))
        other_id = pd.edit([{"Operation": "Create", "TargetType": "Account", "Name": other,
            "Authorizations": ["READ"]}])[0]["Id"]

        # An identity moves to another account and gains an expiry.
        soon = int(time.time()) + 3600
        pd.edit([{"Operation": "Update", "TargetType": "Identity", "Id": identity_id, "Account": other,
            "ExpiresAt": soon}])
        ident = pd.query("Identity", id=identity_id)[0]
        assert (ident["AccountId"], ident["ExpiresAt"]) == (other_id, soon)
        assert changes(modifications(pd, identity_id)[0]) == {
            "Account": (f"{acct}@{UID_DOMAIN}", f"{other}@{UID_DOMAIN}"), "ExpiresAt": (None, soon)}
        assert [u["MappingExpiration"] for u in pd.queryUsers(f"{acct}@idp")] == [soon]
        ad = pd.userLogin(f"{acct}@idp", "READ")
        assert token_subject(condor, ad["Token"]) == f"{other}@{UID_DOMAIN}"
        # ExpiresAt = undefined clears it.
        pd.edit([{"Operation": "Update", "TargetType": "Identity", "Id": identity_id,
            "ExpiresAt": classad.Value.Undefined}])
        assert "ExpiresAt" not in pd.query("Identity", id=identity_id)[0]

        # An account's grants and default project.
        pd.edit([{"Operation": "Update", "TargetType": "Account", "Id": account_id,
            "Authorizations": ["READ", "WRITE"], "DefaultProject": "projb"}])
        account = pd.query("Account", id=account_id)[0]
        assert sorted(account["Authorizations"]) == ["READ", "WRITE"]
        assert (account["DefaultProject"], list(account["Projects"])) == ("ProjB", ["ProjB", "ProjA"])
        expect_error(16, pd.edit, [{"Operation": "Update", "TargetType": "Account", "Id": account_id,
            "DefaultProject": unique("NotHeld")}])
        expect_error(16, pd.edit, [{"Operation": "Update", "TargetType": "Account", "Id": account_id,
            "Projects": ["ProjA"], "DefaultProject": "ProjB"}])
        # Dropping the default project from the list clears it.
        pd.edit([{"Operation": "Update", "TargetType": "Account", "Id": account_id, "Projects": ["ProjA"]}])
        account = pd.query("Account", id=account_id)[0]
        assert "DefaultProject" not in account
        assert list(account["Projects"]) == ["ProjA"]

    def test_account_rename_updates_identities(self, pd):
        acct = unique("renamed")
        account_id, identity_id = onboard(pd, acct, f"{acct}@idp")
        before = pd.query("Identity", id=identity_id)[0]["Version"]
        pd.edit([{"Operation": "Update", "TargetType": "Account", "Id": account_id, "Name": acct + "2"}])
        ident = pd.query("Identity", id=identity_id)[0]
        assert ident["Version"] > before
        assert ident["Account"] == f"{acct}2@{UID_DOMAIN}"
        assert changes(modifications(pd, identity_id)[0]) == {
            "Account": (f"{acct}@{UID_DOMAIN}", f"{acct}2@{UID_DOMAIN}")}
        # A change that leaves the name alone records nothing on them.
        version = ident["Version"]
        pd.edit([{"Operation": "Update", "TargetType": "Account", "Id": account_id, "Authorizations": ["WRITE"]}])
        assert pd.query("Identity", id=identity_id)[0]["Version"] == version

    def test_reserved_subjects(self, condor, pd, mapfiles, restore_mapfiles):
        def create_account(name):
            return pd.edit([{"Operation": "Create", "TargetType": "Account", "Name": name}])
        for name in (SUPER_USER, f"{SUPER_USER.upper()}@{UID_DOMAIN}", OCU_SUPER_USER, OCU_SUPER_USER.upper(),
                     getpass.getuser(), "x@matchsession", "x@Unmapped", "x@family", "x@parent", "x@child",
                     "x@password"):
            expect_error(16, create_account, name)
        # A super user's name is reserved in every domain.
        expect_error(16, create_account, f"{SUPER_USER}@{unique('elsewhere')}.org")
        expect_error(16, create_account, f"{OCU_SUPER_USER.split('@')[0]}@{unique('elsewhere')}.org")

        # In the map file, such a line is malformed.
        acct = unique("super")
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [
            f"{acct}@idp {SUPER_USER} READ 0 ProjA",
            f"{acct}@idp2 {OCU_SUPER_USER} READ 0 ProjA",
        ])
        st = reconcile(condor, pd)
        assert {p["Key"] for p in problems(st, "Malformed")} >= {f"{acct}@idp", f"{acct}@idp2"}
        assert pd.query("Identity", name=f"{acct}@idp") == []

        # At login, rows put there behind the daemon's back are refused.
        onboard(pd, acct, f"{acct}@login")
        write_database(condor, "UPDATE placementd_accounts SET name = ? WHERE name = ?",
            (f"{SUPER_USER}@{UID_DOMAIN}", f"{acct}@{UID_DOMAIN}"))
        expect_error(3, pd.userLogin, f"{acct}@login", "READ")
        write_database(condor, "UPDATE placementd_accounts SET name = ? WHERE name = ?",
            (f"{acct}@{UID_DOMAIN}", f"{SUPER_USER}@{UID_DOMAIN}"))
        pd.userLogin(f"{acct}@login", "READ")

        # The lists are read again on reconfig.
        added = unique("newsuper")
        original = condor.config_file.read_text()
        try:
            with open(condor.config_file, "a") as f:
                f.write(f"\nQUEUE_SUPER_USERS = root, condor, {SUPER_USER}, {added}\n")
            reconcile(condor, pd)
            expect_error(16, create_account, added)
        finally:
            condor.config_file.write_text(original)
            reconcile(condor, pd)
        create_account(added)

    def test_super_user_in_schedd_domain(self, condor, pd, mapfiles, restore_mapfiles):
        # The schedd qualifies a bare super user with its own UID_DOMAIN,
        # which here differs from the placementd's account domain.
        schedd_domain = condor.run_command(["condor_config_val", "UID_DOMAIN"]).stdout.strip()
        assert schedd_domain and schedd_domain.lower() != UID_DOMAIN
        account = f"{SUPER_USER}@{schedd_domain}"
        expect_error(16, pd.edit, [{"Operation": "Create", "TargetType": "Account", "Name": account}])
        assert pd.query("Account", name=account) == []

        ident = unique("sdom") + "@idp"
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{ident} {account} READ 0 ProjA"])
        st = reconcile(condor, pd)
        assert ident in {p["Key"] for p in problems(st, "Malformed")}
        assert pd.query("Identity", name=ident) == []
        assert pd.query("Account", name=account) == []

    def test_bindings_argument_errors(self, pd):
        with pytest.raises(ValueError):
            pd.edit([])
        with pytest.raises(TypeError):
            pd.edit(["not an operation"])
        with pytest.raises(ValueError):
            pd.query("Identity", limit=-1)
        with pytest.raises(htcondor.HTCondorException):
            pd.query("Identity", constraint="Identity ==")
        ads = pd.query("Identity", constraint=classad.ExprTree('Identity == "bob@idp"'))
        assert [ad["Identity"] for ad in ads] == ["bob@idp"]

    # ------------------------------------------------------------ login

    def test_login_after_rename(self, condor, pd):
        acct = unique("henry")
        account_id, identity_id = onboard(pd, acct, f"{acct}@idp")

        ad = pd.userLogin(f"{acct}@idp", "READ", "ProjA")
        assert token_subject(condor, ad["Token"]) == f"{acct}@{UID_DOMAIN}"

        results = pd.edit([{"Operation": "Update", "TargetType": "Account", "Id": account_id,
            "Name": acct + "2"}])
        assert "Warning" in results[0]
        ad = pd.userLogin(f"{acct}@idp", "READ", "ProjA")
        assert token_subject(condor, ad["Token"]) == f"{acct}2@{UID_DOMAIN}"

        tokens = pd.queryTokens(f"{acct}@idp")
        assert len(tokens) == 2
        for tok in tokens:
            assert tok["IdentityId"] == identity_id
            assert tok["AccountId"] == account_id
            assert tok["ProjectId"] == one(pd, "Project", "ProjA")["Id"]
        ident = one(pd, "Identity", f"{acct}@idp")
        assert ident["LastTokenExpiration"] == max(t["TokenExpiration"] for t in tokens)

        expect_error(10, pd.userLogin, f"{acct}@idp", "READ", "ProjB")
        expect_error(4, pd.userLogin, f"{acct}@idp", "WRITE")

    def test_mapfile_login(self, condor, pd):
        ad = pd.userLogin("alice@idp2", "READ,WRITE", "projb")
        assert token_subject(condor, ad["Token"]) == "alice@" + UID_DOMAIN

    def test_login_errors(self, condor, pd, test_dir):
        now = int(time.time())
        acct = unique("lerr")
        user = f"{acct}@idp"
        onboard(pd, acct, user)
        onboard(pd, acct + "x", f"{acct}@expired", expires=now - 60)
        onboard(pd, acct + "i", f"{acct}@instructor", authorizations=("READ", "INSTRUCTOR"))
        onboard(pd, acct + "e", f"{acct}@oldinstructor", authorizations=("READ", "INSTRUCTOR"), expires=now - 60)
        onboard(pd, acct + "n", f"{acct}@student", authorizations=("READ",))

        result = run_client(condor, test_dir, {"_condor_SEC_CLIENT_ENCRYPTION": "NEVER",
            "_condor_SEC_CLIENT_INTEGRITY": "NEVER", "_condor_SEC_CLIENT_CRYPTO_METHODS": "BLOWFISH"},
            f"pd.userLogin({user!r}, 'READ')")
        assert "error 1," in result, result
        expect_error(2, pd.userLogin, "", "READ")
        expect_error(3, pd.userLogin, f"{acct}@nobody", "READ")
        expect_error(6, pd.userLogin, f"{acct}@expired", "READ")
        expect_error(7, pd.userLogin, user, "READ", None, f"{acct}@nobody")
        expect_error(8, pd.userLogin, user, "READ", None, f"{acct}@student")
        expect_error(9, pd.userLogin, user, "READ", None, f"{acct}@oldinstructor")

        # An instructor may log a user in; the token log names both.
        ad = pd.userLogin(user, "READ", None, f"{acct}@instructor")
        assert token_subject(condor, ad["Token"]) == f"{acct}@{UID_DOMAIN}"
        tokens = pd.queryTokens(token_id=token_jti(ad["Token"]))
        assert [(t["Requester"], t["UserName"]) for t in tokens] == [(f"{acct}@instructor", user)]

    def test_token_lifetime_capped_by_expiry(self, condor, pd):
        acct = unique("cap")
        expires = int(time.time()) + 600
        onboard(pd, acct, f"{acct}@idp", expires=expires)
        ad = pd.userLogin(f"{acct}@idp", "READ")
        exp = token_claims(ad["Token"])["exp"]
        assert time.time() < exp <= expires + 1

    def test_query_tokens(self, condor, pd):
        acct = unique("toks")
        user = f"{acct}@idp"
        onboard(pd, acct, user)
        live = pd.userLogin(user, "READ")
        late = 2**31 + 12345     # past 2038: must not be truncated
        for jti, exp in ((acct + "late", late), (acct + "old", int(time.time()) - 60)):
            write_database(condor, "INSERT INTO placementd_tokens (requester_id, foreign_id, ap_id, authz,"
                " token_jti, token_exp, token_project) VALUES (?, ?, ?, 'READ', ?, ?, '')",
                (user, user, f"{acct}@{UID_DOMAIN}", jti, exp))
        assert {t["TokenId"] for t in pd.queryTokens(user)} == {token_jti(live["Token"]), acct + "late", acct + "old"}
        valid = pd.queryTokens(user, valid_only=True)
        assert {t["TokenId"] for t in valid} == {token_jti(live["Token"]), acct + "late"}
        assert [t["TokenExpiration"] for t in valid if t["TokenId"] == acct + "late"] == [late]
        assert [t["TokenId"] for t in pd.queryTokens(token_id=acct + "old")] == [acct + "old"]
        assert all(t["TokenExpiration"] >= time.time() for t in pd.queryTokens(valid_only=True))
        assert [u["TokenExpiration"] for u in pd.queryUsers(user)] == [late]
        assert one(pd, "Identity", user)["LastTokenExpiration"] == late

    def test_query_users_reports_mapped_account(self, pd):
        name = unique("requ")
        user, old, new = f"{name}@idp", f"{name}old", f"{name}new"
        identity_id = onboard(pd, old, user)[1]
        pd.edit([{"Operation": "Create", "TargetType": "Account", "Name": new, "Authorizations": ["READ"]}])
        token = pd.userLogin(user, "READ")["Token"]

        # Re-pointed: the legacy reply names the account the identity maps
        # to now, not the one its newest token was minted for.
        pd.edit([{"Operation": "Update", "TargetType": "Identity", "Id": identity_id, "Account": new}])
        users = pd.queryUsers(user)
        assert len(users) == 1
        assert users[0]["ApUserId"] == f"{new}@{UID_DOMAIN}"
        assert users[0]["MappedAccount"] == f"{new}@{UID_DOMAIN}"
        assert users[0]["Authorized"] is True
        assert users[0]["TokenExpiration"] == token_claims(token)["exp"]
        tokens = pd.queryTokens(user)
        assert [(t["TokenId"], t["ApUserId"]) for t in tokens] == [(token_jti(token), f"{old}@{UID_DOMAIN}")]

        # An identity known only from the token log keeps the token's
        # account, and has no MappedAccount.
        pd.edit([{"Operation": "Delete", "TargetType": "Identity", "Id": identity_id}])
        users = pd.queryUsers(user)
        assert len(users) == 1
        assert (users[0]["ApUserId"], users[0]["Authorized"]) == (f"{old}@{UID_DOMAIN}", False)
        assert "MappedAccount" not in users[0]

    def test_mapfile_changes_reach_login(self, condor, pd, mapfiles, restore_mapfiles):
        acct, authz = unique("chg"), unique("CHG").upper()
        user = f"{acct}@idp"
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{user} {acct} READ 0 ProjA"])
        write_authz(mapfiles["authz"], BASE_AUTHZ + [f"{authz}|Old label|normal|old"])
        reconcile(condor, pd)
        assert [u["Authorizations"] for u in pd.queryUsers(user)] == ["READ"]
        expect_error(4, pd.userLogin, user, f"READ,{authz}")

        # Change the grants, the expiry and the label of existing rows.
        soon = int(time.time()) + 3600
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{user} {acct} READ,{authz} {soon} ProjA"])
        write_authz(mapfiles["authz"], BASE_AUTHZ + [f"{authz}|New label|warning|new"])
        reconcile(condor, pd)
        users = pd.queryUsers(user)
        assert [(u["Authorizations"], u["MappingExpiration"]) for u in users] == [(f"READ,{authz}", soon)]
        assert [(a["Name"], a["Label"], a["Color"]) for a in pd.queryAuthorizations(user)
            if a["Name"] == authz] == [(authz, "New label", "warning")]
        ad = pd.userLogin(user, f"READ,{authz}")
        assert token_claims(ad["Token"])["exp"] <= soon + 1

    def test_reconcile_spelling_updates_holders(self, condor, pd, mapfiles, restore_mapfiles):
        acct, project, authz = unique("spell"), unique("SpellProj"), unique("SPELL").upper()
        write_users(mapfiles["users"], list(BASE_USERS.values()) + [f"{acct}@idp {acct} READ,{authz} 0 {project}"])
        write_authz(mapfiles["authz"], BASE_AUTHZ + [f"{authz}|Spell|normal|"])
        reconcile(condor, pd)
        account = one(pd, "Account", acct)

        # Only the spelling changes.
        write_users(mapfiles["users"], list(BASE_USERS.values()) +
            [f"{acct}@idp {acct} READ,{authz} 0 {project.lower()}"])
        write_authz(mapfiles["authz"], BASE_AUTHZ + [f"{authz.lower()}|Spell|normal|"])
        reconcile(condor, pd)
        after = one(pd, "Account", acct)
        assert list(after["Projects"]) == [project.lower()]
        assert sorted(after["Authorizations"]) == sorted(["READ", authz.lower()])
        assert after["Version"] > account["Version"]
        recorded = {}
        for mod in modifications(pd, account["Id"]):
            if mod["Id"] <= account["Version"]:
                break
            assert (mod["Operation"], mod["Channel"]) == ("update", "mapfile")
            recorded.update(changes(mod))
        assert recorded["Projects"] == ([project], [project.lower()])
        assert sorted(recorded["Authorizations"][1]) == sorted(["READ", authz.lower()])

    def test_schedd_disabled_records_refuse_login(self, condor, pd):
        # The schedd matches project names without case, and user domains
        # without case and by prefix.  Logging in must neither mint for a
        # disabled record nor re-enable it.
        acct, project = unique("sched"), unique("SchedProj")
        onboard(pd, acct, f"{acct}@idp", projects=())
        pd.edit([{"Operation": "Create", "TargetType": "Project", "Name": project.lower()},
            {"Operation": "Update", "TargetType": "Account", "Name": acct, "Projects": [project.lower()]}])
        qusers(condor, "-projects", "-add", project)
        qusers(condor, "-projects", "-disable", project)
        assert schedd_enabled(condor, project, project=True) == {project: False}
        expect_error(12, pd.userLogin, f"{acct}@idp", "READ", project.lower())
        assert schedd_enabled(condor, project, project=True) == {project: False}

        # Without the project, the user record is created (enabled).
        pd.userLogin(f"{acct}@idp", "READ")
        assert schedd_enabled(condor, f"{acct}@{UID_DOMAIN}") == {f"{acct}@{UID_DOMAIN}": True}

        # A disabled user record whose domain differs in case, or is a
        # prefix of the account's.
        for domain in (UID_DOMAIN.upper(), UID_DOMAIN.split(".")[0]):
            user = unique("schedu")
            onboard(pd, user, f"{user}@idp")
            record = f"{user}@{domain}"
            qusers(condor, "-add", record)
            qusers(condor, "-disable", record)
            assert schedd_enabled(condor, record) == {record: False}
            expect_error(12, pd.userLogin, f"{user}@idp", "READ")
            assert schedd_enabled(condor, record) == {record: False}

        # The schedd's record of a new project is created at login.
        fresh = unique("FreshProj")
        pd.edit([{"Operation": "Create", "TargetType": "Project", "Name": fresh},
            {"Operation": "Update", "TargetType": "Account", "Name": acct, "Projects": [fresh]}])
        pd.userLogin(f"{acct}@idp", "READ", fresh)
        assert schedd_enabled(condor, fresh, project=True) == {fresh: True}

    def test_unlogged_token_is_revoked(self, condor, pd):
        # The auth layer records a token before the placementd logs it.  If
        # the log write fails, the client gets no token and the recorded
        # one is removed, which revokes it.
        acct = unique("unlogged")
        onboard(pd, acct, f"{acct}@idp")
        trigger = unique("deny_token_log")

        def minted():
            db = sqlite3.connect(f"file:{tokens_database(condor)}?mode=ro", uri=True)
            try:
                return db.execute("SELECT COUNT(*) FROM idtokens_minting WHERE token_sub = ?",
                    (f"{acct}@{UID_DOMAIN}",)).fetchone()[0]
            finally:
                db.close()

        write_database(condor, f"CREATE TRIGGER {trigger} BEFORE INSERT ON placementd_tokens"
            " BEGIN SELECT RAISE(ABORT, 'token log is read-only'); END")
        try:
            msg = expect_error(11, pd.userLogin, f"{acct}@idp", "READ")
            assert "record the token" in msg
            assert minted() == 0
        finally:
            write_database(condor, f"DROP TRIGGER {trigger}")
        pd.userLogin(f"{acct}@idp", "READ")
        assert minted() == 1

    def test_schedd_failure_is_error_12(self, condor, pd, test_dir):
        acct = unique("noschedd")
        onboard(pd, acct, f"{acct}@idp")
        # A schedd address that refuses connections.
        address_file = test_dir / "unreachable_schedd_address"
        address_file.write_text("<127.0.0.1:9>\n")
        original = condor.config_file.read_text()
        try:
            with open(condor.config_file, "a") as f:
                f.write(f"\nPLACEMENTD.SCHEDD_DAEMON_AD_FILE = {(test_dir / 'no_such_ad').as_posix()}\n"
                    f"PLACEMENTD.SCHEDD_ADDRESS_FILE = {address_file.as_posix()}\n")
            reconcile(condor, pd)
            msg = expect_error(12, pd.userLogin, f"{acct}@idp", "READ")
            assert "schedd" in msg.split("error 12,", 1)[1]
        finally:
            condor.config_file.write_text(original)
            reconcile(condor, pd)
        pd.userLogin(f"{acct}@idp", "READ")

    def test_login_refuses_grantless_account(self, pd):
        acct = unique("nogrants")
        onboard(pd, acct, f"{acct}@idp", authorizations=(), projects=())
        # An empty bounding set would be an unscoped token.
        expect_error(4, pd.userLogin, f"{acct}@idp", "")

    def test_login_refuses_what_may_not_be_minted(self, condor, pd):
        # Rows the daemon would never write, put there behind its back:
        # login still refuses them.
        acct = unique("resv")
        onboard(pd, acct, f"{acct}@idp")
        write_database(condor, "UPDATE placementd_accounts SET name = ? WHERE name = ?",
            (f"condor@{UID_DOMAIN}", f"{acct}@{UID_DOMAIN}"))
        expect_error(3, pd.userLogin, f"{acct}@idp", "READ")
        write_database(condor, "UPDATE placementd_accounts SET name = ? WHERE name = ?",
            (f"{acct}@{UID_DOMAIN}", f"condor@{UID_DOMAIN}"))
        pd.userLogin(f"{acct}@idp", "READ")

        write_database(condor, "INSERT INTO placementd_authorizations (uuid, name, label, source, modification_id)"
            " VALUES (?, 'DAEMON', 'DAEMON', 'internal', (SELECT MAX(id) FROM placementd_modifications))",
            (str(uuid.uuid4()),))
        write_database(condor, "INSERT INTO placementd_account_authorizations (account_id, authorization_id)"
            " SELECT a.id, z.id FROM placementd_accounts a, placementd_authorizations z"
            " WHERE a.name = ? AND z.name = 'DAEMON'", (f"{acct}@{UID_DOMAIN}",))
        expect_error(4, pd.userLogin, f"{acct}@idp", "")
        expect_error(4, pd.userLogin, f"{acct}@idp", "DAEMON")
        pd.userLogin(f"{acct}@idp", "READ")

    def test_empty_capability_token_authorizes_nothing(self, condor, pd, test_dir):
        acct = unique("cap")
        onboard(pd, acct, f"{acct}@idp")
        token = pd.userLogin(f"{acct}@idp", "READ")["Token"]
        token_dir = test_dir / "captokens"
        token_dir.mkdir(exist_ok=True)
        (token_dir / "cap").write_text(token + "\n")
        (token_dir / "cap").chmod(0o600)     # tokens must be private
        env = token_only_env(token_dir)

        def set_capabilities(scope):
            write_database(condor, "UPDATE idtokens_minting SET token_scope = ? WHERE token_jti = ?",
                (scope, token_jti(token)))

        # The collector honors capabilities.  The capability path is live: a
        # token listing ADMINISTRATOR gets it.
        set_capabilities("condor:/ADMINISTRATOR")
        assert ping(condor, env, "collector", "ADMINISTRATOR").returncode == 0
        # An empty list authorizes nothing.
        set_capabilities("")
        assert ping(condor, env, "collector", "ADMINISTRATOR").returncode != 0

        # The schedd treats a capability as a plain token limited to what it
        # lists...
        set_capabilities("condor:/READ")
        rv = ping(condor, env, "schedd", "READ")
        assert rv.returncode == 0 and f"{acct}@{UID_DOMAIN}" in rv.stdout
        # ...so one listing nothing would carry its subject's full
        # authority.  It authorizes nothing instead, as does a missing list.
        for scope in ("", None):
            set_capabilities(scope)
            assert ping(condor, env, "schedd", "READ").returncode != 0
        set_capabilities("condor:/READ")
        assert ping(condor, env, "schedd", "READ").returncode == 0

        # A plain token is unaffected.
        plain_dir = test_dir / "plaintokens"
        plain_dir.mkdir(exist_ok=True)
        rv = condor.run_command(["env", f"_condor_SEC_TOKEN_DIRECTORY={plain_dir.as_posix()}",
            "condor_token_create", "-identity", f"{acct}plain@{UID_DOMAIN}", "-token", "plain"])
        assert rv.returncode == 0
        rv = ping(condor, token_only_env(plain_dir), "schedd", "READ")
        assert rv.returncode == 0 and f"{acct}plain@{UID_DOMAIN}" in rv.stdout

    # ------------------------------------------------------------ revocation

    def test_edits_revoke_tokens(self, condor, pd, test_dir):
        name = unique("rvk")
        now = int(time.time())

        def account(suffix, **kwargs):
            acct = name + suffix
            account_id, identity_id = onboard(pd, acct, f"{acct}@idp", **kwargs)
            return f"{acct}@idp", account_id, identity_id

        def login(user):
            return pd.userLogin(user, "READ")["Token"]

        def refused(token):
            return not minted(condor, token) and not token_accepted(condor, test_dir, token)

        # Deleting the identity.
        user, account_id, identity_id = account("del")
        token = login(user)
        assert token_accepted(condor, test_dir, token)
        pd.edit([{"Operation": "Delete", "TargetType": "Identity", "Id": identity_id}])
        assert refused(token)
        assert revoked_tokens(pd, identity_id) == 1

        # Deleting the account, with its identities.
        user, account_id, identity_id = account("delacct")
        token = login(user)
        pd.edit([{"Operation": "Delete", "TargetType": "Account", "Id": account_id, "Cascade": True}])
        assert refused(token)
        assert revoked_tokens(pd, identity_id) == 1

        # Renaming the account: tokens for the old name go, and new ones
        # carry the new name.
        user, account_id, identity_id = account("ren")
        token = login(user)
        results = pd.edit([{"Operation": "Update", "TargetType": "Account", "Id": account_id,
            "Name": name + "ren2"}])
        assert "revoked" in results[0]["Warning"]
        assert results[0]["RevokedTokens"] == 1
        assert refused(token)
        assert revoked_tokens(pd, account_id) == 1
        assert token_accepted(condor, test_dir, login(user))

        # Narrowing the account's grants.  Widening them, or changing the
        # default project, keeps its tokens.
        user, account_id, identity_id = account("narrow", authorizations=("READ", "WRITE"),
            projects=("ProjA", "ProjB"))
        token = login(user)
        results = pd.edit([{"Operation": "Update", "TargetType": "Account", "Id": account_id,
            "Authorizations": ["READ", "WRITE", "INSTRUCTOR"], "DefaultProject": "ProjB"}])
        assert "RevokedTokens" not in results[0]
        assert revoked_tokens(pd, account_id) is None
        assert token_accepted(condor, test_dir, token)
        pd.edit([{"Operation": "Update", "TargetType": "Account", "Id": account_id, "Authorizations": ["READ"]}])
        assert refused(token)
        assert revoked_tokens(pd, account_id) == 1
        token = login(user)
        pd.edit([{"Operation": "Update", "TargetType": "Account", "Id": account_id, "Projects": ["ProjA"]}])
        assert refused(token)

        # Lowering the identity's expiry revokes the tokens that outlive it.
        user, account_id, identity_id = account("exp", expires=now + 600)
        short = login(user)     # its lifetime is capped at the expiry
        pd.edit([{"Operation": "Update", "TargetType": "Identity", "Id": identity_id,
            "ExpiresAt": classad.Value.Undefined}])
        assert revoked_tokens(pd, identity_id) is None
        full = login(user)
        pd.edit([{"Operation": "Update", "TargetType": "Identity", "Id": identity_id, "ExpiresAt": now + 3600}])
        assert refused(full)
        assert token_accepted(condor, test_dir, short)
        assert revoked_tokens(pd, identity_id) == 1

        # Moving the identity to another account.
        user, account_id, identity_id = account("move")
        pd.edit([{"Operation": "Create", "TargetType": "Account", "Name": name + "moved", "Authorizations": ["READ"]}])
        token = login(user)
        pd.edit([{"Operation": "Update", "TargetType": "Identity", "Id": identity_id, "Account": name + "moved"}])
        assert refused(token)

        # Renaming an authorization the account holds (other than in case):
        # its tokens carry the old name.
        authz = unique("RVKAUTH").upper()
        authz_id = pd.edit([{"Operation": "Create", "TargetType": "Authorization", "Name": authz}])[0]["Id"]
        user, account_id, identity_id = account("authz", authorizations=("READ", authz))
        token = login(user)
        pd.edit([{"Operation": "Update", "TargetType": "Authorization", "Id": authz_id, "Name": authz.lower()}])
        assert minted(condor, token)
        pd.edit([{"Operation": "Update", "TargetType": "Authorization", "Id": authz_id, "Name": authz + "X"}])
        assert refused(token)
        assert revoked_tokens(pd, account_id) == 1

        # Renaming a project is accounting only; deleting one an account
        # holds narrows the account.
        project = unique("RvkProj")
        project_id = pd.edit([{"Operation": "Create", "TargetType": "Project", "Name": project}])[0]["Id"]
        user, account_id, identity_id = account("proj", projects=("ProjA", project))
        token = login(user)
        pd.edit([{"Operation": "Update", "TargetType": "Project", "Id": project_id, "Name": project + "X"}])
        assert minted(condor, token)
        pd.edit([{"Operation": "Delete", "TargetType": "Project", "Id": project_id, "Cascade": True}])
        assert refused(token)
        assert revoked_tokens(pd, account_id) == 1

        # A dry run reports what it would revoke, and revokes nothing.
        user, account_id, identity_id = account("dry")
        token = login(user)
        results = pd.edit([{"Operation": "Delete", "TargetType": "Identity", "Id": identity_id}], dry_run=True)
        assert results[0]["RevokedTokens"] == 1
        assert token_accepted(condor, test_dir, token)

    def test_revoke_operation(self, condor, pd, test_dir):
        name = unique("rvop")
        account_id, identity_id = onboard(pd, name, f"{name}@idp")
        pd.edit([{"Operation": "Create", "TargetType": "Identity", "Identity": f"{name}@idp2", "Account": account_id}])

        def login(user):
            return pd.userLogin(user, "READ")["Token"]

        first, second, other = login(f"{name}@idp"), login(f"{name}@idp"), login(f"{name}@idp2")
        before = pd.query("Identity", id=identity_id)[0]
        results = pd.edit([{"Operation": "Revoke", "TargetType": "Identity", "Id": identity_id,
            "ExpectedVersion": before["Version"]}], on_behalf_of="webadmin@example.org")
        assert results[0]["RevokedTokens"] == 2
        assert results[0]["Version"] > before["Version"]
        assert not token_accepted(condor, test_dir, first)
        assert not minted(condor, second)
        assert token_accepted(condor, test_dir, other)
        mod = modifications(pd, identity_id)[0]
        assert (mod["Operation"], mod["OnBehalfOf"], mod["Id"]) == ("update", "webadmin@example.org",
            results[0]["Version"])
        assert changes(mod) == {"RevokedTokens": (None, 2)}
        # The identity is otherwise unchanged, and may log in again.
        after = pd.query("Identity", id=identity_id)[0]
        assert (after["Account"], after["Source"]) == (before["Account"], before["Source"])
        fresh = login(f"{name}@idp")
        assert token_accepted(condor, test_dir, fresh)

        # An account's tokens, from all its identities.
        results = pd.edit([{"Operation": "Revoke", "TargetType": "Account", "Name": name}])
        assert results[0]["RevokedTokens"] == 2
        assert not minted(condor, fresh) and not minted(condor, other)
        assert revoked_tokens(pd, account_id) == 2
        # Nothing left to revoke: nothing is written.
        last = latest_modification_id(pd)
        results = pd.edit([{"Operation": "Revoke", "TargetType": "Account", "Id": account_id}])
        assert results[0]["RevokedTokens"] == 0
        assert latest_modification_id(pd) == last

        # A map file row's tokens may be revoked without adopting it.
        token = login("bob@idp")
        results = pd.edit([{"Operation": "Revoke", "TargetType": "Identity", "Identity": "bob@idp"}])
        assert results[0]["RevokedTokens"] >= 1
        assert not minted(condor, token)
        assert one(pd, "Identity", "bob@idp")["Source"] == "mapfile"

        expect_error(19, pd.edit, [{"Operation": "Revoke", "TargetType": "Project", "Name": "ProjA"}])

    def test_reconcile_revokes_tokens(self, condor, pd, mapfiles, test_dir, restore_mapfiles):
        name = unique("rvrc")
        keys = ("gone", "soon", "narrow", "moved", "bad", "kept")
        lines = {key: f"{name}{key}@idp {name}{key} READ 0 ProjA" for key in keys}
        lines["narrow"] = f"{name}narrow@idp {name}narrow READ,WRITE 0 ProjA"
        write_users(mapfiles["users"], list(BASE_USERS.values()) + list(lines.values()))
        reconcile(condor, pd)
        tokens = {key: pd.userLogin(f"{name}{key}@idp", "READ")["Token"] for key in keys}
        ids = {key: one(pd, "Identity", f"{name}{key}@idp")["Id"] for key in keys}
        narrow_account = one(pd, "Account", name + "narrow")["Id"]

        soon = int(time.time()) + 3600
        del lines["gone"]
        lines["soon"] = f"{name}soon@idp {name}soon READ {soon} ProjA"
        lines["narrow"] = f"{name}narrow@idp {name}narrow READ 0 ProjA"
        lines["moved"] = f"{name}moved@idp {name}elsewhere READ 0 ProjA"
        # Malformed (six fields), with an earlier expiry that still applies.
        lines["bad"] = f"{name}bad@idp {name}bad READ {soon} ProjA extra"
        write_users(mapfiles["users"], list(BASE_USERS.values()) + list(lines.values()))
        reconcile(condor, pd)

        for key in ("gone", "soon", "narrow", "moved", "bad"):
            assert not minted(condor, tokens[key]), key
        assert not token_accepted(condor, test_dir, tokens["gone"])
        assert token_accepted(condor, test_dir, tokens["kept"])
        for key in ("gone", "soon", "moved", "bad"):
            assert revoked_tokens(pd, ids[key]) == 1, key
        assert revoked_tokens(pd, narrow_account) == 1

    # ------------------------------------------------------------ transport

    def test_unencrypted_edit_refused(self, condor, pd, test_dir):
        project = unique("Plaintext")
        result = run_client(condor, test_dir, {"_condor_SEC_CLIENT_ENCRYPTION": "NEVER",
            "_condor_SEC_CLIENT_INTEGRITY": "NEVER", "_condor_SEC_CLIENT_CRYPTO_METHODS": "BLOWFISH"},
            f"pd.edit([{{'Operation': 'Create', 'TargetType': 'Project', 'Name': '{project}'}}])")
        assert result.startswith("RESULT ERROR"), result
        assert "error 1," in result
        assert pd.query("Project", name=project) == []

    def test_non_administrator_refused(self, condor, pd, test_dir):
        token_dir = test_dir / "joetokens"
        token_dir.mkdir(exist_ok=True)
        rv = condor.run_command(["env", f"_condor_SEC_TOKEN_DIRECTORY={token_dir.as_posix()}",
            "condor_token_create", "-identity", "joe@here", "-token", "joe"])
        assert rv.returncode == 0
        assert (token_dir / "joe").exists()
        env = token_only_env(token_dir)

        # joe authenticates (READ works on the collector)...
        rv = condor.run_command(["env", "_condor_SEC_CLIENT_AUTHENTICATION=REQUIRED"] +
            [f"{k}={v}" for k, v in env.items()] + ["condor_ping", "-type", "collector", "-verbose", "READ"])
        assert rv.returncode == 0
        assert "joe@here" in rv.stdout
        # ...and the placementd refuses him.
        project = unique("Intruder")
        for body in (
            f"pd.edit([{{'Operation': 'Create', 'TargetType': 'Project', 'Name': '{project}'}}])",
            "pd.query('Identity')",
        ):
            result = run_client(condor, test_dir, env, body)
            assert result.startswith("RESULT ERROR Failed to contact PlacementD"), result
        assert pd.query("Project", name=project) == []
        placementd_log = (condor.log_dir / "PlacementdLog").read_text()
        assert "PERMISSION DENIED to joe@here" in placementd_log


# A database written by a daemon from before the schema existed: only the
# token log, here already carrying one of the new columns.
V1_TOKEN = ("requester@idp", "olduser@idp", "olduser@" + UID_DOMAIN, "READ", "v1jti", FAR_FUTURE, "")

V1_USERS = [
    "first@idp first READ,WRITE 0 ProjA",
    "first@idp2 first READ 0 ProjB",
    "split@idp one READ 0 ProjA",
    "split@idp two READ 0 ProjA",
]


@standup
def v1_files(test_dir):
    base = test_dir / "v1"
    base.mkdir(parents=True, exist_ok=True)
    database = base / "TokensDatabase"
    db = sqlite3.connect(database.as_posix())
    db.execute("CREATE TABLE placementd_tokens (requester_id TEXT, foreign_id TEXT, ap_id TEXT, authz TEXT,"
        " token_jti TEXT, token_exp INTEGER, token_project TEXT, identity_uuid TEXT)")
    db.execute("INSERT INTO placementd_tokens (requester_id, foreign_id, ap_id, authz, token_jti, token_exp,"
        " token_project) VALUES (?, ?, ?, ?, ?, ?, ?)", V1_TOKEN)
    db.commit()
    db.close()
    users = base / "placementd_mapfile"
    write_users(users, V1_USERS)
    authz = base / "placementd_authorizations_mapfile"
    write_authz(authz, BASE_AUTHZ)
    return {"database": database, "users": users, "authz": authz}


@standup
def v1_condor(test_dir, v1_files):
    with Condor(
        local_dir=test_dir / "v1" / "condor",
        config={
            "DAEMON_LIST": "MASTER COLLECTOR PLACEMENTD",
            "TOKENS_DATABASE": v1_files["database"].as_posix(),
            "PLACEMENTD_MAPFILE": v1_files["users"].as_posix(),
            "PLACEMENTD_AUTHORIZATIONS_MAPFILE": v1_files["authz"].as_posix(),
            "PLACEMENTD_UID_DOMAIN": UID_DOMAIN,
        },
    ) as condor:
        yield condor


def restart_placementd(condor, while_stopped):
    rv = condor.run_command(["condor_off", "-fast", "-daemon", "placementd"])
    assert rv.returncode == 0
    deadline = time.time() + 60
    while condor.run_command(["condor_status", "-any", "-af", "Name", "-constraint",
            'MyType == "PlacementD"']).stdout.strip():
        assert time.time() < deadline, "placementd did not stop"
        time.sleep(0.5)
    while_stopped()
    rv = condor.run_command(["condor_on", "-daemon", "placementd"])
    assert rv.returncode == 0


def wait_for_placementd_restart(since):
    """The Placementd client for a daemon started at or after since."""
    deadline = time.time() + 60
    while True:
        try:
            ad = placementd_ad()
            if ad.get("DaemonStartTime", 0) >= since:
                pd = htcondor.Placementd(ad)
                pd.query("MapfileStatus")
                return pd
        except Exception:
            if time.time() > deadline:
                raise
        assert time.time() < deadline, "placementd did not restart"
        time.sleep(0.5)


def wait_for_log(condor, text):
    log = condor.log_dir / "PlacementdLog"
    deadline = time.time() + 60
    while text not in log.read_text():
        assert time.time() < deadline, f"no '{text}' in the placementd log"
        time.sleep(0.5)


@action
def v1_pd(v1_condor):
    with v1_condor.use_config():
        htcondor.reload_config()
        return wait_for_placementd()


class TestPlacementdMigration:

    def test_migration_from_v1(self, v1_pd, v1_files):
        db = sqlite3.connect(f"file:{v1_files['database'].as_posix()}?mode=ro", uri=True)
        try:
            version = db.execute("SELECT value FROM placementd_meta WHERE key = 'schema_version'").fetchall()
            columns = [row[1] for row in db.execute("PRAGMA table_info(placementd_tokens)")]
        finally:
            db.close()
        assert version == [("2",)]
        # Only the missing columns were added.
        assert columns.count("identity_uuid") == 1
        assert {"account_uuid", "project_uuid"} <= set(columns)

        # The old token log survives and is served as before.
        tokens = v1_pd.queryTokens("olduser@idp")
        assert len(tokens) == 1
        assert tokens[0]["TokenId"] == "v1jti"
        assert "IdentityId" not in tokens[0]
        assert [u["Authorized"] for u in v1_pd.queryUsers("olduser@idp")] == [False]

        # The first import resolves conflicts instead of leaving users out:
        # the account's lines disagree, so it gets what they share...
        first = one(v1_pd, "Account", "first")
        assert list(first["Authorizations"]) == ["READ"]
        assert sorted(first["Projects"]) == ["ProjA", "ProjB"]
        assert sorted(first["Identities"]) == ["first@idp", "first@idp2"]
        # ...and says what it dropped.
        narrowed = problems(status(v1_pd), "GrantsNarrowed")
        assert [p["Key"] for p in narrowed] == ["first@" + UID_DOMAIN]
        assert "WRITE" in narrowed[0]["Message"]
        # ...while an identity mapped to two accounts gets no row.
        assert v1_pd.query("Identity", name="split@idp") == []

        # One change set, one create each.
        mods = v1_pd.query("Modification")
        assert len({m["ChangesetId"] for m in mods}) == 1
        assert {(m["Operation"], m["Channel"], m["Actor"]) for m in mods} == {("create", "mapfile", "mapfile")}

    def test_reconcile_disabled_by_empty_mapfile(self, v1_condor, v1_pd, v1_files):
        before = snapshot(v1_pd)
        with open(v1_condor.config_file, "a") as f:
            f.write("\nPLACEMENTD_MAPFILE =\n")
        write_users(v1_files["users"], [])     # would revoke everything
        st = reconcile(v1_condor, v1_pd)
        assert st["UserMapfile"] == ""
        assert st["PlacementdMapfileOK"] is True
        assert snapshot(v1_pd) == before

    def test_restart_keeps_v2_database(self, v1_condor, v1_pd, v1_files):
        before = snapshot(v1_pd)
        last_modification = latest_modification_id(v1_pd)
        since = int(time.time())
        restart_placementd(v1_condor, lambda: None)
        with v1_condor.use_config():
            pd = wait_for_placementd_restart(since)
        assert snapshot(pd) == before
        assert latest_modification_id(pd) == last_modification
        assert len(pd.queryTokens("olduser@idp")) == 1
        log = (v1_condor.log_dir / "PlacementdLog").read_text()
        assert log.count("Migrating placementd database") == 1

    def test_v2_without_audit_log_refused(self, v1_condor, v1_pd, v1_files):
        # A database that claims version 2 but was written before the audit
        # log existed.
        def drop_audit_log():
            db = sqlite3.connect(v1_files["database"].as_posix(), timeout=10)
            try:
                db.execute("DROP TABLE placementd_modifications")
                db.commit()
            finally:
                db.close()
        restart_placementd(v1_condor, drop_audit_log)
        wait_for_log(v1_condor, "lacks table placementd_modifications")

    def test_newer_schema_refused(self, v1_condor, v1_pd, v1_files):
        def bump_version():
            db = sqlite3.connect(v1_files["database"].as_posix(), timeout=10)
            try:
                db.execute("UPDATE placementd_meta SET value = '99' WHERE key = 'schema_version'")
                db.commit()
            finally:
                db.close()
        restart_placementd(v1_condor, bump_version)
        wait_for_log(v1_condor, "newer than this daemon supports")
