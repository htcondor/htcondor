# Shared helpers for the test_ocu_* Ornithology tests.
#
# These tests need jobs owned by several different users in one personal
# HTCondor.  A personal condor runs as one OS user, so we can't switch uids;
# instead each "user" authenticates to the schedd with an IDTOKEN naming
# them (alice@ocu.test, bob@ocu.test, ...).  The schedd then records that
# identity as the job's Owner/User, which is all the OCU code looks at.
# The jobs themselves all run as the test's OS user.

import logging
import os
import re
import threading
import time

import htcondor2 as htcondor
import classad2 as classad

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

DOMAIN = "ocu.test"

# The two negotiator-preemption configurations every OCU scenario runs
# under.  The tests give the OCU owner a terrible priority factor so that,
# with preemption on, every other user outranks them.  An OCU claim must
# survive that.
PREEMPTION_CONFIGS = {
	"preempt_off": {
		"NEGOTIATOR_CONSIDER_PREEMPTION": "False",
	},
	# HTCondor's defaults: the negotiator considers claimed slots, but
	# PREEMPTION_REQUIREMENTS forbids priority preemption.
	"preempt_default": {
		"NEGOTIATOR_CONSIDER_PREEMPTION": "True",
		"PREEMPTION_REQUIREMENTS": "False",
	},
	"preempt_on": {
		# The classic priority-preemption policy: a user may preempt a
		# claim held by a user whose priority is sufficiently worse.
		"NEGOTIATOR_CONSIDER_PREEMPTION": "True",
		"PREEMPTION_REQUIREMENTS": "RemoteUserPrio > SubmitterUserPrio * 1.2",
	},
}


# The configuration for tests where the negotiator's preemption policy is
# not what is under test.  With User on the OCU claim, an idle OCU claim
# behaves the same under HTCondor's defaults as with preemption off, so the
# defaults are enough there; test_ocu_isolation runs every configuration.
DEFAULT_PREEMPTION = "preempt_default"


def base_config(preemption):
	config = {
		"UID_DOMAIN": DOMAIN,
		# The pseudo-users must be allowed to submit
		"ALLOW_WRITE": "$(ALLOW_WRITE) *@$(UID_DOMAIN)",
		"NUM_CPUS": "4",
		"MEMORY": "4096",
		"DISK": "4096000",
		"NEGOTIATOR_INTERVAL": "2",
		"NEGOTIATOR_CYCLE_DELAY": "2",
		"NEGOTIATOR_MIN_INTERVAL": "1",
		"SCHEDD_INTERVAL": "5",
		"UPDATE_INTERVAL": "5",
		"STARTD_UPDATE_INTERVAL": "5",
		"SCHEDD_DEBUG": "D_FULLDEBUG D_MATCH",
		"NEGOTIATOR_DEBUG": "D_FULLDEBUG D_MATCH",
		"STARTD_DEBUG": "D_FULLDEBUG",
		# Make priorities take effect immediately
		"PRIORITY_HALFLIFE": "1",
		"NEGOTIATOR_USE_SLOT_WEIGHTS": "False",
	}
	config.update(PREEMPTION_CONFIGS[preemption])
	return config


def user_name(user):
	return f"{user}@{DOMAIN}"


def bare(name):
	"""'alice@ocu.test' or 'alice' -> 'alice'.  (The startd publishes
	RemoteOwner without the domain; the negotiator uses it.)"""
	return str(name).split("@")[0] if name is not None else None


