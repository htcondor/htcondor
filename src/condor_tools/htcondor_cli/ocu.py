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
import classad2 as classad

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli import TMP_DIR
from htcondor_cli.utils import readable_time, readable_size, s


class Create(Verb):
    """
    Creates an OCU when given an OCU request file.
    OCU request file should contain an Owner = and Request_* lines.
    """

    options = {
        "request_file": {
            "args": ("request_file",),
            "help": "Submit file",
        },
    }

    def __init__(self, logger, request_file, **options):
        # Make sure the specified request file exists and is readable
        request_file = Path(request_file)
        if not request_file.exists():
            raise FileNotFoundError(f"Could not find file: {str(request_file)}")
        if os.access(request_file, os.R_OK) is False:
            raise PermissionError(f"Could not access file: {str(request_file)}")

        # Get schedd
        schedd = htcondor.Schedd()

        # place ocu on the local schedd
        with request_file.open() as f:
            ocu_ad = classad.parseOne(f)

        try:
            result_ad = schedd.create_ocu(ocu_ad)
            ocu_id = result_ad["OCUId"]
            name = result_ad["OCUName"]
            print(f"OCU created with Id {ocu_id} and name {name}.")
        except Exception as e:
            raise RuntimeError(f"Error creating ocu: {str(e)}")

class Query(Verb):
    """
    Queries the existing OCUs in the system.
    """
    options = {
            "raw": {
                "args": ("--raw",),
                "action": "store_true",
                "default": False,
                "help": "display raw classad",
                },
            }

    def __init__(self, logger, **options):
        # Get schedd
        schedd = htcondor.Schedd()

        try:
            ad = classad.ClassAd()
            results = schedd.query_ocu(ad)

            if options["raw"]:
                for ad in results:
                    print(ad)
            else:
                print("OCUId  OCUName              Owner                CPUs  Memory Disk Activations")
                for ad in results:
                    ocuID = ad.get("OCUId", "unknown")
                    ocuName = ad.get("OCUName", "unknown")
                    owner = ad.get("Owner", "unknown")
                    cpus = ad.get("RequestCpus", 0)
                    memory = ad.get("RequestMemory", 0)
                    disk = ad.get("RequestDisk", 0)
                    activations = ad.get("OCUOwnerActivations", 0)
                    print(f"{ocuID:<6} {ocuName:<20} {owner:<20} {cpus:<4} {memory:<6} {disk:<4} {activations}")

        except Exception as e:
            raise RuntimeError(f"Error querying ocu: {str(e)}")

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
            remove_ad = schedd.remove_ocu(int(ocu_id))
        except Exception as e:
            logger.error(f"Error while trying to remove ocu {ocu_id}:\n{str(e)}")
            raise RuntimeError(f"Error removing ocu: {str(e)}")

        logger.info(f"Removed {ocu_id}.")

class OCU(Noun):
    """
    Run operations on HTCondor Owner Capacity Units (OCU)
    """

    class create(Create):
        pass

    class query(Query):
        pass

    class status(Status):
        pass

    class remove(Remove):
        pass

    @classmethod
    def verbs(cls):
        return [cls.create, cls.query, cls.status, cls.remove]
