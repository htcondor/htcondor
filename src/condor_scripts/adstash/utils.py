# Copyright 2021 HTCondor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import pwd
import sys
import time
import socket
import random
import logging
import tempfile
import logging.handlers

import htcondor

from pathlib import Path

from adstash.config import debug2fmt


def get_schedds(args):
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
        except IOError:
            logging.exception(f"Error while getting Schedds from Collector {host}")
            continue

        for schedd in schedds:
            if args.schedds and not (schedd["Name"] in args.schedds.split(",")):
                continue
            schedd["MyPool"] = host
            try:
                schedd_ads[schedd["Name"]] = schedd
            except KeyError:
                pass

    schedd_ads = list(schedd_ads.values())
    random.shuffle(schedd_ads)

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

    startd_allow_list = None
    if args.startds is not None:
        startd_allow_list = set(args.startds.split(","))

    startd_ads = {}
    for host in collectors:
        coll = htcondor.Collector(host)
        try:
            # Want one ad per machine, so don't bother with dynamic slots
            slot_ads = coll.query(
                htcondor.AdTypes.Startd,
                constraint='(SlotType == "Static") || (SlotType == "Partitionable")',
                projection=["Name", "Machine", "CondorVersion", "CondorPlatform", "MyAddress"],
            )
            for slot_ad in slot_ads:
                machine = slot_ad["Machine"]
                if machine in startd_ads or (startd_allow_list is not None and machine not in startd_allow_list):
                    continue

                # Remote history bindings only exist in startds running 8.9.7+
                version = tuple(int(x) for x in slot_ad["CondorVersion"].split()[1].split("."))
                if version < (8, 9, 7):
                    logging.warning(
                        f"The Startd on {machine} is running HTCondor < 8.9.7 and will be skipped"
                    )
                    continue

                startd_ads[machine] = slot_ad

        except IOError:
            logging.exception(f"Error while getting list of Startds from Collector {host}")
            continue

    startd_ads = list(startd_ads.values())
    random.shuffle(startd_ads)

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
        fmts = debug2fmt(args.debug_levels)
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


def collect_process_metadata():
    """
    Return a dictionary with:
    - hostname
    - username
    - current time
    - HTCondor version and platform
    """
    result = {}
    result["condor_adstash_hostname"] = socket.gethostname()
    result["condor_adstash_username"] = pwd.getpwuid(os.geteuid()).pw_name
    result["condor_adstash_runtime"] = int(time.time())
    result["condor_adstash_version"] = htcondor.version()
    result["condor_adstash_platform"] = htcondor.platform()
    return result


def get_host_port(host="localhost", port=9200):
    if ":" in host:
        try:
            (host, port) = host.split(":")
        except ValueError:
            raise ValueError(f"Invalid hostname: {host}")
    try:
        port = int(port)
    except ValueError:
        raise ValueError(f"Invalid port: {port}")
    return (host, port)


def atomic_write(data, filepath):
    filepath = str(filepath)
    tmpfile = tempfile.NamedTemporaryFile(delete=False, dir=os.path.dirname(filepath))
    with open(tmpfile.name, "w") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmpfile.name, filepath)
    if os.path.exists(tmpfile.name):
        try:
            os.unlink(tmpfile.name)
        except Exception:
            pass
