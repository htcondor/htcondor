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
import htcondor2 as htcondor
import classad2 as classad
import traceback

from adstash.ad_sources.generic import GenericAdSource
from adstash.convert import to_json, unique_doc_id


class StartdHistorySource(GenericAdSource):


    def fetch_ads(self, startd_ad, max_ads=10000):
        history_kwargs = {}
        if max_ads > 0:
            history_kwargs["match"] = max_ads

        ckpt = self.checkpoint.get(startd_ad["Machine"])
        if ckpt is None:
            logging.warning(f"No checkpoint found for startd {startd_ad['Machine']}, getting all ads available.")
        else:
            since_expr = f"""(GlobalJobId == "{ckpt["GlobalJobId"]}") && (EnteredCurrentStatus == {ckpt["EnteredCurrentStatus"]})"""
            history_kwargs["since"] = classad.ExprTree(since_expr)
            logging.warning(f"Getting ads from {startd_ad['Machine']} since {since_expr}.")
        startd = htcondor.Startd(startd_ad)
        return startd.history(constraint=True, projection=[], **history_kwargs)


    def process_ads(self, interface, ads, startd_ad, metadata={}, chunk_size=0, **kwargs):
        starttime = time.time()
        chunk = []
        startd_checkpoint = None
        ads_posted = 0
        for ad in ads:
            try:
                dict_ad = to_json(ad, return_dict=True)
            except Exception as e:
                message = f"Failure when converting document in {startd_ad['name']} history: {str(e)}"
                exc = traceback.format_exc()
                message += f"\n{exc}"
                logging.warning(message)
                continue

            # Unfortunately, the startd history is in reverse chronological order,
            # therefore the checkpoint should be set to the first ad that is returned.
            # Here, we assume that the interface is responsible for de-duping ads
            # and only update the checkpoint after the full history queue is pushed
            # through by returning the new checkpoint at the end.
            
            if startd_checkpoint is None:  # set checkpoint based on first parseable ad
                startd_checkpoint = {"GlobalJobId": ad["GlobalJobId"], "EnteredCurrentStatus": ad["EnteredCurrentStatus"]}
            chunk.append((unique_doc_id(dict_ad), dict_ad,))

            if (chunk_size > 0) and (len(chunk) >= chunk_size):
                logging.debug(f"Posting {len(chunk)} ads from {startd_ad['Name']}.")
                result = interface.post_ads(chunk, metadata=metadata, **kwargs)
                ads_posted += result["success"]
                yield None  # don't update checkpoint yet, per note above
                chunk = []

        if len(chunk) > 0:
            logging.debug(f"Posting {len(chunk)} ads from {startd_ad['Name']}.")
            result = interface.post_ads(chunk, metadata=metadata, **kwargs)
            ads_posted += result["success"]
        
        endtime = time.time()
        logging.warning(f"Startd {startd_ad['Machine']} history: response count: {ads_posted}; upload time: {(endtime-starttime)/60:.2f} min")
        yield startd_checkpoint  # finally update checkpoint
