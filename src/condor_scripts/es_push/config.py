import sys
import os
import logging
import argparse
import multiprocessing
import re

import htcondor

from pathlib import Path


def get_default_config():

    defaults = {
        "checkpoint_file": Path(htcondor.param.get("LOG", Path.cwd())) / "es_push_checkpoint.json",
        "log_file": Path(htcondor.param.get("LOG", Path.cwd())) / "es_push.log",
        "log_level": "WARNING",
        "threads": 1,
        "collectors": htcondor.param.get("CONDOR_HOST"),
        "schedd_history": False,
        "startd_history": False,
        "schedd_history_max_ads": 10000,
        "startd_history_max_ads": 10000,
        "schedd_history_timeout": 2 * 60,
        "startd_history_timeout": 2 * 60,
        "es_host": "localhost:9200",
        "es_use_https": False,
        "es_timeout": 2 * 60,
        "es_bunch_size": 250,
        "es_index_name": "htcondor_000001",
    }
    return defaults


def get_htcondor_config():

    htcondor.reload_config()
    p = htcondor.param
    conf = {
        "checkpoint_file": p.get("ES_PUSH_CHECKPOINT_FILE"),
        "log_file": p.get("ES_PUSH_LOG"),
        "debug_levels": p.get("ES_PUSH_DEBUG"),
        "threads": p.get("ES_PUSH_NUM_THREADS"),
        "collectors": p.get("ES_PUSH_READ_POOLS"),
        "schedds": p.get("ES_PUSH_READ_SCHEDDS"),
        "startds": p.get("ES_PUSH_READ_STARTDS"),
        "schedd_history": p.get("ES_PUSH_SCHEDD_HISTORY"),
        "startd_history": p.get("ES_PUSH_STARTD_HISTORY"),
        "schedd_history_max_ads": p.get("ES_PUSH_SCHEDD_HISTORY_MAX_ADS"),
        "startd_history_max_ads": p.get("ES_PUSH_STARTD_HISTORY_MAX_ADS"),
        "schedd_history_timeout": p.get("ES_PUSH_SCHEDD_HISTORY_TIMEOUT"),
        "startd_history_timeout": p.get("ES_PUSH_STARTD_HISTORY_TIMEOUT"),
        "es_host": p.get("ES_PUSH_ES_HOST"),
        "es_username": p.get("ES_PUSH_ES_USERNAME"),
        "es_password_file": p.get("ES_PUSH_ES_PASSWORD_FILE"),
        "es_use_https": p.get("ES_PUSH_ES_USE_HTTPS"),
        "es_timeout": p.get("ES_PUSH_ES_TIMEOUT"),
        "es_bunch_size": p.get("ES_PUSH_ES_BUNCH_SIZE"),
        "es_index_name": p.get("ES_PUSH_ES_INDEX_NAME"),
    }

    # Convert debug level
    if conf.get("debug_levels") is not None:
        conf["log_level"] = debug2level(conf["debug_levels"])

    # Grab password from password file
    if conf.get("es_password_file") is not None:
        passwd = Path(conf["es_password_file"])
        try:
            with passwd.open() as f:
                conf["es_password"] = str(f.read(4096)).split("\n")[0]
            if conf["es_password"] == "":
                logging.warning(f"Got empty string from password file {passwd}")
        except Exception:
            logging.exception(
                f"Fatal error while trying to read password file {passwd}"
            )

    # For schedds and startds, "*" is shorthand for all (which is the default)
    for conf_key in ["schedds", "startds"]:
        if conf.get(conf_key) == "*":
            del conf[conf_key]

    # remove None values
    conf = {k: v for k, v in conf.items() if v is not None}

    # normalize values
    conf = normalize_config_types(conf)

    return conf


def get_environment_config():

    env = os.environ
    conf = {
        "checkpoint_file": env.get("ES_PUSH_CHECKPOINT_FILE"),
        "log_file": env.get("ES_PUSH_LOG"),
        "log_level": env.get("ES_PUSH_LOG_LEVEL"),
        "threads": env.get("ES_PUSH_NUM_THREADS"),
        "collectors": env.get("ES_PUSH_READ_POOLS"),
        "schedds": env.get("ES_PUSH_READ_SCHEDDS"),
        "startds": env.get("ES_PUSH_READ_STARTDS"),
        "schedd_history": env.get("ES_PUSH_SCHEDD_HISTORY"),
        "startd_history": env.get("ES_PUSH_STARTD_HISTORY"),
        "schedd_history_max_ads": env.get("ES_PUSH_SCHEDD_HISTORY_MAX_ADS"),
        "startd_history_max_ads": env.get("ES_PUSH_STARTD_HISTORY_MAX_ADS"),
        "schedd_history_timeout": env.get("ES_PUSH_SCHEDD_HISTORY_TIMEOUT"),
        "startd_history_timeout": env.get("ES_PUSH_STARTD_HISTORY_TIMEOUT"),
        "es_host": env.get("ES_PUSH_ES_HOST"),
        "es_username": env.get("ES_PUSH_ES_USERNAME"),
        "es_password": env.get("ES_PUSH_ES_PASSWORD"),
        "es_use_https": env.get("ES_PUSH_ES_USE_HTTPS"),
        "es_timeout": env.get("ES_PUSH_ES_TIMEOUT"),
        "es_bunch_size": env.get("ES_PUSH_ES_BUNCH_SIZE"),
        "es_index_name": env.get("ES_PUSH_ES_INDEX_NAME"),
    }

    # remove None values
    conf = {k: v for k, v in conf.items() if v is not None}

    # normalize values
    conf = normalize_config_types(conf)

    return conf


