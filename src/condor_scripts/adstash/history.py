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

import json
import time
import logging
import datetime
import traceback
import multiprocessing

import htcondor

if args.es_use_opensearch:
    import opensearchpy as elasticsearch
else:
    import elasticsearch

from . import elastic, utils, convert

_LAUNCH_TIME = int(time.time())

# Lower the volume on third-party packages
for k in logging.Logger.manager.loggerDict:
    logging.getLogger(k).setLevel(logging.WARNING)


def process_custom_ads(start_time, job_ad_file, args, metadata=None):
    """
    Given an iterator of ads, process its entire set of history
    """
    logging.info(f"Start processing the adfile: {job_ad_file}")

    my_start = time.time()
    buffered_ads = {}
    count = 0
    total_upload = 0
    if not args.read_only:
        es = elastic.get_server_handle(args)
    try:
        for job_ad in utils.parse_history_ad_file(job_ad_file):
            metadata = metadata or {}
            metadata["condor_history_source"] = "custom"
            metadata["condor_history_runtime"] = int(my_start)
            metadata["condor_history_host_version"] = job_ad.get("CondorVersion", "UNKNOWN")
            metadata["condor_history_host_platform"] = job_ad.get(
                "CondorPlatform", "UNKNOWN"
            )
            metadata["condor_history_host_machine"] = job_ad.get("Machine", "UNKNOWN")
            metadata["condor_history_host_name"] = job_ad.get("Name", "UNKNOWN")

            try:
                dict_ad = convert.to_json(job_ad, return_dict=True)
            except Exception as e:
                message = f"Failure when converting document in {job_ad_file}: {e}"
                exc = traceback.format_exc()
                message += f"\n{exc}"
                logging.warning(message)
                continue

            idx = elastic.get_index(args.es_index_name)
            ad_list = buffered_ads.setdefault(idx, [])
            ad_list.append((convert.unique_doc_id(dict_ad), dict_ad))

            if len(ad_list) == args.es_bunch_size:
                st = time.time()
                if not args.read_only:
                    elastic.post_ads(
                        es.handle, idx, ad_list, metadata=metadata
                    )
                logging.debug(
                    f"Posting {len(ad_list)} ads from {job_ad_file}"
                )
                total_upload += time.time() - st
                buffered_ads[idx] = []

            count += 1

    except Exception:
        message = f"Failure when processing history from {job_ad_file}"
        logging.exception(message)
        return

    # Post the remaining ads
    for idx, ad_list in list(buffered_ads.items()):
        if ad_list:
            logging.debug(
                f"Posting remaining {len(ad_list)} ads from {job_ad_file}"
            )
            if not args.read_only:
                elastic.post_ads(es.handle, idx, ad_list, metadata=metadata)

    total_time = (time.time() - my_start) / 60.0
    total_upload /= 60.0
    logging.info(
        f"{job_ad_file} history: response count: {count}; upload time {total_upload:.2f} min"
    )

    return


