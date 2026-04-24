import pytest
from pathlib import Path
from unittest.mock import patch, MagicMock
from htcondor_cli.snake import Submit

class TestSnakemakeSubmit:
    """Test the submit class for snakemake submission"""

    # TEST 1: Normal working with all flags #
    def test_submit_with_all_required_flags(self, tmp_path):
        # temporary Snakefile
        snakefile = tmp_path/ "my_workflow" / "Snakefile"
        snakefile.parent.mkdir(parents=True, exist_ok=True)
        snakefile.write_text("rule all: \n pass")

        # temporary profile directory
        profile_dir = tmp_path/ "htcondor_profile"
        profile_dir.mkdir()
        (profile_dir/"config.yaml").write_text("cores: 4")

        # temporary jobdir
        jobdir = tmp_path/"logs"

        # Mock shutil.which()
        with patch("shutil.which") as mock_which:
            mock_which.return_value = '/use/bin/snakemake'

            # Mock htcondor.Schedd() for submission
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance

                # Mock the submit result
                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                # Submit
                submit = Submit(
                    logger=None,
                    snakefile=str(snakefile),
                    profile=str(profile_dir),
                    snakemake_args=[],
                    jobdir=str(jobdir),
                    executor="htcondor"
                )

                # Assert that submit was called correctly
                mock_schedd_instance.submit.assert_called_once()
                submit_call_args = mock_schedd_instance.submit.call_args
                submit_description = submit_call_args[0][0]

                # Check arguments and setting
                arguments = submit_description["arguments"]
                assert f"-s {snakefile}" in arguments
                assert f"--executor htcondor" in arguments
                assert f"--profile {profile_dir}" in arguments
                assert f"--htcondor-jobdir {jobdir}" in arguments

                assert submit_description["universe"] == "local"
                assert submit_description["getenv"] == "true"

                assert mock_result.cluster.called


    # TEST 2: Both --jobdir and duplicate --htcondor_jobdir provided #
    def test_jobdir(self, tmp_path):
        """
        - detect --htcondor-jobdir in snakemake_args and respect that
        - create seperate logs if the jobdir is also specified
        This needs to be paired with actually submitting the jobs because we cannot verify that the end result will create the right logs 
        """
        # temporary Snakefile
        snakefile = tmp_path/ "my_workflow" / "Snakefile"
        snakefile.parent.mkdir(parents=True, exist_ok=True)
        snakefile.write_text("rule all: \n pass")

        # temporary profile directory
        profile_dir = tmp_path/ "htcondor_profile"
        profile_dir.mkdir()
        (profile_dir/"config.yaml").write_text("cores: 4")

        # temporary jobdir from --jobdir flag
        jobdir = tmp_path/"logs"

        # Mock shutil.which()
        with patch("shutil.which") as mock_which:
            mock_which.return_value = '/use/bin/snakemake'

            # Mock htcondor.Schedd() for submission
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance

                # Mock the submit result
                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                # Submit
                submit = Submit(
                    logger=None,
                    snakefile=str(snakefile),
                    snakemake_args=["--htcondor-jobdir", "my_logs", "--htcondor-jobdir", "my_actual_logs"],
                    profile=str(profile_dir),
                    jobdir=str(jobdir),
                    executor="htcondor"
                )

                # Assert that submit was called correctly
                mock_schedd_instance.submit.assert_called_once()
                submit_call_args = mock_schedd_instance.submit.call_args
                submit_description = submit_call_args[0][0]

                # Check the arguments present
                arguments = submit_description["arguments"]
                #print(f"Arguments: {arguments}")
                ## check that there is only one htcondor-dir and that should be my_actual_log
                assert "my_actual_logs" in arguments

                ## check the management job is created at specified --jobdir correctly
                assert jobdir.exists()
                assert jobdir.is_dir()
                assert submit_description["log"] == f"{jobdir}/snakemake-mgmt-$(ClusterId).log"
                assert submit_description["output"] == f"{jobdir}/snakemake-mgmt-$(ClusterId).out"
                assert submit_description["error"] == f"{jobdir}/snakemake-mgmt-$(ClusterId).err"
    
    # TEST 3: Snakefile not at submit directory #
    def test_snakefile(self, tmp_path):
       
        # temporary profile directory
        profile_dir = tmp_path/ "htcondor_profile"
        profile_dir.mkdir()
        (profile_dir/"config.yaml").write_text("cores: 4")

        # Snakefile in htcondor_jobdir
        snakefile = tmp_path/ "nested" / "workflow" / "Snakefile"
        snakefile.parent.mkdir(parents=True, exist_ok=True)
        snakefile.write_text("rule all: \n pass")

        # temporary jobdir from --jobdir flag
        jobdir = tmp_path/"logs"

        # Mock shutil.which()
        with patch("shutil.which") as mock_which:
            mock_which.return_value = '/use/bin/snakemake'

            # Mock htcondor.Schedd() for submission
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance

                # Mock the submit result
                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                # Submit
                submit = Submit(
                    logger=None,
                    snakefile=str(snakefile),
                    profile=str(profile_dir),
                    snakemake_args =[],
                    jobdir=str(jobdir),
                    executor="htcondor"
                )

                # Assert that submit was called correctly
                mock_schedd_instance.submit.assert_called_once()
                submit_call_args = mock_schedd_instance.submit.call_args
                submit_description = submit_call_args[0][0]

                # Verify Snakefile path is parsed
                arguments = submit_description["arguments"]
                assert f"-s {snakefile}" in arguments

                # Check rules jobdir and htcondor jobdir are the same
                assert jobdir.exists()
                assert jobdir.is_dir()
                assert f"--htcondor-jobdir {jobdir}" in arguments

    # TEST 4: Additional snakemake arguments #
    def test_snake_args(self, tmp_path):
        """
        Testing that additional snakemake_arg in cli after -- are passed successfully
            - User provides extra args like ["--cores", "16", "--dryrun", "--quiet"]
            - These should appear in the final HTCondor submit arguments
        """
        # temporary Snakefile
        snakefile = tmp_path/ "my_workflow" / "Snakefile"
        snakefile.parent.mkdir(parents=True, exist_ok=True)
        snakefile.write_text("rule all: \n pass")

        # temporary profile directory
        profile_dir = tmp_path/ "htcondor_profile"
        profile_dir.mkdir()
        (profile_dir/"config.yaml").write_text("cores: 4")

        # temporary jobdir from --jobdir flag
        jobdir = tmp_path/"logs"

        # Mock shutil.which()
        with patch("shutil.which") as mock_which:
            mock_which.return_value = '/use/bin/snakemake'

            # Mock htcondor.Schedd() for submission
            with patch("htcondor2.Schedd") as mock_schedd_class:
                mock_schedd_instance = MagicMock()
                mock_schedd_class.return_value = mock_schedd_instance

                # Mock the submit result
                mock_result = MagicMock()
                mock_result.cluster.return_value = 12345
                mock_schedd_instance.submit.return_value = mock_result

                # Additionl arguments
                additional_args = [
                    "--cores", "16",
                    "--dryrun",
                    "--shared-fs-usage", "None",
                    "--jobs", "6"
                ]

                # Submit
                submit = Submit(
                    logger=None,
                    snakefile=str(snakefile),
                    snakemake_args=additional_args,
                    profile=str(profile_dir),
                    jobdir=str(jobdir),
                    executor="htcondor"
                )

                # Assert that submit was called correctly
                mock_schedd_instance.submit.assert_called_once()
                submit_call_args = mock_schedd_instance.submit.call_args
                submit_description = submit_call_args[0][0]

                # Verify
                arguments = submit_description["arguments"]
                assert "--cores" in arguments
                assert "16" in arguments
                assert "--dryrun" in arguments
                assert "--shared-fs-usage" in arguments
                assert "None" in arguments
                assert "--jobs" in arguments
                assert "6" in arguments

                assert f"-s {snakefile}" in arguments
                assert "--executor htcondor" in arguments
                assert f"--htcondor-jobdir {jobdir}" in arguments
                assert f"--profile {profile_dir}" in arguments

