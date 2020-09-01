#!/usr/bin/python3

# Copyright 2020 HTCondor Team, Computer Sciences Department,
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

from __future__ import print_function

import sys
import itertools
import time


if __name__ == "__main__":
    request = int(sys.argv[1])

    # if the request is 0 or less, sleep forever
    if request <= 0:
        cycles = itertools.count()
    else:
        cycles = range(request)

    for cycle in cycles:
        print("[{}/{}] sleeping for 1 second at {}".format(cycle, request, time.time()))
        time.sleep(1)

    print("Done sleeping at {}".format(time.time()))
