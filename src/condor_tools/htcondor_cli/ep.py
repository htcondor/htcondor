import htcondor2
from htcondor2._utils.ansi import underline

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb


class Rehome(Verb):
    """
    Rehome an execution point, killing all running jobs
    """

    options = {
        "ep_name": {
            "args": ("ep_name",),
            "help": "Name of the execution point to rehome",
        },
        "schedd_name": {
            "args": ("schedd_name",),
            "help": "Name of the schedd to rehome to",
        },
        "schedd_pool": {
            "args": ("--schedd-pool",),
            "default": None,
            "help": "Collector pool to use to find the schedd (default: COLLECTOR_HOST)",
        },
        "cancel": {
            "args": ("--cancel",),
            "action": "store_true",
            "default": False,
            "help": "Cancel a previous rehome, unsetting STARTD_DIRECT_ATTACH_SCHEDD",
        },
        "timeout": {
            "args": ("--timeout",),
            "type": int,
            "default": 0,
            "help": "Timeout in seconds for the rehome operation (default: 0)",
        },
    }

    def __init__(self, logger, **options):
        ep_name = options["ep_name"]
        schedd_name = options["schedd_name"]
        schedd_pool = options["schedd_pool"]
        cancel = options["cancel"]
        timeout = options["timeout"]

        collector = htcondor2.Collector()

        # Validate the schedd name before sending the rehome command
        if not cancel:
            try:
                # Use the specified pool if provided, otherwise use default
                if schedd_pool:
                    schedd_collector = htcondor2.Collector(schedd_pool)
                    schedd_ad = schedd_collector.locate(
                        htcondor2.DaemonType.Schedd,
                        schedd_name,
                    )
                else:
                    schedd_ad = collector.locate(
                        htcondor2.DaemonType.Schedd,
                        schedd_name,
                    )
                htcondor2.ping(schedd_ad)
            except Exception as e:
                raise RuntimeError(f"Cannot reach schedd {schedd_name}: {e}")

        location = collector.locate(
            htcondor2.DaemonType.Startd,
            ep_name,
        )

        startd = htcondor2.Startd(location)
        try:
            startd.rehome(schedd_name=schedd_name, schedd_pool=schedd_pool, timeout=timeout, cancel=cancel)
        except htcondor2.HTCondorException as e:
            raise RuntimeError(str(e))
        if cancel:
            logger.info(f"Sent rehome cancel command to {ep_name}")
        else:
            logger.info(f"Sent rehome command to {ep_name}")


class Status(Verb):
    """
    Displays the status of all slots on an execution point
    """

    options = {
        "ep_name": {
            "args": ("ep_name",),
            "help": "Name of the execution point machine to query",
        },
    }

    def __init__(self, logger, **options):
        ep_name = options["ep_name"]

        collector = htcondor2.Collector()
        constraint = f'Machine == "{ep_name}"'
        ads = collector.query(
            ad_type=htcondor2.AdType.Startd,
            constraint=constraint,
        )

        if len(ads) == 0:
            raise RuntimeError(f"No slots found for execution point: {ep_name}")

        # Collect slot info from ads
        slots = []
        for ad in ads:
            slot = {
                "Name": ad.get("Name", ""),
                "Activity": ad.get("Activity", "Unknown"),
                "State": ad.get("State", "Unknown"),
                "Cpus": ad.get("Cpus", 0),
                "Memory": ad.get("Memory", 0),
                "SlotType": ad.get("SlotType", ""),
            }
            slots.append(slot)

        # Sort by slot name
        slots.sort(key=lambda s: s["Name"])

        # Compute column widths
        name_w = max(len("Slot"), max(len(s["Name"]) for s in slots))
        type_w = max(len("Type"), max(len(s["SlotType"]) for s in slots))
        state_w = max(len("State"), max(len(s["State"]) for s in slots))
        activity_w = max(len("Activity"), max(len(s["Activity"]) for s in slots))

        header = f"{'Slot': <{name_w}}  {'Type': <{type_w}}  {'State': <{state_w}}  {'Activity': <{activity_w}}  {'Cpus': >4}  {'Memory': >8}"
        logger.info(underline(header))

        for s in slots:
            memory_str = f"{s['Memory']} MB"
            logger.info(
                f"{s['Name']: <{name_w}}  {s['SlotType']: <{type_w}}  {s['State']: <{state_w}}  {s['Activity']: <{activity_w}}  {s['Cpus']: >4}  {memory_str: >8}"
            )

        logger.info(f"\nTotal slots: {len(slots)}")
        logger.info("")


class EP(Noun):
    """
    Run operations on HTCondor execution points
    """

    class rehome(Rehome):
        pass

    class status(Status):
        pass

    @classmethod
    def verbs(cls):
        return [cls.rehome, cls.status]
