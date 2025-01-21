#!/usr/bin/python3

import os
import platform
import shutil
import errno
import argparse
import sys


BASE_CONFIG_TEMPLATE = """
# Start with a standard personal condor configuration
# (This is based on make-personal-from-tarball)
RELEASE_DIR={prefix_path}
LOCAL_DIR = {pwd}/local_dir
LOCAL_CONFIG_DIR = $(LOCAL_DIR)/config.d
use security: user_based
use role: personal
CONDOR_HOST = $(FULL_HOSTNAME)

# Add some tweaks for the testing environment
NEGOTIATOR_CYCLE_DELAY=1
NEGOTIATOR_INTERVAL=1
NEGOTIATOR_MIN_INTERVAL=1
SCHEDD_INTERVAL=1
SCHEDD_MIN_INTERVAL=1
DAGMAN_USER_LOG_SCAN_INTERVAL = 1
MAIL={true_path}
SENDMAIL={true_path}
LOCAL_CONFIG_FILE={pwd}/condor_config.local
DOCKER_PERFORM_TEST=false
# The default FILETRANSFER_PLUGINS contains support for box,onedrive,
# and gdrive.  On startup, the startd runs each of these with the -classad
# option to probe.  However, this is somewhat slow, as they are each implemented
# in python, and we test none of them.  Ideally, I'd like to just be able to
# remove those from the default, but we don't have a good way to do that.
FILETRANSFER_PLUGINS = $(LIBEXEC)/curl_plugin

# ctest allows tests to run for up to 1500 seconds; after that, we are
# potentially a runaway personal condor instance.
DAEMON_SHUTDOWN=time() - DaemonStartTime > 1500

# The param table assumes singularity/apptainer has been installed
# from the tarball into libexec.  Let's use the system onedrive

SINGULARITY = /usr/bin/singularity
{extra_args}
"""


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("--test", help="Name of HTCondor test")
    parser.add_argument(
        "--working-dir",
        help="Name of top-level CMake binary directory",
        default=".",
        dest="working_dir",
    )
    parser.add_argument(
        "--source-dir",
        help="Name of top-level HTCondor source directory",
        default=".",
        dest="source_dir",
    )
    parser.add_argument(
        "--prefix-path",
        help="Location of the install prefix to test",
        default=".",
        dest="prefix_path",
    )
    parser.add_argument("--dependencies", help="Source file dependencies")
    parser.add_argument(
        "--java",
        help="Enable Java universe",
        dest="java",
        action="store_true",
        default=False,
    )

    return parser.parse_args()


def write_base_config(prefix_path, java=False):
    extra_args = ""
    if not java:
        extra_args += "JAVA="
    with open("condor_config.local", "w") as fp:
        pass
    os.umask(0o022)
    os.makedirs("local_dir")
    pwd = os.getcwd()
    with open("base_config", "w") as fp:
        fp.write(
            BASE_CONFIG_TEMPLATE.format(
                pwd=pwd, prefix_path=prefix_path, extra_args=extra_args,
                true_path="/usr/bin/true" if sys.platform == "darwin" else "/bin/true"
            )
        )


def main():
    args = parse_args()

    rundir = os.path.abspath(
        os.path.join(args.working_dir, "src", "condor_tests", args.test + "_ctest")
    )
    if os.path.exists(rundir):
        shutil.rmtree(rundir)
    os.makedirs(rundir)
    os.chdir(rundir)

    base_src_fname = os.path.join(args.source_dir, "src", "condor_tests", args.test)
    possibilities = [
        base_src_fname + ".run",
        base_src_fname + ".py",
    ]
    for possibility in possibilities:
        if os.path.exists(possibility):
            src_fname = possibility
            break
    else:  # i.e., if we did not break, do this...
        raise Exception(
            "Test file named {} with suffix .run or .py not found!".format(args.test)
        )

    dst_fname = os.path.join(rundir, os.path.split(src_fname)[1])
    shutil.copy(src_fname, dst_fname)

    for src_fname in args.dependencies.split(";"):
        if not src_fname:
            continue
        fname = os.path.split(src_fname)[1]
        dst_fname = os.path.join(rundir, fname)
        src_fname = os.path.join(args.source_dir, src_fname)
        if os.path.isdir(src_fname):
            shutil.copytree(src_fname, dst_fname)
        else:
            shutil.copy(src_fname, dst_fname)

    # TODO: there is much environment munging and path construction below that won't work on Windows

    os.environ["PERL5LIB"] = "..:."

    new_library_path = os.path.join(args.prefix_path, "lib")
    if "LD_LIBRARY_PATH" in os.environ:
        os.environ["LD_LIBRARY_PATH"] = (
            new_library_path + ":" + os.environ["LD_LIBRARY_PATH"]
        )
    else:
        os.environ["LD_LIBRARY_PATH"] = new_library_path

    # macOS clears DYLD_LIBRARY_PATH from the environment of system
    # programs (e.g. /usr/bin/perl), so we can't use that to have the
    # condor_tests binaries find the condor libraries.
    # Making a symlink in src/condor_tests is our best option here.
    if platform.system() == "Darwin":
        try:
            symlink_dst = os.path.join(args.working_dir, "src", "condor_tests", "lib")
            os.symlink(new_library_path, symlink_dst)
        except FileExistsError:
            pass

    os.environ["PATH"] = ":".join(
        [
            os.path.join(args.prefix_path, "bin"),
            os.path.join(args.prefix_path, "sbin"),
            "/bin",
            "/sbin",
            "/usr/sbin",
            "/usr/bin",
        ]
    )

    pythonpath = os.environ.setdefault("PYTHONPATH", "")
    add_to_path = os.path.join(args.prefix_path, "lib", "python")
    if add_to_path not in pythonpath:
        os.environ["PYTHONPATH"] = ":".join([".", add_to_path, pythonpath])

    write_base_config(args.prefix_path, java=args.java)
    # This is not re-generated each time.
    if os.path.exists("derived_condor_config"):
        os.unlink("derived_condor_config")

    os.environ["CONDOR_CONFIG"] = os.path.abspath("base_config")

    command_line = "perl" + " " + os.path.join("..","run_test.pl" ) + " " + args.test

    r = os.system(command_line)
    exit(r >> 8)


if __name__ == "__main__":
    main()
