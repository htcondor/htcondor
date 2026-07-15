# Copyright 2025 HTCondor Team, Computer Sciences Department,
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

import logging
import re

from itertools import chain

from adstash.ad_converters.generic import GenericClassAdConverter


# Attributes (in order) which should be used as a timestamp
TIMESTAMP_ATTRS = [
    "EpochWriteDate"
]


class TransferEpochClassAdConverter(GenericClassAdConverter):

    def __init__(
            self,
            timestamp_fields=TIMESTAMP_ATTRS,
            **kwargs,
        ):
        if kwargs.get("projection"):
            logging.warning("Projections are not used for transfer epoch ads")
        if kwargs.get("ignore_attrs"):
            logging.warning("Attributes cannot be ignored for transfer epoch ads")
        super().__init__(
            timestamp_fields=timestamp_fields,
            **kwargs,
        )

    def expand_plugin_result_ads(self, ads, my_attr_name=None):
        file_number = 0
        for ad in ads:
            result = {}
            debug_results = []
            error_results = iter(())
            result["TransferFileNumber"] = file_number
            file_number += 1
            for attr, value in ad.items():
                if attr.lower() == "developerdata":
                    debug_results = self.expand_debug_ad(value)
                elif attr.lower() == "transfererrordata":
                    error_results = self.expand_error_ads(value)
                else:
                    result.update(self.convert_attr_to_dict(attr, value, ad))
            if result.get("TransferProtocol") is None:
                result["TransferProtocol"] = result.get("TransferUrl", "unknown:").split("+", maxsplit=1)[-1].split(":", maxsplit=1)[0]
            if result.get("TransferType") is None:
                result["TransferType"] = {"InputPluginResultList": "download", "OutputPluginResultList": "upload"}.get(my_attr_name, "unknown")
            if not debug_results:
                result["Attempts"] = 1
                result["Attempt"] = 0
                result["FinalAttempt"] = True
                try:  # add error data if it exists
                    result.update(next(error_results))
                except StopIteration:
                    pass
                yield result
            else:
                for debug_result in debug_results:
                    try:
                        error_result = next(error_results)
                    except StopIteration:
                        error_result = {}
                    if debug_result.get("FinalAttempt"):  # merge and return entire ad on final attempt
                        try:
                            yield debug_result | result | error_result
                        except TypeError:  # backwards compat
                            yield {**debug_result, **result, **error_result}
                    else:  # otherwise only add identifying attrs
                        for attr in ("TransferProtocol", "TransferType", "TransferUrl"):
                            debug_result[attr] = result.get(attr)
                        yield debug_result | error_result

    def expand_debug_ad(self, ad):
        result = {}
        try:
            attempts = int(ad.get("Attempts"))
        except TypeError:
            attempts = None

        if attempts is None:  # Attempts was unspecified in source ad
            result["Attempts"] = 1
            result["Attempt"] = 0
            result["FinalAttempt"] = True
            for attr, value in ad.items():
                result.update(self.convert_attr_to_dict(attr, value, ad))
            yield result
            return

        if attempts == 0:  # Attempts was actually 0 in source ad
            result["Attempts"] = 0
            result["Attempt"] = 0
            result["FinalAttempt"] = True
            for attr, value in ad.items():
                result.update(self.convert_attr_to_dict(attr, value, ad))
            yield result
            return

        attempt_attr_re = re.compile(r"(\D+)(\d+)$")
        attempt_attrs = set()
        for attr, value in ad.items():
            attempt_attr_match = attempt_attr_re.match(attr)
            if attempt_attr_match:
                attempt_attrs.add(attempt_attr_match.group(1))
            else:
                result.update(self.convert_attr_to_dict(attr, value, ad))

        for attempt in range(attempts):
            attempt_result = {
                "Attempt": attempt,
                "FinalAttempt": attempt + 1 == attempts,
            }
            for attr in attempt_attrs:
                attempt_attr = f"{attr}{attempt}"
                value = ad.get(attempt_attr)
                if value is None:
                    continue
                if attr.lower().startswith("transfer"):
                    attr = f"Attempt{attr[len('transfer'):]}"
                attempt_result.update(self.convert_attr_to_dict(attr, value, ad))
            try:
                yield result | attempt_result
            except TypeError:  # backwards compat
                yield {**result, **attempt_result}

    def expand_error_ads(self, ads):
        for ad in ads:
            error_result = {}
            for attr, value in ad.items():
                if attr.lower() == "developerdata":
                    for debug_attr, debug_value in value.items():
                        debug_result = self.convert_attr_to_dict(debug_attr, debug_value, ad)
                        for dr_key in list(debug_result.keys()):  # make sure we're not mutating while iterating on the same obj
                            if dr_key in ad:
                                debug_result[f"Debug{dr_key}"] = debug_result.pop(dr_key)
                        error_result.update(debug_result)
                else:
                    error_result.update(self.convert_attr_to_dict(attr, value, ad))
            yield error_result

    def convert_transfer_ad_to_dicts(self, ad, parent_attr=""):
        """
        Convert a transfer epoch ClassAd and yield a document (dict) with flattened objects.
        Multiple docs could be yielded per transfer epoch ClassAd.
        """
        doc = {}

        # Technically, there could be multiple "*PluginResultList" attrs in an epoch ad.
        # plugin_results is a list of generators of dicts, which will be consumed using itertools.chain().
        plugin_results = []

        # 1. Loop over all attrs
        for attr, value in ad.items():

            # 2. Flatten the namespace
            if parent_attr:
                attr = f"{parent_attr}.{attr}"

            # 3. Pop out plugin result lists
            if attr.endswith("PluginResultList"):
                plugin_results.append(self.expand_plugin_result_ads(value, my_attr_name=attr))

            # 4. Convert top-level attrs
            else:
                doc.update(self.convert_attr_to_dict(attr, value, ad))

        # 5. Loop over plugin results, append and yield them
        if not plugin_results:
            doc["NoPluginResults"] = True
            yield doc
        else:
            for plugin_result in chain(*plugin_results):
                try:
                    yield doc | plugin_result
                except TypeError:  # backwards compat
                    yield {**doc, **plugin_result}

    def get_unique_doc_id(self, doc):
        """
        To uniquely identify documents (not jobs)
        """
        schedd_name = doc.get("ScheddName", "unknown") or "unknown"
        job_id = f"{doc.get('ClusterId') or 0}.{doc.get('ProcId') or 0}"
        shadow_starts = doc.get("NumShadowStarts", 0)
        xfer_protocol = doc.get("TransferProtocol", "unknown")
        xfer_type = doc.get("TransferType", "unknown")
        file_number = doc.get("TransferFileNumber", 0)
        date = doc.get("RecordTime", 0)
        attempt = f"{doc.get('Attempt', 0)}_{doc.get('Attempts', 1)}"
        return f"{schedd_name}#{job_id}#{shadow_starts}#{xfer_protocol}#{xfer_type}#{file_number}#{date}#{attempt}"

    def add_additional_fields(self, doc, ad):
        # Python None converts to JSON null. Helpfully, Elasticsearch will ignore null values.
        if "ScheddName" not in doc:
            doc["ScheddName"] = ad.get("GlobalJobId", "").split("#", maxsplit=1)[0] or None
        doc["ClusterId"] = ad.get("ClusterId", ad.get("GlobalJobId", "#.").split("#")[1].split(".")[0]) or None
        doc["ProcId"] = ad.get("ProcId", ad.get("GlobalJobId", "#.").split("#")[1].split(".")[-1]) or None
        doc["StartdSlot"] = ad.get("RemoteHost", "").split("@", maxsplit=1)[0] or None
        doc["StartdName"] = ad.get("RemoteHost", "").split("@", maxsplit=1)[-1] or None

    def convert_transfer_ad_to_docs(self, ad):

        # Do the bulk of the conversions
        for doc in self.convert_transfer_ad_to_dicts(ad):

            # Add timestamps
            doc["@timestamp"] = doc["RecordTime"] = self.get_timestamp(ad)

            # Add additional fields
            self.add_additional_fields(doc, ad)

            yield doc
