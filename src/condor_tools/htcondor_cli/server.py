import sys
import subprocess
import re

import htcondor2
from htcondor._utils.ansi import Color, colorize, underline

from htcondor_cli.convert_ad import ads_to_daemon_status, health_str
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

CONFIG = htcondor2.param
CONDOR_WHO_CMD = ["condor_who", "-quick"]


class Status(Verb):
    """
    Displays status information about local daemons
    """

    options = {
        # Verb specific CLI options/flags
    }

    def __init__(self, logger, **options):
        defaults = {
            "Health": colorize("UNKNOWN", Color.MAGENTA),
            "Status": "Missing",
            "Type": "",
            "ADDR": "None",
            "PID": "-----",
        }
        daemon_info = {daemon.upper(): dict(defaults) for daemon in CONFIG["DAEMON_LIST"].replace(" ", ",").split(",") if daemon != ''}

        p = subprocess.run(CONDOR_WHO_CMD, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
        stdout = p.stdout.rstrip().decode()
        if p.returncode != 0 or stdout == "":
            raise RuntimeError(
                f"Failed to execute {' '.join(CONDOR_WHO_CMD)}: {p.stderr.rstrip().decode()}"
            )

        addr_to_daemon = {}

        for line in stdout.split("\n"):
            if line == "":
                continue
            elif re.match("^Master .+ Exited with code=\\d+", line):
                logger.info("There are no HTCondor daemons currently running")
                sys.exit(0)

            key, val = line.split("=", 1)
            key = key.upper().strip()
            val = val.strip().strip('"')
            if key in daemon_info.keys():
                daemon_info[key]["Status"] = f" {val} "
            elif "_" in key:
                daemon, info = key.rsplit("_", 1)
                if daemon in daemon_info.keys():
                    daemon_info[daemon][info] = val
                    if info == "ADDR":
                        addr_to_daemon[val] = daemon

        constraint = " || ".join(['MyAddress == "{ADDR}"'.format(**info) for info in daemon_info.values()])
        constraint = f"({constraint}) && SlotType =?= UNDEFINED"

        collector = htcondor2.Collector()
        ads = collector.query(constraint=constraint)

        if len(ads) == 0:
            raise RuntimeError("Collector returned no ClassAds for local daemons")

        system_hp = 0
        fmt_width = [0, 0, 0]

        for daemon_type, status in ads_to_daemon_status(ads):
            daemon = addr_to_daemon.get(status["Address"])
            if daemon is not None:
                daemon_info[daemon].update(status)
                if daemon_type not in daemon:
                    daemon_info[daemon]["Type"] = daemon_type
                system_hp += status["HealthPoints"]

                if len(daemon) > fmt_width[0]:
                    fmt_width[0] = len(daemon)
                if len(daemon_info[daemon]["Type"]) > fmt_width[1]:
                    fmt_width[1] = len(daemon_info[daemon]["Type"])
                if len(daemon_info[daemon]["Status"]) > fmt_width[2]:
                    fmt_width[2] = len(daemon_info[daemon]["Status"])

                assert daemon_info[daemon]["ADDR"] == daemon_info[daemon]["Address"]
            else:
                raise RuntimeError(f"Unexpected ClassAd returned")

        columns = [
            "Daemon",
            ("Type" if fmt_width[1] > 0 else ""),
            "PID",
            "Status",
            "Health",
        ]
        header = "{0: ^{fmt[0]}} {1: ^{fmt[1]}} {2: ^5} {3: ^{fmt[2]}} {4}".format(*columns, fmt=fmt_width)
        logger.info(underline(header))
        for daemon, info in daemon_info.items():
            logger.info(
                "{0: <{fmt[0]}} {Type: ^{fmt[1]}} {PID: >5} {Status: ^{fmt[2]}} {Health}".format(
                    daemon, **info, fmt=fmt_width
                )
            )

        logger.info(f"\nOverall HTCondor Health: {health_str(system_hp, len(daemon_info))}\n")

        return


class Server(Noun):
    """
    Run operations on the local HTCondor system
    """

    class status(Status):
        pass

    @classmethod
    def verbs(cls):
        return [cls.status]
