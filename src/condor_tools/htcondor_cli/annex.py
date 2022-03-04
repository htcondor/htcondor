import os
from pathlib import Path
import getpass
import argparse

import htcondor

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

# Most of the annex create code is stored in a separate file.
from htcondor_cli.annex_create import annex_create


def increment(hash, key, subkey, subsubkey, number):
    value = hash[key][subkey].get(subsubkey, 0)
    value += number
    hash[key][subkey][subsubkey] = value


class Create(Verb):
    """
    Create an HPC annex.
    """


    options = {
        "annex_name": {
            "args": ("--name",),
            "dest": "annex_name",
            "help": "Provide an name for this annex",
            "required": True,
        },
        "nodes": {
            "args": ("--nodes",),
            "help": "Number of HPC nodes to schedule. Defaults to %(default)s",
            "type": int,
            "default": 2,
        },
        "lifetime": {
            "args": ("--lifetime",),
            "help": "Annex lifetime (in seconds). Defaults to %(default)s",
            "type": int,
            "default": 3600,
        },
        "target": {
            "args": ("--machine",),
            "dest": "target",
            "help": "HPC machine name (e.g. stampede2)",
            "required": True,
        },
        "allocation": {
            "args": ("--project",),
            "dest": "allocation",
            "help": "The project name associated with HPC allocation (may be optional on some HPC machines)",
            "default": None,
        },
        "queue_name": {
            "args": ("--queue_name",),
            "help": f"""HPC queue name. Defaults to {htcondor.param.get("ANNEX_HPC_QUEUE_NAME", 'a machine-specific default (e.g. "development" for "stampede2")')}""",
            "default": htcondor.param.get("ANNEX_HPC_QUEUE_NAME"),
        },
        "owners": {
            "args": ("--owners",),
            #"help": "List (comma-separated) of annex owners. Defaults to current user (%(default)s)",
            "help": argparse.SUPPRESS,  # hidden option
            "default": getpass.getuser(),
        },
        "collector": {
            "args": ("--pool",),
            "dest": "collector",
            "help": "Collector that the annex reports to. Defaults to %(default)s",
            "default": htcondor.param.get("ANNEX_COLLECTOR", "htcondor-cm-hpcannex.osgdev.chtc.io"),
        },
        "ssh_target": {
            "args": ("--ssh_target",),
            #"help": "SSH target to use to talk with the HPC scheduler. Defaults to %(default)s",
            "help": argparse.SUPPRESS,  # hidden option
            "default": f"{getpass.getuser()}@{htcondor.param.get('ANNEX_SSH_HOST', 'login.xsede.org')}",
        },
        "token_file": {
            "args": ("--token_file",),
            "help": "Token file. Defaults to %(default)s",
            "type": Path,
            "default": Path(htcondor.param.get("ANNEX_TOKEN_FILE", f"~/.condor/tokens.d/{getpass.getuser()}@annex.osgdev.chtc.io")),
        },
        "password_file": {
            "args": ("--password_file",),
            #"help": "Password file. Defaults to %(default)s",
            "help": argparse.SUPPRESS,  # hidden option
            "type": Path,
            "default": Path(htcondor.param.get("ANNEX_PASSWORD_FILE", "~/.condor/annex_password_file")),
        },
        "control_path": {
            "args": ("--tmp_dir",),
            "dest": "control_path",
            "help": "Location to store temporary annex control files, probably should not be changed. Defaults to %(default)s",
            "type": Path,
            "default": Path(htcondor.param.get("ANNEX_TMP_DIR", "~/.hpc-annex")),
        }
    }

    def __init__(self, logger, **options):
        annex_create(logger, **options)


