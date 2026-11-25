#!/usr/bin/env pytest

# Pure python unit test of htcondor2._utils.safe_file; no daemons required

import os
import stat

import pytest

from htcondor2._utils.safe_file import (
    UnsafePathError,
    atomic_write,
    is_safe_regular_file,
    safe_makedirs,
    safe_open_read,
)

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


POSIX_ONLY = pytest.mark.skipif(os.name == "nt", reason="POSIX permissions/ownership")


def make_symlink(target, link):
    """Create a symlink or skip the test where symlinks are unavailable (e.g. unprivileged Windows)"""
    try:
        os.symlink(target, link, target_is_directory=os.path.isdir(target))
    except (OSError, NotImplementedError) as e:
        pytest.skip(f"cannot create symlinks: {e}")


def other_uid(path):
    """A uid that does not own path"""
    return os.lstat(path).st_uid + 1


class TestSafeMakedirs:

    def test_creates_nested(self, tmp_path):
        target = tmp_path / "a" / "b"
        safe_makedirs(target)
        assert target.is_dir()


    def test_existing_ok(self, tmp_path):
        target = tmp_path / "cache"
        safe_makedirs(target)
        safe_makedirs(target)
        assert target.is_dir()


    @POSIX_ONLY
    def test_private_mode(self, tmp_path):
        target = tmp_path / "cache"
        safe_makedirs(target)
        assert stat.S_IMODE(os.lstat(target).st_mode) & 0o077 == 0


    @POSIX_ONLY
    def test_rejects_other_writable(self, tmp_path):
        target = tmp_path / "cache"
        target.mkdir()
        os.chmod(target, 0o777)
        with pytest.raises(UnsafePathError):
            safe_makedirs(target)


    @POSIX_ONLY
    @pytest.mark.parametrize("existing_mode, mode", [
        (0o770, 0o770),  # group writable, group write granted
        (0o750, 0o770),  # fewer permissions than granted is fine
        (0o777, 0o777),  # world writable, world write granted
        (0o703, 0o707),  # world writable without group write
    ])
    def test_accepts_writers_granted_by_mode(self, tmp_path, existing_mode, mode):
        target = tmp_path / "cache"
        target.mkdir()
        os.chmod(target, existing_mode)
        safe_makedirs(target, mode=mode)


    @POSIX_ONLY
    @pytest.mark.parametrize("existing_mode, mode", [
        (0o777, 0o770),  # world write not granted
        (0o770, 0o707),  # group write not granted
        (0o720, 0o755),  # read/execute for group does not grant write
    ])
    def test_rejects_writers_not_granted_by_mode(self, tmp_path, existing_mode, mode):
        target = tmp_path / "cache"
        target.mkdir()
        os.chmod(target, existing_mode)
        with pytest.raises(UnsafePathError):
            safe_makedirs(target, mode=mode)


    @POSIX_ONLY
    def test_creates_with_granted_mode(self, tmp_path):
        # umask may strip bits, but never adds any, so creation always passes
        target = tmp_path / "shared"
        safe_makedirs(target, mode=0o770)
        assert stat.S_IMODE(os.lstat(target).st_mode) & 0o007 == 0


    @POSIX_ONLY
    def test_rejects_other_owner(self, tmp_path, monkeypatch):
        target = tmp_path / "cache"
        target.mkdir(mode=0o700)
        monkeypatch.setattr(os, "geteuid", lambda: other_uid(target))
        with pytest.raises(UnsafePathError):
            safe_makedirs(target)


    def test_rejects_symlink(self, tmp_path):
        real = tmp_path / "real"
        real.mkdir(mode=0o700)
        link = tmp_path / "link"
        make_symlink(real, link)
        with pytest.raises(OSError):
            safe_makedirs(link)


    def test_rejects_file(self, tmp_path):
        target = tmp_path / "file"
        target.write_text("x")
        with pytest.raises(OSError):
            safe_makedirs(target)


