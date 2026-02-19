#!/usr/bin/env pytest

# Test HTCondor offline ads functionality.
#
# HTCondor's collector supports two related features:
#
#   Offline ads – Machine ClassAds that are stored persistently on disk by
#   the collector's OfflineCollectorPlugin, even after the startd stops
#   updating them (e.g. because the machine is hibernating). They survive
#   a collector restart and are reloaded into the in-memory store on
#   start-up.  A machine ad becomes offline either by carrying Offline=true
#   when advertised (UPDATE_STARTD_AD) or by being sent via the
#   UPDATE_STARTD_AD_WITH_ACK command.
#
#   Absent ads – When ABSENT_REQUIREMENTS is configured and a machine ad
#   expires (its ClassAdLifetime elapses without a refresh), the collector
#   evaluates the expression against the expiring ad instead of deleting it.
#   If the expression is true the ad is marked Absent=true, given a fresh
#   ClassAdLifetime (ABSENT_EXPIRE_ADS_AFTER), and kept in the in-memory
#   store.  Normal (condor_status-style) queries automatically filter out
#   absent ads; they can be retrieved by including "Absent == true" in the
#   query constraint.

import logging
import time

import classad2 as classad
import htcondor2 as htcondor

from ornithology import (
    standup,
    action,
    Condor,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Fake address for synthetic machine ads.  We are not running a real startd,
# so we use a placeholder that satisfies the collector's hash-key requirements.
_FAKE_ADDR = "<127.0.0.1:9999?addrs=127.0.0.1-9999&noUDP>"


def _machine_ad(name, **extra):
    """Return a minimal Machine ClassAd suitable for advertising to a collector."""
    attrs = {
        "MyType": "Machine",
        "TargetType": "Job",
        "Name": name,
        "Machine": name,
        "MyAddress": _FAKE_ADDR,
        "StartdIpAddr": "127.0.0.1",
        "IsPytest": True,
    }
    attrs.update(extra)
    return classad.ClassAd(attrs)


# ──────────────────────────────────────────────────────────────────────────────
# Shared fixtures for offline-ad tests
# ──────────────────────────────────────────────────────────────────────────────

@standup
def condor_offline(test_dir):
    """Minimal HTCondor pool (collector + master) with persistent offline ad storage."""
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "DAEMON_LIST": "COLLECTOR MASTER",
            "USE_SHARED_PORT": False,
            # Enable persistent offline ad storage.
            "COLLECTOR_PERSISTENT_AD_LOG": str(test_dir / "offline.ads"),
        },
    ) as condor:
        yield condor


@action
def offline_machine_name():
    return "offline-test-machine.example.com"


@action
def advertised_offline_ad(condor_offline, offline_machine_name):
    """Advertise a machine ad with Offline=true and wait for it to appear in the
    collector's in-memory store.  Returns the queried ClassAd, or None on timeout."""
    collector = condor_offline.get_local_collector()
    collector.advertise(
        [_machine_ad(offline_machine_name, Offline=True)],
        "UPDATE_STARTD_AD",
    )
    constraint = f'Name == "{offline_machine_name}" && Offline == true'
    for _ in range(30):
        ads = collector.query(htcondor.AdTypes.Startd, constraint)
        if ads:
            return ads[0]
        time.sleep(1)
    return None


# ──────────────────────────────────────────────────────────────────────────────
# TestOfflineAdBasic – the ad appears with the right attributes
# ──────────────────────────────────────────────────────────────────────────────

class TestOfflineAdBasic:
    """An ad advertised with Offline=true is stored in the collector and on disk."""

    def test_offline_ad_appears_in_collector(self, advertised_offline_ad):
        assert advertised_offline_ad is not None, (
            "Timed out waiting for offline ad to appear in collector"
        )

    def test_offline_attribute_is_true(self, advertised_offline_ad):
        assert advertised_offline_ad is not None
        assert advertised_offline_ad.get("Offline") is True

    def test_offline_ad_name_is_correct(self, advertised_offline_ad, offline_machine_name):
        assert advertised_offline_ad is not None
        assert advertised_offline_ad.get("Name") == offline_machine_name

    def test_persistent_log_file_created(self, advertised_offline_ad, test_dir):
        """The offline plugin must write a non-empty persistent log file."""
        assert advertised_offline_ad is not None
        log_path = test_dir / "offline.ads"
        assert log_path.exists(), "Persistent offline ad log was not created"
        assert log_path.stat().st_size > 0, "Persistent offline ad log is empty"

    def test_machine_name_in_persistent_log(self, advertised_offline_ad, offline_machine_name, test_dir):
        """The machine name must be written into the persistent log."""
        assert advertised_offline_ad is not None
        log_path = test_dir / "offline.ads"
        content = log_path.read_text(errors="replace")
        assert offline_machine_name in content, (
            f'"{offline_machine_name}" not found in persistent log {log_path}'
        )


# ──────────────────────────────────────────────────────────────────────────────
# TestOfflineAdPersistence – the ad survives a collector restart
# ──────────────────────────────────────────────────────────────────────────────

