# Copyright 2022 HTCondor Team, Computer Sciences Department,
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

import time
import logging
import multiprocessing
import queue

from adstash.utils import get_schedds, get_startds, collect_process_metadata
from adstash.ad_sources.registry import ADSTASH_AD_SOURCE_REGISTRY
from adstash.interfaces.registry import ADSTASH_INTERFACE_REGISTRY


def _schedd_ckpt_updater(args, checkpoint_queue, ad_source):
    while True:
        try:
            checkpoint = checkpoint_queue.get(timeout=args.schedd_history_timeout)
        except queue.Empty:
            logging.warning(f"Nothing to consume in schedd checkpoint queue in last {args.schedd_history_timeout} seconds, exiting early.")
            break
        else:
            if checkpoint is None:
                break
            logging.debug(f"Got schedd checkpoint {checkpoint}")
            ad_source.update_checkpoint(checkpoint)


def _startd_ckpt_updater(args, checkpoint_queue, ad_source):
    while True:
        try:
            checkpoint = checkpoint_queue.get(timeout=args.startd_history_timeout)
        except queue.Empty:
            logging.warning(f"Nothing to consume in startd checkpoint queue in last {args.schedd_history_timeout} seconds, exiting early.")
            break
        else:
            if checkpoint is None:
                break
            logging.debug(f"Got startd checkpoint {checkpoint}")
            ad_source.update_checkpoint(checkpoint)


