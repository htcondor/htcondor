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
        "reboot": {
            "args": ("--reboot",),
            "action": "store_true",
            "default": False,
            "help": "Reboot the execution point host after evicting jobs (requires STARTD_REHOME_ALLOW_REBOOT on the EP)",
        },
    }

    def __init__(self, logger, **options):
        ep_name = options["ep_name"]
        schedd_name = options["schedd_name"]
        schedd_pool = options["schedd_pool"]
        cancel = options["cancel"]
        timeout = options["timeout"]
        reboot = options["reboot"]

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
            startd.rehome(schedd_name=schedd_name, schedd_pool=schedd_pool, timeout=timeout, cancel=cancel, reboot=reboot)
        except htcondor2.HTCondorException as e:
            raise RuntimeError(str(e))
        if cancel:
            logger.info(f"Sent rehome cancel command to {ep_name}")
        elif reboot:
            logger.info(f"Sent rehome command to {ep_name} (host will reboot if permitted)")
        else:
            logger.info(f"Sent rehome command to {ep_name}")


class Controller(Verb):
    """
    Designate (or clear) the access point allowed to evict any claim on an
    execution point
    """

    options = {
        "ep_name": {
            "args": ("ep_name",),
            "help": "Name of the execution point to control",
        },
        "ap_name": {
            "args": ("--ap",),
            "default": None,
            "help": "Name of the access point (schedd) to make the controller",
        },
        "ap_pool": {
            "args": ("--pool",),
            "default": None,
            "help": "Collector pool to use to find the access point (default: COLLECTOR_HOST)",
        },
        "clear": {
            "args": ("--clear",),
            "action": "store_true",
            "default": False,
            "help": "Clear the execution point's controller instead of setting one",
        },
    }

    def __init__(self, logger, **options):
        ep_name = options["ep_name"]
        ap_name = options["ap_name"]
        ap_pool = options["ap_pool"]
        clear = options["clear"]

        collector = htcondor2.Collector()

        location = collector.locate(
            htcondor2.DaemonType.Startd,
            ep_name,
        )
        startd = htcondor2.Startd(location)

        if clear:
            # Discover the current controller (if any) before clearing, so we
            # can tell that AP to drop this EP from its controlled list.
            controller_name = None
            try:
                ads = collector.query(
                    ad_type=htcondor2.AdType.Startd,
                    constraint=f'Machine == "{ep_name}"',
                    projection=["Controller"],
                )
                for ad in ads:
                    if ad.get("Controller"):
                        controller_name = ad["Controller"]
                        break
            except Exception:
                pass

            try:
                startd.clear_controller()
            except htcondor2.HTCondorException as e:
                raise RuntimeError(str(e))
            logger.info(f"Cleared controller on {ep_name}")

            # Best-effort: tell the former controller it no longer controls this
            # EP.  Clearing the EP already succeeded, so don't fail if the AP is
            # unreachable -- just warn.
            if controller_name:
                try:
                    ap_ad = collector.locate(htcondor2.DaemonType.Schedd, controller_name)
                    htcondor2.Schedd(ap_ad).register_controlled_ep(ep_name, remove=True)
                except Exception as e:
                    logger.warning(f"Could not notify {controller_name} that it no longer controls {ep_name}: {e}")
            return

        if not ap_name:
            raise RuntimeError("Specify the controlling access point with --ap, or use --clear")

        # Step 1: ask the access point to mint a controller token.
        try:
            if ap_pool:
                ap_collector = htcondor2.Collector(ap_pool)
                ap_ad = ap_collector.locate(htcondor2.DaemonType.Schedd, ap_name)
            else:
                ap_ad = collector.locate(htcondor2.DaemonType.Schedd, ap_name)
            schedd = htcondor2.Schedd(ap_ad)
            grant = schedd.request_controller_token()
        except Exception as e:
            raise RuntimeError(f"Cannot obtain controller token from {ap_name}: {e}")

        # Step 2: deliver the token to the execution point.
        controller_name = grant.get("ControllerName") or ap_name
        try:
            startd.set_controller(
                controller_name=controller_name,
                token=grant["Token"],
                controller_pool=grant.get("ControllerPool"),
                controller_identity=grant.get("ControllerIdentity"),
            )
        except htcondor2.HTCondorException as e:
            raise RuntimeError(str(e))

        logger.info(f"Set controller of {ep_name} to {controller_name}")

        # Step 3: tell the AP it now controls this EP, so it appears in
        # "htcondor ap controlled".  The EP already accepted control, so don't
        # fail the whole operation if this bookkeeping update can't be made.
        try:
            schedd.register_controlled_ep(ep_name)
        except Exception as e:
            logger.warning(f"Set controller on {ep_name}, but could not update {ap_name}'s controlled-EP list: {e}")


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

    class controller(Controller):
        pass

    class status(Status):
        pass

    @classmethod
    def verbs(cls):
        return [cls.rehome, cls.controller, cls.status]
