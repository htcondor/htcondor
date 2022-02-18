import htcondor

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb


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
        collector = htcondor.Collector()
        location_ads = collector.query(
            ad_type=htcondor.AdTypes.Master,
            constraint=f'AnnexName =?= "{annex_name}"',
        )

        if len(location_ads) == 0:
            print(f"No resources found in annex '{annex_name}'.")
            return

        print(f"Shutting down annex '{annex_name}'...")
        for location_ad in location_ads:
            htcondor.send_command(
                location_ad,
                # Make DaemonOff vs DaemonFast a command-line option?
                htcondor.DaemonCommands.DaemonOffFast,
                "STARTD",
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