def adstash(args):
    starttime = time.time()

    interface_info = ADSTASH_INTERFACE_REGISTRY[args.interface]
    iface_kwargs = {}
    src_kwargs = {}
    if interface_info["type"] == "se":
        iface_kwargs = {
            "host": args.se_host,
            "url_prefix": args.se_url_prefix,
            "username": args.se_username,
            "password": args.se_password,
            "use_https": args.se_use_https,
            "ca_certs": args.se_ca_certs,
            "timeout": args.se_timeout,
            "log_mappings": args.se_log_mappings,
        }
        src_kwargs = {
            "chunk_size": args.se_bunch_size,
            "index": args.se_index_name,
        }
    elif interface_info["type"] == "jsonfile":
        iface_kwargs = {
            "log_mappings": args.se_log_mappings,
            "log_dir": args.json_dir,
        }

    interface = interface_info["class"]()(**iface_kwargs)

    if args.read_ad_file is not None:
        metadata = collect_process_metadata()
        metadata["condor_adstash_source"] = "ad_file"
        if args.read_schedd_history:
            logging.warning("Skipping querying schedds since --read_ad_file was set.")
            args.read_schedd_history = False
        if args.read_startd_history:
            logging.warning("Skipping querying startds since --read_ad_file was set.")
            args.read_startd_history = False
        ad_source = ADSTASH_AD_SOURCE_REGISTRY["ad_file"]()()
        ads = ad_source.fetch_ads(args.read_ad_file)
        for _ in ad_source.process_ads(interface, ads, metadata=metadata, **src_kwargs):
            pass

    if args.read_schedd_history:
        schedd_starttime = time.time()

        # Get Schedd daemon ads
        schedd_ads = []
        schedd_ads = get_schedds(args)
        logging.warning(f"There are {len(schedd_ads)} schedds to query")

        metadata = collect_process_metadata()
        metadata["condor_adstash_source"] = "schedd_history"

        ad_source = ADSTASH_AD_SOURCE_REGISTRY["schedd_history"]()(checkpoint_file=args.checkpoint_file)

        futures = []
        manager = multiprocessing.Manager()
        checkpoint_queue = manager.Queue()

        with multiprocessing.Pool(processes=args.threads, maxtasksperchild=1) as pool:

            if len(schedd_ads) > 0:
                for schedd_ad in schedd_ads:
                    name = schedd_ad["Name"]

                    future = pool.apply_async(
                        schedd_history_processor,
                        (ad_source, schedd_ad, checkpoint_queue, interface, metadata, args, src_kwargs),
                    )
                    futures.append((name, future))

            ckpt_updater = multiprocessing.Process(target=_schedd_ckpt_updater, args=(args, checkpoint_queue, ad_source))
            ckpt_updater.start()

            # Report processes if they timeout or error
            for name, future in futures:
                try:
                    logging.warning(f"Waiting for Schedd {name} to finish.")
                    future.get(args.schedd_history_timeout)
                except multiprocessing.TimeoutError:
                    logging.warning(f"Waited too long for Schedd {name}; it may still complete in the background.")
                except Exception:
                    logging.exception(f"Error getting progress from Schedd {name}.")

            checkpoint_queue.put(None)
            logging.warning("Joining the schedd checkpoint queue.")
            ckpt_updater.join(timeout=(len(schedd_ads) * args.schedd_history_timeout * len(schedd_ads)))
            logging.warning("Shutting down the schedd checkpoint queue.")
            ckpt_updater.terminate()
            manager.shutdown()
            logging.warning("Shutting down the schedd multiprocessing pool.")

        logging.warning(f"Processing time for schedd history: {(time.time()-schedd_starttime)/60:.2f} mins")

    if args.read_startd_history:
        startd_starttime = time.time()

        # Get Startd daemon ads
        startd_ads = []
        startd_ads = get_startds(args)
        logging.warning(f"There are {len(startd_ads)} startds to query.")

        metadata = collect_process_metadata()
        metadata["condor_adstash_source"] = "startd_history"

        ad_source = ADSTASH_AD_SOURCE_REGISTRY["startd_history"]()(checkpoint_file=args.checkpoint_file)

        futures = []
        manager = multiprocessing.Manager()
        checkpoint_queue = manager.Queue()

        with multiprocessing.Pool(processes=args.threads, maxtasksperchild=1) as pool:

            if len(startd_ads) > 0:
                for startd_ad in startd_ads:
                    name = startd_ad["Machine"]

                    future = pool.apply_async(
                        startd_history_processor,
                        (ad_source, startd_ad, checkpoint_queue, interface, metadata, args, src_kwargs),
                    )
                    futures.append((name, future))

            ckpt_updater = multiprocessing.Process(target=_startd_ckpt_updater, args=(args, checkpoint_queue, ad_source))
            ckpt_updater.start()

            # Report processes if they timeout or error
            for name, future in futures:
                try:
                    logging.warning(f"Waiting for Startd {name} to finish.")
                    future.get(args.startd_history_timeout)
                except multiprocessing.TimeoutError:
                    logging.warning(f"Waited too long for Startd {name}; it may still complete in the background.")
                except Exception:
                    logging.exception(f"Error getting progress from Startd {name}.")

            checkpoint_queue.put(None)
            logging.warning("Joining the startd checkpoint queue.")
            ckpt_updater.join(timeout=(len(startd_ads) * args.startd_history_timeout * len(startd_ads)))
            logging.warning("Shutting down the startd checkpoint queue.")
            ckpt_updater.terminate()
            manager.shutdown()
            logging.warning("Shutting down the startd multiprocessing pool.")

        logging.warning(f"Processing time for startd history: {(time.time()-startd_starttime)/60:.2f} mins")

    if args.read_schedd_job_epoch_history:
        schedd_starttime = time.time()

        # Get Schedd daemon ads
        schedd_ads = []
        schedd_ads = get_schedds(args)
        logging.warning(f"There are {len(schedd_ads)} schedds to query")

        metadata = collect_process_metadata()
        metadata["condor_adstash_source"] = "schedd_job_epoch_history"

        ad_source = ADSTASH_AD_SOURCE_REGISTRY["schedd_job_epoch_history"]()(checkpoint_file=args.checkpoint_file)

        futures = []
        manager = multiprocessing.Manager()
        checkpoint_queue = manager.Queue()

        with multiprocessing.Pool(processes=args.threads, maxtasksperchild=1) as pool:

            if len(schedd_ads) > 0:
                for schedd_ad in schedd_ads:
                    name = schedd_ad["Name"]

                    future = pool.apply_async(
                        schedd_job_epoch_history_processor,
                        (ad_source, schedd_ad, checkpoint_queue, interface, metadata, args, src_kwargs),
                    )
                    futures.append((name, future))

            ckpt_updater = multiprocessing.Process(target=_schedd_ckpt_updater, args=(args, checkpoint_queue, ad_source))
            ckpt_updater.start()

            # Report processes if they timeout or error
            for name, future in futures:
                try:
                    logging.warning(f"Waiting for Schedd {name} to finish.")
                    future.get(args.schedd_history_timeout)
                except multiprocessing.TimeoutError:
                    logging.warning(f"Waited too long for Schedd {name}; it may still complete in the background.")
                except Exception:
                    logging.exception(f"Error getting progress from Schedd {name}.")

            checkpoint_queue.put(None)
            logging.warning("Joining the schedd checkpoint queue.")
            ckpt_updater.join(timeout=(len(schedd_ads) * args.schedd_history_timeout * len(schedd_ads)))
            logging.warning("Shutting down the schedd checkpoint queue.")
            ckpt_updater.terminate()
            manager.shutdown()
            logging.warning("Shutting down the schedd multiprocessing pool.")

        logging.warning(f"Processing time for schedd job epoch history: {(time.time()-schedd_starttime)/60:.2f} mins")

    if args.read_schedd_transfer_epoch_history:
        schedd_starttime = time.time()

        # Get Schedd daemon ads
        schedd_ads = []
        schedd_ads = get_schedds(args)
        logging.warning(f"There are {len(schedd_ads)} schedds to query")

        metadata = collect_process_metadata()
        metadata["condor_adstash_source"] = "schedd_transfer_epoch_history"

        ad_source = ADSTASH_AD_SOURCE_REGISTRY["schedd_transfer_epoch_history"]()(checkpoint_file=args.checkpoint_file)

        futures = []
        manager = multiprocessing.Manager()
        checkpoint_queue = manager.Queue()

        with multiprocessing.Pool(processes=args.threads, maxtasksperchild=1) as pool:

            if len(schedd_ads) > 0:
                for schedd_ad in schedd_ads:
                    name = schedd_ad["Name"]

                    future = pool.apply_async(
                        schedd_transfer_epoch_history_processor,
                        (ad_source, schedd_ad, checkpoint_queue, interface, metadata, args, src_kwargs),
                    )
                    futures.append((name, future))

            ckpt_updater = multiprocessing.Process(target=_schedd_ckpt_updater, args=(args, checkpoint_queue, ad_source))
            ckpt_updater.start()

            # Report processes if they timeout or error
            for name, future in futures:
                try:
                    logging.warning(f"Waiting for Schedd {name} to finish.")
                    future.get(args.schedd_history_timeout)
                except multiprocessing.TimeoutError:
                    logging.warning(f"Waited too long for Schedd {name}; it may still complete in the background.")
                except Exception:
                    logging.exception(f"Error getting progress from Schedd {name}.")

            checkpoint_queue.put(None)
            logging.warning("Joining the schedd checkpoint queue.")
            ckpt_updater.join(timeout=(len(schedd_ads) * args.schedd_history_timeout * len(schedd_ads)))
            logging.warning("Shutting down the schedd checkpoint queue.")
            ckpt_updater.terminate()
            manager.shutdown()
            logging.warning("Shutting down the schedd multiprocessing pool.")

        logging.warning(f"Processing time for schedd transfer epoch history: {(time.time()-schedd_starttime)/60:.2f} mins")

    processing_time = int(time.time() - starttime)
    return processing_time


