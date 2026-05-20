#!/usr/bin/env pytest

# Before the fix, a job that left a FIFO (named pipe) in its scratch
# directory would hang the output file transfer: the shadow would
# open() the FIFO and block forever waiting for a writer, eventually
# causing the job to fail or be held. After the fix, named pipes are
# silently skipped by the file transfer logic and the job completes.

from ornithology import *


@standup
def condor(test_dir):
    # Hold any job that throws a shadow exception so a hang on output
    # transfer turns into a quick, observable test failure rather than
    # a timeout.
    with Condor(
        test_dir / "condor",
        config={
            "PERIODIC_EXPR_INTERVAL": "1",
            "SYSTEM_PERIODIC_HOLD": "(NumShadowExceptions >= 1)",
        },
    ) as condor:
        yield condor


@action
def job_script_contents():
    return format_script("""
#!/usr/bin/env python3
import os

# Make a named pipe in the scratch directory.  With the old code,
# the shadow would attempt to read from this on output transfer
# and block forever.
os.mkfifo("my_fifo")

# Also leave a normal file behind so we can confirm that ordinary
# output transfer still works while the FIFO is skipped.
with open("regular_output.txt", "w") as f:
    f.write("hello from job\\n")

exit(0)
""")


@action
def job_script(test_dir, job_script_contents):
    script_path = test_dir / "fifo_job.py"
    write_file(script_path, job_script_contents)
    return script_path


@action
def submitted_job(condor, test_dir, path_to_python, job_script):
    submit = {
        "executable": str(path_to_python),
        "arguments": str(job_script),
        "universe": "vanilla",
        "should_transfer_files": "YES",
        "when_to_transfer_output": "ON_EXIT",
        "transfer_executable": "False",
        "output": "job.out",
        "error": "job.err",
        "log": "job.log",
    }
    ctj = condor.submit(submit, count=1)
    assert ctj.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        timeout=120,
        verbose=True,
    )
    return ctj


class TestFifoInScratch:
    def test_job_completed(self, submitted_job):
        # If the FIFO is not skipped, output transfer hangs and the
        # SYSTEM_PERIODIC_HOLD expression in the @standup will fire,
        # or this would simply time out.  Reaching this point means
        # the job completed normally.
        assert submitted_job.state.all_complete()

    def test_regular_output_transferred(self, submitted_job, test_dir):
        # Sanity check: ordinary output files alongside the FIFO are
        # still transferred back normally.
        regular = test_dir / "regular_output.txt"
        assert regular.exists()
        assert regular.read_text() == "hello from job\n"

    def test_fifo_not_transferred(self, submitted_job, test_dir):
        # The FIFO itself should not appear in the submit directory:
        # file transfer should have excluded it entirely.
        assert not (test_dir / "my_fifo").exists()
