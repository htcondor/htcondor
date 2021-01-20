#!/usr/bin/env python
"""
Script for pushing condor_history ads to Elasticsearch.
"""

import sys
import time
import logging
import multiprocessing

from es_push import history, utils, config


def main_driver(args):
    """
    Driver method for the push script.
    """
    starttime = time.time()

    # Get all the schedd ads
    if args.schedd_history:
        schedd_ads = []
        schedd_ads = utils.get_schedds(args)
        logging.info(f"There are {len(schedd_ads)} schedds to query")

    # Get all the startd ads
    if args.startd_history:
        startd_ads = []
        startd_ads = utils.get_startds(args)
        logging.info(f"There are {len(startd_ads)} startds to query.")

    # Process histories
    with multiprocessing.Pool(
        processes=args.threads, maxtasksperchild=1
    ) as pool:
        metadata = utils.collect_metadata()

        if args.schedd_history:
            history.process_histories(
                schedd_ads=schedd_ads,
                starttime=starttime,
                pool=pool,
                args=args,
                metadata=metadata,
            )

        if args.startd_history:
            history.process_histories(
                startd_ads=startd_ads,
                starttime=starttime,
                pool=pool,
                args=args,
                metadata=metadata,
            )

    logging.info(f"Total processing time: {((time.time() - starttime) / 60.0)} mins")

    return 0


def main():
    """
    Main method for the push script.
    """

    # get args
    args = config.get_config(sys.argv[1:])

    # dry_run implies read_only
    args.read_only = args.read_only or args.dry_run

    # set up logging
    utils.set_up_logging(args)

    logging.warning("="*30)
    logging.warning("Starting up Elasticsearch push script")
    logging.debug(f"Using args:\n{args}")
    main_driver(args)
    logging.warning("Stopping Elasticsearch push script")
    logging.warning("="*30)


if __name__ == "__main__":
    main()
