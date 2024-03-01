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
import re
import sys
import logging
import argparse

import htcondor

from pathlib import Path

from adstash.interfaces.registry import ADSTASH_INTERFACES


def get_default_config(name="ADSTASH"):

    defaults = {
        "process_name": name,
        "sample_interval": 20*60,
        "checkpoint_file": Path(htcondor.param.get("LOG", Path.cwd())) / "adstash_checkpoint.json",
        "log_file": Path(htcondor.param.get("LOG", Path.cwd())) / "adstash.log",
        "log_level": "WARNING",
        "threads": 1,
        "collectors": htcondor.param.get("CONDOR_HOST"),
        "read_schedd_history": False,
        "read_startd_history": False,
        "read_ad_file": None,
        "schedd_history_max_ads": 10000,
        "startd_history_max_ads": 10000,
        "schedd_history_timeout": 2 * 60,
        "startd_history_timeout": 2 * 60,
        "interface": "null",
        "se_host": "localhost:9200",
        "se_use_https": False,
        "se_timeout": 2 * 60,
        "se_bunch_size": 250,
        "se_index_name": "htcondor-000001",
        "se_log_mappings": True,
        "json_dir": Path.cwd(),
    }
    return defaults


def get_htcondor_config(name="ADSTASH"):

    htcondor.reload_config()
    p = htcondor.param
    conf = {
        "sample_interval": p.get(f"{name}_SAMPLE_INTERVAL"),
        "checkpoint_file": p.get(f"{name}_CHECKPOINT_FILE"),
        "log_file": p.get(f"{name}_LOG"),
        "debug_levels": p.get(f"{name}_DEBUG"),
        "threads": p.get(f"{name}_NUM_THREADS"),
        "collectors": p.get(f"{name}_READ_POOLS"),
        "schedds": p.get(f"{name}_READ_SCHEDDS"),
        "startds": p.get(f"{name}_READ_STARTDS"),
        "read_schedd_history": p.get(f"{name}_SCHEDD_HISTORY"),
        "read_startd_history": p.get(f"{name}_STARTD_HISTORY"),
        "read_ad_file": p.get(f"{name}_AD_FILE"),
        "schedd_history_max_ads": p.get(f"{name}_SCHEDD_HISTORY_MAX_ADS"),
        "startd_history_max_ads": p.get(f"{name}_STARTD_HISTORY_MAX_ADS"),
        "schedd_history_timeout": p.get(f"{name}_SCHEDD_HISTORY_TIMEOUT"),
        "startd_history_timeout": p.get(f"{name}_STARTD_HISTORY_TIMEOUT"),
        "interface": p.get(f"{name}_INTERFACE"),
        "se_host": p.get(f"{name}_ES_HOST", p.get(f"{name}_SE_HOST")),
        "se_url_prefix": p.get(f"{name}_SE_URL_PREFIX"),
        "se_username": p.get(f"{name}_ES_USERNAME", p.get(f"{name}_SE_USERNAME")),
        "se_password_file": p.get(f"{name}_ES_PASSWORD_FILE", p.get(f"{name}_SE_PASSWORD_FILE")),
        "se_use_https": p.get(f"{name}_ES_USE_HTTPS", p.get(f"{name}_SE_USE_HTTPS")),
        "se_timeout": p.get(f"{name}_ES_TIMEOUT", p.get(f"{name}_SE_TIMEOUT")),
        "se_bunch_size": p.get(f"{name}_ES_BUNCH_SIZE", p.get(f"{name}_SE_BUNCH_SIZE")),
        "se_index_name": p.get(f"{name}_ES_INDEX_NAME", p.get(f"{name}_SE_INDEX_NAME")),
        "se_ca_certs": p.get(f"{name}_ES_CA_CERTS", p.get(f"{name}_SE_CA_CERTS")),
        "se_log_mappings": p.get(f"{name}_SE_LOG_MAPPINGS"),
        "json_dir": p.get(f"{name}_JSON_DIR"),
    }

    # Convert debug level
    if conf.get("debug_levels") is not None and conf.get("debug_levels") != "":
        conf["log_level"] = debug2level(conf["debug_levels"])

    # Grab password from password file
    if conf.get("se_password_file") is not None and conf.get("se_password_file") != "":
        passwd = Path(conf["se_password_file"])
        try:
            with passwd.open() as f:
                conf["se_password"] = str(f.read(4096)).split("\n")[0]
            if conf["se_password"] == "":
                logging.error(f"Got empty string from password file {passwd}")
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