class User:
	"""A pseudo-user who submits jobs with their own IDTOKEN."""

	def __init__(self, condor, test_dir, name):
		self.condor = condor
		self.test_dir = test_dir
		self.name = name
		self.token_dir = test_dir / f"tokens_{name}"
		self.token_dir.mkdir(parents=True, exist_ok=True)
		rv = condor.run_command(["condor_token_create", "-identity", user_name(name)])
		assert rv.returncode == 0, f"token create for {name} failed: {rv.stderr}"
		token_file = self.token_dir / name
		token_file.write_text(rv.stdout.strip() + "\n")
		os.chmod(token_file, 0o600)
		self._nsub = 0

	def env(self):
		return [
			"env",
			"_condor_SEC_PASSWORD_DIRECTORY=/nothere",
			"_condor_SEC_CLIENT_AUTHENTICATION_METHODS=IDTOKEN",
			f"_condor_SEC_TOKEN_DIRECTORY={self.token_dir.as_posix()}",
		]

	def run(self, args, **kwargs):
		return self.condor.run_command(self.env() + list(args), **kwargs)

	def submit(self, description, count=1):
		"""Submit as this user.  Returns the cluster id."""
		self._nsub += 1
		sub = self.test_dir / f"{self.name}_{self._nsub}.sub"
		lines = [f"{k} = {v}" for k, v in description.items()]
		# Record whether the slot each job ran on was an OCU slot.
		lines.append("job_machine_attrs = OCU OCUName OCUId")
		lines.append(f"queue {count}")
		sub.write_text("\n".join(lines) + "\n")
		rv = self.run(["condor_submit", "-terse", sub.as_posix()])
		assert rv.returncode == 0, f"submit as {self.name} failed: {rv.stdout} {rv.stderr}"
		m = re.match(r"\s*(\d+)\.", rv.stdout)
		assert m, f"could not parse condor_submit -terse output: {rv.stdout!r}"
		cluster = int(m.group(1))
		logger.info(f"{self.name} submitted cluster {cluster} x{count}: {description}")
		return cluster


def sleep_job(path_to_sleep, test_dir, seconds, *, log_name, cpus=1, extra=None):
	desc = {
		"executable": path_to_sleep,
		"arguments": str(seconds),
		"should_transfer_files": "NO",
		"log": (test_dir / log_name).as_posix(),
		"leave_in_queue": "True",
		"request_cpus": str(cpus),
		"request_memory": "64",
		"request_disk": "1024",
	}
	if extra:
		desc.update(extra)
	return desc


def set_priority_factor(condor, user, factor):
	rv = condor.run_command(["condor_userprio", "-setfactor", user_name(user), str(factor)])
	assert rv.returncode == 0, f"setfactor failed: {rv.stdout} {rv.stderr}"


def create_ocu(condor, owner, cpus=1, memory=64, disk=1024, extra=None):
	schedd = condor.get_local_schedd()
	request = classad.ClassAd()
	request["Owner"] = owner
	request["RequestCpus"] = cpus
	request["RequestMemory"] = memory
	request["RequestDisk"] = disk
	if extra:
		for k, v in extra.items():
			request[k] = v
	result = schedd.create_ocu(request)
	logger.info(f"create_ocu({owner}) -> {result}")
	return result


def query_ocus(condor):
	"""Returns {ocu_id: ad}"""
	schedd = condor.get_local_schedd()
	return {int(ad["OCUId"]): ad for ad in schedd.query_ocu(classad.ClassAd())}


def wait_for(predicate, timeout=120, interval=1, what="condition"):
	deadline = time.time() + timeout
	last = None
	while time.time() < deadline:
		last = predicate()
		if last:
			return last
		time.sleep(interval)
	raise AssertionError(f"timed out after {timeout}s waiting for {what} (last={last!r})")


def wait_ocus_state(condor, ids, state, timeout=120):
	def check():
		ocus = query_ocus(condor)
		return all(i in ocus and ocus[i].get("OCUState") == state for i in ids)
	wait_for(check, timeout=timeout, what=f"OCUs {ids} in state {state}")


SLOT_PROJECTION = [
	"Name", "State", "Activity", "SlotType", "OCU", "OCUName",
	"RemoteOwner", "RemoteUser", "AccountingGroup", "JobId", "PublicClaimId",
]


_startd_addr = {}


def startd_slots(condor):
	"""Query the startd directly (not the collector) for current slot ads.

	Deliberately quiet and does not switch config, so the SlotMonitor
	thread can call it while the test thread does other things.
	"""
	key = id(condor)
	if key not in _startd_addr:
		with condor.use_config():
			loc = condor.get_local_collector().locate(htcondor.DaemonType.Startd)
		_startd_addr[key] = loc["MyAddress"]
	startd = htcondor.Collector(_startd_addr[key])
	return startd.query(ad_type=htcondor.AdType.Startd, projection=SLOT_PROJECTION)


