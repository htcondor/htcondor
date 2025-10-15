import sys
import os
import os.path
import logging
import subprocess
import shlex
import shutil
import tempfile
import time
import getpass


from datetime import datetime
from pathlib import Path

import htcondor2 as htcondor

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli import TMP_DIR
from htcondor_cli.utils import readable_time, readable_size, s

class Create(Verb):
    """
    Creates an OCU when given a submit file
    """

    options = {
        "submit_file": {
            "args": ("submit_file",),
            "help": "Submit file",
        },
    }

    def __init__(self, logger, submit_file, **options):
        # Make sure the specified submit file exists and is readable
        submit_file = Path(submit_file)
        if not submit_file.exists():
            raise FileNotFoundError(f"Could not find file: {str(submit_file)}")
        if os.access(submit_file, os.R_OK) is False:
            raise PermissionError(f"Could not access file: {str(submit_file)}")

        # Get schedd
        schedd = htcondor.Schedd()

        # place ocu on the local schedd
        with submit_file.open() as f:
            submit_data = f.read()
        submit_description = htcondor.Submit(submit_data)

        # Set s_method to HTC_JOB_SUBMIT
        JSM_HTC_JOB_SUBMIT = 3
        submit_description.setSubmitMethod(JSM_HTC_JOB_SUBMIT, True)

        # The Job class can only submit a single job at a time
        submit_qargs = submit_description.getQArgs()
        if submit_qargs != "" and submit_qargs != "1":
            raise ValueError("Can only place one OCU at a time")

        # Behave like condor_submit, to minimize astonishment.
        submit_description.issue_credentials()

        # Turn this job into an OCU holder
        submit_description["+IsOCUHolder"] = "true"

        # Allow an empty Executable line
        submit_description["Executable"] = "ocu"

        try:
            result = schedd.submit(submit_description, count=1)
            cluster_id = result.cluster()
            print(f"OCU placed with Id {cluster_id}.")
        except Exception as e:
            raise RuntimeError(f"Error placing ocu:\n{str(e)}")

class Status(Verb):
    """
    Shows current status of OCUs
    """

    def __init__(self, logger):
        collector = htcondor.Collector()

        submitter_ads = collector.query(htcondor.AdType.Submitter, constraint="True")

        if len(submitter_ads) == 0:
            print("There are no submitters in this pool, therefore no OCUs.")
            sys.exit(0)

        print("Owner                          OCUClaimed  OCUClaimedBorrowed OCUsWantedJobs  OCURunningJobs")
        print("-----                          ----------  ------------------ --------------  --------------")

        for ad in submitter_ads:
            name = ad.get("Name", "unknown")
            ocu_requested = ad.get("OCUWantedJobs", 0)
            ocu_running = ad.get("OCURunningJobs", 0)
            ocu_claimed = ad.get("OCUClaimsClaimed", 0)
            ocu_borrowed = ad.get("OCUClaimsBorrowed", 0)

            print(f"{name:<30} {ocu_claimed:>10} {ocu_borrowed:>19} {ocu_requested:>14} {ocu_running:>14}")

class Remove(Verb):
    """
    Remove an existing OCU
    """

    options = {
        "ocu_id": {
            "args": ("ocu_id",),
            "help": "OCU ID to remove",
        },
    }

    def __init__(self, logger, ocu_id, **options):

        schedd = htcondor.Schedd()

        try:
            remove_ad = schedd.act(
                htcondor.JobAction.Remove,
                job_spec=f"(ClusterId == {ocu_id})",
                reason=f"via htcondor ocu remove (by user {getpass.getuser()})"
            )
        except Exception as e:
            logger.error(f"Error while trying to remove ocu {ocu_id}:\n{str(e)}")
            raise RuntimeError(f"Error removing ocu: {str(e)}")

        logger.info(f"Removed {ocu_id}.")
        if remove_ad.get("TotalError", 0) > 0:
            logger.warning(f"Warning: Could not remove {remove_ad['TotalError']} ocu.")


class OCU(Noun):
    """
    Run operations on HTCondor Owner Capacity Units (OCU)
    """

    class create(Create):
        pass

    class status(Status):
        pass

    class remove(Remove):
        pass

    @classmethod
    def verbs(cls):
        return [cls.create, cls.status, cls.remove]
