#!/usr/bin/env pytest

# An OCU claim loses the slot it is holding to an ordinary job.
#
# An OCU is a claim the schedd holds on an execute slot on a user's behalf, so
# the capacity is there when that user has work for it.  The claim has to
# outlive the moment it is idle -- holding capacity through an idle period is
# the entire point of reserving it in advance.
#
# It does not.  This test fills a two-CPU pool (one CPU runs a seed job, one is
# held by an OCU claim), notes by name the slot the claim is holding, and then
# submits an ordinary job -- no OCUWanted, no OCUWilling, no entitlement to the
# reserved capacity at all.  Within a negotiation cycle that job is running on
# the very slot the claim was holding.  The failure message prints the slot
# table before and after, with the claim's slot marked, so the handoff is
# visible rather than inferred.
#
# The cause is that an OCU claim's slot advertises no RemoteUser:
#
#   * Resource::request_claim() takes the claim's owner from ATTR_USER in the
#     request ad (condor_startd.V6/command.cpp).  The ad the schedd sends for
#     an OCU carries Owner but never User, so c_owner and c_user stay empty.
#   * Claim::publish() only publishes RemoteUser, RemoteOwner and RemoteProject
#     when they are non-empty (condor_startd.V6/claim.cpp), so the slot
#     advertises none of them.
#   * The negotiator's whole preemption path is inside
#     `if (!remoteUser.empty())` (condor_negotiator.V6/matchmaker.cpp).  A
#     claimed slot naming nobody is therefore scored as unclaimed capacity and
#     handed to whoever asks for it next.
#   * The startd sees a preempting claim against a slot whose activity is idle
#     and goes straight to preempting_state with no retirement
#     (condor_startd.V6/ResState.cpp, transition 10).
#
# The test is parametrized over four cases:
#
#   cli                `htcondor ocu create` with a request file, which is the
#                      documented path and what a user gets today.
#   api                Schedd.create_ocu() with Owner set, as test_ocu_basic.py
#                      does.  Identical to the CLI: neither sets User.
#   api_plus_User      Schedd.create_ocu() with User set as well as Owner.
#   api_no_preemption  As `api`, but with NEGOTIATOR_CONSIDER_PREEMPTION off,
#                      which asks whether disabling preemption protects the
#                      claim without the User attribute.  Measured: it does.
#                      The claim keeps its slot and the ordinary job never runs
#                      at all, even though the slot still advertises no
#                      RemoteUser -- so that case still fails
#                      test_claim_advertises_its_owner.
#
# So there are two independent ways to keep the claim: set User on the request
# ad, or turn off NEGOTIATOR_CONSIDER_PREEMPTION pool-wide.  Only the first
# fixes the claim's advertisement; the second merely stops anyone acting on it,
# at the cost of preemption everywhere.  A pool that sets it cannot observe
# this bug, which is worth knowing when reading a test that sets it.

import logging
import re
import time
import getpass

import htcondor2 as htcondor
import classad2 as classad

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# How long to let the negotiator try to give the claim's slot away.  With
# NEGOTIATOR_INTERVAL at 2 seconds this is about ten cycles; when the claim is
# taken it is taken in the first one or two.
STEAL_WINDOW = 20

# The claim is the one slot that is claimed with nothing running on it.  A slot
# running a job is Claimed/Busy, and the partitionable slot is Unclaimed.
CLAIM_CONSTRAINT = 'State == "Claimed" && Activity == "Idle"'

SLOT_PROJECTION = [
    "Name",
    "State",
    "Activity",
    "Cpus",
    "PartitionableSlot",
    "RemoteUser",
    "RemoteOwner",
    "RemoteProject",
]


def slots(condor, constraint="true"):
    """Ask the startd itself rather than the collector: the collector's copy
    lags by UPDATE_INTERVAL, which is long enough to report a claim that is
    already gone, or to miss one that is already there."""
    return condor.direct_status(
        htcondor.DaemonTypes.Startd,
        htcondor.AdTypes.Startd,
        constraint=constraint,
        projection=SLOT_PROJECTION,
    )


def slot_table(condor, mark=None, note=None):
    """Render every slot, marking the one the OCU claim is holding."""
    lines = []
    for ad in sorted(slots(condor), key=lambda a: a["Name"]):
        name = ad["Name"]
        suffix = ""
        if mark is not None and name == mark:
            suffix = "   <== {}".format(note if note else "the OCU claim's slot")
        lines.append(
            "    {:<34} {:>9}/{:<6} cpus={} remote_user={}{}".format(
                name,
                ad.get("State", "?"),
                ad.get("Activity", "?"),
                ad.get("Cpus", "?"),
                ad["RemoteUser"] if "RemoteUser" in ad else "<none>",
                suffix,
            )
        )
    return "\n".join(lines)