def ocu_slots(condor):
	"""Startd slot ads currently advertising an OCU claim, by name"""
	return {ad["Name"]: ad for ad in startd_slots(condor) if ad.get("OCU") is True}


def wait_ocu_slots(condor, count, timeout=120):
	return wait_for(lambda: (lambda s: s if len(s) == count else None)(ocu_slots(condor)),
		timeout=timeout, what=f"{count} OCU slots in startd")


def job_ads(condor, cluster):
	schedd = condor.get_local_schedd()
	return schedd.query(constraint=f"ClusterId == {cluster}",
		projection=["ClusterId", "ProcId", "Owner", "JobStatus", "RemoteHost", "LastRemoteHost",
		            "MachineAttrOCU0", "MachineAttrOCUName0", "MachineAttrOCUId0",
		            "NumJobStarts", "NumShadowStarts", "OCUWanted", "OCUWilling", "HoldReason"])


def wait_cluster(condor, cluster, statuses, timeout=180, count=None):
	"""Wait until every job in the cluster has JobStatus in statuses"""
	def check():
		ads = job_ads(condor, cluster)
		if count is not None and len(ads) != count:
			return None
		if ads and all(ad["JobStatus"] in statuses for ad in ads):
			return ads
		return None
	return wait_for(check, timeout=timeout, what=f"cluster {cluster} in {statuses}")


def ran_on(ad):
	return ad.get("LastRemoteHost") or ad.get("RemoteHost")


class SlotMonitor:
	"""Background sampler of the startd's slot ads.

	Records every (slot, OCU-ness, remote owner) observation so tests can
	assert invariants over the whole scenario, not just at the end:
	  * a slot advertising OCU=true is only ever used by the OCU's owner
	    (or an approved borrower),
	  * an OCU slot never stops being an OCU slot (its claim is never
	    stolen) while the OCU exists.
	"""

	def __init__(self, condor, interval=0.5):
		self.condor = condor
		self.interval = interval
		self.samples = []   # (time, {name: ad})
		self._stop = threading.Event()
		self._thread = threading.Thread(target=self._run, daemon=True)

	def _run(self):
		while not self._stop.is_set():
			try:
				slots = {ad["Name"]: ad for ad in startd_slots(self.condor)}
				self.samples.append((time.time(), slots))
			except Exception as e:
				logger.debug(f"SlotMonitor sample failed: {e}")
			self._stop.wait(self.interval)

	def start(self):
		self._thread.start()
		return self

	def stop(self):
		self._stop.set()
		self._thread.join(timeout=30)

	def ocu_slot_users(self):
		"""{slot_name: set of RemoteOwner values seen while it was an OCU slot}"""
		seen = {}
		for _, slots in self.samples:
			for name, ad in slots.items():
				if ad.get("OCU") is True:
					owners = seen.setdefault(name, set())
					if ad.get("RemoteOwner"):
						owners.add(bare(ad["RemoteOwner"]))
		return seen

	def max_ocu_slots(self):
		"""Most slots seen advertising an OCU claim at one time"""
		return max((sum(1 for ad in slots.values() if ad.get("OCU") is True)
			for _, slots in self.samples), default=0)

	def lost_ocu_slots(self, names):
		"""Return [(t, name, ad)] where a slot in names was seen NOT advertising OCU"""
		lost = []
		for t, slots in self.samples:
			for name in names:
				ad = slots.get(name)
				if ad is None or ad.get("OCU") is not True:
					lost.append((t, name, dict(ad) if ad else None))
		return lost

	def non_ocu_owners_on(self, names):
		"""[(t, name, RemoteOwner)] for every sample a slot in names had a RemoteOwner"""
		out = []
		for t, slots in self.samples:
			for name in names:
				ad = slots.get(name)
				if ad and ad.get("RemoteOwner"):
					out.append((t, name, bare(ad["RemoteOwner"]), ad.get("OCU")))
		return out
