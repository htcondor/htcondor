import logging
import json

from collections import OrderedDict
from argparse import Namespace
from typing import Tuple
from pathlib import Path

from adstash.interfaces.generic import GenericInterface
from adstash.mapping.functions import get_default_mapping_properties, merge_properties, merge_dynamic_templates
from adstash.mapping import job, job_epoch, transfer_epoch
from adstash.settings import SearchEngineSettings, calculate_field_limit


AD_TYPE_DEFAULT_MAPPINGS = {
    "history": job,
    "job_epoch_history": job_epoch,
    "transfer_epoch_history": transfer_epoch,
}


def test_interface(interface: GenericInterface) -> None:
    if interface.is_search_engine:
        logging.info(f"Testing connection to {interface.__class__.__name__}")
        interface.ping()


def check_interface_health(interface: GenericInterface) -> None:
    if not interface.is_search_engine:
        return
    logging.info(f"Getting cluster health for {interface.__class__.__name__}")
    health = interface.get_health()
    status = health.get("status", "unknown")
    report = f"Cluster health status is {status}"
    if status == "green":
        logging.info(report)
    elif status == "red":
        logging.critical(report)
        logging.critical("Pushing ads is likely to fail!")
    else:
        logging.warning(report)


def compute_index_mappings(
        interface: GenericInterface,
        index: str,
        ad_type: str,
        custom_properties: dict,
        custom_templates: OrderedDict,
    ) -> dict:

    existing_mappings = {}
    if interface.is_search_engine:
        logging.info(f"Getting existing index mappings from {interface.__class__.__name__}")
        active_index = interface.get_active_index(index)
        existing_mappings = interface.get_mappings(active_index)
    mappings = existing_mappings.copy()

    existing_properties = mappings.pop("properties", {})

    default_properties = get_default_mapping_properties(AD_TYPE_DEFAULT_MAPPINGS[ad_type])
    default_templates = AD_TYPE_DEFAULT_MAPPINGS[ad_type].DYNAMIC_TEMPLATES

    mappings["properties"] = merge_properties(existing_properties, custom_properties, default_properties)
    mappings["dynamic_templates"] = merge_dynamic_templates(default_templates, custom_templates)

    return mappings


def set_index_settings(
        interface: GenericInterface,
        index: str,
        mappings: dict,
        custom_settings: dict,
    ) -> dict:


    existing_settings = {}
    if interface.is_search_engine:
        logging.info(f"Getting existing index settings from {interface.__class__.__name__}")
        active_index = interface.get_active_index(index)
        existing_settings = interface.get_settings(active_index)

    # Only pass along the field limit, other settings are likely to be immutable
    field_limit_settings = {}
    try:
        field_limit_settings = {
            "index": {
                "mapping": {
                    "total_fields": {
                        "limit": existing_settings["index"]["mapping"]["total_fields"]["limit"]
                    }
                }
            }
        }
    except (IndexError, ValueError, KeyError):
        logging.debug(f"No existing total_fields.limit found in settings: {existing_settings}")
    existing_settings = field_limit_settings

    ses = SearchEngineSettings(
        index_name=index,
        mappings=mappings,
        custom_settings=custom_settings,
        existing_settings=existing_settings)

    if interface.is_search_engine:
        logging.info(f"Pushing computed index settings to {interface.__class__.__name__}")
        interface.update_settings(
            index=index,
            settings=ses.update_settings)

    return ses.settings


def log_mappings(interface_name: str, log_dir: Path, mappings: dict, settings: dict) -> None:
    logging.info(f"Writing computed mappings and settings to {log_dir}")
    try:
        with open(log_dir / f"condor_adstash_{interface_name}_last_mappings.json", "w") as f:
            json.dump(mappings, f, indent=2)
        with open(log_dir / f"condor_adstash_{interface_name}_last_settings.json", "w") as f:
            json.dump(settings, f, indent=2)
    except Exception:
        logging.exception(f"Failed to write last mappings and/or settings to {log_dir}")


def init_index(ad_type: str, args: Namespace) -> None:
    """Write out JSON files for initializing a new search engine index."""
    if ad_type not in AD_TYPE_DEFAULT_MAPPINGS:
        raise ValueError(f"Unknown ad type '{ad_type}', choose from: {', '.join(AD_TYPE_DEFAULT_MAPPINGS)}")

    interface = GenericInterface()

    mappings = compute_index_mappings(
        interface=interface,
        index=args.se_index_name,
        ad_type=ad_type,
        custom_properties=args.custom_field_properties or {},
        custom_templates=args.custom_dynamic_templates or OrderedDict(),
    )

    ses = SearchEngineSettings(
        index_name=args.se_index_name,
        mappings=mappings,
        custom_settings=args.custom_index_settings or {},
    )

    print(f"Preparing index using default mappings for {ad_type} ads.")

    ses.write_index_settings(
        output_directory=args.init_output_directory,
        use_alias=args.use_alias,
        use_ilm=args.use_ilm,
        use_template=args.use_template,
    )


def setup_index(interface: GenericInterface, ad_type: str, args: Namespace) -> Tuple[dict]:
    test_interface(interface=interface)
    check_interface_health(interface=interface)
    # Compute mappings
    mappings = compute_index_mappings(
        interface=interface,
        index=args.se_index_name,
        ad_type=ad_type,
        custom_properties=args.custom_field_properties or {},
        custom_templates=args.custom_dynamic_templates or OrderedDict(),
        )
    # Update settings (including total_fields.limit) before pushing mappings.
    settings = set_index_settings(
        interface=interface,
        index=args.se_index_name,
        mappings=mappings,
        custom_settings=args.custom_index_settings or {},
    )
    # Now push updated mappings
    if interface.is_search_engine:
        logging.info(f"Pushing computed index mappings to {interface.__class__.__name__}")
        try:
            interface.update_mappings(args.se_index_name, mappings)
        except Exception as e:
            if "total fields" in str(e).lower() and "exceeded" in str(e).lower():
                active_index = interface.get_active_index(args.se_index_name)
                current_settings = interface.get_settings(active_index)
                current_limit = int(current_settings["index"]["mapping"]["total_fields"]["limit"])
                new_limit = max(calculate_field_limit(mappings), current_limit * 2)
                logging.warning(
                    f"Field limit exceeded pushing mappings (limit may have reset after rollover), "
                    f"bumping to {new_limit} and retrying"
                )
                interface.update_settings(args.se_index_name, {"index.mapping.total_fields.limit": new_limit})
                interface.update_mappings(args.se_index_name, mappings)
            else:
                raise
    if args.se_log_mappings:
        log_mappings(
            interface_name=args.interface,
            log_dir=args.se_log_mappings_dir,
            mappings=mappings,
            settings=settings
        )
    return (mappings, settings,)
