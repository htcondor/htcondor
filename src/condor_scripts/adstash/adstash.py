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

from adstash.utils import get_schedds, get_startds, collect_process_metadata
from adstash.ad_sources.registry import ADSTASH_AD_SOURCE_REGISTRY
from adstash.interfaces.registry import ADSTASH_INTERFACE_REGISTRY

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
            logging.warning(f"Skipping querying schedds since --read_ad_file was set.")
            args.read_schedd_history = False
        if args.read_startd_history:
            logging.warning(f"Skipping querying startds since --read_ad_file was set.")
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

            def _schedd_ckpt_updater():
                while True:
                    try:
                        checkpoint = checkpoint_queue.get()
                        if checkpoint is None:
                            break
                    except EOFError as e:
                        logging.warning(f"EOFError: Nothing to consume in checkpoint queue {str(e)}")
                        break
                    ad_source.update_checkpoint(checkpoint)

            ckpt_updater = multiprocessing.Process(target=_schedd_ckpt_updater)
            ckpt_updater.start()

            # Report processes if they timeout or error
            for name, future in futures:
                try:
                    future.get(args.schedd_history_timeout)
                except multiprocessing.TimeoutError:
                    logging.warning(f"Schedd {name} history timed out; ignoring progress.")
                except Exception:
                    logging.exception(f"Error getting progress from {name}.")

            checkpoint_queue.put(None)
            ckpt_updater.join()

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

            def _startd_ckpt_updater():
                while True:
                    try:
                        checkpoint = checkpoint_queue.get()
                        if checkpoint is None:
                            break
                    except EOFError as e:
                        logging.warning(f"EOFError: Nothing to consume in checkpoint queue {str(e)}")
                        break
                    ad_source.update_checkpoint(checkpoint)

            ckpt_updater = multiprocessing.Process(target=_startd_ckpt_updater)
            ckpt_updater.start()

            # Report processes if they timeout or error
            for name, future in futures:
                try:
                    future.get(args.startd_history_timeout)
                except multiprocessing.TimeoutError:
                    logging.warning(f"Startd {name} history timed out; ignoring progress.")
                except Exception:
                    logging.exception(f"Error getting progress from {name}.")

            checkpoint_queue.put(None)
            ckpt_updater.join()

        logging.warning(f"Processing time for startd history: {(time.time()-startd_starttime)/60:.2f} mins")

    processing_time = int(time.time() - starttime)
    return processing_time


def schedd_history_processor(src, schedd_ad, ckpt_queue, iface, metadata, args, src_kwargs):
    metadata["condor_history_runtime"] = int(time.time())
    metadata["condor_history_host_version"] = schedd_ad.get("CondorVersion", "UNKNOWN")
    metadata["condor_history_host_platform"] = schedd_ad.get("CondorPlatform", "UNKNOWN")
    metadata["condor_history_host_machine"] = schedd_ad.get("Machine", "UNKNOWN")
    metadata["condor_history_host_name"] = schedd_ad.get("Name", "UNKNOWN")
    ads = src.fetch_ads(schedd_ad, max_ads=args.schedd_history_max_ads)
    for ckpt in src.process_ads(iface, ads, schedd_ad, metadata=metadata, **src_kwargs):
        if ckpt is not None:
            ckpt_queue.put({schedd_ad["Name"]: ckpt})


def startd_history_processor(src, startd_ad, ckpt_queue, iface, metadata, args, src_kwargs):
    ads = src.fetch_ads(startd_ad, max_ads=args.startd_history_max_ads)
    for ckpt in src.process_ads(iface, ads, startd_ad, metadata=metadata, **src_kwargs):
        if ckpt is not None:
            ckpt_queue.put({startd_ad["Machine"]: ckpt})