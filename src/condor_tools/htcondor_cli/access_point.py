import htcondor2
import classad2
from htcondor2._utils.ansi import underline
from pathlib import Path

from htcondor_cli.convert_ad import echo_ads_to_daemon_status, health_str
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

class Status(Verb):
    """
    Displays status information all/specified access points
    """

    options = {
        "hostnames": {
            "args": ("hostname",),
            "nargs": "*",
            "action": "append",
            "help": "Specific Access Point hostnames to query",
        }
    }

    def __init__(self, logger, **options):
        hostnames = [host for sublist in options["hostname"] for host in sublist]
        constraint = None
        if len(hostnames) > 0:
            constraint = " || ".join(f'Machine == "{host}"' for host in hostnames)

        try:
            collector = htcondor2.Collector()
            ads = collector.query(ad_type=htcondor2.AdType.Schedd, constraint=constraint)
        except Exception:
            ads = []

        if len(ads) == 0:
            if len(hostnames):
                raise RuntimeError("Failed to locate specified Access Points information from Collector")
            else:
                local_schedd_ad = Path(htcondor2.param["SCHEDD_DAEMON_AD_FILE"])
                if local_schedd_ad.exists():
                    ads.append(classad2.parseOne(local_schedd_ad.open().read()))
                else:
                    raise RuntimeError("Failed to locate any Access Point information")

        ap_status = {}
        width = 0

        for daemon, status, ad in echo_ads_to_daemon_status(ads):
            assert daemon == "SCHEDD"
            host = status["Machine"]
            width = len(host) if len(host) > width else width

            if host in hostnames:
                hostnames.remove(host)

            if host in ap_status:
                ap_status[host]["HP"] += status["HealthPoints"]
                ap_status[host]["Count"] += 1
            else:
                ap_status[host] = {"HP": status["HealthPoints"], "Count": 1, "RDCDC" : ad["RecentDaemonCoreDutyCycle"]}

        if len(ap_status) > 0:
            columns = ["Access Point", "Health", "DutyCycle"]
            logger.info(underline("{0: ^{width}}   {1}   {2}".format(*columns, width=width)))
            for host, status in ap_status.items():
                rdcdc = status["RDCDC"] * 100
                health = health_str(status['HP'], status['Count'])
                logger.info(f"{host: <{width}}   {health: >6}   {rdcdc: >10.2}%")

        if len(hostnames) > 0:
            print(
                "{0}Failed to find the following hosts:".format(
                    "" if len(ap_status) == 0 else "\n"
                )
            )
            for host in hostnames:
                print(host)

        logger.info("")


class AccessPoint(Noun):
    """
    Run operations on pool access points
    """

    class status(Status):
        pass

    @classmethod
    def verbs(cls):
        return [cls.status]
