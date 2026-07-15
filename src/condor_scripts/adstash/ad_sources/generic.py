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

import json
import logging
import traceback

from pathlib import Path

from adstash.utils import atomic_write
from adstash.ad_converters.generic import GenericClassAdConverter
from adstash.interfaces.generic import GenericInterface


class GenericAdSource(object):


    def __init__(self, checkpoint_file=Path.cwd() / "checkpoint.json", **kwargs):
        self.checkpoint_file = checkpoint_file
        self.checkpoint = self.load_checkpoint()


    def load_checkpoint(self, checkpoint_file=None):
        if checkpoint_file is None:
            checkpoint_file = self.checkpoint_file
        try:
            with open(checkpoint_file, "r") as f:
                checkpoint = json.load(f)
        except IOError:
            logging.warning(f"Could not find checkpoint file {checkpoint_file}, fetching all ads available.")
            checkpoint = {}
        return checkpoint


    def update_checkpoint(self, update, checkpoint_file=None):
        if update is None:
            return
        if checkpoint_file is None:
            checkpoint_file = self.checkpoint_file
        checkpoint = self.load_checkpoint(checkpoint_file)
        checkpoint.update(update)
        atomic_write(json.dumps(checkpoint, indent=2), checkpoint_file)


    def fetch_ads(self):
        return []


    def process_ads(self, interface: GenericInterface, converter: GenericClassAdConverter, ads: list, metadata={}, chunk_size=0, **kwargs):
        chunk = []
        generic_checkpoint = None
        ads_posted = 0
        for ad in ads:
            try:
                dict_ad = converter.convert_ad_to_doc(ad)
            except Exception as e:
                message = f"Failure when converting document from ClassAd: {str(e)}"
                exc = traceback.format_exc()
                message += f"\n{exc}"
                logging.warning(message)
                continue
            if generic_checkpoint is None:  # set checkpoint from first parseable ad
                generic_checkpoint = {"GlobalJobId": ad.get("GlobalJobId")}
            chunk.append((converter.get_unique_doc_id(dict_ad), dict_ad,))
            if (chunk_size > 0) and (len(chunk) >= chunk_size):
                result = interface.post_ads(chunk, metadata=metadata, **kwargs)
                ads_posted += result["success"]
                yield None  # don't update checkpoint yet
                chunk = []
        if len(chunk) > 0:
            result = interface.post_ads(chunk, metadata=metadata, **kwargs)
            ads_posted += result["success"]
        yield generic_checkpoint
