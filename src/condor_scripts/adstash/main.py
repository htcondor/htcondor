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

from adstash.utils import get_schedds, get_startds, collect_process_metadata, set_up_logging
from adstash.ad_sources.registry import ADSTASH_AD_SOURCE_REGISTRY
from adstash.interfaces.registry import ADSTASH_INTERFACE_REGISTRY
from adstash.index_setup import setup_index
from adstash.ad_converters.job import JobClassAdConverter
from adstash.ad_converters.job_epoch import JobEpochClassAdConverter
from adstash.ad_converters.transfer_epoch import TransferEpochClassAdConverter


CHECKPOINT_KEY_TEMPLATE = {
    "schedd_history": "{name}",
    "startd_history": "{name}",
    "schedd_job_epoch_history": "Job Epoch {name}",
    "schedd_transfer_epoch_history": "Transfer Epoch {name}",
}

AD_TYPE_CONVERTERS = {
    "history": JobClassAdConverter,
    "job_epoch_history": JobEpochClassAdConverter,
    "transfer_epoch_history": TransferEpochClassAdConverter,
}


def _ckpt_updater(args, checkpoint_queue, ad_source, daemon_type="unknown daemon"):
    """
    This function watches the checkpoint queue and updates as checkpoints are added
    to the queue. It exits when:
    1. a timeout is reached, or
    2. the queue is empty, or
    3. the value None is fetched from the queue.
    """
    set_up_logging(args)
    timeout = vars(args).get(f"{daemon_type}_history_timeout")
    while True:
        try:
            logging.debug(f"Waiting {timeout} seconds for checkpoint...")
            checkpoint = checkpoint_queue.get(timeout=timeout)
            logging.debug(f"Got checkpoint {checkpoint}...")
        except queue.Empty:
            logging.warning(f"Nothing to consume in {daemon_type} checkpoint queue in last {timeout} seconds, exiting early.")
            break
        else:
            if checkpoint is None:
                break
            logging.debug(f"Got {daemon_type} checkpoint {checkpoint}")
            ad_source.update_checkpoint(checkpoint)


