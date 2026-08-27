import os
import os.path
import logging

from pathlib import Path

import htcondor2 as htcondor
import classad2 as classad

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb


class Create(Verb):
    """
    Creates a slot bundle request from a request file.

    A bundle is a request for N slots that all match one resource request,
    held by the schedd as idle claims.  The request file is a ClassAd-style
    file containing the resource request (Requirements, RequestCpus,
    RequestMemory, ...), an Owner, and a num_requested giving the number of
    slots wanted in the bundle.
    """

    options = {
        "request_file": {
            "args": ("request_file",),
            "help": "Bundle request file",
        },
    }

    def __init__(self, logger, request_file, **options):
        request_file = Path(request_file)
        if not request_file.exists():
            raise FileNotFoundError(f"Could not find file: {str(request_file)}")
        if os.access(request_file, os.R_OK) is False:
            raise PermissionError(f"Could not access file: {str(request_file)}")

        schedd = htcondor.Schedd()

        with request_file.open() as f:
            bundle_ad = classad.parseOne(f)

        # Accept a friendly num_requested spelling in the request file.
        if "num_requested" in bundle_ad and "BundleNumRequested" not in bundle_ad:
            bundle_ad["BundleNumRequested"] = bundle_ad["num_requested"]

        try:
            result_ad = schedd.create_bundle(bundle_ad)
            if result_ad["Result"] != 0:
                raise RuntimeError(f"Failed to create bundle: {result_ad.get('ErrorString', 'unknown error')}")
            bundle_id = result_ad["BundleId"]
            print(f"Bundle created with Id {bundle_id}.")
        except Exception as e:
            raise RuntimeError(f"Error creating bundle: {str(e)}")


class Query(Verb):
    """
    Queries the existing slot bundle requests on the schedd.
    """
    options = {
        "raw": {
            "args": ("--raw",),
            "action": "store_true",
            "default": False,
            "help": "display raw classad",
        },
    }

    def __init__(self, logger, **options):
        schedd = htcondor.Schedd()

        try:
            ad = classad.ClassAd()
            results = schedd.query_bundle(ad)

            if options["raw"]:
                for ad in results:
                    print(ad)
            else:
                print("BundleId                       Requested Satisfied")
                for ad in results:
                    bundle_id = ad.get("BundleId", "unknown")
                    requested = ad.get("BundleNumRequested", 0)
                    satisfied = ad.get("BundleNumSatisfied", 0)
                    print(f"{bundle_id:<30} {requested:>9} {satisfied:>9}")

        except Exception as e:
            raise RuntimeError(f"Error querying bundles: {str(e)}")


class Remove(Verb):
    """
    Remove an existing slot bundle request.
    """

    options = {
        "bundle_id": {
            "args": ("bundle_id",),
            "help": "Bundle ID to remove",
        },
    }

    def __init__(self, logger, bundle_id, **options):

        schedd = htcondor.Schedd()

        try:
            remove_ad = schedd.remove_bundle(bundle_id)
            if remove_ad["Result"] != 0:
                raise RuntimeError(f"Failed to remove bundle {bundle_id}: {remove_ad.get('ErrorString', 'unknown error')}")
            released = remove_ad.get("BundleClaimsReleased", 0)
        except Exception as e:
            logger.error(f"Error while trying to remove bundle {bundle_id}:\n{str(e)}")
            raise RuntimeError(f"Error removing bundle: {str(e)}")

        logger.info(f"Removed {bundle_id} (released {released} held claim(s)).")


class Bundle(Noun):
    """
    Run operations on HTCondor slot bundles
    """

    class create(Create):
        pass

    class query(Query):
        pass

    class remove(Remove):
        pass

    @classmethod
    def verbs(cls):
        return [cls.create, cls.query, cls.remove]