class TestAtomicWrite:

    def test_writes_new_file(self, tmp_path):
        target = tmp_path / "out.csv"
        with atomic_write(target) as f:
            f.write("a,b\n1,2\n")
        assert target.read_text() == "a,b\n1,2\n"
        assert os.listdir(tmp_path) == ["out.csv"]


    def test_newlines_preserved(self, tmp_path):
        target = tmp_path / "out.csv"
        with atomic_write(target, newline="") as f:
            f.write("a,b\r\n1,2\r\n")
        assert target.read_bytes() == b"a,b\r\n1,2\r\n"


    def test_replaces_existing(self, tmp_path):
        target = tmp_path / "out.csv"
        target.write_text("old")
        with atomic_write(target) as f:
            f.write("new")
        assert target.read_text() == "new"


    @POSIX_ONLY
    def test_private_mode(self, tmp_path):
        target = tmp_path / "out.csv"
        with atomic_write(target) as f:
            f.write("x")
        assert stat.S_IMODE(os.lstat(target).st_mode) == 0o600


    def test_error_keeps_original(self, tmp_path):
        target = tmp_path / "out.csv"
        target.write_text("original")
        with pytest.raises(RuntimeError):
            with atomic_write(target) as f:
                f.write("partial")
                raise RuntimeError("boom")
        assert target.read_text() == "original"
        assert os.listdir(tmp_path) == ["out.csv"]


    def test_replaces_symlink_without_following(self, tmp_path):
        victim = tmp_path / "victim"
        victim.write_text("precious")
        link = tmp_path / "out.csv"
        make_symlink(victim, link)
        with atomic_write(link) as f:
            f.write("data")
        assert victim.read_text() == "precious"
        assert not link.is_symlink()
        assert link.read_text() == "data"


    def test_dangling_symlink_not_followed(self, tmp_path):
        victim = tmp_path / "victim"
        link = tmp_path / "out.csv"
        make_symlink(victim, link)
        with atomic_write(link) as f:
            f.write("data")
        assert not victim.exists()
        assert link.read_text() == "data"


class TestSafeOpenRead:

    def test_reads_regular_file(self, tmp_path):
        target = tmp_path / "in.csv"
        target.write_text("a,b\n")
        with safe_open_read(target) as f:
            assert f.read() == "a,b\n"


    def test_missing_file(self, tmp_path):
        with pytest.raises(FileNotFoundError):
            safe_open_read(tmp_path / "missing.csv")


    def test_rejects_symlink(self, tmp_path):
        victim = tmp_path / "victim"
        victim.write_text("secret")
        link = tmp_path / "in.csv"
        make_symlink(victim, link)
        with pytest.raises(OSError):
            safe_open_read(link)


    def test_rejects_directory(self, tmp_path):
        with pytest.raises(OSError):
            safe_open_read(tmp_path)


    @POSIX_ONLY
    def test_rejects_other_owner(self, tmp_path, monkeypatch):
        target = tmp_path / "in.csv"
        target.write_text("x")
        monkeypatch.setattr(os, "geteuid", lambda: other_uid(target))
        with pytest.raises(UnsafePathError):
            safe_open_read(target)


    def test_newlines_preserved(self, tmp_path):
        # csv module output uses \r\n; the reader must see it unchanged (newline="")
        target = tmp_path / "in.csv"
        target.write_bytes(b"a,b\r\n1,2\r\n")
        with safe_open_read(target, newline="") as f:
            assert f.read() == "a,b\r\n1,2\r\n"


class TestIsSafeRegularFile:

    def test_regular_file(self, tmp_path):
        target = tmp_path / "f"
        target.write_text("x")
        assert is_safe_regular_file(target)


    def test_missing(self, tmp_path):
        assert not is_safe_regular_file(tmp_path / "missing")


    def test_directory(self, tmp_path):
        assert not is_safe_regular_file(tmp_path)


    def test_symlink(self, tmp_path):
        victim = tmp_path / "victim"
        victim.write_text("x")
        link = tmp_path / "f"
        make_symlink(victim, link)
        assert not is_safe_regular_file(link)


    @POSIX_ONLY
    def test_other_owner(self, tmp_path, monkeypatch):
        target = tmp_path / "f"
        target.write_text("x")
        monkeypatch.setattr(os, "geteuid", lambda: other_uid(target))
        assert not is_safe_regular_file(target)
