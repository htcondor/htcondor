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

import pytest

import htcondor2 as htcondor


def test_can_use_collector(pool):
    # we really just want to make sure the query goes through
    assert len(pool.collector.query(constraint="true")) > 0


def test_can_use_schedd(pool):
    # there shouldn't be any jobs in the pool
    # (but really we just want to make sure that the query goes through)
    assert len(pool.schedd.query(constraint="true")) == 0


@pytest.mark.parametrize("queue_method", ["submit"])
def test_can_submit_a_job(pool, queue_method):
    sub = htcondor.Submit({"executable": "foobar"})
    method = getattr(pool.schedd, queue_method)
    # We assume that a failure to submit results in a raised exception.
    result = method(sub)


def test_different_pools_have_different_collectors(pool, another_pool):
    # Collector.location is always None, so we need to dig a little deeper
    constraint = 'MyType == "Collector"'
    pool_collector_ad = pool.collector.query(constraint=constraint)[0]
    another_pool_collector_ad = another_pool.collector.query(constraint=constraint)[0]

    assert (
        pool_collector_ad["CollectorIpAddr"]
        != another_pool_collector_ad["CollectorIpAddr"]
    )


def test_different_pools_have_different_schedds(pool, another_pool):
    # This was wrong in version 1, but for now I don't care to change v2.
    # assert pool.schedd.location != another_pool.schedd.location
    assert pool.schedd._addr != another_pool.schedd._addr


def test_schedd_is_right_schedd(pool):
    via_pool = pool.schedd
    via_locate = htcondor.Schedd(pool.collector.locate(htcondor.DaemonTypes.Schedd))

    # This was wrong in version 1, but for now I don't care to change v2.
    # assert via_pool.location == via_locate.location
    assert via_pool._addr == via_locate._addr

def test_schedd_is_right_schedd_with_another_pool_present(pool, another_pool):
    via_pool = pool.schedd
    via_locate = htcondor.Schedd(pool.collector.locate(htcondor.DaemonTypes.Schedd))
    via_another_pool = another_pool.schedd


    # This was wrong in version 1, but for now I don't care to change v2.
    # assert via_pool.location != via_another_pool.location
    assert via_pool._addr != via_another_pool._addr
    # assert via_pool.location == via_locate.location
    assert via_pool._addr == via_locate._addr
