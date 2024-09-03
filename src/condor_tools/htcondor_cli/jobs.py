import time
import re
import getpass

from datetime import datetime
from collections import defaultdict

import htcondor
import classad

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli.utils import readable_time, readable_size, s

JSM_HTC_JOBLIST_SUBMIT = 6

class Submit(Verb):
    """
    Submits a list of jobs given a submit template and a data table
    """

    options = {
        "positional 1": {
            "args": ("template_file",),
            "help": "Jobs submit template filename",
        },
        "table arg": {
            "args": ("--table","-table"),
            "help": "Jobs submit data table filename",
            "dest": "table_file",
            "default": None,
        },
        "iterator arg": {
            "args": ("--iterator","-iterator"),
            "help": "Jobs submit data iterator",
            "dest": "queue_args",
            "default": None
        },
    }

    def __init__(self, logger, template_file, queue_args, table_file, **options):

        schedd = htcondor.Schedd()
        sub = self.parse_template_file(template_file)

        if sub is None:
            raise ValueError((f"Required template not found in {template_file}"))

        iter_label=""
        if queue_args:
            sub.setQArgs(queue_args);
            iter_label = queue_args;
        elif table_file:
            sub.setQArgs(f"FROM TABLE {table_file}")
            iter_label = f"table {table_file}";

        # Submit the jobs
        try:
            submit_result = schedd.submit(sub, itemdata=sub.itemdata())
            logger.info(f"Submitted a joblist containing {submit_result.num_procs()} jobs to cluster {submit_result.cluster()}.")
        except RuntimeError as e:
            logger.error(f"Error while submitting joblist from {template_file} with {iter_label}:\n{str(e)}")
            return

    def parse_template_file(self, template_file):
        # load the submit object

        try:
            with open(template_file) as fh:
                subtext = fh.read()
        except RuntimeError as e:
            logger.error(f"Could not open template {template_file}:\n{str(e)}")
            return

        sub = htcondor.Submit(subtext)
        sub.setSubmitMethod(JSM_HTC_JOBLIST_SUBMIT, True)
        return sub


class Remove(Verb):
    """
    Removes a given job list
    """

    options = {
        "joblist_id": {
            "args": ("joblist_id",),
            "help": "Job list id",
        },
        "owner": {
            "args": ("--owner",),
            "help": "Remove jobs from the named user instead of your own jobs (requires superuser)",
            "default": getpass.getuser(),
        },
    }

    def __init__(self, logger, joblist_id, **options):

        schedd = htcondor.Schedd()

        try:
            remove_ad = schedd.act(
                htcondor.JobAction.Remove,
                job_spec=f"(ClusterId == {options['joblist_id']})",
                reason=f"via htcondor jobs remove (by user {getpass.getuser()})"
            )
        except Exception as e:
            logger.error(f"Error while trying to remove joblist {joblist_id}:\n{str(e)}")

        logger.info(f"Removed {remove_ad['TotalSuccess']} jobs in joblist {joblist_id} for user {options['owner']}.")
        if remove_ad.get("TotalError", 0) > 0:
            logger.warning(f"Warning: Could not remove {remove_ad['TotalError']} matching jobs.")


class Status(Verb):
    """
    Displays given active job list status
    """

    options = {
        "joblist_id": {
            "args": ("joblist_id",),
            "help": "Job list id",
        },
        "owner": {
            "args": ("--owner",),
            "help": "Show jobs from the named user instead of your own jobs",
            "default": getpass.getuser(),
        },
        "skip_history": {
            "args": ("--skip-history",),
            "action": "store_true",
            "default": False,
            "help": "Skip checking history for completed or removed jobs",
        },
    }

    def __init__(self, logger, joblist_id, **options):

        # Get the jobs
        query_opts = htcondor.QueryOpts.DefaultMyJobsOnly
        ads = schedd.query(
            constraint = f"""(ClusterId == {joblist_id})""",
            opts = htcondor.QueryOpts.ClusterAds + query_opts,
        )
        if len(ads) == 0:
            logger.error(f"""No active job lists found matching "{joblist_id}" for user {options["owner"]}.""")
            # TODO: check history?
            return

        # TODO print status from ads



class List(Verb):
    """
    List all active jobs
    """

    options = {
        "allusers": {
            "args": ("--allusers",),
            "help": "List jobs from all users",
            "action": "store_true",
        },
    }

    def __init__(self, logger, **options):

       schedd = htcondor.Schedd()

       query_opts = htcondor.QueryOpts.DefaultMyJobsOnly
       if options["allusers"]:
           query_opts = htcondor.QueryOpts.Default
       ads = schedd.query(
            projection = ["User", "ClusterId", "JobBatchName", "TotalSubmitProcs"],
            opts = query_opts + htcondor.QueryOpts.ClusterAds,
        )

       if len(ads) == 0:
           logger.error(f"""No active job lists found.""")
           return

       by_owner = set()
       for ad in ads:
           by_owner.add((ad["User"], ad["ClusterId"], ad["TotalSubmitProcs"]))

       by_owner = list(by_owner)
       by_owner.sort()

       if options["allusers"]:
           owner_width = 0
           for (owner, dummy) in by_owner:
               owner_width = max(owner_width, len(owner))
           fmt = f"{{owner:<{owner_width}}}  {{id:8}} {{num}}"
           header = fmt.format(owner="USER", id="ID", num="COUNT")
       else:
           fmt = "{id:8} {num}"
           header = "ID      COUNT"

       print(header)
       for (owner, id, count) in by_owner:
           print(fmt.format(owner=owner, id=id, num=count))


class Jobs(Noun):
    """
    Run operations on HTCondor job lists
    """

    class submit(Submit): pass
    class list(List):     pass
    class status(Status): pass
    class remove(Remove): pass

    @classmethod
    def verbs(cls):
        return [cls.submit, cls.list, cls.status, cls.remove]
