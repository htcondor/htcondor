#!/usr/bin/env pytest

"""
Test condor_rm -transfer functionality.

This test verifies that the condor_rm -transfer option correctly:
1. Transfers job output files to the final destination (submit directory)
2. Then removes the job from the queue

The test submits a job that creates an output file, waits for it to start
running, then uses condor_rm -transfer to remove it. The test verifies
that the output file was successfully transferred before the job was removed.
"""

import time

from ornithology import (
    action,
    ClusterState,
)

@action
def output_file(test_dir):
    """The expected output file name"""
    return test_dir / "my_output_file"


@action
def the_job(default_condor, test_dir, output_file):
    """Submit a job that creates an output file"""
    the_job = default_condor.submit(
        {
            "shell":                    "touch my_output_file && sleep 3600",
            "log":                      (test_dir / "the_job.log").as_posix(),
            "error":                    (test_dir / "the_job.err").as_posix(),
            "should_transfer_files":    "YES",
            "when_to_transfer_output":  "ON_EXIT_OR_EVICT",
            "transfer_output_files":    "my_output_file",
        }
    )

    # Wait for the job to start running
    assert the_job.wait(
        condition=ClusterState.any_running,
        timeout=60,
    )

    # Give job time to create the output file
    time.sleep(2)

    return the_job


@action
def rm_with_transfer(default_condor, the_job, test_dir):
    """Remove the job using condor_rm -transfer"""
    job_id = f"{the_job.clusterid}.0"
    
    # Run condor_rm -transfer to transfer output and remove
    result = default_condor.run_command(["condor_rm", "-transfer", job_id])

    # Wait for the job to be removed
    assert the_job.wait(
        condition=ClusterState.all_terminal,
        timeout=60,
    )
    
    return result


class TestCondorRmTransfer:

    def test_condor_rm_transfer_succeeds(self, rm_with_transfer):
        """Test that condor_rm -transfer command succeeds"""
        assert rm_with_transfer.returncode == 0

    def test_condor_rm_transfer_reports_transfer(self, rm_with_transfer):
        """Test that condor_rm -transfer reports the job was marked for removal with file transfer"""
        assert "marked for removal after file transfer" in rm_with_transfer.stdout

    def test_output_file_was_transferred(self, rm_with_transfer, output_file):
        """Test that the output file was transferred to submit directory"""
        assert output_file.exists(), f"Output file {output_file} was not transferred"
        # File should exist and be a regular file
        assert output_file.is_file(), f"Output file {output_file} is not a regular file"

    def test_job_was_removed(self, the_job, rm_with_transfer):
        """Test that the job was actually removed from the queue"""
        # Query the job - it should either be removed or gone from queue
        job_ads = the_job.query()
        if len(job_ads) > 0:
            # If still in queue, should be removed (JobStatus == 3)
            assert job_ads[0].get("JobStatus") == 3, f"Job status is {job_ads[0].get('JobStatus')}, expected 3 (REMOVED)"
            # Check the remove reason contains our transfer message
            remove_reason = job_ads[0].get("RemoveReason", "")
            assert "condor_rm -transfer" in remove_reason, f"RemoveReason doesn't mention transfer: {remove_reason}"


# =============================================================================
# Test condor_rm -transfer with default ON_EXIT (not ON_EXIT_OR_EVICT)
# =============================================================================

@action
def on_exit_output_file(test_dir):
    """The expected output file name for ON_EXIT test"""
    return test_dir / "on_exit_output_file"


@action
def on_exit_job(default_condor, test_dir, on_exit_output_file):
    """Submit a job with default when_to_transfer_output (ON_EXIT)"""
    the_job = default_condor.submit(
        {
            "shell":                    "touch on_exit_output_file && sleep 3600",
            "log":                      (test_dir / "on_exit_job.log").as_posix(),
            "error":                    (test_dir / "on_exit_job.err").as_posix(),
            "should_transfer_files":    "YES",
            # Explicitly set ON_EXIT (the default) - this is NOT ON_EXIT_OR_EVICT
            "when_to_transfer_output":  "ON_EXIT",
            "transfer_output_files":    "on_exit_output_file",
        }
    )

    # Wait for the job to start running
    assert the_job.wait(
        condition=ClusterState.any_running,
        timeout=60,
    )

    # Give job time to create the output file
    time.sleep(2)

    return the_job


@action
def on_exit_rm_with_transfer(default_condor, on_exit_job, test_dir):
    """Remove the ON_EXIT job using condor_rm -transfer"""
    job_id = f"{on_exit_job.clusterid}.0"

    # Run condor_rm -transfer to transfer output and remove
    result = default_condor.run_command(["condor_rm", "-transfer", job_id])

    # Wait for the job to be removed
    assert on_exit_job.wait(
        condition=ClusterState.all_terminal,
        timeout=60,
    )

    return result


class TestCondorRmTransferOnExit:
    """
    Test condor_rm -transfer with when_to_transfer_output=ON_EXIT (the default).

    Previously, condor_rm -transfer only worked with ON_EXIT_OR_EVICT.
    This test verifies that it now also works with the default ON_EXIT setting.
    """

    def test_condor_rm_transfer_on_exit_succeeds(self, on_exit_rm_with_transfer):
        """Test that condor_rm -transfer command succeeds for ON_EXIT job"""
        assert on_exit_rm_with_transfer.returncode == 0

    def test_condor_rm_transfer_on_exit_reports_transfer(self, on_exit_rm_with_transfer):
        """Test that condor_rm -transfer reports the ON_EXIT job was marked for removal with file transfer"""
        assert "marked for removal after file transfer" in on_exit_rm_with_transfer.stdout

    def test_on_exit_output_file_was_transferred(self, on_exit_rm_with_transfer, on_exit_output_file):
        """Test that the output file was transferred even with ON_EXIT setting"""
        assert on_exit_output_file.exists(), f"Output file {on_exit_output_file} was not transferred (ON_EXIT job)"
        assert on_exit_output_file.is_file(), f"Output file {on_exit_output_file} is not a regular file"

    def test_on_exit_job_was_removed(self, on_exit_job, on_exit_rm_with_transfer):
        """Test that the ON_EXIT job was actually removed from the queue"""
        job_ads = on_exit_job.query()
        if len(job_ads) > 0:
            assert job_ads[0].get("JobStatus") == 3, f"Job status is {job_ads[0].get('JobStatus')}, expected 3 (REMOVED)"
            remove_reason = job_ads[0].get("RemoveReason", "")
            assert "condor_rm -transfer" in remove_reason, f"RemoveReason doesn't mention transfer: {remove_reason}"
