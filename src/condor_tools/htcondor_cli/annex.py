import os
import time
from pathlib import Path
import atexit
import getpass
import argparse
from collections import defaultdict

import htcondor2 as htcondor

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

# Most of the annex add/create code is stored in a separate file.
from htcondor_cli.annex_create import annex_add, annex_create, create_annex_token
from htcondor_cli.annex_validate import SYSTEM_TABLE

class Create(Verb):
    """
    Create an HPC annex.
    """

    options = {
        "annex_name": {
            "args": ("annex_name",),
            "metavar": "annex-name",
            "help": "Provide a name for your annex",
        },
        "queue_at_system": {
            "args": ("queue_at_system",),
            "metavar": "queue@system",
            "help": "Specify the queue and the HPC system",
        },
        "nodes": {
            "args": ("--nodes",),
            "help": "Number of HPC nodes to schedule. Defaults to %(default)s",
            "type": int,
            "default": 1,
        },
        "lifetime": {
            "args": ("--lifetime",),
            "help": "Annex lifetime (in seconds). Defaults to %(default)s",
            "type": int,
            "default": 3600,
        },
        "allocation": {
            "args": ("--project",),
            "dest": "allocation",
            "help": "The project name associated with HPC allocation (may be optional on some HPC systems)",
            "default": None,
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
        "token_file": {
            "args": ("--token_file",),
            "help": "Token file.  Normally obtained automatically.",
            "type": Path,
            "default": None,
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
        },
        "cpus": {
            "args": ("--cpus",),
            "help": "Number of CPUs to request (shared queues only).  Unset by default.",
            "type": int,
            "default": None,
        },
        "mem_mb": {
            "args": ("--mem_mb",),
            # TODO: Parse units instead of requiring this to be a number of MBs
            "help": "Memory (in MB) to request (shared queues only).  Unset by default.",
            "type": int,
            "default": None,
        },
        "login_name": {
            "args": ("--login-name","--login",),
            "help": "The (SSH) login name to use for this capacity request.  Uses SSH's default.",
            "default": None,
        },
        "login_host": {
            "args": ("--login-host","--host",),
            "help": "The (SSH) login host to use for this capacity request.  The default is system-specific.",
            "default": None,
        },
        "startd_noclaim_shutdown": {
            "args": ("--idle-time", "--startd-noclaim-shutdown"),
            "metavar": "SECONDS",
            "dest": "startd_noclaim_shutdown",
            "help": "The number of seconds to remain idle before shutting down.  Default and suggested minimum is 300 seconds.",
            "default": 300,
            "type": int,
        },
        "gpus": {
            "args": ("--gpus",),
            "help": "Number of GPUs to request (GPU queues only).  Unset by default.",
            "type": str,
            "default": None,
        },
        "gpu_type": {
            "args": ("--gpu-type",),
            "help": "Type of GPU to request (GPU queues only).  Unset by default.",
            "default": None,
        },
        "test": {
            "args": ("--test",),
            "help": argparse.SUPPRESS,
            "type": int,
            "default": None,
        },
    }

    def __init__(self, logger, **options):
        if not htcondor.param.get("HPC_ANNEX_ENABLED", False):
            raise ValueError("HPC Annex functionality has not been enabled by your HTCondor administrator.")
        annex_create(logger, **options)


class Add(Create):
    def __init__(self, logger, **options):
        annex_add(logger, **options)


class Systems(Verb):
    """
    Display the known systems and queues.
    """

    def __init__(self, logger, **options):
        for system in SYSTEM_TABLE.keys():
            print(f"    {system}")
            for queue in SYSTEM_TABLE[system].queues:
                print(f"        {queue}")
            print(f"")


class Status(Verb):
    """
    Summarize the usage of an annex.
    """

    options = {
        "annex_name": {
            "args": ("annex_name",),
            "help": "annex name",
            "nargs": "?",
        },
    }


    # Unlike `shutdown`, status should probably not require a name.
    def __init__(self, logger, **options):
        if not htcondor.param.get("HPC_ANNEX_ENABLED", False):
            raise ValueError("HPC Annex functionality has not been enabled by your HTCondor administrator.")

        the_annex_name = options["annex_name"]

        schedd = htcondor.Schedd()
        query = f'hpc_annex_name =!= undefined'
        if the_annex_name is not None:
            query = f'hpc_annex_name == "{the_annex_name}"'
        annex_jobs = schedd.query(query, opts=htcondor.QueryOpts.DefaultMyJobsOnly)

        ## This all very ugly and can't possibly be the best way to do this.

        # Each annex request is represented by its own job, but we want
        # to present aggregate information for each annex name.
        status = { job["hpc_annex_name"]: {} for job in annex_jobs }

        lifetimes = {}
        requested_nodes = defaultdict(int)
        for job in annex_jobs:
            annex_name = job["hpc_annex_name"]
            request_id = job.eval("hpc_annex_request_id")

            count = job.get('hpc_annex_nodes', "0")
            requested_nodes[annex_name] += int(count)

            status[annex_name][request_id] = "requested"
            if job.get("hpc_annex_PID") is not None:
                status[annex_name][request_id] = "submitting (1)"
            if job.get("hpc_annex_PILOT_DIR") is not None:
                status[annex_name][request_id] = "submitting (2)"
            if job.get("hpc_annex_JOB_ID") is not None:
                status[annex_name][request_id] = "submitted"
            if job["JobStatus"] == htcondor.JobStatus.IDLE:
                status[annex_name][request_id] = "granted"

            lifetimes[request_id] = job.get("hpc_annex_lifetime")

        annex_collector = htcondor.param.get("ANNEX_COLLECTOR", "htcondor-cm-hpcannex.osgdev.chtc.io")
        collector = htcondor.Collector(annex_collector)

        constraint = 'AnnexName =!= undefined'
        if the_annex_name is not None:
            constraint = f'AnnexName == "{the_annex_name}"'
        annex_token_domain = htcondor.param.get("ANNEX_TOKEN_DOMAIN", "annex.osgdev.chtc.io")
        constraint = f'{constraint} && AuthenticatedIdentity == "{getpass.getuser()}@{annex_token_domain}"'

        token_file = create_annex_token(logger, "status")
        atexit.register(lambda: os.unlink(token_file))
        annex_slots = collector.query(constraint=constraint, ad_type=htcondor.AdTypes.Startd)

        annex_attrs = {}
        for slot in annex_slots:
            annex_name = slot["AnnexName"]
            request_id = slot["hpc_annex_request_id"]
            if status.get(annex_name) is None:
                status[annex_name] = {}
            status[annex_name][request_id] = defaultdict(int)
            annex_attrs[annex_name] = {}

        for slot in annex_slots:
            annex_name = slot["AnnexName"]
            request_id = slot["hpc_annex_request_id"]

            # Ignore dynamic slot, since we just care about aggregates.
            # We could write a similar check for static slots, but we
            # don't ever start any annexes with static slots.  (Bad
            # admin, no cookie!)
            if slot.get("PartitionableSlot", False):
                status[annex_name][request_id]["TotalCPUs"] += slot["TotalSlotCPUs"]
                status[annex_name][request_id]["BusyCPUs"] += len(slot["ChildCPUs"])
                status[annex_name][request_id]["NodeCount"] += 1

            slot_birthday = slot.get("DaemonStartTime")
            annex_birthday = annex_attrs[annex_name].get('first_birthday')
            if annex_birthday is None or slot_birthday < annex_birthday:
                annex_attrs[annex_name]['first_birthday'] = slot_birthday
                annex_attrs[annex_name]['first_request_id'] = slot.get("hpc_annex_request_id")
            annex_birthday = annex_attrs[annex_name].get('last_birthday')
            if annex_birthday is None or slot_birthday > annex_birthday:
                annex_attrs[annex_name]['last_birthday'] = slot_birthday
                annex_attrs[annex_name]['last_request_id'] = slot.get("hpc_annex_request_id")

        all_jobs = defaultdict(int)
        running_jobs = defaultdict(int)
        constraint = 'TargetAnnexName =!= undefined'
        if the_annex_name is not None:
            constraint = f'TargetAnnexName == "{the_annex_name}"'
        target_jobs = schedd.query(
            constraint,
            opts=htcondor.QueryOpts.DefaultMyJobsOnly,
            projection=['JobStatus', 'TargetAnnexName', 'hpc_annex_nodes'],
        )
        for job in target_jobs:
            target_annex_name = job['TargetAnnexName']
            all_jobs[target_annex_name] += 1

            if job['JobStatus'] == htcondor.JobStatus.RUNNING:
                running_jobs[target_annex_name] += 1

        if len(target_jobs) == 0 and the_annex_name is not None:
            print(f"Found no jobs targeting annex '{the_annex_name}'.");

        if len(status) == 0 and len(annex_slots) == 0:
            if the_annex_name is None:
                print(f"Found no established or requested annexes.")
            else:
                print(f"Found no established or requested annexes named '{the_annex_name}'.")
            return

        #
        # Do the actual reporting (after calculating some aggregates).
        #
        for annex_name, annex_status in status.items():
            requests = len(annex_status)

            busy_CPUs = 0
            total_CPUs = 0
            node_count = 0
            requested_and_left = 0
            requested_but_not_joined = 0
            for request_ID, values in annex_status.items():
                if isinstance(values, str):
                    if values == "granted":
                        requested_and_left += 1
                    else:
                        requested_but_not_joined += 1
                else:
                    total_CPUs += values.get("TotalCPUs", 0)
                    busy_CPUs += values.get("BusyCPUs", 0)
                    node_count += values.get("NodeCount", 0)
            requested_and_active = requests - requested_but_not_joined - requested_and_left

            if the_annex_name is None:
                if total_CPUs == 0:
                    print(f"Annex '{annex_name}' is not established.", end='')
                else:
                    all_job_count = all_jobs.get(annex_name, 0)
                    running_job_count = running_jobs.get(annex_name, 0)
                    print(f"Annex '{annex_name}' is established and running {running_job_count} of {all_job_count} jobs on {busy_CPUs} of {total_CPUs} CPUs.", end='')

                if requests != requested_and_active:
                    print(f"  {requested_but_not_joined}/{requested_and_active}/{requested_and_left} requests are pending/active/retired.", end='')

                print()
                continue

            # Is the annex currently active?
            if total_CPUs != 0:
                print(f"Annex '{annex_name}' is established.")

                # When did the annex start, and how long does it have to run?
                oldest_time = annex_attrs[annex_name]['first_birthday']
                oldest_hours = lifetimes.get(annex_attrs[annex_name]['first_request_id'])
                youngest_time = annex_attrs[annex_name]['last_birthday']
                youngest_hours = lifetimes.get(annex_attrs[annex_name]['last_request_id'])

                now = int(time.time())
                print(
                    f"Its oldest established request is about "
                    f"{(now - oldest_time)/(60*60):.2f} hours old",
                    end=""
                )
                if oldest_hours is not None:
                    print(
                        f" and will retire in "
                        f"{((oldest_time + int(oldest_hours)) - now)/(60*60):.2f} hours."
                    )
                else:
                    print(".")

                if annex_attrs[annex_name]['first_request_id'] != annex_attrs[annex_name]['last_request_id']:
                    print(
                        f"Its youngest established request is about "
                        f"{(now - youngest_time)/(60*60):.2f} hours old",
                        end=""
                    )
                    if youngest_hours is not None:
                        print(
                            f" and will retire in "
                            f"{((youngest_time + int(youngest_hours)) - now)/(60*60):.2f} hours."
                        )
                    else:
                        print(".")
            else:
                print(f"Annex '{annex_name}' is not established.")

            # How big is it?
            requested_nodes = requested_nodes.get(annex_name, 0)
            print(f"You requested {requested_nodes} nodes for this annex, of which {node_count} are in an established annex.")
            print(f"There are {total_CPUs} CPUs in the established annex, of which {busy_CPUs} are busy.")

            # How many jobs target it?  Of those, how many are running?
            all_job_count = all_jobs.get(annex_name, 0)
            running_job_count = running_jobs.get(annex_name, 0)
            print(f"{all_job_count} jobs must run on this annex, and {running_job_count} currently are.")

            # How many resource requests were made, and what's their status?
            print(f"You requested resources for this annex {requests} times; {requested_but_not_joined} are pending, {requested_and_active} comprise the established annex, and {requested_and_left} have retired.")

        if the_annex_name is None:
            print()
            print(f"For more information about an annex, specify it on the command line: 'htcondor annex status <specific-annex-name>'")


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
        if not htcondor.param.get("HPC_ANNEX_ENABLED", False):
            raise ValueError("HPC Annex functionality has not been enabled by your HTCondor administrator.")

        annex_collector = htcondor.param.get("ANNEX_COLLECTOR", "htcondor-cm-hpcannex.osgdev.chtc.io")
        collector = htcondor.Collector(annex_collector)

        token_file = create_annex_token(logger, "shutdown")
        atexit.register(lambda: os.unlink(token_file))
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
            security_context.setConfig("SEC_CLIENT_AUTHENTICATION_METHODS", "FS IDTOKENS PASSWORD")
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


    class add(Add):
        pass


    class systems(Systems):
        pass


    @classmethod
    def verbs(cls):
        return [cls.create, cls.add, cls.status, cls.shutdown, cls.systems]