def get_environment_config(name="ADSTASH"):

    env = os.environ
    conf = {
        "standalone": env.get(f"{name}_STANDALONE"),
        "sample_interval": env.get(f"{name}_SAMPLE_INTERVAL"),
        "checkpoint_file": env.get(f"{name}_CHECKPOINT_FILE"),
        "log_file": env.get(f"{name}_LOG"),
        "log_level": env.get(f"{name}_LOG_LEVEL"),
        "threads": env.get(f"{name}_NUM_THREADS"),
        "collectors": env.get(f"{name}_READ_POOLS"),
        "schedds": env.get(f"{name}_READ_SCHEDDS"),
        "startds": env.get(f"{name}_READ_STARTDS"),
        "read_schedd_history": env.get(f"{name}_SCHEDD_HISTORY"),
        "read_startd_history": env.get(f"{name}_STARTD_HISTORY"),
        "read_ad_file": env.get(f"{name}_AD_FILE"),
        "schedd_history_max_ads": env.get(f"{name}_SCHEDD_HISTORY_MAX_ADS"),
        "startd_history_max_ads": env.get(f"{name}_STARTD_HISTORY_MAX_ADS"),
        "schedd_history_timeout": env.get(f"{name}_SCHEDD_HISTORY_TIMEOUT"),
        "startd_history_timeout": env.get(f"{name}_STARTD_HISTORY_TIMEOUT"),
        "interface": env.get(f"{name}_INTERFACE"),
        "se_host": env.get(f"{name}_SE_HOST", env.get(f"{name}_ES_HOST")),
        "se_url_prefix": env.get(f"{name}_SE_URL_PREFIX"),
        "se_username": env.get(f"{name}_SE_USERNAME", env.get(f"{name}_ES_USERNAME")),
        "se_password": env.get(f"{name}_SE_PASSWORD", env.get(f"{name}_ES_PASSWORD")),
        "se_use_https": env.get(f"{name}_SE_USE_HTTPS", env.get(f"{name}_ES_USE_HTTPS")),
        "se_timeout": env.get(f"{name}_SE_TIMEOUT", env.get(f"{name}_ES_TIMEOUT")),
        "se_bunch_size": env.get(f"{name}_SE_BUNCH_SIZE", env.get(f"{name}_ES_BUNCH_SIZE")),
        "se_index_name": env.get(f"{name}_SE_INDEX_NAME", env.get(f"{name}_ES_INDEX_NAME")),
        "se_ca_certs": env.get(f"{name}_SE_CA_CERTS", env.get(f"{name}_CA_CERTS")),
        "se_log_mappings": env.get(f"{name}_SE_LOG_MAPPINGS"),
        "json_dir": env.get(f"{name}_JSON_DIR"),
    }

    # remove None values
    conf = {k: v for k, v in conf.items() if v is not None}

    # normalize values
    conf = normalize_config_types(conf)

    return conf