def debug2level(debug):

    level = "WARNING"
    debug_levels = set(re.split("[\s,]+", debug))
    if ("D_FULLDEBUG" in debug_levels) or ("D_ALL" in debug_levels):
        level = "INFO"
    if ("D_FULLDEBUG:2" in debug_levels) or ("D_ALL:2" in debug_levels):
        level = "DEBUG"
    return level


def debug2fmt(debug=None):

    fmt_list = ["%(asctime)s", "%(message)s"]
    datefmt = "%m/%d/%y %H:%M:%S"

    if debug is None:
        fmt = " ".join(fmt_list)
        return {"fmt": fmt, "datefmt": datefmt}

    debug_levels = set(re.split("[\s,]+", debug))

    if "D_CATEGORY" in debug_levels:
        fmt_list.insert(1, "(%(levelname)s)")

    if "D_PID" in debug_levels:
        fmt_list.insert(1, "(pid:%(process)d)")

    if "D_TIMESTAMP" in debug_levels:
        if "D_SUB_SECOND" in debug_levels:
            fmt_list[0] = "%(created).3f"
        else:
            fmt_list[0] = "%(created)d"

    elif "D_SUB_SECOND" in debug_levels:
        datefmt = "%m/%d/%y %H:%M:%S"
        fmt_list[0] = "%(asctime)s.%(msecs)03d"

    fmt = " ".join(fmt_list)
    return {"fmt": fmt, "datefmt": datefmt}


def normalize_config_types(conf):

    integers = [
        "threads",
        "schedd_history_max_ads",
        "startd_history_max_ads",
        "schedd_history_timeout",
        "startd_history_timeout",
        "es_timeout",
        "es_bunch_size",
    ]
    bools = ["schedd_history", "startd_history", "es_use_https"]

    # integers
    for key in integers:
        if key in conf:
            try:
                conf[key] = int(conf[key])
            except ValueError:
                logging.error(
                    f"Config file entry for {key} must be an integer (got: {conf[key]})"
                )
                sys.exit(1)

    # booleans
    for key in bools:
        if key in conf:
            try:
                if str(conf[key]).lower() in ["true", "yes", "1"]:
                    conf[key] = True
                elif str(conf[key]).lower() in ["false", "no", "0"]:
                    conf[key] = False
                else:
                    raise ValueError(f"Value for {key} must be of boolean type")
            except ValueError:
                logging.error(
                    f"Config file entry {key} must be a boolean (got: {conf[key]}"
                )
                sys.exit(1)

    return conf


def get_config(argv=None):

    if argv is None:
        argv = sys.argv[1:]

    # Start with defaults
    defaults = get_default_config()

    # Overwrite default values from condor_config
    defaults.update(get_htcondor_config())

    # Overwrite default values from environment
    defaults.update(get_environment_config())

    parser = argparse.ArgumentParser()
    parser.set_defaults(**defaults)

    # Configurable args
    parser.add_argument(
        "--checkpoint_file",
        help=(
            "Location of checkpoint file (will be created if missing) "
            "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--log_file", help=("Log file location " "[default: %(default)s]"),
    )
    parser.add_argument(
        "--log_level",
        help=(
            "Log level (CRITICAL/ERROR/WARNING/INFO/DEBUG) " "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--threads",
        type=int,
        help=("Number of parallel threads for querying " "[default: %(default)d]"),
    )
    parser.add_argument(
        "--collectors",
        help=(
            "Comma-separated list of Collector addresses used to locate Schedds/Startds"
            "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--schedds",
        help=(
            "Comma-separated list of Schedd names to process "
            "[default is to process all Schedds located by Collectors]"
        ),
    )
    parser.add_argument(
        "--startds",
        help=(
            "Comma-separated list of Startd machines to process "
            "[default is to process all Startds located by Collectors]"
        ),
    )
    parser.add_argument(
        "--schedd_history",
        action="store_true",
        help=("Poll Schedd histories " "[default: %(default)s]"),
    )
    parser.add_argument(
        "--startd_history",
        action="store_true",
        help=("Poll Startd histories " "[default: %(default)s]"),
    )
    parser.add_argument(
        "--schedd_history_max_ads",
        type=int,
        help=(
            "Abort after reading this many Schedd history entries "
            "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--startd_history_max_ads",
        type=int,
        help=(
            "Abort after reading many Startd history entries " "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--schedd_history_timeout",
        type=int,
        help=(
            "Abort polling Schedd history after this many seconds "
            "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--startd_history_timeout",
        type=int,
        help=(
            "Abort polling Startd history after this many seconds "
            "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--es_host",
        help=(
            "Host of the Elasticsearch instance to be used " "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--es_username", help=("Username to use with Elasticsearch instance"),
    )
    parser.add_argument(
        "--es_password", help=argparse.SUPPRESS,  # don't encourage use on command-line
    )
    parser.add_argument(
        "--es_use_https",
        action="store_true",
        help=("Use HTTPS when connecting to Elasticsearch " "[default: %(default)s]"),
    )
    parser.add_argument(
        "--es_timeout",
        type=int,
        help=(
            "Max seconds to wait to connect to Elasticsearch " "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--es_bunch_size",
        type=int,
        help=("Send docs to ES in bunches of this number " "[default: %(default)s]"),
    )
    parser.add_argument(
        "--es_index_name", help=("Elasticsearch index name " "[default: %(default)s]"),
    )
    parser.add_argument(
        "--read_only", action="store_true", help="Only read the info, don't submit it.",
    )
    parser.add_argument(
        "--dry_run",
        action="store_true",
        help=(
            "Don't even read info, just pretend to. "
            "(Still query Collectors for Schedds and Startds though.)"
        ),
    )

    return parser.parse_args(argv)