def schedd_history_processor(src, schedd_ad, ckpt_queue, iface, metadata, args, src_kwargs):
    metadata["condor_history_runtime"] = int(time.time())
    metadata["condor_history_host_version"] = schedd_ad.get("CondorVersion", "UNKNOWN")
    metadata["condor_history_host_platform"] = schedd_ad.get("CondorPlatform", "UNKNOWN")
    metadata["condor_history_host_machine"] = schedd_ad.get("Machine", "UNKNOWN")
    metadata["condor_history_host_name"] = schedd_ad.get("Name", "UNKNOWN")
    try:
        ads = src.fetch_ads(schedd_ad, max_ads=args.schedd_history_max_ads)
    except Exception as e:
        logging.error(f"Could not fetch ads from {schedd_ad['Name']}: {e.__class__.__name__}: {str(e)}")
        return
    else:
        if ads is None:
            return
    try:
        for ckpt in src.process_ads(iface, ads, schedd_ad, metadata=metadata, **src_kwargs):
            if ckpt is not None:
                ckpt_queue.put({schedd_ad["Name"]: ckpt})
    except Exception as e:
        logging.error(f"Could not push ads from {schedd_ad['Name']}: {e.__class__.__name__}: {str(e)}")


def startd_history_processor(src, startd_ad, ckpt_queue, iface, metadata, args, src_kwargs):
    try:
        ads = src.fetch_ads(startd_ad, max_ads=args.startd_history_max_ads)
    except Exception as e:
        logging.error(f"Could not fetch ads from {startd_ad['Machine']}: {e.__class__.__name__}: {str(e)}")
        return
    else:
        if ads is None:
            return
    try:
        for ckpt in src.process_ads(iface, ads, startd_ad, metadata=metadata, **src_kwargs):
            if ckpt is not None:
                ckpt_queue.put({startd_ad["Machine"]: ckpt})
    except Exception as e:
        logging.error(f"Could not push ads from {startd_ad['Machine']}: {e.__class__.__name__}: {str(e)}")