def poll_until(predicate, timeout, interval=2):
    deadline = time.time() + timeout
    while time.time() < deadline:
        result = predicate()
        if result:
            return result
        time.sleep(interval)
    return None


@config(
    params={
        "cli": {"id": "cli", "method": "cli", "config": {}},
        "api": {"id": "api", "method": "api", "config": {}},
        "api_plus_User": {
            "id": "api_plus_User",
            "method": "api_plus_User",
            "config": {},
        },
        # Does turning off negotiator preemption protect the claim on its own,
        # without the User attribute?  Worth knowing, because a slot that names
        # nobody is not scored as a preemption in the first place.
        "api_no_preemption": {
            "id": "api_no_preemption",
            "method": "api",
            "config": {"NEGOTIATOR_CONSIDER_PREEMPTION": "False"},
        },
    }
)
def case(request):
    return request.param


@action
def ocu_creation_method(case):
    return case["method"]


@action
def case_dir(test_dir, case):
    """A directory per parametrization.  Job event logs must not be shared
    between cases: each case gets a fresh pool whose cluster ids restart at 1,
    so a reused log would let one case's handle read the previous case's
    events."""
    path = test_dir / "case-{}".format(case["id"])
    path.mkdir(parents=True, exist_ok=True)
    return path


@standup
def condor(test_dir, case):
    # Two CPUs, and both are spoken for once the claim exists: one runs the
    # seed job, one is held by the claim.  A pool with room to spare cannot
    # answer this question -- an ordinary job would run on the spare CPU
    # whether or not the claim was safe, and the test would pass vacuously.
    with Condor(
        test_dir / "condor-{}".format(case["id"]),
        config={
            "NUM_CPUS": "2",
            "MEMORY": "4096",
            "DISK": "4096000",
            "NEGOTIATOR_CYCLE_DELAY": "2",
            "NEGOTIATOR_INTERVAL": "2",
            "OCU_SUPER_USERS": getpass.getuser(),
            **case["config"],
        },
    ) as condor:
        yield condor


@action
def seed_job(condor, case_dir, path_to_sleep):
    """A long-running ordinary job.  It does two things: it occupies one of the
    two CPUs, and it gives the schedd a SubmitterData record for this user,
    without which the negotiator will not negotiate for the OCU."""
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "600",
            "log": (case_dir / "seed_job.log").as_posix(),
            "should_transfer_files": False,
            "Request_Cpus": 1,
            "Request_Memory": 1,
            "Request_Disk": 1,
        },
        count=1,
    )

    assert handle.wait(
        condition=ClusterState.all_running,
        fail_condition=ClusterState.any_held,
        verbose=True,
        timeout=120,
    ), "the seed job never started, so the pool is not working"

    yield handle
    # Let the removal finish before the pool is shut down.  Otherwise the
    # running job's starter can lose its shadow mid-vacate and hold up the
    # startd's shutdown for 30 seconds waiting to reconnect.
    handle.remove()
    handle.wait(condition=ClusterState.all_terminal, timeout=30)


def create_ocu_via_cli(condor, case_dir):
    """The documented path: a request file of `Owner =` and `Request_*` lines
    fed to `htcondor ocu create` (condor_tools/htcondor_cli/ocu.py)."""
    request_file = case_dir / "ocu_request.ad"
    request_file.write_text(
        'Owner = "{}"\n'
        "RequestCpus = 1\n"
        "RequestMemory = 1\n"
        "RequestDisk = 1\n".format(getpass.getuser())
    )

    result = condor.run_command(
        ["htcondor", "ocu", "create", request_file.as_posix()], echo=True
    )
    assert result.returncode == 0, "`htcondor ocu create` failed: {}".format(
        result.stderr
    )

    # "OCU created with Id 1 and name ocu_1."
    match = re.search(r"Id (\d+)", result.stdout)
    assert match, "could not find the OCU id in: {}".format(result.stdout)
    return int(match.group(1))


def create_ocu_via_api(condor, with_user):
    """What test_ocu_basic.py does, optionally with the candidate fix."""
    schedd = condor.get_local_schedd()

    request = classad.ClassAd()
    request["Owner"] = getpass.getuser()
    if with_user:
        # The candidate fix.  The execute point reads the claim's owner out of
        # User, not Owner.
        request["User"] = getpass.getuser()
    request["RequestCpus"] = 1
    request["RequestMemory"] = 1
    request["RequestDisk"] = 1

    result = schedd.create_ocu(request)
    logger.debug("OCU create result: %s", result)
    assert result["Result"] == 0, "failed to create OCU: {}".format(
        result["ErrorString"] if "ErrorString" in result else "unknown"
    )
    return int(result["OCUId"])