@action
def offline_ad_after_restart(condor_offline, advertised_offline_ad, offline_machine_name):
    """Restart the collector and wait for the offline ad to reappear.  The
    OfflineCollectorPlugin replays ads from the persistent log at start-up.

    We use condor_offline.status() inside the retry loop rather than caching a
    Collector object.  status() calls get_local_collector() inside a fresh
    use_config() context on every invocation, which calls htcondor.reload_config()
    and re-reads the COLLECTOR_ADDRESS_FILE.  This is necessary because the pool
    uses COLLECTOR_HOST=$(CONDOR_HOST):0 (a random port chosen at start-up), so
    after a restart the collector picks a NEW port and updates the address file.
    A cached Collector object would silently keep connecting to the dead old port.
    """
    assert advertised_offline_ad is not None, "Prerequisite: offline ad must have been advertised"

    cp = condor_offline.run_command(["condor_restart", "-collector"])
    assert cp.returncode == 0, f"condor_restart -collector failed: {cp.stderr}"

    # Poll using status() so that the collector address is re-resolved on every
    # attempt after the restart.
    constraint = f'Name == "{offline_machine_name}" && Offline == true'
    for _ in range(60):
        try:
            ads = condor_offline.status(
                ad_type=htcondor.AdTypes.Startd,
                constraint=constraint,
            )
            if ads:
                return ads[0]
        except Exception:
            pass  # Collector may be briefly unavailable during restart.
        time.sleep(1)
    return None


class TestOfflineAdPersistence:
    """Offline ads are reloaded from the persistent log after a collector restart."""

    def test_offline_ad_survives_restart(self, offline_ad_after_restart):
        assert offline_ad_after_restart is not None, (
            "Offline ad did not reappear in collector after restart"
        )

    def test_offline_attribute_preserved_after_restart(self, offline_ad_after_restart):
        assert offline_ad_after_restart is not None
        assert offline_ad_after_restart.get("Offline") is True


# ──────────────────────────────────────────────────────────────────────────────
# TestOfflineAdInvalidation – INVALIDATE removes the ad from memory and disk
# ──────────────────────────────────────────────────────────────────────────────

@action
def offline_ad_invalidated(condor_offline, advertised_offline_ad, offline_machine_name):
    """Send INVALIDATE_STARTD_ADS for the offline machine and wait for it to
    disappear from the collector.  Returns True when gone, False on timeout."""
    assert advertised_offline_ad is not None, "Prerequisite: offline ad must have been advertised"

    collector = condor_offline.get_local_collector()
    invalidate_ad = classad.ClassAd({
        "MyType": "Machine",
        "Name": offline_machine_name,
        "Machine": offline_machine_name,
        "MyAddress": _FAKE_ADDR,
        "StartdIpAddr": "127.0.0.1",
    })
    collector.advertise([invalidate_ad], "INVALIDATE_STARTD_ADS")

    constraint = f'Name == "{offline_machine_name}"'
    for _ in range(30):
        ads = collector.query(htcondor.AdTypes.Startd, constraint)
        if not ads:
            return True
        time.sleep(1)
    return False


class TestOfflineAdInvalidation:
    """Sending INVALIDATE_STARTD_ADS removes an offline ad from the collector
    and from the persistent log (the ad must not reappear after a restart)."""

    def test_offline_ad_gone_after_invalidate(self, offline_ad_invalidated):
        assert offline_ad_invalidated, (
            "Offline ad did not disappear from collector after INVALIDATE_STARTD_ADS"
        )

    def test_destroy_recorded_in_persistent_log(
        self, offline_ad_invalidated, offline_machine_name, test_dir
    ):
        """Invalidation must write a DestroyClassAd entry to the persistent log.

        The ClassAdCollection log is append-only: old CreateClassAd / SetAttribute
        entries for the machine remain in the file even after the ad is destroyed.
        The correct signal that the ad was invalidated is the presence of an op-102
        (DestroyClassAd) record whose key is the machine's offline key.

        The offline key is produced by AdNameHashKey::sprint() – which formats the
        name as "< name >" – followed by RemoveAllWhitespace(), giving "<name>".
        The on-disk record therefore looks like:
            102 <offline-test-machine.example.com>
        """
        assert offline_ad_invalidated
        log_path = test_dir / "offline.ads"
        assert log_path.exists(), "Persistent offline ad log was not created"
        content = log_path.read_text(errors="replace")
        # Op 102 = CondorLogOp_DestroyClassAd; key has whitespace removed.
        expected = f"102 <{offline_machine_name}>"
        assert expected in content, (
            f"Expected DestroyClassAd entry '{expected}' not found in persistent log"
        )


# ──────────────────────────────────────────────────────────────────────────────
# Absent-ad tests
# ──────────────────────────────────────────────────────────────────────────────

