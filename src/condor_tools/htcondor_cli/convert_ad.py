from htcondor._utils.ansi import Color, colorize, stylize, AnsiOptions
from time import time
from math import log

DAEMON_HP_DEFAULT = -1
DAEMON_HP_MAX = 100

def clamp(percent: float) -> float:
    return sorted([0.0, percent, 1.0])[1]

def health_str(hp: int, num_daemons: int = 1) -> str:
    percent = clamp(hp / (DAEMON_HP_MAX * num_daemons))
    if percent < 0.25:
        health = stylize("Bad", AnsiOptions(color=Color.BRIGHT_RED, bold=True))
    elif percent < 0.5:
        health = stylize("Poor", AnsiOptions(color_id=208, bold=True)) # 208 is orange
    elif percent < 0.75:
        health = stylize("Decent", AnsiOptions(color=Color.BRIGHT_YELLOW, bold=True))
    else:
        health = stylize("Good", AnsiOptions(color=Color.GREEN, bold=True))

    return health


def convertRecentDaemonCoreDutyCycle(ad) -> float:
    """Convert Recent DaemonCore Duty Cycle (rdcdc) into health"""

    # Daemon Core Duty Cycle is healthy (100%) up to 95% with a
    # steep cliff reulting in 98% being unhealthy (0%).
    # To achieve this cliff we use `f(x) = -(0.95 / 0.03)x + 31.0334`
    # with a minimum of 0 and maximum of 1
    # - Cole Bollig 2024-08-16

    duty_cycle = clamp(ad["RecentDaemonCoreDutyCycle"])
    return clamp(-(0.95 / 0.03) * duty_cycle + 31.0334)


def convertTransferQueueStats(ad) -> float:
    percent = 0
    for xfer_type in ["Upload", "Download"]:
        bytes_per_sec = ad.get(f"FileTransfer{xfer_type}BytesPerSecond_1m", 0.0)
        num_xfers = ad.get(f"TransferQueueNum{xfer_type}ing", 0)

        if num_xfers == 0 or (bytes_per_sec / num_xfers) != 0:
            percent += 0.25

        mb_waiting = ad.get(f"FileTransferMBWaitingTo{xfer_type}", 0.0)
        wait_time = ad.get(f"TransferQueue{xfer_type}WaitTime", 0.0)
        num_waiting = ad.get(f"TransferQueueNumWaitingTo{xfer_type}", 0)

        if num_waiting == 0 or mb_waiting < 100 or (wait_time / num_waiting) < 300:
            percent += 0.25

    return clamp(percent)


DAEMON_HEALTH_TABLE = {
    "MASTER": [
        (100, convertRecentDaemonCoreDutyCycle),
    ],
    "COLLECTOR": [
        (100, convertRecentDaemonCoreDutyCycle),
    ],
    "NEGOTIATOR": [
        (100, convertRecentDaemonCoreDutyCycle),
    ],
    "SCHEDD": [
        (72, convertRecentDaemonCoreDutyCycle),
        (28, convertTransferQueueStats)
    ],
    "STARTD": [
        (100, convertRecentDaemonCoreDutyCycle),
    ],
}


def adtype_to_daemon(ad_type: str) -> str:
    conversion = {
        "DAEMONMASTER": "MASTER",
        "COLLECTOR": "COLLECTOR",
        "NEGOTIATOR": "NEGOTIATOR",
        "SCHEDULER": "SCHEDD",
        "STARTD": "STARTD",
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
        "Health": colorize("UNKNOWN", Color.MAGENTA),
        "Machine": ad["Machine"],
    }

    # Daemon Health (HP) is on a scale of 0 (Bad) -> 100 (Good)
    portions = [(worth * convert(ad)) for worth, convert in DAEMON_HEALTH_TABLE.get(daemon, [])]
    HP = sum(portions) if len(portions) > 0 else DAEMON_HP_DEFAULT

    if HP > DAEMON_HP_DEFAULT:
        status["Health"] = health_str(HP)
    status["HealthPoints"] = sorted([0, HP, DAEMON_HP_MAX])[1]

    return (daemon, status)


def ads_to_daemon_status(ads: list):
    for ad in ads:
        info = _ad_to_daemon_status(ad)
        if info is not None:
            yield info