@action
def the_claim(condor, case_dir, seed_job, ocu_creation_method):
    """Create an OCU and return the slot its claim is holding."""
    if ocu_creation_method == "cli":
        ocu_id = create_ocu_via_cli(condor, case_dir)
    else:
        ocu_id = create_ocu_via_api(
            condor, with_user=(ocu_creation_method == "api_plus_User")
        )

    claimed = poll_until(lambda: slots(condor, CLAIM_CONSTRAINT), timeout=120)
    assert claimed, "the OCU never claimed a slot, so there is nothing to " \
        "test:\n" + slot_table(condor)
    assert len(claimed) == 1, "expected exactly one idle claim, got:\n" + slot_table(
        condor
    )
    claim_ad = claimed[0]

    # Establish that the pool really is full, so that an ordinary job running
    # later can only mean it took this claim's slot.  A failure here is a
    # broken test setup, which is a different problem from the one under test.
    for ad in slots(condor, "PartitionableSlot =?= true"):
        assert ad["Cpus"] == 0, (
            "the pool still has unclaimed CPUs, so this test cannot tell a "
            "stolen claim from spare capacity:\n" + slot_table(condor)
        )

    claim = {
        "name": claim_ad["Name"],
        "ad": claim_ad,
        "before": slot_table(condor, mark=claim_ad["Name"]),
    }
    logger.debug("the OCU claim is holding %s", claim["name"])

    yield claim

    try:
        condor.get_local_schedd().remove_ocu(ocu_id)
    except Exception:
        pass


@action
def ordinary_job(condor, case_dir, path_to_sleep, the_claim):
    """Submit a job with no claim on the reserved capacity and see where, if
    anywhere, it ends up running."""
    handle = condor.submit(
        description={
            "executable": path_to_sleep,
            "arguments": "600",
            "log": (case_dir / "ordinary_job.log").as_posix(),
            "leave_in_queue": True,
            "should_transfer_files": False,
            "Request_Cpus": 1,
            "Request_Memory": 1,
            "Request_Disk": 1,
        },
        count=1,
    )

    def running_on():
        for ad in handle.query(projection=["ClusterId", "ProcId", "RemoteHost"]):
            if "RemoteHost" in ad:
                return ad["RemoteHost"]
        return None

    host = poll_until(running_on, timeout=STEAL_WINDOW)
    logger.debug("after %ss the ordinary job is running on %s", STEAL_WINDOW, host)

    yield {
        "cluster": handle.clusterid,
        "host": host,
        "after": slot_table(
            condor, mark=the_claim["name"], note="the slot the OCU claim was holding"
        ),
    }
    handle.remove()


class TestOCUClaimHoldsItsSlot:
    def test_claim_advertises_its_owner(self, the_claim):
        """The slot an OCU claim holds must say who it is held for.  Nothing
        downstream -- negotiator preemption, startd policy, the schedd's own
        matching -- can protect a claim that names nobody."""
        assert "RemoteUser" in the_claim["ad"] and the_claim["ad"]["RemoteUser"] != "", (
            "The OCU claim on {} advertises no RemoteUser, so the negotiator "
            "scores it as unclaimed capacity:\n{}".format(
                the_claim["name"], the_claim["before"]
            )
        )

    def test_ordinary_job_does_not_take_the_claims_slot(
        self, the_claim, ordinary_job
    ):
        """The headline: an ordinary job must not be handed the slot an OCU
        claim is holding."""
        assert ordinary_job["host"] != the_claim["name"], (
            "The OCU claim was holding {claim}:\n{before}\n\n"
            "An ordinary job (cluster {cluster}) with no OCU attributes and no "
            "entitlement to the reserved capacity then took that exact slot, "
            "within {window}s:\n{after}".format(
                claim=the_claim["name"],
                before=the_claim["before"],
                cluster=ordinary_job["cluster"],
                window=STEAL_WINDOW,
                after=ordinary_job["after"],
            )
        )

    def test_claim_still_holds_a_slot(self, condor, the_claim, ordinary_job):
        """Stated the other way round, and without depending on the startd
        reusing the slot name: after an ordinary job has come along, the pool
        should still contain the claim that was there before it."""
        assert slots(condor, CLAIM_CONSTRAINT), (
            "The OCU claim that was holding {claim} is gone; no slot in the "
            "pool is claimed and idle any more:\n{before}\n\nafter:\n{after}".format(
                claim=the_claim["name"],
                before=the_claim["before"],
                after=ordinary_job["after"],
            )
        )