@standup
def condor_absent(test_dir):
    """Minimal HTCondor pool configured for absent-ad testing.

    CLASSAD_LIFETIME is set to 15 seconds so machine ads expire quickly.
    ABSENT_REQUIREMENTS = True means every expiring machine ad becomes absent
    rather than being deleted.  ABSENT_EXPIRE_ADS_AFTER is set to a full day
    so absent ads stay around for the duration of the test.
    """
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "DAEMON_LIST": "COLLECTOR MASTER",
            "USE_SHARED_PORT": False,
            "COLLECTOR_PERSISTENT_AD_LOG": str(test_dir / "absent.ads"),
            # Short ad lifetime so the synthetic ad expires quickly.
            "CLASSAD_LIFETIME": "15",
            # Mark every expiring machine ad as absent instead of deleting it.
            "ABSENT_REQUIREMENTS": "True",
            # Keep absent ads alive for the test.
            "ABSENT_EXPIRE_ADS_AFTER": "86400",
        },
    ) as condor:
        yield condor


@action
def absent_machine_name():
    return "absent-test-machine.example.com"


@action
def advertise_soon_expiring_ad(condor_absent, absent_machine_name):
    """Advertise a machine ad with ClassAdLifetime=5 seconds.

    The collector will expire this ad within seconds and, because
    ABSENT_REQUIREMENTS=True, will promote it to an absent ad.
    Returns True when the initial ad appears, False on timeout.
    """
    collector = condor_absent.get_local_collector()
    # ClassAdLifetime=5 forces the ad to expire after 5 s.
    collector.advertise(
        [_machine_ad(absent_machine_name, ClassAdLifetime=5)],
        "UPDATE_STARTD_AD",
    )
    # Confirm the fresh ad appears in the collector before it expires.
    for _ in range(15):
        ads = collector.query(htcondor.AdTypes.Startd, f'Name == "{absent_machine_name}"')
        if ads:
            return True
        time.sleep(1)
    return False


@action
def absent_ad(condor_absent, absent_machine_name, advertise_soon_expiring_ad):
    """Wait for the expiring ad to be promoted to Absent=true.

    The collector's house-keeper expires the ad after ClassAdLifetime seconds
    and calls OfflineCollectorPlugin::expire(), which evaluates
    ABSENT_REQUIREMENTS.  If true the ad is kept with Absent=true.
    Returns the absent ClassAd, or None on timeout.
    """
    assert advertise_soon_expiring_ad, "Prerequisite: initial ad must have appeared in collector"
    collector = condor_absent.get_local_collector()
    constraint = f'Name == "{absent_machine_name}" && Absent == true'
    for _ in range(60):
        ads = collector.query(htcondor.AdTypes.Startd, constraint)
        if ads:
            return ads[0]
        time.sleep(2)
    return None


class TestAbsentAds:
    """When ABSENT_REQUIREMENTS matches an expiring machine ad, the collector
    retains it with Absent=true instead of deleting it."""

    def test_absent_ad_appears(self, absent_ad):
        assert absent_ad is not None, (
            "Timed out waiting for machine ad to be promoted to absent"
        )

    def test_absent_attribute_is_true(self, absent_ad):
        assert absent_ad is not None
        assert absent_ad.get("Absent") is True

    def test_absent_ad_name_is_correct(self, absent_ad, absent_machine_name):
        assert absent_ad is not None
        assert absent_ad.get("Name") == absent_machine_name

    def test_absent_ad_not_in_normal_query(self, condor_absent, absent_machine_name, absent_ad):
        """Absent ads must be invisible to ordinary queries.

        When ABSENT_REQUIREMENTS is configured the collector automatically
        filters out absent ads from any query that does not explicitly
        reference the Absent attribute.  This mimics the behaviour of
        condor_status without the -absent flag.
        """
        assert absent_ad is not None, "Prerequisite: absent ad must exist"
        collector = condor_absent.get_local_collector()
        # Plain constraint – does not mention Absent.
        ads = collector.query(
            htcondor.AdTypes.Startd,
            f'Name == "{absent_machine_name}"',
        )
        absent_in_results = [a for a in ads if a.get("Absent") is True]
        assert len(absent_in_results) == 0, (
            "Absent ad appeared in a normal (no-absent-constraint) query"
        )

    def test_absent_ad_visible_with_absent_constraint(
        self, condor_absent, absent_machine_name, absent_ad
    ):
        """Absent ads are returned when the query explicitly requests Absent=true.

        Including Absent in the constraint disables the server-side absent
        filter, so the absent ad is returned.  This mimics condor_status -absent.
        """
        assert absent_ad is not None, "Prerequisite: absent ad must exist"
        collector = condor_absent.get_local_collector()
        ads = collector.query(
            htcondor.AdTypes.Startd,
            f'Name == "{absent_machine_name}" && Absent == true',
        )
        assert len(ads) == 1, (
            f"Expected 1 absent ad for {absent_machine_name}, got {len(ads)}"
        )

    def test_absent_ad_in_persistent_log(self, absent_ad, absent_machine_name, test_dir):
        """Absent ads are written to the persistent log so they survive restarts."""
        assert absent_ad is not None, "Prerequisite: absent ad must exist"
        log_path = test_dir / "absent.ads"
        assert log_path.exists(), "Persistent absent ad log was not created"
        content = log_path.read_text(errors="replace")
        assert absent_machine_name in content, (
            f'"{absent_machine_name}" not found in persistent log {log_path}'
        )
