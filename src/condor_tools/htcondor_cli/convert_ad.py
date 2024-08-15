from htcondor._utils.ansi import Color, colorize
from time import time

DAEMON_HP_DEFAULT = -1

def health_str(hp: int, ceil: int = 100) -> str:
    if hp / ceil < 0.25:
        health = colorize("Poor", Color.BRIGHT_RED)
    elif hp / ceil < 0.75:
        health = colorize("Decent", Color.BRIGHT_YELLOW)
    else:
        health = colorize("Good", Color.BRIGHT_GREEN)

    return health


def duty_cycle_to_health(worth: int, duty_cycle: float) -> int:
    duty_cycle = sorted([0.0, duty_cycle, 1.0])[1]
    return int(worth * max(
        -(0.05 / 0.95 * duty_cycle) + 1 if duty_cycle <= 0.95
        else -(0.95 / 0.03 * duty_cycle) + 31.033,
        0
    ))


def adtype_to_daemon(ad_type: str) -> str:
    conversion = {
        "DAEMONMASTER" : "MASTER",
        "COLLECTOR"    : "COLLECTOR",
        "NEGOTIATOR"   : "NEGOTIATOR",
        "SCHEDULER"    : "SCHEDD",
        "STARTD"       : "STARTD",
    }

    return conversion.get(ad_type.upper())


def _ad_to_daemon_status(ad) -> tuple:
    if "MyType" not in ad:
        return None

    daemon = adtype_to_daemon(ad["MyType"])
    if daemon is None:
        return None

    status = {
        "Address": ad["MyAddress"],
        "Health" : colorize("UNKNOWN", Color.MAGENTA),
        "Machine": ad["Machine"],
    }

    HP = DAEMON_HP_DEFAULT

    if daemon == "MASTER":
        HP = duty_cycle_to_health(100, ad.get("RecentDaemonCoreDutyCycle", 0.0))

    elif daemon == "COLLECTOR":
        HP = duty_cycle_to_health(100, ad.get("RecentDaemonCoreDutyCycle", 0.0))

    elif daemon == "NEGOTIATOR":
        HP = duty_cycle_to_health(10, ad.get("RecentDaemonCoreDutyCycle", 0.0))

        if time() - ad.get("LastNegotiationCycleTime0", 0) < 60:
            HP += 30

        if ad.get("LastNegotiationCycleDuration0", 0) < 10:
            HP += 30

        total_matches = total_considered = 0
        for i in range(3):
            total_matches += ad.get(f"LastNegotiationCycleMatches{i}", 0)
            total_considered += ad.get(f"LastNegotiationCycleNumJobsConsidered{i}", 0)
        if total_considered == 0 or (total_matches / total_considered) > 0.25:
            HP += 30

    elif daemon == "SCHEDD":
        HP = duty_cycle_to_health(72, ad.get("RecentDaemonCoreDutyCycle", 0.0))

        for xfer_type in ["Upload", "Download"]:
            bytes_per_sec = ad.get(f"FileTransfer{xfer_type}BytesPerSecond_1m", 0.0)
            num_xfers = ad.get(f"TransferQueueNum{xfer_type}ing", 0)

            if num_xfers == 0 or (bytes_per_sec / num_xfers) >= 25:
                HP += 7

            mb_waiting = ad.get(f"FileTransferMBWaitingTo{xfer_type}", 0.0)
            wait_time = ad.get(f"TransferQueue{xfer_type}WaitTime", 0.0)
            num_waiting = ad.get(f"TransferQueueNumWaitingTo{xfer_type}", 0)

            if num_waiting == 0 or (mb_waiting > 100 and (wait_time / num_waiting) < 300):
                HP += 7

    elif daemon == "STARTD":
        HP = duty_cycle_to_health(100, ad.get("RecentDaemonCoreDutyCycle", 0.0))

    if HP > DAEMON_HP_DEFAULT:
        status["Health"] = health_str(HP)
    status["HealthPoints"] = sorted([0, HP, 100])[1]

    return (daemon, status)


def ads_to_daemon_status(ads: list) -> list:
    for ad in ads:
        info = _ad_to_daemon_status(ad)
        if info is not None:
            yield info