def adstash(args):
    """Main execution loop."""
    starttime = time.time()

    interface_class = ADSTASH_INTERFACE_REGISTRY[args.interface]()
    interface_kwargs = {}
    ad_source_kwargs = {}

    if interface_class.is_search_engine:  # set up search engine-specific options
        interface_kwargs = {
            "host": args.se_host,
            "url_prefix": args.se_url_prefix,
            "username": args.se_username,
            "password": args.se_password,
            "use_https": args.se_use_https,
            "ca_certs": args.se_ca_certs,
            "timeout": args.se_timeout,
        }
        ad_source_kwargs = {
            "chunk_size": args.se_bunch_size,
            "index": args.se_index_name,
        }
    else:  # set up JSON file-specific options
        interface_kwargs = {
            "json_dir": args.json_dir,
        }

    interface = interface_class(**interface_kwargs)

    metadata = collect_process_metadata()
    skip_daemons = args.read_ad_file is not None
    for source_type, source_cls in ADSTASH_AD_SOURCE_REGISTRY.items():

        if source_type == "ad_file" and args.read_ad_file is not None:

            # Currently, we assume generic ad files are from job history
            mappings, settings = setup_index(interface=interface, ad_type="history", args=args)
            converter = AD_TYPE_CONVERTERS["history"](
                mapping=mappings,
                custom_ignore_attrs=args.custom_ignore_attrs,
            )

            metadata["condor_adstash_source"] = "ad_file"
            ad_source = source_cls()(args=args)
            ads = ad_source.fetch_ads(args.read_ad_file)
            for _ in ad_source.process_ads(interface, converter, ads, metadata=metadata, **ad_source_kwargs):
                pass

        else:
            read_daemon = f"read_{source_type}"
            if vars(args)[read_daemon]:
                daemon_type, ad_type = source_type.split("_", maxsplit=1)
                if skip_daemons:
                    logging.warning(f"Skipping querying {daemon_type}s since --read_ad_file was set.")
                    continue

                mappings, settings = setup_index(interface=interface, ad_type=ad_type, args=args)
                converter = AD_TYPE_CONVERTERS[ad_type](
                    mapping=mappings,
                    projection=vars(args)[f"{daemon_type}_history_projection"],
                    custom_ignore_attrs=args.custom_ignore_attrs,
                )

                name_attr = "Name"
                if source_type.startswith("startd_"):
                    name_attr = "Machine"

                source_starttime = time.time()
                metadata["condor_adstash_source"] = source_type
                ad_source = source_cls()(checkpoint_file=args.checkpoint_file)

                # Get daemon ads
                daemon_ads = []
                daemon_ads = {"schedd": get_schedds, "startd": get_startds}[daemon_type](args)
                logging.warning(f"There are {len(daemon_ads)} {daemon_type}s to query")

                futures = []
                manager = multiprocessing.Manager()
                checkpoint_queue = manager.Queue()

                with multiprocessing.Pool(processes=args.threads, maxtasksperchild=1) as pool:

                    if len(daemon_ads) > 0:
                        for daemon_ad in daemon_ads:
                            daemon_name = daemon_ad[name_attr]
                            checkpoint_key = CHECKPOINT_KEY_TEMPLATE[source_type].format(name=daemon_name)

                            future = pool.apply_async(
                                history_processor,
                                (ad_source, daemon_type, daemon_ad, checkpoint_queue, checkpoint_key, interface, converter, metadata, args, ad_source_kwargs),
                            )
                            futures.append((daemon_name, future))

                    ckpt_updater = multiprocessing.Process(target=_ckpt_updater, args=(args, checkpoint_queue, ad_source, daemon_type))
                    ckpt_updater.start()

                    # Report processes if they timeout or error
                    timeout = vars(args)[f"{daemon_type}_history_timeout"]
                    succeeded = 0
                    timed_out = []
                    errored = []
                    for daemon_name, future in futures:
                        try:
                            if not future.ready():
                                logging.warning(f"Waiting for {daemon_type.capitalize()} {daemon_name} to finish (timeout: {timeout}s).")
                            future.get(timeout)
                            succeeded += 1
                        except multiprocessing.TimeoutError:
                            logging.warning(f"Waited too long for {daemon_type.capitalize()} {daemon_name}; it may still complete in the background.")
                            timed_out.append(daemon_name)
                        except Exception:
                            logging.exception(f"Error getting progress from {daemon_type.capitalize()} {daemon_name}.")
                            errored.append(daemon_name)
                    total = len(futures)
                    summary = f"{succeeded}/{total} {daemon_type}s completed"
                    if timed_out:
                        summary += f", {len(timed_out)} timed out: {', '.join(timed_out)}"
                    if errored:
                        summary += f", {len(errored)} errored: {', '.join(errored)}"
                    logging.warning(summary)

                    checkpoint_queue.put(None)
                    logging.warning(f"Joining the {daemon_type} checkpoint queue.")
                    ckpt_updater.join(timeout=(len(daemon_ads) * vars(args)[f"{daemon_type}_history_timeout"]))
                    logging.warning(f"Shutting down the {daemon_type} checkpoint queue.")
                    ckpt_updater.terminate()
                    manager.shutdown()
                    logging.warning(f"Shutting down the {daemon_type} multiprocessing pool.")

                logging.warning(f"Processing time for {daemon_type} history: {(time.time()-source_starttime)/60:.2f} mins")

    processing_time = int(time.time() - starttime)
    return processing_time


def history_processor(src, daemon_type, daemon_ad, ckpt_queue, ckpt_key, iface, converter, metadata, args, ad_source_kwargs):
    """Fetch condor_history from the given daemon and push docs to the given interface"""
    set_up_logging(args)
    metadata = metadata.copy()
    metadata["condor_history_runtime"] = int(time.time())
    metadata["condor_history_host_version"] = daemon_ad.get("CondorVersion", "UNKNOWN")
    metadata["condor_history_host_platform"] = daemon_ad.get("CondorPlatform", "UNKNOWN")
    metadata["condor_history_host_machine"] = daemon_ad.get("Machine", "UNKNOWN")
    metadata["condor_history_host_name"] = daemon_ad.get("Name", "UNKNOWN")
    daemon_name = daemon_ad.get("Name", daemon_ad.get("Machine", "UNKNOWN"))
    try:
        logging.debug("Started fetching ads")
        ads = src.fetch_ads(daemon_ad, max_ads=vars(args)[f"{daemon_type}_history_max_ads"], projection=vars(args)[f"{daemon_type}_history_projection"])
        logging.debug("Finished fetching ads")
    except Exception as e:
        logging.error(f"Could not fetch ads from {daemon_name}: {e.__class__.__name__}: {str(e)}")
        return
    if ads is None:
        logging.info(f"{daemon_type} {daemon_name} did not return any ads")
        return
    try:
        logging.info(f"Processing ads from {daemon_type} {daemon_name}")
        for ckpt in src.process_ads(iface, converter, ads, daemon_ad, metadata=metadata, **ad_source_kwargs):
            if ckpt is not None:
                ckpt_queue.put({ckpt_key: ckpt})
    except Exception as e:
        logging.exception(f"Could not push ads from {daemon_name}: {e.__class__.__name__}: {str(e)}")
