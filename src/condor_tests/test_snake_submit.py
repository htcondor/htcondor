import pytest
import os
from pathlib import Path
from unittest.mock import patch, MagicMock
from htcondor_cli.snake import Submit


class TestSnakemakeSubmit:
    """Test submission logic with parsed snakefile and args"""

    # === INTEGRATION TESTS: Argument parsing through REMAINDER ==="

    def test_submit_extract_jobdir_after_snakefile_from_remainder(self, tmp_path):
        """Integration: --jobdir appears in REMAINDER after snakefile"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")
        jobdir = tmp_path / "extracted_logs"

        with patch("shutil.which") as mock_which:
            mock_which.return_value = "/usr/bin/snakemake"
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance
                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                # Pass snakefile with --jobdir in REMAINDER
                remainder = [str(snakefile), "--jobdir", str(jobdir), "--", "--profile", "htcondor_profile"]
                submit = Submit(
                    logger=None,
                    snakemake_args=remainder,
                )

                # Verify --jobdir was extracted and used
                submit_description = mock_schedd_instance.submit.call_args[0][0]
                arguments = submit_description["arguments"]
                
                # Verify the extracted jobdir is used
                assert f"--htcondor-jobdir {jobdir}" in arguments
                
                # Verify --jobdir is not in the arguments passed to snakemake
                assert "--jobdir" not in arguments
                
                # Verify snakefile is there
                assert f"-s {snakefile}" in arguments
                
                # Verify other args made it through
                assert "--profile" in arguments

    def test_submit_extract_jobdir_before_snakefile_from_remainder(self, tmp_path):
        """Integration: --jobdir appears in REMAINDER before snakefile"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")
        jobdir = tmp_path / "extracted_logs"

        with patch("shutil.which") as mock_which:
            mock_which.return_value = "/usr/bin/snakemake"
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance
                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                # Pass --jobdir before snakefile in REMAINDER
                remainder = ["--jobdir", str(jobdir), str(snakefile), "--", "--profile", "htcondor_profile"]
                submit = Submit(
                    logger=None,
                    snakemake_args=remainder,
                )

                # Verify --jobdir was extracted and used
                submit_description = mock_schedd_instance.submit.call_args[0][0]
                arguments = submit_description["arguments"]
                
                # Verify the extracted jobdir is used
                assert f"--htcondor-jobdir {jobdir}" in arguments
                
                # Verify --jobdir is not in the arguments passed to snakemake
                assert "--jobdir" not in arguments
                
                # Verify snakefile is there
                assert f"-s {snakefile}" in arguments

    def test_submit_extract_multiple_jobdir_last_wins_from_remainder(self, tmp_path):
        """Integration: multiple --jobdir in REMAINDER, last one should be used"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")
        jobdir_first = tmp_path / "logs1"
        jobdir_last = tmp_path / "logs_final"

        with patch("shutil.which") as mock_which:
            mock_which.return_value = "/usr/bin/snakemake"
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance
                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                # Multiple --jobdir in REMAINDER, last should win
                remainder = [str(snakefile), "--jobdir", str(jobdir_first), "--jobdir", str(jobdir_last), "--", "--profile", "htcondor_profile"]
                submit = Submit(
                    logger=None,
                    snakemake_args=remainder,
                )

                # Verify last --jobdir was used
                submit_description = mock_schedd_instance.submit.call_args[0][0]
                arguments = submit_description["arguments"]
                
                # Verify LAST jobdir is used
                assert f"--htcondor-jobdir {jobdir_last}" in arguments
                
                # Verify first jobdir is NOT used
                assert str(jobdir_first) not in submit_description["arguments"]
                
                # Verify NO --jobdir appears in arguments passed to snakemake
                assert "--jobdir" not in arguments

    def test_submit_parse_snakefile_only_from_remainder(self, tmp_path):
        """Integration: specifying with only snakefile, no additional args and separator"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")
        jobdir = tmp_path / "logs"

        with patch("shutil.which") as mock_which:
            mock_which.return_value = "/usr/bin/snakemake"
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance
                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                # Pass snakefile through REMAINDER
                submit = Submit(
                    logger=None,
                    snakemake_args=[str(snakefile)],
                    jobdir=str(jobdir),
                )

                # Verify snakefile was extracted and submitted
                mock_schedd_instance.submit.assert_called_once()
                submit_description = mock_schedd_instance.submit.call_args[0][0]
                
                # Verify executable and arguments
                assert submit_description["executable"] == "/usr/bin/snakemake"
                arguments = submit_description["arguments"]
                assert f"-s {snakefile}" in arguments
                assert f"--htcondor-jobdir {jobdir}" in arguments
                
                # Verify universe and environment
                assert submit_description["universe"] == "local"
                assert submit_description["getenv"] == "true"

    def test_submit_parse_snakefile_with_separator_and_args(self, tmp_path):
        """Integration: specifying snakefile with -- separator and additional snakemake args"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")
        jobdir = tmp_path / "logs"

        with patch("shutil.which") as mock_which:
            mock_which.return_value = "/usr/bin/snakemake"
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance
                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                # Pass through REMAINDER
                remainder = [str(snakefile), "--", "--jobs", "6", "--cores", "4", "--dry-run"]
                submit = Submit(
                    logger=None,
                    snakemake_args=remainder,
                    jobdir=str(jobdir),
                )

                # Verify both snakefile and additional args made it to submission
                submit_description = mock_schedd_instance.submit.call_args[0][0]
                
                # Verify executable and arguments
                assert submit_description["executable"] == "/usr/bin/snakemake"
                arguments = submit_description["arguments"]
                assert f"-s {snakefile}" in arguments
                assert "--jobs" in arguments
                assert "6" in arguments
                assert "--cores" in arguments
                assert "4" in arguments
                assert "--dry-run" in arguments
                
                # Verify universe and environment
                assert submit_description["universe"] == "local"
                assert submit_description["getenv"] == "true"

    def test_submit_parse_no_snakefile_with_separator(self, tmp_path):
        """Integration: specifying only -- separator and args and uses default Snakefile"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")
        jobdir = tmp_path / "logs"

        original_cwd = os.getcwd()
        try:
            os.chdir(tmp_path) # change the current directory to where Snakefile is

            with patch("shutil.which") as mock_which:
                mock_which.return_value = "/usr/bin/snakemake"
                with patch("htcondor2.Schedd") as mock_schedd_class:
                    mock_schedd_instance = MagicMock()
                    mock_schedd_class.return_value = mock_schedd_instance
                    mock_result = MagicMock()
                    mock_result.cluster.return_value = 12345
                    mock_schedd_instance.submit.return_value = mock_result

                    # Pass through REMAINDER
                    remainder = ["--", "--jobs", "6"]
                    submit = Submit(
                        logger=None,
                        snakemake_args=remainder,
                        jobdir=str(jobdir),
                    )

                
                    submit_description = mock_schedd_instance.submit.call_args[0][0]
                    
                    # Verify executable and arguments
                    assert submit_description["executable"] == "/usr/bin/snakemake"
                    arguments = submit_description["arguments"]
                    assert "-s" in arguments
                    assert "--jobs" in arguments
                    assert "6" in arguments
                    
                    # Verify universe and environment
                    assert submit_description["universe"] == "local"
                    assert submit_description["getenv"] == "true"
        finally:
            os.chdir(original_cwd)

    # === ROBUSTNESS TESTS ===

    def test_submit_default_snakefile(self, tmp_path):
        """Test that code defaults to 'Snakefile' in current directory"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")
        jobdir = tmp_path / "logs"

        original_cwd = os.getcwd()
        try:
            os.chdir(tmp_path)

            with patch("shutil.which") as mock_which:
                mock_which.return_value = "/usr/bin/snakemake"
                with patch("htcondor2.Schedd") as mock_schedd_class:
                    mock_schedd_instance = MagicMock()
                    mock_schedd_class.return_value = mock_schedd_instance
                    mock_result = MagicMock()
                    mock_result.cluster.return_value = 12345
                    mock_schedd_instance.submit.return_value = mock_result

                    # No snakefile provided, no args
                    submit = Submit(logger=None, snakemake_args=[], jobdir=str(jobdir))

                    mock_schedd_instance.submit.assert_called_once()
                    submit_description = mock_schedd_instance.submit.call_args[0][0]
                    
                    # Verify executable and arguments
                    assert submit_description["executable"] == "/usr/bin/snakemake"
                    arguments = submit_description["arguments"]
                    assert "Snakefile" in arguments
                    
                    # Verify universe and environment
                    assert submit_description["universe"] == "local"
                    assert submit_description["getenv"] == "true"
        finally:
            os.chdir(original_cwd)

    def test_submit_htcondor_submission_error(self, tmp_path):
        """Test error handling when HTCondor submission fails"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")
        jobdir = tmp_path / "logs"

        with patch("shutil.which") as mock_which:
            mock_which.return_value = "/usr/bin/snakemake"
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance
                # Simulate submission failure
                mock_schedd_instance.submit.side_effect = RuntimeError("Schedd connection failed")

                with patch("sys.exit") as mock_exit:
                    with patch("builtins.print"):
                        submit = Submit(
                            logger=None,
                            snakefile=str(snakefile),
                            snakemake_args=[],
                            jobdir=str(jobdir),
                        )
                    # Verify sys.exit was called
                    mock_exit.assert_called_once_with(1)

    def test_submit_empty_snakemake_args(self, tmp_path):
        """Test submission with empty snakemake_args list"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")
        jobdir = tmp_path / "logs"

        with patch("shutil.which") as mock_which:
            mock_which.return_value = "/usr/bin/snakemake"
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance
                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                submit = Submit(
                    logger=None,
                    snakefile=str(snakefile),
                    snakemake_args=[],
                    jobdir=str(jobdir),
                )

                mock_schedd_instance.submit.assert_called_once()
                submit_description = mock_schedd_instance.submit.call_args[0][0]
                
                # Verify executable and arguments
                assert submit_description["executable"] == "/usr/bin/snakemake"
                arguments = submit_description["arguments"]
                assert f"-s {snakefile}" in arguments
                
                # Verify universe and environment
                assert submit_description["universe"] == "local"
                assert submit_description["getenv"] == "true"

    def test_submit_creates_jobdir(self, tmp_path):
        """Test that jobdir is created if it doesn't exist"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")

        jobdir = tmp_path / "nonexistent" / "logs"

        with patch("shutil.which") as mock_which:
            mock_which.return_value = "/usr/bin/snakemake"

            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance

                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                submit = Submit(
                    logger=None,
                    snakefile=str(snakefile),
                    snakemake_args=[],
                    jobdir=str(jobdir),
                )

                # Verify jobdir was created
                assert jobdir.exists()
                assert jobdir.is_dir()

    def test_submit_snakefile_not_found(self, tmp_path):
        """Test error handling when snakefile doesn't exist"""
        missing_snakefile = tmp_path / "missing" / "Snakefile"
        jobdir = tmp_path / "logs"

        with pytest.raises(FileNotFoundError):
            submit = Submit(
                logger=None,
                snakefile=str(missing_snakefile),
                snakemake_args=[],
                jobdir=str(jobdir),
            )

    def test_submit_snakemake_not_found(self, tmp_path):
        """Test error handling when snakemake executable not found"""
        snakefile = tmp_path / "Snakefile"
        snakefile.write_text("rule all:\n    pass")

        jobdir = tmp_path / "logs"

        with patch("shutil.which") as mock_which:
            mock_which.return_value = None # simulate snakemake executable cannot be found in the sys

            with patch("sys.exit") as mock_exit:
                with patch("builtins.print"): 
                    submit = Submit(
                        logger=None,
                        snakefile=str(snakefile),
                        snakemake_args=[],
                        jobdir=str(jobdir),
                    )
                # Verify that sys.exit was called with exit code 1
                mock_exit.assert_called_once_with(1)

    def test_submit_nested_snakefile_path(self, tmp_path):
        """Test submission with nested snakefile path"""
        snakefile = tmp_path / "nested" / "workflow" / "Snakefile"
        snakefile.parent.mkdir(parents=True, exist_ok=True)
        snakefile.write_text("rule all:\n    pass")

        jobdir = tmp_path / "logs"

        with patch("shutil.which") as mock_which:
            mock_which.return_value = "/usr/bin/snakemake"

            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance

                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                submit = Submit(
                    logger=None,
                    snakefile=str(snakefile),
                    snakemake_args=[],
                    jobdir=str(jobdir),
                )

                # Verify nested path is preserved
                submit_call_args = mock_schedd_instance.submit.call_args
                submit_description = submit_call_args[0][0]
                
                # Verify executable and arguments
                assert submit_description["executable"] == "/usr/bin/snakemake"
                arguments = submit_description["arguments"]
                assert f"-s {snakefile}" in arguments
                assert f"--htcondor-jobdir {jobdir}" in arguments

                # Verify universe and environment
                assert submit_description["universe"] == "local"
                assert submit_description["getenv"] == "true"

                # Verify logging paths
                assert f"{jobdir}/snakemake-mgmt-$(ClusterId).log" == submit_description["log"]
                assert f"{jobdir}/snakemake-mgmt-$(ClusterId).out" == submit_description["output"]
                assert f"{jobdir}/snakemake-mgmt-$(ClusterId).err" == submit_description["error"]

                # Verify job name
                assert submit_description["JobBatchName"] == f"snakemake-mgmt-$(ClusterId)"