def schedd_job_epoch_history_processor(src, schedd_ad, ckpt_queue, iface, metadata, args, src_kwargs):
    metadata["condor_history_runtime"] = int(time.time())
    metadata["condor_history_host_version"] = schedd_ad.get("CondorVersion", "UNKNOWN")
    metadata["condor_history_host_platform"] = schedd_ad.get("CondorPlatform", "UNKNOWN")
    metadata["condor_history_host_machine"] = schedd_ad.get("Machine", "UNKNOWN")
    metadata["condor_history_host_name"] = schedd_ad.get("Name", "UNKNOWN")
    try:
        ads = src.fetch_ads(schedd_ad, max_ads=args.schedd_history_max_ads)
    except Exception as e:
        logging.error(f"Could not fetch job epoch ads from {schedd_ad['Name']}: {e.__class__.__name__}: {str(e)}")
        return
    else:
        if ads is None:
            return
    try:
        for ckpt in src.process_ads(iface, ads, schedd_ad, metadata=metadata, **src_kwargs):
            if ckpt is not None:
                ckpt_queue.put({f"Job Epoch {schedd_ad['Name']}": ckpt})
    except Exception as e:
        logging.error(f"Could not push job epoch ads from {schedd_ad['Name']}: {e.__class__.__name__}: {str(e)}")


def schedd_transfer_epoch_history_processor(src, schedd_ad, ckpt_queue, iface, metadata, args, src_kwargs):
    metadata["condor_history_runtime"] = int(time.time())
    metadata["condor_history_host_version"] = schedd_ad.get("CondorVersion", "UNKNOWN")
    metadata["condor_history_host_platform"] = schedd_ad.get("CondorPlatform", "UNKNOWN")
    metadata["condor_history_host_machine"] = schedd_ad.get("Machine", "UNKNOWN")
    metadata["condor_history_host_name"] = schedd_ad.get("Name", "UNKNOWN")
    try:
        ads = src.fetch_ads(schedd_ad, max_ads=args.schedd_history_max_ads)
    except Exception as e:
        logging.exception(f"Could not fetch transfer epoch ads from {schedd_ad['Name']}: {e.__class__.__name__}: {str(e)}")
        return
    else:
        if ads is None:
            return
    try:
        for ckpt in src.process_ads(iface, ads, schedd_ad, metadata=metadata, **src_kwargs):
            if ckpt is not None:
                ckpt_queue.put({f"Transfer Epoch {schedd_ad['Name']}": ckpt})
    except Exception as e:
        logging.exception(f"Could not push transfer epoch ads from {schedd_ad['Name']}: {e.__class__.__name__}: {str(e)}")
