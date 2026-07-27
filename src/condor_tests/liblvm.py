# Copyright 2024 HTCondor Team, Computer Sciences Department,
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

"""
Shared helpers for the LVM startd-disk-enforcement regression tests
(test_lvm_*.py). Feature lives in src/condor_startd.V6/VolumeManager.{h,cpp}
and src/condor_starter.V6.1/starter.cpp; Linux-only, requires root.

Manual PV/VG/thin-pool helpers mirror setup/create-backing-lvm-pieces.sh
from https://github.com/ColeBollig/htcondor-lvm-integration.
"""

import os
import platform
import shutil
import subprocess

from pathlib import Path

# Fits comfortably on a CI runner while leaving room for an ext4 filesystem
# plus a couple of small per-job LVs.
SMALL_BACKING_FILE_MB = 512

# Default LVM_THIN_LV_EXTRA_SIZE_MB (2000 MB) exceeds our backing store.
SMALL_THIN_EXTRA_MB = 32

REQUIRED_LVM_TOOLS = (
    "losetup",
    "pvcreate", "pvremove", "pvs",
    "vgcreate", "vgremove", "vgs",
    "lvcreate", "lvremove", "lvs",
    "mkfs.ext4",
    "sudo",
)

# SlotBrokenReason value for a d-slot whose LV failed to clean up.
# See src/condor_startd.V6/claim.cpp, STARTER_EXIT_IMMORTAL_LVM.
LV_BROKEN_REASON = "Could not clean up Logical Volume"


def using_sudo():
    return os.environ.get("HTCONDOR_TEST_USE_SUDO") == "1"


def is_root():
    return hasattr(os, "geteuid") and os.geteuid() == 0


def lvm_tools_present():
    return all(shutil.which(tool) is not None for tool in REQUIRED_LVM_TOOLS)


def LVMTestable():
    """
    True if this host can exercise LVM disk enforcement tests: Linux,
    running as (or able to reach root via passwordless sudo) root, with the
    lvm2/e2fsprogs command line tools installed.
    """
    if platform.system() != "Linux":
        return False
    if not (is_root() or using_sudo()):
        return False
    return lvm_tools_present()


LVM_SKIP_REASON = (
    "LVM disk enforcement tests require Linux, root (or "
    "HTCONDOR_TEST_USE_SUDO=1 with passwordless sudo), and the "
    "lvm2/e2fsprogs command line tools"
)


def lvm_config(thin, backing_file, backing_size_mb=SMALL_BACKING_FILE_MB,
               thin_extra_mb=SMALL_THIN_EXTRA_MB, extra=None):
    """
    Base HTCondor config for a pool using the startd's automatic
    loopback-backed LVM setup (i.e. LVM_VOLUME_GROUP_NAME left unset).
    """
    config = {
        "STARTD_ENFORCE_DISK_LIMITS": "True",
        "LVM_BACKING_FILE": str(backing_file),
        "LVM_BACKING_FILE_SIZE_MB": str(backing_size_mb),
        # Unique per process so concurrent ctest runs don't share a VG.
        "LVM_AUTO_VG_NAME": f"condor_test_{os.getpid()}",
        # Surfaces VolumeManager's mount/namespace trace lines.
        "STARTER_DEBUG": "D_FULLDEBUG",
        "STARTD_DEBUG": "D_FULLDEBUG",
    }
    if thin:
        config["LVM_USE_THIN_PROVISIONING"] = "True"
        config["LVM_THIN_LV_EXTRA_SIZE_MB"] = str(thin_extra_mb)
    if extra:
        config.update(extra)
    return config


def root_cmd(args):
    """Prefix a command with sudo -n (requires passwordless sudo)."""
    return ["sudo", "-n"] + [str(a) for a in args]


def run_root(args, check=True, **kwargs):
    kwargs.setdefault("stdout", subprocess.PIPE)
    kwargs.setdefault("stderr", subprocess.STDOUT)
    kwargs.setdefault("text", True)
    return subprocess.run(root_cmd(args), check=check, **kwargs)


class ManualLVMStore:
    """
    Loopback-backed PV/VG/thin-pool a test can point
    LVM_VOLUME_GROUP_NAME/LVM_THINPOOL_NAME at, instead of the startd's
    automatic loopback setup.
    """

    def __init__(self, backing_file, vg_name, size_mb=SMALL_BACKING_FILE_MB, thinpool_name=None):
        self.backing_file = Path(backing_file)
        self.vg_name = vg_name
        self.thinpool_name = thinpool_name
        self.size_mb = size_mb
        self.loop_dev = None

    def create(self):
        self.backing_file.parent.mkdir(parents=True, exist_ok=True)
        with open(self.backing_file, "wb") as f:
            f.truncate(self.size_mb * 1024 * 1024)
        self.loop_dev = run_root(["losetup", "--show", "-f", str(self.backing_file)]).stdout.strip()
        run_root(["pvcreate", "-f", self.loop_dev])
        run_root(["vgcreate", self.vg_name, self.loop_dev])
        if self.thinpool_name:
            run_root(["lvcreate", "--type", "thin-pool", "-l", "90%FREE", self.vg_name, "-n", self.thinpool_name])
        return self

    def destroy(self):
        if self.thinpool_name:
            run_root(["lvremove", "-f", f"{self.vg_name}/{self.thinpool_name}"], check=False)
        run_root(["vgremove", "-f", self.vg_name], check=False)
        if self.loop_dev:
            run_root(["pvremove", "-f", self.loop_dev], check=False)
            run_root(["losetup", "-d", self.loop_dev], check=False)
        if self.backing_file.exists():
            self.backing_file.unlink()

    def __enter__(self):
        self.create()
        return self

    def __exit__(self, *exc):
        self.destroy()


def create_foreign_lv(vg_name, lv_name, size_mb):
    """Create an LV without condor's htcondor_lv tag (VolumeManager.cpp)."""
    run_root(["lvcreate", "-n", lv_name, "-L", f"{size_mb}M", vg_name])


def remove_foreign_lv(vg_name, lv_name):
    run_root(["lvremove", "-f", f"{vg_name}/{lv_name}"], check=False)


def vg_exists(vg_name):
    result = run_root(["vgs", "--noheadings", "-o", "vg_name"], check=False)
    return vg_name in result.stdout.split()


def pv_exists(pv_path):
    result = run_root(["pvs", "--noheadings", "-o", "pv_name"], check=False)
    return pv_path in result.stdout.split()


def loop_dev_for_file(backing_file):
    result = run_root(["losetup", "-j", str(backing_file)], check=False)
    if not result.stdout.strip():
        return None
    return result.stdout.split(":")[0].strip()
