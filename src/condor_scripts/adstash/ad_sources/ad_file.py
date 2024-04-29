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

import logging
import classad
import traceback

from adstash.ad_sources.generic import GenericAdSource
from adstash.convert import to_json, unique_doc_id


class FileAdSource(GenericAdSource):


    def __init__(self, checkpoint_file=None, **kwargs):
        pass


    def fetch_ads(self, ad_file):
        """
        Generates one ClassAd at a time from ad_file.
        Necessary because classad.parseAds()
        cannot handle files with "weird" ad separators
        (e.g. "**** metadataA=foo metadata2=bar")
        """
        try:
            with open(ad_file) as f:
                ad_string = ""
                for line in f:
                    if line.startswith("***") or line.strip() == "":
                        if ad_string == "":
                            continue
                        yield classad.parseOne(ad_string)
                        ad_string = ""
                    ad_string += line
        except IOError as e:
            logging.error(f"Could not read {ad_file}: {str(e)}")
            return
        except Exception:
            logging.exception(f"Error while reading {ad_file} ({str(e)}), displaying traceback.")
            return


    def process_ads(self, interface, ads, metadata={}, chunk_size=0, **kwargs):
        chunk = []
        for ad in ads:
            try:
                dict_ad = to_json(ad, return_dict=True)
            except Exception as e:
                message = f"Failure when converting document from ClassAd: {str(e)}"
                exc = traceback.format_exc()
                message += f"\n{exc}"
                logging.warning(message)
                continue
            chunk.append((unique_doc_id(dict_ad), dict_ad,))
            if (chunk_size > 0) and (len(chunk) >= chunk_size):
                interface.post_ads(chunk, metadata=metadata, **kwargs)
                yield
                chunk = []
        if len(chunk) > 0:
            interface.post_ads(chunk, metadata=metadata, **kwargs)
            yield