def process_schedd(start_time, since, checkpoint_queue, schedd_ad, args, metadata=None):
    """
    Given a schedd, process its entire set of history since last checkpoint.
    """
    logging.info(f"Start processing the scheduler: {schedd_ad['Name']}")

    my_start = time.time()
    metadata = metadata or {}
    metadata["condor_history_source"] = "schedd"
    metadata["condor_history_runtime"] = int(my_start)
    metadata["condor_history_host_version"] = schedd_ad.get("CondorVersion", "UNKNOWN")
    metadata["condor_history_host_platform"] = schedd_ad.get(
        "CondorPlatform", "UNKNOWN"
    )
    metadata["condor_history_host_machine"] = schedd_ad.get("Machine", "UNKNOWN")
    metadata["condor_history_host_name"] = schedd_ad.get("Name", "UNKNOWN")
    last_completion = since["EnteredCurrentStatus"]
    since_str = (
        f"""(ClusterId == {since['ClusterId']}) && (ProcId == {since['ProcId']})"""
    )

    schedd = htcondor.Schedd(schedd_ad)
    max_ads = args.schedd_history_max_ads  # specify number of history entries to read 
    if max_ads > 10000:
        logging.debug(f"Please note that the maximum number of queries per scheduler is also limited by the scheduler's config (HISTORY_HELPER_MAX_HISTORY).")
        logging.info(f"Note that a too large number of schedd_history_max_ads can cause condor_adstash to break!")
    logging.info(f"Querying {schedd_ad['Name']} for history ads since: {since_str}")
    buffered_ads = {}
    count = 0
    total_upload = 0
    timed_out = False
    if not args.read_only:
        es = elastic.get_server_handle(args)
    try:
        if not args.dry_run:
            history_iter = schedd.history(
                constraint="true",
                projection=[],
                match=max_ads,  # default=10000
                since=since_str
            )
        else:
            history_iter = []

        for job_ad in history_iter:
            try:
                dict_ad = convert.to_json(job_ad, return_dict=True)
            except Exception as e:
                message = f"Failure when converting document on {schedd_ad['Name']} history: {e}"
                exc = traceback.format_exc()
                message += f"\n{exc}"
                logging.warning(message)
                continue

            idx = elastic.get_index(args.es_index_name)
            ad_list = buffered_ads.setdefault(idx, [])
            ad_list.append((convert.unique_doc_id(dict_ad), dict_ad))

            if len(ad_list) == args.es_bunch_size:
                st = time.time()
                if not args.read_only:
                    elastic.post_ads(
                        es.handle, idx, ad_list, metadata=metadata
                    )
                logging.debug(
                    f"Posting {len(ad_list)} ads from {schedd_ad['Name']} (process_schedd)"
                )
                total_upload += time.time() - st
                buffered_ads[idx] = []

            count += 1

            # Find the most recent job and use its job id as the since parameter
            job_completion = convert.record_time(job_ad, fallback_to_launch=False)
            if job_completion > last_completion:
                last_completion = job_completion
                since = {
                    "ClusterId": job_ad.get("ClusterId", 0),
                    "ProcId": job_ad.get("ProcId", 0),
                    "EnteredCurrentStatus": job_completion,
                }

            if utils.time_remaining(my_start, args.schedd_history_timeout) <= 0:
                message = f"History crawler on {schedd_ad['Name']} has been running for more than {args.schedd_history_timeout} seconds; pushing last ads and exiting."
                logging.error(message)
                timed_out = True
                break

    except RuntimeError:
        message = f"Failed to query schedd {schedd_ad['Name']} for job history"
        logging.exception(message)
        return since

    except Exception:
        message = f"Failure when processing schedd history query on {schedd_ad['Name']}"
        logging.exception(message)
        return since

    # Post the remaining ads
    for idx, ad_list in list(buffered_ads.items()):
        if ad_list:
            logging.debug(
                f"Posting remaining {len(ad_list)} ads from {schedd_ad['Name']} (process_schedd)"
            )
            if not args.read_only:
                elastic.post_ads(es.handle, idx, ad_list, metadata=metadata)

    total_time = (time.time() - my_start) / 60.0
    total_upload /= 60.0
    last_formatted = datetime.datetime.fromtimestamp(last_completion).strftime(
        "%Y-%m-%d %H:%M:%S"
    )
    logging.info(
        f"Schedd {schedd_ad['Name']} history: response count: {count}; last job {last_formatted}; query time {total_time - total_upload:.2f} min; upload time {total_upload:.2f} min"
    )
    if count >= max_ads:
        logging.warning(
            f"Max ads ({max_ads}) was reached "
            f"for {schedd_ad['Name']}, older history may be missing!"
        )

    # If we got to this point without a timeout, all these jobs have
    # been processed and uploaded, so we can update the checkpoint
    if not timed_out:
        checkpoint_queue.put((schedd_ad["Name"], since))

    return since


