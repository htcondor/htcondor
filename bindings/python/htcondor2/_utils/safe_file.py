#!/usr/bin/python3

# Copyright 2026 HTCondor Team, Computer Sciences Department,
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
Symlink-safe file helpers for tools that keep files in directories they
may not exclusively own (e.g. a cache under the current working directory).
Python counterpart to the C++ safe_open_wrapper() family in src/safefile/.
"""

import contextlib
import os
import stat
import tempfile
from typing import Iterator, Optional, TextIO, Union

PathLike = Union[str, "os.PathLike[str]"]

# Not available on Windows; symlinks are then rejected via lstat() checks
_O_NOFOLLOW = getattr(os, "O_NOFOLLOW", 0)
# Windows only: keep the C runtime from translating newlines under Python's text layer
_O_BINARY = getattr(os, "O_BINARY", 0)


class UnsafePathError(OSError):
    """Raised when a path is not safe to read from or write to"""


def _check_owner(st: os.stat_result, path: str) -> None:
    """Require the current (effective) user to own the file (POSIX only)"""
    if hasattr(os, "geteuid") and st.st_uid != os.geteuid():
        raise UnsafePathError(f"{path} is not owned by the current user")


def safe_makedirs(path: PathLike, mode: int = 0o700) -> None:
    """
    Create a directory (and parents) owned by the current user. The directory
    may only be writable by group/others if mode grants it (the default 0o700
    allows neither); granting it lets those users add, remove, or replace
    entries, so pair it with atomic_write()/safe_open_read().
    """
    path = os.fspath(path)
    os.makedirs(path, mode=mode, exist_ok=True)

    st = os.lstat(path)
    if stat.S_ISLNK(st.st_mode) or not stat.S_ISDIR(st.st_mode):
        raise UnsafePathError(f"{path} is not a directory")
    _check_owner(st, path)
    # Write access for group/others is only acceptable if the caller's mode grants it
    unexpected = st.st_mode & (stat.S_IWGRP | stat.S_IWOTH) & ~mode
    if os.name != "nt" and unexpected:
        raise UnsafePathError(f"{path} is writable by other users")


def is_safe_regular_file(path: PathLike) -> bool:
    """Check that path is a regular file (not a symlink) owned by the current user"""
    try:
        st = os.lstat(os.fspath(path))
    except OSError:
        return False
    if not stat.S_ISREG(st.st_mode):
        return False
    try:
        _check_owner(st, os.fspath(path))
    except UnsafePathError:
        return False
    return True


def safe_open_read(path: PathLike, encoding: str = "utf-8", newline: Optional[str] = None) -> TextIO:
    """Open an existing regular file owned by the current user for reading, without following symlinks"""
    path = os.fspath(path)

    # Pre-check covers platforms without O_NOFOLLOW (missing file raises FileNotFoundError)
    pre = os.lstat(path)
    if stat.S_ISLNK(pre.st_mode):
        raise UnsafePathError(f"{path} is a symbolic link")

    fd = os.open(path, os.O_RDONLY | _O_NOFOLLOW | _O_BINARY)
    try:
        st = os.fstat(fd)
        if not stat.S_ISREG(st.st_mode):
            raise UnsafePathError(f"{path} is not a regular file")
        # The file opened must be the one that was checked
        if (st.st_dev, st.st_ino) != (pre.st_dev, pre.st_ino):
            raise UnsafePathError(f"{path} changed while being opened")
        _check_owner(st, path)
        return os.fdopen(fd, "r", encoding=encoding, newline=newline)
    except BaseException:
        os.close(fd)
        raise


@contextlib.contextmanager
def atomic_write(path: PathLike, encoding: str = "utf-8", newline: Optional[str] = None) -> Iterator[TextIO]:
    """
    Write a file atomically: content goes to a new private (0600) temporary
    file in the same directory, which then replaces path. An existing symlink
    at path is replaced, never followed. On error path is left untouched.
    """
    path = os.fspath(path)
    directory = os.path.dirname(os.path.abspath(path))
    # mkstemp creates the file exclusively (O_EXCL, and O_NOFOLLOW where available) with mode 0600
    fd, tmp_path = tempfile.mkstemp(dir=directory, prefix=f".{os.path.basename(path)}.", suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding=encoding, newline=newline) as f:
            yield f
        os.replace(tmp_path, path)
    except BaseException:
        with contextlib.suppress(OSError):
            os.unlink(tmp_path)
        raise
