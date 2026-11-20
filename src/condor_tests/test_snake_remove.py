#!/usr/bin/env pytest

import signal
import pytest
import htcondor2
from unittest.mock import patch, MagicMock
from htcondor_cli.snake import Remove, JSM_HTC_SNAKE_SUBMIT


@pytest.fixture
def mock_schedd():
    """
    Patch htcondor2.Schedd and yield the mock instance so tests can configure
    schedd.act()'s return value/side effect and assert on calls.
    """
    with patch("htcondor2.Schedd") as mock_schedd_class:
        mock_schedd_instance = MagicMock()
        mock_schedd_class.return_value = mock_schedd_instance
        yield mock_schedd_instance


class TestSnakemakeRemove:
    """Test removing logic for htcondor snake remove"""

    @pytest.mark.parametrize("options", [
        {"fast": True, "peaceful": False},
        {},
    ], ids=["fast_flag", "no_flag_defaults_to_fast"])
    def test_fast_removal_constraint(self, mock_schedd, options):
        """--fast (or no method given) removes the management job and its child jobs."""
        mgmt_id = "2604"
        mock_schedd.act.return_value = {"TotalSuccess": 1, "TotalError": 0}

        Remove(logger=MagicMock(), mgmt_id=mgmt_id, **options)

        mock_schedd.act.assert_called_once()
        args, kwargs = mock_schedd.act.call_args
        assert args[0] == htcondor2.JobAction.Remove
        assert args[1] == (f"(ClusterId == {mgmt_id} && JobSubmitMethod == {JSM_HTC_SNAKE_SUBMIT})"
                           f" || SnakeManagerJobId == {mgmt_id}")
        assert kwargs.get("reason")

    @pytest.mark.parametrize("act_result, level, message", [
        ({"TotalSuccess": 1, "TotalError": 0}, "info",
         "Removing the management job 2604. All associated jobs will be removed shortly after that."),
        ({"TotalSuccess": 0, "TotalError": 1}, "warning",
         "Failed to remove job 2604: schedd reported 1 error(s)."),
        ({"TotalSuccess": 0, "TotalError": 0}, "info",
         "Job 2604 was not found as a `htcondor snake submit` management job."),
    ], ids=["removed", "schedd_error", "not_found"])
    def test_fast_removal_result(self, mock_schedd, act_result, level, message):
        """The message logged depends on what schedd.act() reports."""
        mock_logger = MagicMock()
        mock_schedd.act.return_value = act_result

        Remove(logger=mock_logger, mgmt_id="2604", fast=True, peaceful=False)

        getattr(mock_logger, level).assert_called_once_with(message)

    def test_remove_mgmt_running_peaceful(self, mock_schedd):
        """Peaceful (SIGTERM) removal method for a valid workflow that is submitted"""
        mgmt_id = "2604"
        target_pid = 12345
        mock_logger = MagicMock()
        mock_schedd.query.return_value = [
            {"JobStatus": 2, "SnakeMgmtPID": target_pid},
        ]

        with patch("htcondor_cli.snake.os.kill") as mock_kill:
            Remove(logger=mock_logger, mgmt_id=mgmt_id, fast=False, peaceful=True)

        mock_schedd.query.assert_called_once()
        _, kwargs = mock_schedd.query.call_args
        assert kwargs["constraint"] == f"ClusterId == {mgmt_id} && JobSubmitMethod == {JSM_HTC_SNAKE_SUBMIT}"
        assert kwargs["projection"] == ["JobStatus", "SnakeMgmtPID"]

        mock_kill.assert_called_once_with(target_pid, signal.SIGTERM)
        mock_logger.info.assert_called_once()

    @pytest.mark.parametrize("status", [1, 5], ids=["idle", "held"])
    def test_peaceful_idle_or_held_removed_directly(self, mock_schedd, status):
        """Job is Idle or Held on the first query: no Snakemake process exists,
        so the management job is removed through the schedd without signalling."""
        mgmt_id = "2604"
        mock_logger = MagicMock()
        mock_schedd.query.return_value = [{"JobStatus": status, "SnakeMgmtPID": 54321}]

        with patch("htcondor_cli.snake.os.kill") as mock_kill, \
             patch.object(Remove, "_remove_mgmt_job") as mock_remove:
            Remove(logger=mock_logger, mgmt_id=mgmt_id, fast=False, peaceful=True)

        mock_schedd.query.assert_called_once()
        mock_kill.assert_not_called()
        mock_remove.assert_called_once_with(mock_schedd, int(mgmt_id))

    @pytest.mark.parametrize("status", [3, 4], ids=["removed", "completed"])
    def test_peaceful_removed_or_completed_before_signal(self, mock_schedd, status):
        """Job is Removed or Completed on the first query, so there is nothing to
        signal or remove."""
        mgmt_id = "2604"
        mock_logger = MagicMock()
        mock_schedd.query.return_value = [{"JobStatus": status, "SnakeMgmtPID": 54321}]

        with patch("htcondor_cli.snake.os.kill") as mock_kill, \
             patch.object(Remove, "_remove_mgmt_job") as mock_remove:
            Remove(logger=mock_logger, mgmt_id=mgmt_id, fast=False, peaceful=True)

        mock_schedd.query.assert_called_once()
        mock_kill.assert_not_called()
        mock_remove.assert_not_called()
        mock_logger.info.assert_called_once_with(
            f"Management job {mgmt_id} has already been removed or completed."
        )

    @pytest.mark.parametrize("requery", [
        [{"JobStatus": 1, "SnakeMgmtPID": 54321}],
        [{"JobStatus": 5, "SnakeMgmtPID": 54321}],
        [{"JobStatus": 2, "SnakeMgmtPID": 54321}],
    ], ids=["idle", "held", "running_same_pid"])
    def test_peaceful_stale_pid_removes_mgmt_job(self, mock_schedd, requery):
        """os.kill raises ProcessLookupError and the re-query shows no live process
        (Idle, Held, or Running with the same stale PID), so the management job is
        removed through the schedd."""
        mgmt_id = "2604"
        target_pid = 54321
        mock_logger = MagicMock()
        mock_schedd.query.side_effect = [[{"JobStatus": 2, "SnakeMgmtPID": target_pid}], requery]

        with patch("htcondor_cli.snake.os.kill", side_effect=ProcessLookupError) as mock_kill, \
             patch.object(Remove, "_remove_mgmt_job") as mock_remove:
            Remove(logger=mock_logger, mgmt_id=mgmt_id, fast=False, peaceful=True)

        mock_kill.assert_called_once_with(target_pid, signal.SIGTERM)
        assert mock_schedd.query.call_count == 2
        mock_remove.assert_called_once_with(mock_schedd, int(mgmt_id))
        mock_logger.warning.assert_not_called()
        mock_logger.error.assert_not_called()

    @pytest.mark.parametrize("requery", [
        [{"JobStatus": 3, "SnakeMgmtPID": 54321}],
        [{"JobStatus": 4, "SnakeMgmtPID": 54321}],
    ], ids=["removed", "completed"])
    def test_peaceful_stale_pid_already_exited(self, mock_schedd, requery):
        """os.kill raises ProcessLookupError and the re-query shows the job Removed or
        Completed, so it has already exited and nothing is removed."""
        mgmt_id = "2604"
        target_pid = 54321
        mock_logger = MagicMock()
        mock_schedd.query.side_effect = [[{"JobStatus": 2, "SnakeMgmtPID": target_pid}], requery]

        with patch("htcondor_cli.snake.os.kill", side_effect=ProcessLookupError) as mock_kill, \
             patch.object(Remove, "_remove_mgmt_job") as mock_remove:
            Remove(logger=mock_logger, mgmt_id=mgmt_id, fast=False, peaceful=True)

        mock_kill.assert_called_once_with(target_pid, signal.SIGTERM)
        assert mock_schedd.query.call_count == 2
        mock_remove.assert_not_called()
        mock_logger.info.assert_called_once_with(f"Management job {mgmt_id} has already exited.")

    @pytest.mark.parametrize("requery_pid, expected_warning", [
        (67890, "Management job 2604 restarted with a new process (PID 67890). "
                "Run `htcondor snake remove 2604 --peaceful` again to stop it."),
        (None, "Management job 2604 is starting up. Please try again in a few seconds."),
    ], ids=["new_pid", "no_pid_yet"])
    def test_peaceful_stale_pid_restarted(self, mock_schedd, requery_pid, expected_warning):
        """os.kill raises ProcessLookupError and the re-query shows the job Running
        under a new process (or one that hasn't reported its PID yet), so the user is
        warned to retry and nothing is removed."""
        mgmt_id = "2604"
        target_pid = 54321
        mock_logger = MagicMock()
        mock_schedd.query.side_effect = [
            [{"JobStatus": 2, "SnakeMgmtPID": target_pid}],
            [{"JobStatus": 2, "SnakeMgmtPID": requery_pid}],
        ]

        with patch("htcondor_cli.snake.os.kill", side_effect=ProcessLookupError) as mock_kill, \
             patch.object(Remove, "_remove_mgmt_job") as mock_remove:
            with pytest.raises(SystemExit) as exc_info:
                Remove(logger=mock_logger, mgmt_id=mgmt_id, fast=False, peaceful=True)

        assert exc_info.value.code == 1
        mock_kill.assert_called_once_with(target_pid, signal.SIGTERM)
        assert mock_schedd.query.call_count == 2
        mock_remove.assert_not_called()
        mock_logger.warning.assert_called_once_with(expected_warning)

    def test_remove_mgmt_job_success(self, mock_schedd):
        """_remove_mgmt_job removes only the management job (no SnakeManagerJobId
        clause, so child jobs are left alone) and logs success."""
        mgmt_id = "2604"
        mock_logger = MagicMock()
        mock_schedd.query.return_value = [{"JobStatus": 1, "SnakeMgmtPID": None}]
        mock_schedd.act.return_value = {"TotalSuccess": 1, "TotalError": 0}

        Remove(logger=mock_logger, mgmt_id=mgmt_id, fast=False, peaceful=True)

        mock_schedd.act.assert_called_once()
        args, kwargs = mock_schedd.act.call_args
        assert args[0] == htcondor2.JobAction.Remove
        assert args[1] == f"ClusterId == {mgmt_id} && JobSubmitMethod == {JSM_HTC_SNAKE_SUBMIT}"
        assert "SnakeManagerJobId" not in args[1]
        assert "--peaceful" in kwargs["reason"]
        mock_logger.info.assert_called_once_with(
            f"Removed management job {mgmt_id} from the queue."
        )

    def test_remove_mgmt_job_failure(self, mock_schedd):
        """_remove_mgmt_job: schedd removes nothing, so warn and exit 1."""
        mgmt_id = "2604"
        mock_logger = MagicMock()
        mock_schedd.query.return_value = [{"JobStatus": 1, "SnakeMgmtPID": None}]
        mock_schedd.act.return_value = {"TotalSuccess": 0, "TotalError": 1}

        with pytest.raises(SystemExit) as exc_info:
            Remove(logger=mock_logger, mgmt_id=mgmt_id, fast=False, peaceful=True)

        assert exc_info.value.code == 1
        mock_schedd.act.assert_called_once()
        mock_logger.warning.assert_called_once_with(
            f"Could not remove management job {mgmt_id}: schedd reported 1 error(s)."
        )

    def test_running_job_missing_pid(self, mock_schedd):
        """JobStatus == 2 but SnakeMgmtPID is absent/None"""
        mgmt_id = "2604"
        mock_logger = MagicMock()
        mock_schedd.query.return_value = [
            {"JobStatus":2, "SnakeMgmtPID": None}
        ]

        with pytest.raises(SystemExit) as exc_info:
            Remove(logger=mock_logger, mgmt_id=mgmt_id, fast=False, peaceful=True)

        assert exc_info.value.code == 1
        mock_logger.warning.assert_called_once()
        assert f"Management job {mgmt_id} is starting up. Please try again in a few seconds." in mock_logger.warning.call_args[0][0]

    @pytest.mark.parametrize("mgmt_id", [None, "not-an-int"], ids=["missing", "not_integer"])
    def test_invalid_mgmt_id(self, mock_schedd, mgmt_id):
        """A missing or non-integer management ID exits before contacting the schedd."""
        with pytest.raises(SystemExit) as exc_info:
            Remove(logger=None, mgmt_id=mgmt_id)
        assert exc_info.value.code == 1
        mock_schedd.act.assert_not_called()
        mock_schedd.query.assert_not_called()

    @pytest.mark.parametrize("method, options", [
        ("act", {"fast": True, "peaceful": False}),
        ("query", {"fast": False, "peaceful": True}),
    ], ids=["fast", "peaceful"])
    def test_schedd_exception(self, mock_schedd, method, options):
        """A schedd failure is reported and exits 1 instead of crashing."""
        mock_logger = MagicMock()
        getattr(mock_schedd, method).side_effect = RuntimeError("Schedd connection failed")

        with pytest.raises(SystemExit) as exc_info:
            Remove(logger=mock_logger, mgmt_id="2604", **options)

        assert exc_info.value.code == 1
        mock_logger.error.assert_called_once_with(
            "Could not remove the management job: Schedd connection failed"
        )
