import os

import htcondor

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb


def increment(hash, key, subkey, subsubkey, number):
    value = hash[key][subkey].get(subsubkey, 0)
    value += number
    hash[key][subkey][subsubkey] = value


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

        #
        # This is obviously the wrong format, but there's only so much
        # work I'm willing to do before I get word on what it should be.
        #
        #   - This probably shouldn't be a table.
        #   - It may make sense to report on the (non-active) requests
        #     in their own section.
        #   - It may be helpful to report how many jobs (owned by this
        #     user) are targetting each annex, and of those, how many
        #     are running.
        #
        print(f'          \t\t           REQUESTS\t        CPUS')
        print(f'ANNEX NAME\t\tPENDING/ACTIVE/DONE\t  BUSY/TOTAL')
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

            print(f'{annex_name}\t\t    {requested_but_not_joined}/{requests - requested_but_not_joined - requested_and_left}/{requested_and_left}\t\t    {busy_CPUs}/{total_CPUs}')


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
        print("After each resource shuts itself down, the remote SLURM job(s) will clean up and then exit.");
        print("Annex requests that are still in progress have not been affected.")


class Annex(Noun):
    """
    Operations on HPC Annexes.
    """


    class shutdown(Shutdown):
        pass


    class status(Status):
        pass


    @classmethod
    def verbs(cls):
        return [cls.shutdown, cls.status]

