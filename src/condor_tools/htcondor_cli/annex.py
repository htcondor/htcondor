import os
from pathlib import Path
import getpass
import argparse

import htcondor

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

# Most of the annex create code is stored in a separate file.
from htcondor_cli.annex_create import annex_create


class Create(Verb):
    """
    Create an HPC annex.
    """


    options = {
        "annex_name": {
            "args": ("--name",),
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
            "help": "HPC machine name (e.g. stampede2)",
            "required": True,
        },
        "allocation": {
            "args": ("--project",),
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
            help=argparse.SUPPRESS,  # hidden option
            "default": getpass.getuser(),
        },
        "collector": {
            "args": ("--pool",),
            "help": "Collector that the annex reports to. Defaults to %(default)s",
            "default": htcondor.param.get("ANNEX_COLLECTOR", "htcondor-cm-hpcannex.osgdev.chtc.io"),
        },
        "ssh_target": {
            "args": ("--ssh_target",),
            #"help": "SSH target to use to talk with the HPC scheduler. Defaults to %(default)s",
            help=argparse.SUPPRESS,  # hidden option
            "default": f"{getpass.getuser()}@{htcondor.param.get("ANNEX_SSH_HOST", "login.xsede.org")}",
        }
        "token_file": {
            "args": ("--token_file",),
            "help": "Token file. Defaults to %(default)s",
            "type": Path,
            "default": Path(htcondor.param.get("ANNEX_TOKEN_FILE", f"~/.condor/tokens.d/{getpass.getuser()}@annex.osgdev.chtc.io")),
        },
        "password_file": {
            "args": ("--password_file",),
            #"help": "Password file. Defaults to %(default)s",
            help=argparse.SUPPRESS,  # hidden option
            "type": Path,
            "default": Path(htcondor.param.get("ANNEX_PASSWORD_FILE", "~/.condor/annex_password_file")),
        },
        "control_path": {
            "args": ("--tmp_dir",),
            "help": "Location to store temporary annex control files, probably should not be changed. Defaults to %(default)s",
            "type": Path,
            "default": Path(htcondor.param.get("ANNEX_TMP_DIR", "~/.hpc-annex")),
        }
    }

    def __init__(self, logger, **options):
        annex_create(logger, **options)


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


    @classmethod
    def verbs(cls):
        return [cls.shutdown,]

