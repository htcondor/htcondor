#!/usr/bin/env pytest

#
# A job's sandbox layout must not depend on whether its common input files
# arrived by common file transfer (staged once per EP, then mapped) or by
# ordinary file transfer (the fallback, forced here with
# FORBID_COMMON_FILE_TRANSFER).  In particular, preserve_relative_paths must
# be honored both ways.  [HTCONDOR-4042]
#
# We run the same jobs, with and without preserve_relative_paths, against a
# pool which uses common file transfer and against one which forbids it.
# Each job reports the files in its sandbox and their link counts.  Each
# job must see the same files in the same places either way.  The common
# pool maps by hardlinking, so there every common file must be a hardlink
# (to the staged copy); in the fallback pool, none may be.
#

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

from ornithology import (
    Condor,
    action,
    ClusterState,
    format_script,
    write_file,
)

import json
import sys


# "dir/a.txt" is a file in a subdirectory; "tree/" is the contents of a
# directory with its own subdirectory.
COMMON_INPUT = "dir/a.txt, tree/"

EXPECTED_LAYOUT = {
    True:   {"dir/a.txt", "tree/sub/b.txt"},
    False:  {"a.txt", "sub/b.txt"},
}


def make_condor(test_dir, name, config):
    return Condor(
        local_dir=test_dir / name,
        config={
            "NUM_CPUS":         2,
            "SHADOW_DEBUG":     "D_CATEGORY D_SUB_SECOND D_PID D_FULLDEBUG",
            ** config,
        },
    )


@action
def common_condor(test_dir):
    # The link-count check depends on mapping by hardlink, which these
    # settings select (they're the defaults, but say so).
    with make_condor(test_dir, "common.d", {
        "FORBID_HARDLINK_MAPPING":      "FALSE",
        "STARTD_ENFORCE_DISK_LIMITS":   "FALSE",
    }) as the_condor:
        yield the_condor


@action
def fallback_condor(test_dir):
    with make_condor(test_dir, "fallback.d", {
        "FORBID_COMMON_FILE_TRANSFER": "TRUE",
    }) as the_condor:
        yield the_condor


@action
def user_dir(test_dir):
    d = test_dir / "user"
    (d / "dir").mkdir(parents=True, exist_ok=True)
    (d / "dir" / "a.txt").write_text("a\n")
    (d / "tree" / "sub").mkdir(parents=True, exist_ok=True)
    (d / "tree" / "sub" / "b.txt").write_text("b\n")
    return d


@action
def report_script(test_dir):
    # Print {relative path: link count} for every .txt file in the sandbox.
    script_path = test_dir / "report_sandbox.py"
    write_file(script_path, format_script(
    f"""
        #!{sys.executable}
        import json, os
        report = {{}}
        for root, dirs, files in os.walk("."):
            for f in files:
                if f.endswith(".txt"):
                    path = os.path.normpath(os.path.join(root, f))
                    report[path] = os.lstat(path).st_nlink
        print(json.dumps(report))
    """
    ))
    return script_path


def run_job(the_condor, test_dir, user_dir, report_script, name, preserve):
    handle = the_condor.submit(
        description={
            "universe":                 "vanilla",
            "executable":               report_script.as_posix(),
            "transfer_executable":      True,
            "should_transfer_files":    "YES",
            "initialdir":               user_dir.as_posix(),
            "transfer_common_input":    COMMON_INPUT,
            "preserve_relative_paths":  preserve,
            "request_cpus":             1,
            "request_memory":           1,
            # The staged catalog's (quantized) size comes out of the
            # prompting job's disk request, so leave some room.
            "request_disk":             "10M",
            "log":                      (test_dir / f"{name}.log").as_posix(),
            "output":                   (test_dir / f"{name}.out").as_posix(),
            "error":                    (test_dir / f"{name}.err").as_posix(),
        },
        count=1,
    )
    assert handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        timeout=180,
    )
    report = json.loads((test_dir / f"{name}.out").read_text())
    logger.debug(f"{name}: {report}")
    return report


@action(params={"preserve": True, "flatten": False})
def preserve(request):
    return request.param


@action
def common_report(common_condor, test_dir, user_dir, report_script, preserve):
    return run_job(common_condor, test_dir, user_dir, report_script, f"common_{preserve}", preserve)


@action
def fallback_report(fallback_condor, test_dir, user_dir, report_script, preserve):
    return run_job(fallback_condor, test_dir, user_dir, report_script, f"fallback_{preserve}", preserve)


class TestCIFPreserveRelativePaths:

    def test_fallback_layout(self, fallback_report, preserve):
        assert set(fallback_report) == EXPECTED_LAYOUT[preserve]

    def test_fallback_files_were_transferred(self, fallback_report):
        assert all(nlink == 1 for nlink in fallback_report.values())

    def test_common_layout(self, common_report, preserve):
        assert set(common_report) == EXPECTED_LAYOUT[preserve]

    def test_common_files_were_mapped(self, common_report):
        # A link count of 1 would mean the file wasn't mapped from the
        # staged copy, and the layout check would prove nothing.
        assert all(nlink >= 2 for nlink in common_report.values())
