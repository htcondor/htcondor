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

class GenericInterface(object):
    """This is an example of an interface that requires some type of
    handle to a remote server and requires setting up an index.
    Suitable as a base class for search engine type interfaces."""


    def __init__(self, **kwargs):
        self.handle = None


    def get_handle(self, **kwargs):
        if self.handle is not None:
            return self.handle
        print("Initializing up dummy client")
        self.handle = object()
        return self.handle


    def setup_index(self, **kwargs):
        print("Setting up dummy index")
        client = self.get_handle()


    def post_ads(self, ads, metadata={}, **kwargs):
        print("Printing ads to screen")
        client = self.get_handle()
        errors = 0
        successes = 0
        for ad in ads:
            try:
                print(ad)
                successes += 1
            except Exception:
                errors += 1
        return {"success": successes, "error": errors}
