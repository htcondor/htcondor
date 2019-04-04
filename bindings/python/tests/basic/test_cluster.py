# Copyright 2019 HTCondor Team, Computer Sciences Department,
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

import htcondor_basic as htc


@pytest.fixture(scope="function")
def cluster_pair():
    s = htc.Submit({"executable": r"foobar", "args": "hello world"})
    c1 = s.queue(count=1)
    c2 = s.queue(count=1)
    yield c1, c2
    c1.remove()
    c2.remove()


def test_can_be_used_as_dict_key(cluster):
    {cluster: "foobar"}


def test_can_be_put_in_set(cluster):
    {cluster}


def test_does_not_have_same_hash_as_bare_id(cluster):
    assert hash(cluster) != hash(cluster.id)


def test_cluster_works_with_new_style_d_format_string(cluster):
    assert "{:d}".format(cluster) == "{}".format(cluster.id)


def test_cluster_works_with_old_style_d_format_string(cluster):
    assert "%d" % cluster == "{}".format(cluster.id)


def test_cluster_from_queue_has_ad(cluster):
    cluster.ad


def test_cluster_from_constructor_has_ad(cluster):
    c = htc.Cluster(cluster.id)

    c.ad


def test_cluster_from_constructor_after_job_is_dead_has_ad(cluster):
    cluster.remove()

    c = htc.Cluster(cluster.id)

    c.ad


def test_cluster_from_queue_can_read_events(cluster):
    events = list(cluster.events())

    assert len(events) > 0


def test_cluster_from_constructor_can_read_events(cluster):
    c = htc.Cluster(cluster.id)
    events = list(c.events())

    assert len(events) > 0


def test_cluster_from_constructor_after_remove_can_read_events(cluster):
    cluster.remove()

    c = htc.Cluster(cluster.id)
    events = list(c.events())

    assert len(events) > 0


def test_str_has_id(cluster):
    assert "{}".format(cluster.id) in str(cluster)


def test_basic_query(cluster):
    ad = next(cluster.query(projection=["ProcId"]))

    assert ad["ProcId"] == 0


def test_query_with_no_projection_returns_lots_of_attributes(cluster):
    ad = next(cluster.query())

    assert len(ad) > 2


def test_hold(cluster):
    cluster.hold()

    ad = next(cluster.query(projection=["JobStatus"]))

    assert ad["JobStatus"] == 5


def test_hold_then_release(cluster):
    cluster.hold()

    ad = next(cluster.query(projection=["JobStatus"]))

    assert ad["JobStatus"] == 5

    cluster.release()

    ad = next(cluster.query(projection=["JobStatus"]))

    assert ad["JobStatus"] != 5
