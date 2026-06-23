#!/usr/bin/env pytest

# Tests for the startd "controller" relationship: an access point (AP) that an
# administrator has granted the privilege to evict any claim on an execution
# point (EP).  The relationship is established in two steps, both exercised
# here through the Python bindings (which the "htcondor ep controller" CLI is a
# thin wrapper over):
#
#   1. htcondor2.Schedd.request_controller_token() asks the AP to mint an
#      AP-signed capability token.
#   2. htcondor2.Startd.set_controller() delivers that token to the EP, which
#      (if it has no controller yet) records the AP and begins advertising the
#      Controller slot attribute.
#
# We verify: token issuance, that setting a controller makes the EP advertise
# it, that a second set is refused (an EP has at most one controller), that the
# relationship survives an EP restart, and that clearing removes it.
#
# The security-meaningful part -- the controller AP evicting a claim owned by a
# *different* AP -- requires two APs with distinct authenticated identities and
# is not exercised here; it is covered by the C++ authorization (PunchHole)
# logic and by manual multi-AP testing.  See CONTROLLER_DESIGN.md.

import time
import logging

import pytest
import htcondor2 as htcondor

from ornithology import (
    standup,
    Condor,
)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@standup
def pool(test_dir):
    # set_controller persists the controller name via the runtime persistent
    # config, which requires these to be enabled and a writable directory.
    persistent_dir = test_dir / "persistent"
    persistent_dir.mkdir(parents=True, exist_ok=True)

    # request_controller_token mints an AP-signed IDTOKEN, which needs a pool
    # token signing key.  Pointing SEC_TOKEN_POOL_SIGNING_KEY_FILE at a "POOL"
    # file inside SEC_PASSWORD_DIRECTORY makes the daemons auto-generate it on
    # startup.
    passwords_dir = test_dir / "passwords"
    passwords_dir.mkdir(parents=True, exist_ok=True)

    config = {
        "DAEMON_LIST": "MASTER COLLECTOR NEGOTIATOR SCHEDD STARTD",
        "ENABLE_PERSISTENT_CONFIG": "true",
        "PERSISTENT_CONFIG_DIR": str(persistent_dir),
        "SEC_PASSWORD_DIRECTORY": str(passwords_dir),
        "SEC_TOKEN_POOL_SIGNING_KEY_FILE": str(passwords_dir / "POOL"),
    }
    with Condor(local_dir=test_dir / "pool", config=config) as condor:
        yield condor


def _controller_attr(condor):
    """Return the Controller attribute the startd currently advertises, or None.

    Queries the startd directly to avoid waiting out collector update latency.
    """
    with condor.use_config():
        collector = htcondor.Collector()
        startd_ad = collector.locate(htcondor.DaemonType.Startd)
        startd = htcondor.Collector(startd_ad["MyAddress"])
        ads = startd.query(
            ad_type=htcondor.AdTypes.Startd,
            projection=["Name", "Controller"],
        )
    for ad in ads:
        if "Controller" in ad:
            return ad["Controller"]
    return None


def _wait_for_controller(condor, expected, timeout=60):
    """Poll until the advertised Controller equals expected (None to wait for absence)."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            current = _controller_attr(condor)
        except Exception:
            current = "__unreachable__"
        if current == expected:
            return True
        time.sleep(1)
    return False


class TestStartdController:
    def test_controller_lifecycle(self, pool, test_dir):
        condor = pool

        with condor.use_config():
            collector = htcondor.Collector()
            schedd_ad = collector.locate(htcondor.DaemonType.Schedd)
            startd_ad = collector.locate(htcondor.DaemonType.Startd)
            ap_name = schedd_ad["Name"]

            schedd = htcondor.Schedd(schedd_ad)
            startd = htcondor.Startd(startd_ad)

            # No controller to start with.
            assert _controller_attr(condor) is None

            # Step 1: the AP mints a controller token for us.
            grant = schedd.request_controller_token()
            assert grant["Token"]
            assert grant["ControllerName"] == ap_name

            # Step 2: deliver the token to the EP.
            startd.set_controller(
                controller_name=grant["ControllerName"],
                token=grant["Token"],
                controller_pool=grant.get("ControllerPool"),
                controller_identity=grant.get("ControllerIdentity"),
            )

            # The EP now advertises its controller.
            assert _wait_for_controller(condor, ap_name)

            # The AP does not yet know it controls anything -- that bookkeeping
            # is a separate step the CLI performs after the EP accepts control.
            assert schedd.get_controlled_eps() == []

            # Drive that step (register_controlled_ep) directly, as the
            # "htcondor ep controller" CLI does, and confirm the AP now lists
            # the EP via get_controlled_eps (what "htcondor ap controlled" uses).
            ep_name = startd_ad["Machine"]
            schedd.register_controlled_ep(ep_name)
            assert [ad["Name"] for ad in schedd.get_controlled_eps()] == [ep_name]

            # An EP has at most one controller: a second set must be refused.
            with pytest.raises(htcondor.HTCondorException):
                startd.set_controller(
                    controller_name=grant["ControllerName"],
                    token=grant["Token"],
                )
            # ...and the original controller is unchanged.
            assert _controller_attr(condor) == ap_name

        # The relationship must survive an EP restart (persisted to disk).
        restart = condor.run_command(["condor_restart", "-fast", "-startd"])
        assert restart.returncode == 0
        assert _wait_for_controller(condor, ap_name, timeout=120)

        # The AP was not restarted, so it still lists the EP it controls.
        assert [ad["Name"] for ad in schedd.get_controlled_eps()] == [ep_name]

        with condor.use_config():
            collector = htcondor.Collector()
            startd_ad = collector.locate(htcondor.DaemonType.Startd)
            startd = htcondor.Startd(startd_ad)

            # Clearing removes the controller and stops the advertisement.
            startd.clear_controller()
            assert _wait_for_controller(condor, None)

            # The CLI likewise informs the AP on --clear; drive that and confirm
            # the AP no longer lists the EP.
            schedd.register_controlled_ep(ep_name, remove=True)
            assert schedd.get_controlled_eps() == []

            # Clearing again is a harmless no-op.
            startd.clear_controller()