def process_startd(start_time, since, checkpoint_queue, startd_ad, args, metadata=None):
    """
    Given a startd, process its entire set of history since last checkpoint.
    """
    my_start = time.time()
    metadata = metadata or {}
    metadata["condor_history_source"] = "startd"
    metadata["condor_history_runtime"] = int(my_start)
    metadata["condor_history_host_version"] = startd_ad.get("CondorVersion", "UNKNOWN")
    metadata["condor_history_host_platform"] = startd_ad.get(
        "CondorPlatform", "UNKNOWN"
    )
    metadata["condor_history_host_machine"] = startd_ad.get("Machine", "UNKNOWN")
    metadata["condor_history_host_name"] = startd_ad.get("Name", "UNKNOWN")
    last_completion = since["EnteredCurrentStatus"]
    since_str = f"""(GlobalJobId == "{since['GlobalJobId']}") && (EnteredCurrentStatus == {since['EnteredCurrentStatus']})"""

    max_ads = args.startd_history_max_ads  # specify number of history entries to read 
    if max_ads > 10000:
        logging.debug(f"Please note that the maximum number of queries per scheduler is also limited by the scheduler's config (HISTORY_HELPER_MAX_HISTORY).")
        logging.info(f"Note that a too large number of startd_history_max_ads can cause condor_adstash to break!")

    startd = htcondor.Startd(startd_ad)
    logging.info(f"Querying {startd_ad['Machine']} for history since: {since_str}")
    buffered_ads = {}
    count = 0
    total_upload = 0
    timed_out = False
    if not args.read_only:
        es = elastic.get_server_handle(args)
    try:
        if not args.dry_run:
            history_iter = startd.history(
                requirements="true",
                projection=[],
                match=max_ads,  # default=10000
                since=since_str
            )
        else:
            history_iter = []

        for job_ad in history_iter:
            try:
                dict_ad = convert.to_json(job_ad, return_dict=True)
            except Exception as e:
                message = f"Failure when converting document on {startd_ad['Machine']} history: {e}"
                exc = traceback.format_exc()
                message += f"\n{exc}"
                logging.warning(message)
                continue

            idx = elastic.get_index(args.es_index_name)
            ad_list = buffered_ads.setdefault(idx, [])
            ad_list.append((convert.unique_doc_id(dict_ad), dict_ad))

            if len(ad_list) == args.es_bunch_size:
                st = time.time()
                if not args.read_only:
                    elastic.post_ads(
                        es.handle, idx, ad_list, metadata=metadata
                    )
                logging.debug(
                    f"Posting {len(ad_list)} ads from {startd_ad['Machine']} (process_startd)"
                )
                total_upload += time.time() - st
                buffered_ads[idx] = []

            count += 1

            job_completion = job_ad.get("EnteredCurrentStatus")
            if job_completion > last_completion:
                last_completion = job_completion
                since = {
                    "GlobalJobId": job_ad.get("GlobalJobId"),
                    "EnteredCurrentStatus": job_ad.get("EnteredCurrentStatus"),
                }

            if utils.time_remaining(my_start, args.startd_history_timeout) <= 0:
                message = f"History crawler on {startd_ad['Machine']} has been running for more than {args.schedd_history_timeout} seconds; pushing last ads and exiting."
                logging.error(message)
                timed_out = True
                break

    except RuntimeError:
        message = f"Failed to query startd {startd_ad['Machine']} for job history"
        logging.exception(message)
        return since

    except Exception:
        message = f"Failure when processing startd history query on {startd_ad['Machine']}"
        logging.exception(message)
        return since

    # Post the remaining ads
    for idx, ad_list in list(buffered_ads.items()):
        if ad_list:
            logging.debug(
                f"Posting remaining {len(ad_list)} ads from {startd_ad['Machine']} (process_startd)"
            )
            if not args.read_only:
                elastic.post_ads(es.handle, idx, ad_list, metadata=metadata)

    total_time = (time.time() - my_start) / 60.0
    total_upload /= 60.0
    last_formatted = datetime.datetime.fromtimestamp(last_completion).strftime(
        "%Y-%m-%d %H:%M:%S"
    )
    logging.info(
        f"Startd {startd_ad['Machine']} history: response count: {count}; last job {last_formatted}; query time {total_time - total_upload:.2f} min; upload time {total_upload:.2f} min"
    )
    if count >= max_ads:
        logging.warning(
            f"Max ads ({max_ads}) was reached "
            f"for {startd_ad['Machine']}, some history may be missing!"
        )

    # If we got to this point without a timeout, all these jobs have
    # been processed and uploaded, so we can update the checkpoint
    if not timed_out:
        checkpoint_queue.put((startd_ad["Machine"], since))

    return since


