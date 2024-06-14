# Copyright 2024 HTCondor Team, Computer Sciences Department,
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
import json
import logging
import htcondor
import traceback
import re

from itertools import chain

from adstash.ad_sources.generic import GenericAdSource


class ScheddTransferEpochHistorySource(GenericAdSource):


    def __init__(self, *args, **kwargs):
        self.text_attrs = {
            "AttemptError",
            "ErrorString",
            "TransferError",
        }
        self.indexed_keyword_attrs = {
            "Endpoint",
            "ErrorType",
            "FailedName",
            "FailureType",
            "GLIDEIN_ResourceName",
            "GLIDEIN_Site",
            "GlobalJobId",
            "HttpCacheHitOrMiss",
            "HttpCacheHost",
            "Owner",
            "PelicanClientVersion",
            "ProjectName",
            "ScheddName",
            "ServerVersion",
            "StartdName",
            "StartdSlot",
            "TransferHostName",
            "TransferLocalMachineName",
            "TransferProtocol",
            "TransferType",
        }
        self.noindex_keyword_attrs = {
            "TransferFileName",
            "TransferUrl",
        }
        self.float_attrs = {
            "AttemptTime",
            "ConnectionTimeSeconds",
            "TimeToFirstByte",
        }
        self.int_attrs = {
            "Attempt",
            "Attempts",
            "AttemptFileBytes",
            "ClusterId",
            "ErrorCode",
            "LibcurlReturnCode",
            "NumShadowStarts",
            "ProcId",
            "TransferFileBytes",
            "TransferHttpStatusCode",
            "TransferTotalBytes",
            "TransferTries",
        }
        self.date_attrs = {
            "AttemptEndTime",
            "EpochWriteDate",
            "RecordTime",
            "TransferEndTime",
            "TransferStartTime",
        }
        self.bool_attrs = {
            "FinalAttempt",
            "TransferSuccess",
        }
        self.nested_attrs = set()
        self.known_attrs = (
            self.text_attrs |
            self.indexed_keyword_attrs |
            self.noindex_keyword_attrs |
            self.float_attrs |
            self.int_attrs |
            self.date_attrs |
            self.bool_attrs |
            self.nested_attrs )
        self.known_attr_map = {attr.lower(): attr for attr in self.known_attrs}
        self.dynamic_templates = [
            {
                "strings_as_keywords": {  # Store unknown strings as keywords
                    "match_mapping_type": "string",
                    "mapping": {"type": "keyword", "index": "false", "norms": "false", "ignore_above": 256},
                }
            },
        ]
        super().__init__(*args, **kwargs)


    def fetch_ads(self, schedd_ad, max_ads=10000):
        history_kwargs = {}
        if max_ads > 0:
            history_kwargs["match"] = max_ads

        ckpt = self.checkpoint.get(f"Transfer Epoch {schedd_ad['Name']}")
        if ckpt is None:
            logging.warning(f"No transfer epoch checkpoint found for schedd {schedd_ad['Name']}, getting all ads available.")
        else:
            since_exprs = []
            for attr in ("EpochWriteDate", "ClusterId", "ProcId", "NumShadowStarts",):
                if attr in ckpt:
                    since_exprs.append(f"({attr} == {ckpt[attr]})")
            since_expr = " && ".join(since_exprs)
            history_kwargs["since"] = since_expr
            logging.warning(f"Getting transfer epoch ads from {schedd_ad['Name']} since {since_expr}.")
        schedd = htcondor.Schedd(schedd_ad)
        return schedd.jobEpochHistory(constraint=True, projection=[], ad_type="TRANSFER", **history_kwargs)


    def unique_doc_id(self, doc):
        """
        To uniquely identify documents (not jobs)
        """
        job_id = f"{doc.get('ClusterId') or 0}.{doc.get('ProcId') or 0}"
        shadow_starts = doc.get("NumShadowStarts", 0)
        xfer_protocol = doc.get("TransferProtocol", "unknown")
        xfer_type = doc.get("TransferType", "unknown")
        date = doc.get("RecordTime", 0)
        attempt = f"{doc.get('Attempt', 0)}_{doc.get('Attempts', 1)}"
        return f"{job_id}#{shadow_starts}#{xfer_protocol}#{xfer_type}#{date}#{attempt}"


    def normalize(self, attr, value):
        attr = self.known_attr_map.get(attr.lower(), attr.lower())
        if value is None:
            pass
        else:
            try:
                if attr not in self.known_attrs:
                    value = str(value)
                elif attr in (self.int_attrs | self.date_attrs):
                    try:
                        value = int(value)
                    except TypeError:
                        value = str(value)
                        attr = f"{attr.lower()}_string"
                elif attr in self.float_attrs:
                    try:
                        value = float(value)
                    except TypeError:
                        value = str(value)
                        attr = f"{attr.lower()}_string"
                elif attr in self.bool_attrs:
                    try:
                        value = bool(value)
                    except TypeError:
                        value = str(value)
                        attr = f"{attr.lower()}_string"
                else:
                    value = str(value)
            except Exception as e:
                attr = f"{attr}_error"
                value = f"{e.__class__.__name__}: {str(e)}"
            if isinstance(value, str) and attr not in self.text_attrs and len(value) > 255:
                value = f"{value[:253]}..."
        return attr, value


    def expand_plugin_result_ads(self, ads):
        debug_results = []
        result = {}
        for ad in ads:
            for attr, value in ad.items():
                if attr == "DeveloperData":
                    debug_results = self.expand_debug_ad(value)
                else:
                    attr, value = self.normalize(attr, value)
                    result[attr] = value
            if not debug_results:
                yield result
            else:
                for debug_result in debug_results:
                    if debug_result.get("FinalAttempt"):  # merge and return entire ad on final attempt
                        yield debug_result | result
                    else:  # otherwise only add identifying attrs
                        for attr in ("TransferProtocol", "TransferType", "TransferUrl"):
                            debug_result[attr] = result.get(attr)
                        yield debug_result


    def expand_debug_ad(self, ad):
        result = {}
        try:
            attempts = int(ad.get("Attempts", 0))
        except TypeError:
            attempts = 0
        if attempts == 0:
            result["Attempts"] = 1
            result["Attempt"] = 0
            result["FinalAttempt"] = True
            for attr, value in ad.items():
                attr, value = self.normalize(attr, value)
                result[attr] = value
            yield result
            return

        attempt_attr_re = re.compile(r"(\D+)(\d+)$")
        attempt_attrs = set()
        for attr, value in ad.items():
            attempt_attr_match = attempt_attr_re.match(attr)
            if attempt_attr_match:
                attempt_attrs.add(attempt_attr_match.group(1))
            else:
                attr, value = self.normalize(attr, value)
                result[attr] = value

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
                if attr.startswith("Transfer"):
                    attr = attr.replace("Transfer", "Attempt", 1)
                attr, value = self.normalize(attr, value)
                attempt_result[attr] = value
            yield result | attempt_result


    def to_json_list(self, ad, return_dict=False):
        result = {}

        # Technically, there could be multiple "*PluginResultList" attrs in an epoch ad.
        # plugin_results is a list of generators of dicts, which will be consumed using itertools.chain().
        plugin_results = []

        # Python None converts to JSON null. Helpfully, Elasticsearch will ignore null values.
        result["RecordTime"] = ad.get("EpochWriteDate", int(time.time()))
        result["ScheddName"] = ad.get("GlobalJobId", "").split("#", maxsplit=1)[0] or None
        result["ClusterId"] = ad.get("ClusterId", ad.get("GlobalJobId", "#.").split("#")[1].split(".")[0]) or None
        result["ProcId"] = ad.get("ProcId", ad.get("GlobalJobId", "#.").split("#")[1].split(".")[-1]) or None
        result["StartdSlot"] = ad.get("RemoteHost", "").split("@", maxsplit=1)[0] or None
        result["StartdName"] = ad.get("RemoteHost", "").split("@", maxsplit=1)[-1] or None
        for attr, value in result.items():
            attr, value = self.normalize(attr, value)
            result[attr] = value
        for attr, value in ad.items():
            if attr.endswith("PluginResultList"):
                plugin_results.append(self.expand_plugin_result_ads(value))
            else:
                attr, value = self.normalize(attr, value)
                result[attr] = value
        for plugin_result in chain(*plugin_results or [{}]):
            if return_dict:
                yield result | plugin_result
            else:
                yield json.dumps(result | plugin_result)


    def process_ads(self, interface, ads, schedd_ad, metadata={}, chunk_size=0, **kwargs):
        starttime = time.time()
        chunk = []
        schedd_checkpoint = None
        ads_posted = 0
        for ad in ads:
            try:
                dict_ads = self.to_json_list(ad, return_dict=True)
            except Exception as e:
                message = f"Failure when converting document in {schedd_ad['name']} transfer epoch history: {str(e)}"
                exc = traceback.format_exc()
                message += f"\n{exc}"
                logging.warning(message)
                continue

            # Unfortunately, the schedd history is in reverse chronological order,
            # therefore the checkpoint should be set to the first ad that is returned.
            # Here, we assume that the interface is responsible for de-duping ads
            # and only update the checkpoint after the full history queue is pushed
            # through by returning the new checkpoint at the end.

            if schedd_checkpoint is None:  # set checkpoint based on first parseable ad
                schedd_checkpoint = {}
                for attr in ("EpochWriteDate", "ClusterId", "ProcId", "NumShadowStarts",):
                    if attr in ad:
                        schedd_checkpoint[attr] = ad[attr]
            for dict_ad in dict_ads:
                dict_ad["ScheddName"] = schedd_ad["name"]
                chunk.append((self.unique_doc_id(dict_ad), dict_ad,))

            if (chunk_size > 0) and (len(chunk) >= chunk_size):
                logging.debug(f"Posting {len(chunk)} transfer epoch ads from {schedd_ad['Name']}.")
                result = interface.post_ads(chunk, metadata=metadata, ad_source=self, **kwargs)
                ads_posted += result["success"]
                yield None  # don't update checkpoint yet, per note above
                chunk = []

        if len(chunk) > 0:
            logging.debug(f"Posting {len(chunk)} transfer epoch ads from {schedd_ad['Name']}.")
            result = interface.post_ads(chunk, metadata=metadata, ad_source=self, **kwargs)
            ads_posted += result["success"]

        endtime = time.time()
        logging.warning(f"Schedd {schedd_ad['Name']} transfer epoch history: response count: {ads_posted}; upload time: {(endtime-starttime)/60:.2f} min")
        yield schedd_checkpoint  # finally update checkpoint
