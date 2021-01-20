"""
Various helper utilities for the HTCondor-ES integration
"""

import os
import pwd
import sys
import time
import shlex
import socket
import random
import json
import logging
import logging.handlers

import htcondor

from pathlib import Path

from . import config


def get_schedds(args=None):
    """
    Return a list of schedd ads representing all the schedds in the pool.
    """
    collectors = args.collectors
    if collectors:
        collectors = collectors.split(",")
    else:
        collectors = []
        logging.warning("The list of Collectors to query is empty")

    schedd_ads = {}
    for host in collectors:
        coll = htcondor.Collector(host)
        try:
            schedds = coll.locateAll(htcondor.DaemonTypes.Schedd)
        except IOError as e:
            logging.warning(str(e))
            continue

        for schedd in schedds:
            schedd["MyPool"] = host
            try:
                schedd_ads[schedd["Name"]] = schedd
            except KeyError:
                pass

    schedd_ads = list(schedd_ads.values())
    random.shuffle(schedd_ads)

    if args and args.schedds:
        return [s for s in schedd_ads if s["Name"] in args.schedds.split(",")]

    return schedd_ads


def get_startds(args=None):
    """
    Return a list of startd ads representing all the startds in the pool.
    """
    collectors = args.collectors
    if collectors:
        collectors = collectors.split(",")
    else:
        collectors = []
        logging.warning("The list of Collectors to query is empty")

    startd_ads = {}
    for host in collectors:
        coll = htcondor.Collector(host)
        try:
            # Get one ad per machine
            name_ads = coll.query(
                htcondor.AdTypes.Startd,
                constraint='(SlotType == "Static") || (SlotType == "Partitionable")',
                projection=["Name", "CondorVersion"],
            )
            for ad in name_ads:
                try:
                    # Remote history bindings only exist in startds running 8.9.7+
                    version = tuple(
                        [int(x) for x in ad["CondorVersion"].split()[1].split(".")]
                    )
                    if ad["Name"][0:6] == "slot1@":
                        if version < (8, 9, 7):
                            logging.warning(
                                f"The Startd on {ad['Name'].split('@')[-1]} is running HTCondor < 8.9.7 and will be skipped"
                            )
                            continue
                        startd = coll.locate(htcondor.DaemonTypes.Startd, ad["Name"])
                        startd["MyPool"] = host
                        startd_ads[startd["Machine"]] = startd
                except Exception:
                    continue

        except IOError as e:
            logging.warning(str(e))
            continue

    startd_ads = list(startd_ads.values())
    random.shuffle(startd_ads)

    if args and args.startds:
        return [s for s in startd_ads if s["Machine"] in args.startds.split(",")]

    return startd_ads


def time_remaining(starttime, timeout, positive=True):
    """
    Return the remaining time (in seconds) until starttime + timeout
    Returns 0 if there is no time remaining
    """
    elapsed = time.time() - starttime
    if positive:
        return max(0, timeout - elapsed)
    return timeout - elapsed


def set_up_logging(args):
    """Configure root logger"""
    logger = logging.getLogger()

    # Get the log level from the config
    log_level = getattr(logging, args.log_level.upper(), None)
    if not isinstance(log_level, int):
        logging.error(f"Invalid log level: {log_level}")
        sys.exit(1)
    logger.setLevel(log_level)

    # Determine format
    fmt = "%(asctime)s %(message)s"
    datefmt = "%m/%d/%y %H:%M:%S"
    if "debug_levels" in args:
        fmts = config.debug2fmt(args.debug_levels)
        fmt = fmts["fmt"]
        datefmt = fmts["datefmt"]

    # Check if we're logging to a file
    if args.log_file is not None:
        log_path = Path(args.log_file)

        # Make sure the parent directory exists
        if not log_path.parent.exists():
            logging.debug(f"Attempting to create {log_path.parent}")
            try:
                log_path.parent.mkdir(parents=True)
            except Exception:
                logging.exception(
                    f"Error while creating log file directory {log_path.parent}"
                )
                sys.exit(1)

        filehandler = logging.handlers.RotatingFileHandler(
            args.log_file, maxBytes=100000
        )
        filehandler.setFormatter(logging.Formatter(fmt=fmt, datefmt=datefmt))
        logger.addHandler(filehandler)

    # Check if logging to stdout is worthwhile
    if os.isatty(sys.stdout.fileno()):
        streamhandler = logging.StreamHandler(stream=sys.stdout)
        streamhandler.setFormatter(logging.Formatter(fmt=fmt, datefmt=datefmt))
        logger.addHandler(streamhandler)


def collect_metadata():
    """
    Return a dictionary with:
    - hostname
    - username
    - current time
    """
    result = {}
    result["es_push_hostname"] = socket.gethostname()
    result["es_push_username"] = pwd.getpwuid(os.geteuid()).pw_name
    result["es_push_runtime"] = int(time.time())
    return result
