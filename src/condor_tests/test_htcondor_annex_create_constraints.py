#!/usr/bin/env pytest

import os

import pytest

import subprocess

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


TEST_CASES = [
    # Verify that Bridges 2's GPU weirdness works for its whole-node queue.
    ( ['GPU@bridges2', '--gpus', 'v100-16:7'], False ),
    ( ['GPU@bridges2', '--gpus', 'v100-16:8'], True ),
    ( ['GPU@bridges2', '--gpus', 'v100-16:9'], False ),
    ( ['GPU@bridges2', '--gpus', 'v100-32:7'], False ),
    ( ['GPU@bridges2', '--gpus', 'v100-32:8'], True ),
    ( ['GPU@bridges2', '--gpus', 'v100-32:9'], False ),
    ( ['GPU@bridges2', '--gpus', 'v100-32:15'], False ),
    ( ['GPU@bridges2', '--gpus', 'v100-32:16'], True ),
    ( ['GPU@bridges2', '--gpus', 'v100-32:17'], False ),

    # The "GPU-shared" queue on Bridges 2 has a different constraint
    # implementation than anyone else's, so test it extensively.
    ( ['GPU-shared@bridges2', '--gpus', 'v100-16:1'], True ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-16:2'], True ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-16:3'], True ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-16:4'], True ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-16:5'], False ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-32:1'], True ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-32:2'], True ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-32:3'], True ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-32:4'], True ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-32:5'], False ),

    # Repeat everything above but using --gpus and --gpu-type.
    ( ['GPU@bridges2', '--gpu-type', 'v100-16', '--gpus', '7'], False ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-16', '--gpus', '8'], True ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-16', '--gpus', '9'], False ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-32', '--gpus', '7'], False ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-32', '--gpus', '8'], True ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-32', '--gpus', '9'], False ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-32', '--gpus', '15'], False ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-32', '--gpus', '16'], True ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-32', '--gpus', '17'], False ),

    # Verify that whole-node GPU queues don't require you to pass --gpus.
    ( ['GPU@bridges2', '--gpu-type', 'v100-16'], True ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-32'], True ),
    ( ['gpu@anvil', '--nodes', '2'], True ),
    ( ['gpu-debug@anvil', '--lifetime', '1800', '--nodes', '1'], True ),
    # ... and that doing so doesn't mean you don't have to supply a type
    # for queues that require types.
    ( ['GPU@bridges2'], False ),

    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-16', '--gpus', '1'], True ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-16', '--gpus', '2'], True ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-16', '--gpus', '3'], True ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-16', '--gpus', '4'], True ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-16', '--gpus', '5'], False ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-32', '--gpus', '1'], True ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-32', '--gpus', '2'], True ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-32', '--gpus', '3'], True ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-32', '--gpus', '4'], True ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-32', '--gpus', '5'], False ),

    # The GPU partitions on Bridges 2 ignore CPU and memory.  (This is the
    # expected behavior for a whole-node queue, but that needs to be checked
    # anyway; the special case here is the shared GPU partition.)  We test
    # a "normal" shared-GPU partition which requires rather than forbids
    # CPU and/or memory specification on Expanse, below.

    ( ['GPU@bridges2', '--gpus', 'v100-32:8', '--cpus', '4'], False ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-32', '--gpus', '8', '--cpus', '4'], False ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-32:3', '--cpus', '4'], False ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-32', '--gpus', '3', '--cpus', '4'], False ),

    ( ['GPU@bridges2', '--gpus', 'v100-32:8', '--mem_mb', '4'], False ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-32', '--gpus', '8', '--mem_mb', '4'], False ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-32:3', '--mem_mb', '4'], False ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-32', '--gpus', '3', '--mem_mb', '4'], False ),

    ( ['GPU@bridges2', '--gpus', 'v100-32:8', '--cpus', '4', '--mem_mb', '4'], False ),
    ( ['GPU@bridges2', '--gpu-type', 'v100-32', '--gpus', '8', '--cpus', '4', '--mem_mb', '4'], False ),
    ( ['GPU-shared@bridges2', '--gpus', 'v100-32:3', '--cpus', '4', '--mem_mb', '4'], False ),
    ( ['GPU-shared@bridges2', '--gpu-type', 'v100-32', '--gpus', '3', '--cpus', '4', '--mem_mb', '4'], False ),

    # Verify that whole-node partitions, CPU or GPU, don't allow
    # CPUs or memory to be specified.  Also, verify that they
    # respect the max node count.
    ( ['wholenode@anvil', '--nodes', '2'], True ),
    ( ['wholenode@anvil', '--nodes', '24'], False),
    ( ['wholenode@anvil', '--nodes', '2', '--cpus', '4'], False ),
    ( ['wholenode@anvil', '--nodes', '2', '--mem_mb', '1024'], False ),
    ( ['wholenode@anvil', '--nodes', '2', '--cpus', '4', '--mem_mb', '1024'], False ),

    ( ['wide@anvil', '--nodes', '24'], True ),
    ( ['wide@anvil', '--nodes', '96'], False ),
    ( ['wide@anvil', '--nodes', '2', '--cpus', '4'], False ),
    ( ['wide@anvil', '--nodes', '2', '--mem_mb', '1024'], False ),
    ( ['wide@anvil', '--nodes', '2', '--cpus', '4', '--mem_mb', '1024'], False ),

    ( ['gpu@anvil', '--nodes', '2', '--gpus', '4', ], True ),
    ( ['gpu@anvil', '--nodes', '5', '--gpus', '4', ], False ),
    ( ['gpu@anvil', '--nodes', '2', '--gpus', '1', ], False ),
    ( ['gpu@anvil', '--nodes', '2', '--gpus', '5', ], False ),
    ( ['gpu@anvil', '--nodes', '2', '--gpus', '4', '--cpus', '4'], False ),
    ( ['gpu@anvil', '--nodes', '2', '--gpus', '4', '--mem_mb', '1024'], False ),
    ( ['gpu@anvil', '--nodes', '2', '--gpus', '4', '--cpus', '4', '--mem_mb', '1024'], False ),

    ( ['gpu-debug@anvil', '--lifetime', '1800', '--nodes', '1', '--gpus', '2', ], True ),
    ( ['gpu-debug@anvil', '--lifetime', '1800', '--nodes', '2', '--gpus', '2', ], False ),
    ( ['gpu-debug@anvil', '--lifetime', '1800', '--nodes', '1', '--gpus', '1', ], False ),
    ( ['gpu-debug@anvil', '--lifetime', '1800', '--nodes', '1', '--gpus', '2', '--cpus', '4'], False ),
    ( ['gpu-debug@anvil', '--lifetime', '1800', '--nodes', '1', '--gpus', '2', '--mem_mb', '1024'], False ),
    ( ['gpu-debug@anvil', '--lifetime', '1800', '--nodes', '1', '--gpus', '2', '--cpus', '4', '--mem_mb', '1024'], False ),

    # Verify that shared-CPU partitions require CPUs or memory.  Also
    # verify that shared-CPU and shared-GPU partitions don't allow
    # an incorrect node count to be specified.   (It's at least possible
    # that some point somebody will configure a shared partition which
    # allows multiple nodes.)
    ( ['shared@expanse', '--nodes', '2', '--project', 'project'], False ),
    ( ['shared@expanse', '--nodes', '1', '--project', 'project'], False),
    ( ['shared@expanse', '--cpus', '4', '--project', 'project'], True ),
    ( ['shared@expanse', '--cpus', '4'], False ),
    ( ['shared@expanse', '--cpus', '180', '--project', 'project'], False ),
    ( ['shared@expanse', '--mem_mb', '1024', '--project', 'project'], True ),
    ( ['shared@expanse', '--mem_mb', str(1024*1024), '--project', 'project'], False ),
    ( ['shared@expanse', '--nodes', '1', '--cpus', '4', '--project', 'project'], True ),
    ( ['shared@expanse', '--nodes', '2', '--cpus', '4', '--project', 'project'], False ),
    ( ['shared@expanse', '--nodes', '1', '--mem_mb', '1024', '--project', 'project'], True ),
    ( ['shared@expanse', '--nodes', '2', '--mem_mb', '1024', '--project', 'project'], False ),
    ( ['shared@expanse', '--cpus', '4', '--mem_mb', '1024', '--project', 'project'], True ),
    ( ['shared@expanse', '--cpus', '180', '--mem_mb', '1024', '--project', 'project'], False ),
    ( ['shared@expanse', '--cpus', '4', '--mem_mb', str(1024*1024), '--project', 'project'], False ),
    ( ['shared@expanse', '--nodes', '1', '--cpus', '4', '--mem_mb', '1024', '--project', 'project'], True ),
    ( ['shared@expanse', '--nodes', '1', '--cpus', '180', '--mem_mb', '1024', '--project', 'project'], False ),
    ( ['shared@expanse', '--nodes', '1', '--cpus', '4', '--mem_mb', str(1024*1024), '--project', 'project'], False ),
    ( ['shared@expanse', '--nodes', '1', '--cpus', '4', '--mem_mb', '1024', '--project', 'project'], True ),

    ( ['gpu-shared@expanse', '--gpus', '1', '--project', 'project'], False ),
    ( ['gpu-shared@expanse', '--nodes', '1', '--gpus', '1', '--project', 'project'], False ),
    ( ['gpu-shared@expanse', '--gpus', '5', '--project', 'project'], False ),
    ( ['gpu-shared@expanse', '--nodes', '5', '--gpus', '1', '--project', 'project'], False ),

    ( ['gpu-shared@expanse', '--cpus', '4', '--gpus', '1', '--project', 'project'], True ),
    ( ['gpu-shared@expanse', '--cpus', '4', '--nodes', '1', '--gpus', '1', '--project', 'project'], True ),
    ( ['gpu-shared@expanse', '--cpus', '4', '--gpus', '5', '--project', 'project'], False ),
    ( ['gpu-shared@expanse', '--cpus', '4', '--nodes', '5', '--gpus', '1', '--project', 'project'], False ),

    ( ['gpu-shared@expanse', '--mem_mb', '1024', '--gpus', '1', '--project', 'project'], True ),
    ( ['gpu-shared@expanse', '--mem_mb', '1024', '--nodes', '1', '--gpus', '1', '--project', 'project'], True ),
    ( ['gpu-shared@expanse', '--mem_mb', '1024', '--gpus', '5', '--project', 'project'], False ),
    ( ['gpu-shared@expanse', '--mem_mb', '1024', '--nodes', '1', '--gpus', '5', '--project', 'project'], False ),

    ( ['gpu-shared@expanse', '--cpus', '4', '--mem_mb', '1024', '--gpus', '1', '--project', 'project'], True ),
    ( ['gpu-shared@expanse', '--cpus', '4', '--mem_mb', '1024', '--nodes', '1', '--gpus', '1', '--project', 'project'], True ),
    ( ['gpu-shared@expanse', '--cpus', '4', '--mem_mb', '1024', '--gpus', '5', '--project', 'project'], False ),
    ( ['gpu-shared@expanse', '--cpus', '4', '--mem_mb', '1024', '--nodes', '1', '--gpus', '5', '--project', 'project'], False ),

    ( ['gpu-shared@expanse', '--cpus', '80', '--mem_mb', '1024', '--gpus', '1', '--project', 'project'], False ),
    ( ['gpu-shared@expanse', '--cpus', '4', '--mem_mb', str(1024*1024), '--gpus', '1', '--project', 'project'], False),
    ( ['gpu-shared@expanse', '--cpus', '80', '--mem_mb', '1024', '--nodes', '1', '--gpus', '1', '--project', 'project'], False ),
    ( ['gpu-shared@expanse', '--cpus', '4', '--mem_mb', str(1024*1024), '--nodes', '1', '--gpus', '1', '--project', 'project'], False),

    # Verify that specifying a GPU type or a GPU count for a queue without
    # GPUs fails.
    ( ['normal@stampede2'], True ),
    ( ['normal@stampede2', '--gpus', '1'], False ),
    ( ['normal@stampede2', '--gpu-type', 'default'], False ),

    # Verify that not specifying a GPU type, or specifying an invalid GPU
    # type, for a GPU queue that requires one fails.
    ( ['GPU@bridges2', '--gpus', 'v100-16:8'], True ),
    ( ['GPU@bridges2', '--gpus', '8'], False ),
    ( ['GPU@bridges2', '--gpus', 'not-a-type:8'], False ),
    ( ['GPU@bridges2', '--gpus', '8', '--gpu-type', 'not-a-type'], False ),

    # Verify that specifying a GPU type (with `:` or `--gpu-type`) for
    # a GPU queue that doesn't have types fails.
    ( ['gpu@anvil', '--nodes', '2', '--gpus', 'a_type:4', ], False ),
    ( ['gpu@anvil', '--nodes', '2', '--gpus', '4', '--gpu-type', 'a_type' ], False ),

    # Verify that queues respect max_duration.
    ( ['normal@stampede2', '--nodes', '2', '--lifetime', str((48 * 60 * 60) - 1)], True ),
    ( ['normal@stampede2', '--nodes', '2', '--lifetime', str((48 * 60 * 60) + 1)], False ),

    ( ['development@stampede2', '--nodes', '2', '--lifetime', str((2 * 60 * 60) - 1)], True ),
    ( ['development@stampede2', '--nodes', '2', '--lifetime', str((2 * 60 * 60) + 1)], False ),

    ( ['wholenode@anvil', '--nodes', '2', '--lifetime', str((96 * 60 * 60) - 1)], True ),
    ( ['wholenodel@anvil', '--nodes', '2', '--lifetime', str((96 * 60 * 60) + 1)], False ),

    ( ['wide@anvil', '--nodes', '2', '--lifetime', str((12 * 60 * 60) - 1)], True ),
    ( ['wide@anvil', '--nodes', '2', '--lifetime', str((12 * 60 * 60) + 1)], False ),

    ( ['EM@bridges2', '--cpus', '24', '--lifetime', str((120 * 60 * 60) - 1)], True ),
    ( ['EM@bridges2', '--cpus', '24', '--lifetime', str((120 * 60 * 60) + 1)], False ),
]


@pytest.fixture(params=TEST_CASES)
def test_case(request):
    return (request.param[0], request.param[1])


@pytest.fixture
def args(test_case):
    return test_case[0]


@pytest.fixture
def expected(test_case):
    return test_case[1]


def test_hac(args, expected):
    env = {**os.environ, '_CONDOR_HPC_ANNEX_ENABLED': 'TRUE',}
    rv = subprocess.run(['htcondor', 'annex', 'create', 'example', '--test', '1', *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env,
        timeout=20)

    if expected:
        assert rv.returncode == 0 and rv.stdout == b''
    else:
        assert rv.returncode == 1 and rv.stdout.startswith(b'Error while trying to run annex create:')
