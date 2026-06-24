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
                raise RuntimeError("Failed to locate specified Access Point(s)")
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


class Claims(Verb):
    """
    Displays active claims held by the access point
    """

    options = {
        "ap_name": {
            "args": ("--name",),
            "help": "Name or address of the access point to query",
        }
    }

    def __init__(self, logger, **options):
        ap_name = options.get("name")

        if ap_name:
            collector = htcondor2.Collector()
            location = collector.locate(htcondor2.DaemonType.Schedd, ap_name)
            schedd = htcondor2.Schedd(location)
        else:
            schedd = htcondor2.Schedd()

        try:
            claim_ads = schedd.get_claims()
        except Exception as e:
            raise RuntimeError(f"Failed to query claims: {e}")

        if len(claim_ads) == 0:
            logger.info("No active claims")
            return

        name_width = len("Name")
        for ad in claim_ads:
            name = ad.get("Name", "???")
            name_width = max(name_width, len(name))

        columns = ["Name", "Activity", "Source", "Activations"]
        header = f"{columns[0]: <{name_width}}   {columns[1]: <10}   {columns[2]: <12}   {columns[3]}"
        logger.info(underline(header))

        for ad in claim_ads:
            name = ad.get("Name", "???")
            activity = ad.get("Activity", "Unknown")
            activations = ad.get("NumJobStarts", 0)

            if ad.get("OCU", False):
                source = "OCU"
            elif ad.get("IsDirectAttach"):
                source = "DirectAttach"
            else:
                source = "Negotiator"

            logger.info(f"{name: <{name_width}}   {activity: <10}   {source: <12}   {activations}")

        logger.info("")


class AccessPoint(Noun):
    """
    Run operations on pool access points
    """

    class status(Status):
        pass

    class claims(Claims):
        pass

    @classmethod
    def verbs(cls):
        return [cls.status, cls.claims]
