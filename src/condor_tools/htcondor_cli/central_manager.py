import htcondor2
from htcondor2._utils.ansi import underline

from htcondor_cli.convert_ad import ads_to_daemon_status, health_str
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb


class Status(Verb):
    """
    Displays status information all central managers defined by COLLECTOR_HOST
    """

    options = {
        # Verb specific CLI options/flags
    }

    def __init__(self, logger, **options):
        constraint = '(MyType == "Negotiator" || MyType == "Collector")'
        ads = []

        collectors = htcondor2.param["COLLECTOR_HOST"].replace(" ", ",").replace("\t", ",")
        for host in collectors.split(","):
            if host == "":
                continue

            try:
                collector = htcondor2.Collector(host)
                ads.extend(collector.query(constraint=constraint))
            except:
                logger.info(f"Failed to query Collector at {host}")

        if len(ads) == 0:
            raise RuntimeError("Failed to locate any Central Managers")

        cm_status = {}
        width = 0

        for daemon, status in ads_to_daemon_status(ads):
            host = status["Machine"]
            width = len(host) if len(host) > width else width

            if host in cm_status:
                cm_status[host]["HP"] += status["HealthPoints"]
                cm_status[host]["Count"] += 1
            else:
                cm_status[host] = {"HP": status["HealthPoints"], "Count": 1}

        columns = ["Central Manager", "Health"]
        logger.info(underline("{0: ^{width}}   {1}".format(*columns, width=width)))
        for host, status in cm_status.items():
            logger.info(f"{host: <{width}}   {health_str(status['HP'], status['Count'])}")
        logger.info("")


class CentralManager(Noun):
    """
    Run operations on pool central managers
    """

    class status(Status):
        pass

    @classmethod
    def verbs(cls):
        return [cls.status]
