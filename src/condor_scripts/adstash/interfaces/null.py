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


class NullInterface(object):
    """This is the bare minimum of an interface class.
    Suitable as a base class for simple file interfaces."""


    def __init__(self, **kwargs):
        pass


    def post_ads(self, ads, **kwargs):
        n = 0
        for ad in ads:
            n += 1
        return {"success": n, "error": 0}