def load_checkpoint(checkpoint_file):
    try:
        with open(checkpoint_file, "r") as fd:
            checkpoint = json.load(fd)
    except IOError:
        checkpoint = {}

    return checkpoint


def update_checkpoint(checkpoint_file, name, since):
    checkpoint = load_checkpoint(checkpoint_file)

    checkpoint[name] = since

    with open(checkpoint_file, "w") as fd:
        json.dump(checkpoint, fd, indent=4)


def process_histories(
    schedd_ads=[], startd_ads=[], starttime=None, pool=None, args=None, metadata=None
):
    """
    Process history files for each schedd listed in a given
    multiprocessing pool
    """
    checkpoint = load_checkpoint(args.checkpoint_file)
    timeout = 2 * 60

    futures = []
    metadata = metadata or {}
    metadata["es_push_source"] = "condor_history"

    manager = multiprocessing.Manager()
    checkpoint_queue = manager.Queue()

    if len(schedd_ads) > 0:
        timeout = args.schedd_history_timeout
        for schedd_ad in schedd_ads:
            name = schedd_ad["Name"]

            # Check for last completion time
            # If there was no previous completion, get full history
            since = checkpoint.get(
                name, {"ClusterId": 0, "ProcId": 0, "EnteredCurrentStatus": 0}
            )

            future = pool.apply_async(
                process_schedd,
                (starttime, since, checkpoint_queue, schedd_ad, args, metadata),
            )
            futures.append((name, future))

    if len(startd_ads) > 0:
        timeout = args.startd_history_timeout
        for startd_ad in startd_ads:
            machine = startd_ad["Machine"]

            # Check for last completion time ("since")
            since = checkpoint.get(
                machine, {"GlobalJobId": "Unknown", "EnteredCurrentStatus": 0}
            )

            future = pool.apply_async(
                process_startd,
                (starttime, since, checkpoint_queue, startd_ad, args, metadata),
            )
            futures.append((machine, future))

    def _chkp_updater():
        while True:
            try:
                job = checkpoint_queue.get()
                if job is None:  # Swallow poison pill
                    break
            except EOFError as error:
                logging.warning(
                    "EOFError - Nothing to consume left in the queue %s", error
                )
                break
            update_checkpoint(args.checkpoint_file, *job)

    chkp_updater = multiprocessing.Process(target=_chkp_updater)
    chkp_updater.start()

    # Report processes if they timeout or error
    for name, future in futures:
        try:
            future.get(timeout)
        except multiprocessing.TimeoutError:
            # This implies that the checkpoint hasn't been updated
            message = f"Daemon {name} history timed out; ignoring progress."
            logging.warning(message)
        except elasticsearch.exceptions.TransportError:
            message = f"Transport error while sending history data of {name}; ignoring progress."
            logging.exception(message)
        except Exception:
            message = f"Error getting progress from {name}."
            logging.exception(message)

    checkpoint_queue.put(None)  # Send a poison pill
    chkp_updater.join()

    logging.warning(
        f"Processing time for history: {((time.time() - starttime) / 60.0)} mins"
    )
