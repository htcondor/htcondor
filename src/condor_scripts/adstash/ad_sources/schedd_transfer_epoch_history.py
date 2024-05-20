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
import htcondor
import traceback

from adstash.ad_sources.generic import GenericAdSource
from adstash.convert import to_json


class ScheddTransferEpochHistorySource(GenericAdSource):


    def fetch_ads(self, schedd_ad, max_ads=10000):
        history_kwargs = {}
        if max_ads > 0:
            history_kwargs["match"] = max_ads

        ckpt = self.checkpoint.get(f"Transfer Epoch {schedd_ad['Name']}")
        if ckpt is None:
            logging.warning(f"No transfer epoch checkpoint found for schedd {schedd_ad['Name']}, getting all ads available.")
        else:
            since_expr = f"""(ClusterId == {ckpt["ClusterId"]}) && (ProcId == {ckpt["ProcId"]}) && (NumShadowStarts == {ckpt["NumShadowStarts"]})"""
            history_kwargs["since"] = since_expr
            logging.warning(f"Getting transfer epoch ads from {schedd_ad['Name']} since {since_expr}.")
        schedd = htcondor.Schedd(schedd_ad)
        return schedd.jobEpochHistory(constraint=True, projection=[], ad_type="TRANSFER", **history_kwargs)


    def unique_doc_id(self, doc):
        """
        Return a string of format "<ClusterId>.<ProcId>#<NumShadowStarts>"
        To uniquely identify documents (not jobs)
        """
        return f"{doc['ClusterId']}.{doc['ProcId']}#{doc['NumShadowStarts']}"


    def process_ads(self, interface, ads, schedd_ad, metadata={}, chunk_size=0, **kwargs):
        starttime = time.time()
        chunk = []
        schedd_checkpoint = None
        ads_posted = 0
        for ad in ads:
            try:
                dict_ad = to_json(ad, return_dict=True)
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
                schedd_checkpoint = {"ClusterId": ad["ClusterId"], "ProcId": ad["ProcId"], "NumShadowStarts": ad["NumShadowStarts"]}
            chunk.append((self.unique_doc_id(dict_ad), dict_ad,))

            if (chunk_size > 0) and (len(chunk) >= chunk_size):
                logging.debug(f"Posting {len(chunk)} transfer epoch ads from {schedd_ad['Name']}.")
                result = interface.post_ads(chunk, metadata=metadata, **kwargs)
                ads_posted += result["success"]
                yield None  # don't update checkpoint yet, per note above
                chunk = []

        if len(chunk) > 0:
            logging.debug(f"Posting {len(chunk)} transfer epoch ads from {schedd_ad['Name']}.")
            result = interface.post_ads(chunk, metadata=metadata, **kwargs)
            ads_posted += result["success"]

        endtime = time.time()
        logging.warning(f"Schedd {schedd_ad['Name']} transfer epoch history: response count: {ads_posted}; upload time: {(endtime-starttime)/60:.2f} min")
        yield schedd_checkpoint  # finally update checkpoint
