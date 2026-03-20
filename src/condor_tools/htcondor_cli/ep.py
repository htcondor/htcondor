import htcondor2
from htcondor2._utils.ansi import underline

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb


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

    class status(Status):
        pass

    @classmethod
    def verbs(cls):
        return [cls.status]
