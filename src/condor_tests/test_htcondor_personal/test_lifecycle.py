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

from htcondor.personal import PersonalPool, PersonalPoolState


def test_context_manager_starts_and_stops_pool(local_dir):
    with PersonalPool(local_dir=local_dir) as pool:
        assert pool.state is PersonalPoolState.READY

    assert pool.state is PersonalPoolState.STOPPED


def test_pool_state_progression_with_use_config_false(local_dir):
    pool = PersonalPool(local_dir=local_dir, use_config=False)

    assert pool.state is PersonalPoolState.UNINITIALIZED

    pool.initialize()

    assert pool.state is PersonalPoolState.INITIALIZED

    pool.start()

    assert pool.state is PersonalPoolState.READY

    pool.stop()

    assert pool.state is PersonalPoolState.STOPPED


def test_pool_state_progression_with_use_config_true(local_dir):
    pool = PersonalPool(local_dir=local_dir, use_config=True)

    assert pool.state is PersonalPoolState.INITIALIZED

    pool.initialize()

    assert pool.state is PersonalPoolState.INITIALIZED

    pool.start()

    assert pool.state is PersonalPoolState.READY

    pool.stop()

    assert pool.state is PersonalPoolState.STOPPED


def test_pool_state_progression_without_initialize_with_use_config_true(local_dir):
    pool = PersonalPool(local_dir=local_dir, use_config=False)

    assert pool.state is PersonalPoolState.UNINITIALIZED

    pool.start()

    assert pool.state is PersonalPoolState.READY

    pool.stop()

    assert pool.state is PersonalPoolState.STOPPED


def test_pool_state_progression_without_initialize_with_use_config_false(local_dir):
    pool = PersonalPool(local_dir=local_dir, use_config=True)

    assert pool.state is PersonalPoolState.INITIALIZED

    pool.start()

    assert pool.state is PersonalPoolState.READY

    pool.stop()

    assert pool.state is PersonalPoolState.STOPPED


def test_can_start_twice(local_dir):
    pool = PersonalPool(local_dir=local_dir).start().start()

    assert pool.state is PersonalPoolState.READY

    # cleanup
    pool.stop()


def test_can_stop_twice(local_dir):
    pool = PersonalPool(local_dir=local_dir).start().stop().stop()

    assert pool.state is PersonalPoolState.STOPPED


def test_can_start_stop_start_stop(local_dir):
    pool = PersonalPool(local_dir=local_dir).start().stop().start().stop()

    assert pool.state is PersonalPoolState.STOPPED


def test_can_start_two_pools(local_dir, another_local_dir):
    with PersonalPool(local_dir=local_dir) as pool:
        with PersonalPool(local_dir=another_local_dir) as another_pool:
            assert pool.state is PersonalPoolState.READY
            assert another_pool.state is PersonalPoolState.READY