class Status(Verb):
    """
    Summarize the usage of an annex.
    """

    options = {
        "annex_name": {
            "args": ("--annex-name",),
            "help": "annex name",
        },
    }


    # Unlike `shutdown`, status should probably not require a name.
    def __init__(self, logger, **options):
        the_annex_name = options["annex_name"];

        schedd = htcondor.Schedd()
        if the_annex_name is None:
            annex_jobs = schedd.query(f'hpc_annex_name =!= undefined')
        else:
            annex_jobs = schedd.query(f'hpc_annex_name == "{the_annex_name}"')

        # Each annex request is represented by its own job, but we want
        # to present aggregate information for each annex name.

        status = { job["hpc_annex_name"]: {} for job in annex_jobs }
        for job in annex_jobs:
            annex_name = job["hpc_annex_name"]
            request_id = job["hpc_annex_request_id"]

            status[annex_name][request_id] = "requested"
            if job.get("hpc_annex_PID") is not None:
                status[annex_name][request_id] = "submitting (1)"
            if job.get("hpc_annex_PILOT_DIR") is not None:
                status[annex_name][request_id] = "submitting (2)"
            if job.get("hpc_annex_JOB_ID") is not None:
                status[annex_name][request_id] = "submitted"
            if job["JobStatus"] == htcondor.JobStatus.IDLE:
                status[annex_name][request_id] = "granted"

        annex_collector = htcondor.param.get("ANNEX_COLLECTOR", "htcondor-cm-hpcannex.osgdev.chtc.io")
        collector = htcondor.Collector(annex_collector)
        if the_annex_name is None:
            annex_slots = collector.query(constraint=f'AnnexName =!= undefined', ad_type=htcondor.AdTypes.Startd)
        else:
            annex_slots = collector.query(constraint=f'AnnexName == "{the_annex_name}"', ad_type=htcondor.AdTypes.Startd)

        for slot in annex_slots:
            annex_name = slot["AnnexName"]
            request_id = slot["hpc_annex_request_id"]
            status[annex_name][request_id] = {}

        for slot in annex_slots:
            annex_name = slot["AnnexName"]
            request_id = slot["hpc_annex_request_id"]

            # Ignore dynamic slot, since we just care about aggregates.
            # We could write a similar check for static slots, but we
            # don't ever start any annexes with static slots.  (Bad
            # admin, no cookie!)
            if slot.get("PartitionableSlot", False):
                increment(status, annex_name, request_id, "TotalCPUs", slot["TotalSlotCPUs"])
                increment(status, annex_name, request_id, "BusyCPUs", len(slot["ChildCPUs"]))

        # This could be substantially optimized with a projection.
        running_jobs = {}
        target_jobs = schedd.query(f'TargetAnnexName =!= undefined', opts=htcondor.QueryOpts.DefaultMyJobsOnly)
        for job in target_jobs:
            if job['JobStatus'] == htcondor.JobStatus.RUNNING:
                count = running_jobs.get(job['TargetAnnexName'], 0)
                count += 1
                running_jobs[job['TargetAnnexName']] = count

        #
        # Do the actual reporting (after calculating some aggregates).
        #
        for annex_name, annex_status in status.items():
            requests = len(annex_status)

            busy_CPUs = 0
            total_CPUs = 0
            requested_and_left = 0
            requested_but_not_joined = 0
            for request_ID, values in annex_status.items():
                if isinstance(values, str):
                    if values == "granted":
                        requested_and_left += 1
                    else:
                        requested_but_not_joined += 1
                else:
                    total_CPUs += values["TotalCPUs"]
                    busy_CPUs += values["BusyCPUs"]
            requested_and_active = requests - requested_but_not_joined - requested_and_left

            #
            # This what the recipe actually has.
            #
            # Other possibilities include:
            # * how long the annex has left, based on the requested duration
            # * how many jobs target the current annex (e.g., running x/y jobs)
            # * how long ago the requests were made.
            # * (approximately) how long ago the annex became active (based
            #   on the DaemonStartTime of the slot ads).
            #
            if total_CPUs != 0:
                running_job_count = running_jobs.get(annex_name, 0)
                print(f"Annex '{annex_name}' is active and running {running_job_count} jobs on {busy_CPUs} of {total_CPUs} CPUs.", end='')
            else:
                print(f"Annex '{annex_name}' is not active.", end='')

            if requests != requested_and_active:
                print(f"  {requested_but_not_joined}/{requested_and_active}/{requested_and_left} requests are pending/active/retired.", end='')

            print()

class Shutdown(Verb):
    """
    Shut an HPC annex down.
    """


    options = {
        "annex_name": {
            "args": ("annex_name",),
            "help": "annex name",
        },
    }


    #
    # This (presently) only shuts down currently-operating annexes, and
    # only annexes for which every startd has successfully reported to the
    # collector.  We should add an option to scan the queue for the SLURM
    # job IDs of every annex request with a matching name and scancel the
    # it; this shouldn't be the default because it will require the user to
    # log in again.
    #
    def __init__(self, logger, annex_name, **options):
        annex_collector = htcondor.param.get("ANNEX_COLLECTOR", "htcondor-cm-hpcannex.osgdev.chtc.io")
        collector = htcondor.Collector(annex_collector)
        location_ads = collector.query(
            ad_type=htcondor.AdTypes.Master,
            constraint=f'AnnexName =?= "{annex_name}"',
        )

        if len(location_ads) == 0:
            print(f"No resources found in annex '{annex_name}'.")
            return

        password_file = htcondor.param.get("ANNEX_PASSWORD_FILE", "~/.condor/annex_password_file")
        password_file = os.path.expanduser(password_file)

        # There's a bug here where I should be able to write
        #   with htcondor.SecMan() as security_context:
        # instead, but then security_context is a `lockedContext` object
        # which doesn't have a `setConfig` attribute.
        security_context = htcondor.SecMan()
        with security_context:
            security_context.setConfig("SEC_DEFAULT_AUTHENTICATION_METHODS", "FS IDTOKENS PASSWORD")
            security_context.setConfig("SEC_PASSWORD_FILE", password_file)

            print(f"Shutting down annex '{annex_name}'...")
            for location_ad in location_ads:
                htcondor.send_command(
                    location_ad,
                    htcondor.DaemonCommands.OffFast,
                    "MASTER",
                )

        print(f"... each resource in '{annex_name}' has been commanded to shut down.")
        print("It may take some time for each resource to finish shutting down.");
        print("Annex requests that are still in progress have not been affected.")


class Annex(Noun):
    """
    Operations on HPC Annexes.
    """


    class shutdown(Shutdown):
        pass


    class status(Status):
        pass


    class create(Create):
        pass


    @classmethod
    def verbs(cls):
        return [cls.create, cls.status, cls.shutdown]

