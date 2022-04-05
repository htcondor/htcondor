#!/usr/bin/python3
r"""Print HTCondor machine ClassAd attributes derived from Google Cloud Metadata Servers

Converts Google Cloud instance metadata into HTCondor ClassAd attribute/value
pairs to be advertised by an HTCondor startd.  ClassAd attributes are limited
to alphanumeric characters and the underscore.  Values are strings parsed
according to a set of matching rules.

https://cloud.google.com/compute/docs/metadata/default-metadata-values
https://htcondor.readthedocs.io/en/latest/misc-concepts/classad-mechanism.html#attributes
https://htcondor.readthedocs.io/en/latest/misc-concepts/classad-mechanism.html#composing-literals

For help:

    python3 common-cloud-attributes-google.py --help

Style and linting:
    pylint common-cloud-attributes-google.py
    yapf -i --style '{based_on_style: google, column_limit: 80}' \
            common-cloud-attributes-google.py

License:
    Apache 2.0
"""
# default metadata available at

from types import SimpleNamespace
from typing import Optional, Sequence
import argparse
import re
import requests


def query_google_cloud_metadata(scheme: str = "http",
                                host: str = "metadata.google.internal",
                                path: str = "/computeMetadata/v1/",
                                headers: dict = None,
                                params: dict = None) -> SimpleNamespace:
    """Retrieve all information from a Google Cloud metadata server

    Args:
      scheme: protocol used by metadata server (probably HTTP)
      host: hostname or IP address of metadata server (probably link-local)
      path: path within URL to query
      headers: client headers to use while performing HTTP query
      params: parameters to add to HTTP query, indicating format for response

    Returns:
      An object whose attributes correspond to the instance metadata.  Because
      the metadata were JSON-deserialized, they are in Python native types.
      e.g.,

      metadata.instance.id = 12345678901234
      metadata.instance.zone = "us-central1-a"

    Raises:
      1. Requests 2.27 begin raising requests.JSONDecodeError; in the meantime
      decoding failure is complicated: https://github.com/psf/requests/pull/5856
      2. there is also potential for requests.exceptions.HTTPError
    """
    if not headers:
        headers = {"Metadata-Flavor": "Google"}
    if not params:
        params = {"recursive": "true"}
    response = requests.get(f"{scheme}://{host}{path}",
                            headers=headers,
                            params=params)
    response.raise_for_status()
    return response.json(object_hook=lambda d: SimpleNamespace(**d))


def safe_htcondor_attribute(attribute: str) -> str:
    """Convert input attribute name into a valid HTCondor attribute name

    HTCondor ClassAd attribute names consist only of alphanumeric characters or
    underscores.  It is not clearly documented, but the alphanumeric characters
    are probably restricted to ASCII.  Attribute names created from multiple
    words typically capitalize the first letter in each word for readability,
    although all comparisions are case-insensitive.

    e.g., "central-manager" -> "CentralManager"

    Args:
      attribute: a string representing the name of an attribute

    Returns:
      The attribute name stripped of invalid characters and re-capitalized in
      the manner typical of HTCondor ClassAd attributes.

    Raises:
      None
    """
    # splitting by invalid characters removes them from the resulting array
    split_attr = re.split(r"[^\w]", attribute, flags=re.ASCII)
    safe_attr = "".join([word.capitalize() for word in split_attr if word])
    return safe_attr


def convert_metadata_to_classad(
        metadata: SimpleNamespace,
        use_short_id: bool = True,
        custom_metadata_keys: Optional[Sequence[str]] = None) -> dict:
    """Filter useful attributes from metadata and return HTCondor ClassAd

    Args:
      metadata: an object whose attributes represent instance metadata
      use_short_id: boolean that determines whether to convert long resource names
        to short resource IDs
      custom_metadata_keys: instance metadata can include custom key/value pairs
        defined by the user; specifying keys here will cause them to be included
        in the returned ClassAd

    Returns:
      A dictionary that represents part of an HTCondor machine ClassAd

    Raises:
      None
    """
    # see warnings re: mutable defaults at
    # https://docs.python.org/3/reference/compound_stmts.html#function-definitions
    if custom_metadata_keys is None:
        custom_metadata_keys = []

    ad = {}
    # begin by populating the fields whose values are long "resource names"
    # such as "projects/centos-cloud/global/images/centos-7-v20210701". The
    # boolean value use_short_id will shorten to id "centos-7-v20210701".
    ad["Image"] = metadata.instance.image
    ad["VMType"] = metadata.instance.machineType
    ad["Zone"] = metadata.instance.zone
    ad["Region"] = metadata.instance.zone.rsplit("-", 1)[0]

    # optionally convert the resource names to short resource IDs
    if use_short_id:
        ad = {k: v.rsplit("/", 1)[-1] for k, v in ad.items()}

    # All strings must be quoted.
    ad = {k: f'"{v}"' for k, v in ad.items()}

    # remaining values should not be in resource name format
    ad["InstanceID"] = f'"{metadata.instance.id}"'
    ad["Provider"] = '"Google"'
    ad["Platform"] = '"GCE"'
    ad["Interruptible"] = metadata.instance.scheduling.preemptible.capitalize()

    # have to sanitize custom metadata keys
    for key in custom_metadata_keys:
        safe_val = getattr(metadata.instance.attributes, key, None)
        if safe_val:
            safe_attr = safe_htcondor_attribute(key)
            ad[safe_attr] = f'"{safe_val}"'
    return ad


def main():
    """Command line interface for module."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-c",
                        "--custom-metadata-keys",
                        type=str,
                        nargs="+",
                        help="VM custom metadata to add to machine ClassAd")
    parser.add_argument("--protocol",
                        type=str,
                        dest="scheme",
                        default="http",
                        help="Protocol for instance metadata host")
    parser.add_argument("--host",
                        type=str,
                        default="metadata.google.internal",
                        help="VM instance metadata server name")
    parser.add_argument("--path",
                        type=str,
                        default="/computeMetadata/v1/",
                        help="VM instance metadata URL path")
    parser.add_argument(
        "--no-short-id",
        action='store_true',
        help="Disable advertising short resource IDs in favor of long names")
    args = parser.parse_args()

    try:
        metadata = query_google_cloud_metadata(scheme=args.scheme,
                                               host=args.host,
                                               path=args.path)
    except Exception as err:
        # But, for now, we don't really care about the why of failure
        raise SystemExit("Metadata server did not return valid JSON!") from err

    classad = convert_metadata_to_classad(
        metadata,
        use_short_id=not args.no_short_id,
        custom_metadata_keys=args.custom_metadata_keys)

    for attr, val in classad.items():
        print(f"{attr}={val}")
    print("- update:true")


if __name__ == "__main__":
    main()