def debug2level(debug):

    level = "WARNING"
    debug_levels = set(re.split(r"[\s,]+", debug))
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

    debug_levels = set(re.split(r"[\s,]+", debug))

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
        "sample_interval",
        "threads",
        "schedd_history_max_ads",
        "startd_history_max_ads",
        "schedd_history_timeout",
        "startd_history_timeout",
        "es_timeout",
        "es_bunch_size",
    ]
    bools = [
        "standalone",
        "read_schedd_history",
        "read_startd_history",
        "to_elasticsearch",
        "to_json",
        "se_use_https",
        "se_log_mappings",
    ]

    # interfaces
    if ("interface" in conf) and (conf["interface"].lower() not in ADSTASH_INTERFACES):
        logging.error(f"Unknown interface selected: {conf['interface']}.")
        logging.error(f"Choose from: {', '.join(ADSTASH_INTERFACES)}.")
        sys.exit(1)

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

    # Get this process's name first
    name_parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        add_help=False
        )
    name_parser.add_argument(
        "--process_name",
        default="ADSTASH",
        metavar="NAME",
        help=(
            "This adstash process's name (used for config/env variables)"
            "[default: %(default)s]"
        ),
    )
    args, remaining_argv = name_parser.parse_known_args()
    process_name = args.process_name

    # Start with defaults
    defaults = get_default_config(name=process_name)

    # Overwrite default values from condor_config
    defaults.update(get_htcondor_config(name=process_name))

    # Overwrite default values from environment
    defaults.update(get_environment_config(name=process_name))

    # Set up main arg parser
    parser = argparse.ArgumentParser(parents=[name_parser])
    parser.set_defaults(**defaults)

    # Configurable args
    parser.add_argument(
        "--standalone",
        action="store_true",
        help=(
            "Run %(prog)s in standalone mode "
            "(run once, do not contact condor_master)"
        ),
    )
    parser.add_argument(
        "--sample_interval",
        type=int,
        metavar="SECONDS",
        help=(
            "Number of seconds between polling the list(s) of daemons "
            "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--checkpoint_file",
        type=Path,
        metavar="PATH",
        help=(
            "Location of checkpoint file (will be created if missing) "
            "[default: %(default)s]"
        ),
    )
    parser.add_argument(
        "--log_file",
        type=Path,
        metavar="PATH",
        help="Log file location [default: %(default)s]",
    )
    parser.add_argument(
        "--log_level",
        metavar="LEVEL",
        help="Log level (CRITICAL/ERROR/WARNING/INFO/DEBUG) [default: %(default)s]",
    )
    parser.add_argument(
        "--threads",
        type=int,
        help=("Number of parallel threads for querying [default: %(default)d]"),
    )
    parser.add_argument(
        "--interface",
        type=str.lower,
        choices=ADSTASH_INTERFACES,
        help="Push ads via the chosen interface [default: %(default)s]",
    )
    parser.add_argument(
        "--read_only",
        action="store_true",
        help="Deprecated (use --interface null). Only read the info, don't submit it.",
    )
    parser.add_argument(
        "--dry_run",
        action="store_true",
        help="Deprecated (use --interface null). Only read the info, don't submit it.",
    )

    source_group = parser.add_argument_group(
        title="ClassAd source options",
        description="Choose a source of HTCondor ClassAd information."
    )
    source_group.add_argument(
        "--schedd_history",
        action="store_true",
        dest="read_schedd_history",
        help=(
            "Poll Schedd histories "
            "[default: %(default)s]"
        ),
    )
    source_group.add_argument(
        "--startd_history",
        action="store_true",
        dest="read_startd_history",
        help=(
            "Poll Startd histories "
            "[default: %(default)s]"
        ),
    )
    source_group.add_argument(
        "--ad_file",
        type=Path,
        metavar="PATH",
        dest="read_ad_file",
        help=(
            "Load Job ClassAds from this file instead of querying daemons. "
            "(Ignores --schedd_history and --startd_history.)"
        ),
    )

    history_group = parser.add_argument_group("Options for HTCondor daemon (Schedd, Startd, etc.) history sources")
    history_group.add_argument(
        "--collectors",
        help=(
            "Comma-separated list of Collector addresses used to locate Schedds/Startds"
            "[default: %(default)s]"
        ),
    )
    history_group.add_argument(
        "--schedds",
        help=(
            "Comma-separated list of Schedd names to process "
            "[default is to process all Schedds located by Collectors]"
        ),
    )
    history_group.add_argument(
        "--startds",
        help=(
            "Comma-separated list of Startd machines to process "
            "[default is to process all Startds located by Collectors]"
        ),
    )
    history_group.add_argument(
        "--schedd_history_max_ads",
        type=int,
        metavar="NUM_ADS",
        help=(
            "Abort after reading this many Schedd history entries "
            "[default: %(default)s]"
        ),
    )
    history_group.add_argument(
        "--startd_history_max_ads",
        type=int,
        metavar="NUM_ADS",
        help=(
            "Abort after reading this many Startd history entries "
            "[default: %(default)s]"
        ),
    )
    history_group.add_argument(
        "--schedd_history_timeout",
        type=int,
        metavar="SECONDS",
        help=(
            "Abort polling Schedd history after this many seconds "
            "[default: %(default)s]"
        ),
    )
    history_group.add_argument(
        "--startd_history_timeout",
        type=int,
        metavar="SECONDS",
        help=(
            "Abort polling Startd history after this many seconds "
            "[default: %(default)s]"
        ),
    )

    se_group = parser.add_argument_group(
        title = "Search engine (Elasticsearch, OpenSearch, etc.) interface options",
        description = "Note that --es_argname options are deprecated, please use --se_argname instead."
    )
    se_group.add_argument(
        "--se_host", "--es_host",
        dest="se_host",
        metavar="HOST[:PORT]",
        help=("Search engine host:port [default: %(default)s]"),
    )
    se_group.add_argument(
        "--se_url_prefix",
        metavar="PREFIX",
        help=("Search engine URL prefix"),
    )
    se_group.add_argument(
        "--se_username", "--es_username",
        dest="se_username",
        metavar="USERNAME",
        help=("Search engine username"),
    )
    se_group.add_argument(
        "--se_password", "--es_password",
        dest="se_password",
        help=argparse.SUPPRESS,  # don't encourage use on command-line
    )
    se_group.add_argument(
        "--se_use_https", "--es_use_https",
        dest="se_use_https",
        action="store_true",
        help=("Use HTTPS when connecting to search engine [default: %(default)s]"),
    )
    se_group.add_argument(
        "--se_timeout", "--es_timeout",
        dest="se_timeout",
        type=int,
        metavar="SECONDS",
        help=("Max time to wait for search engine queries [default: %(default)s]"),
    )
    se_group.add_argument(
        "--se_bunch_size", "--es_bunch_size",
        dest="se_bunch_size",
        type=int,
        metavar="NUM_DOCS",
        help=("Group ads in bunches of this size to send to search engine [default: %(default)s]"),
    )
    se_group.add_argument(
        "--se_index_name", "--es_index_name",
        dest="se_index_name",
        metavar="INDEX_NAME",
        help=("Push ads to this search engine index or alias [default: %(default)s]"),
    )
    se_group.add_argument(
        "--se_no_log_mappings",
        dest="se_log_mappings",
        action="store_false",
        help="Don't write a JSON file with mappings to the log directory",
    )
    se_group.add_argument(
        "--se_ca_certs", "--ca_certs",
        dest="se_ca_certs",
        type=Path,
        metavar="PATH",
        help=("Path to root certificate authority file (will use certifi's CA if not set)"),
    )

    json_group = parser.add_argument_group("JSON file interface options")
    json_group.add_argument(
        "--json_local",
        action="store_true",
        help="Deprecated (use --interface=jsonfile). Store only locally in json format",
    )
    json_group.add_argument(
        "--json_dir",
        type=Path,
        metavar="PATH",
        help="Directory to store JSON files, which are named by timestamp [defaults to current directory]",
    )

    # Parse args and add process name back to the list
    args = parser.parse_args(remaining_argv)
    args_dict = vars(args)
    args_dict['process_name'] = process_name

    # Check for deprecated args
    for arg in remaining_argv:
        if arg.startswith("--es_"):
            logging.warning(f"Use of --es_argname arguments is deprecated, use --se_argname instead.")
        if arg in ["--read_only", "--dry_run"]:
            logging.warning(f"Use of --{arg} is deprecated, use --interface=null instead.")
            args.interface = "null"
        if arg == "--json_local":
            logging.warning(f"Use of --json_local is deprecated, use --interface=jsonfile instead.")
            args.interface="jsonfile"

    return args
