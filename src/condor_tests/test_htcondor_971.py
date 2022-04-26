#!/usr/bin/env pytest

import os
import re
import time

from ornithology import (
    config,
    standup,
    action,
    Condor,
    write_file,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

#
# See HTCONDOR-971 for a detailed explanation of what we're testing here.
#

@config(params={
     "schedd": ( "SCHEDD" ),
     "startd": ( "STARTD" ),
})
def daemon(request):
    return request.param


@standup
def path_to_exit_zero(test_dir):
    exit_zero_script = '''#!/bin/bash
    echo "exit_zero = 0"
    exit 0
    '''
    path = test_dir / "exit_zero"
    write_file(path, exit_zero_script)
    return path


@standup
def path_to_exit_one(test_dir):
    exit_one_script = '''#!/bin/bash
    echo "exit_one = 1"
    exit 1
    '''
    path = test_dir / "exit_one"
    write_file(path, exit_one_script)
    return path


@standup
def path_to_sig_kill(test_dir):
    sig_kill_script = '''#!/bin/bash
    echo "sig_kill = 1"
    kill -KILL $$
    '''
    path = test_dir / "sig_kill"
    write_file(path, sig_kill_script)
    return path


@standup
def condor(test_dir, daemon, path_to_exit_zero, path_to_exit_one, path_to_sig_kill):
    raw_config = f"""
    {daemon}_DEBUG = D_ALWAYS
    {daemon}_CRON_JOBLIST = exit_zero, exit_one, sig_kill

    {daemon}_CRON_exit_zero_EXECUTABLE = {path_to_exit_zero}
    {daemon}_CRON_exit_zero_MODE = OneShot
    {daemon}_CRON_exit_zero_RECONFIG_RERUN = true

    {daemon}_CRON_exit_one_EXECUTABLE = {path_to_exit_one}
    {daemon}_CRON_exit_one_MODE = OneShot
    {daemon}_CRON_exit_one_RECONFIG_RERUN = true

    {daemon}_CRON_sig_kill_EXECUTABLE = {path_to_sig_kill}
    {daemon}_CRON_sig_kill_MODE = OneShot
    {daemon}_CRON_sig_kill_RECONFIG_RERUN = true
    """
    with Condor( test_dir / "condor", raw_config=raw_config ) as condor:
        yield condor


@action
def daemon_log(condor, daemon):
    return condor._get_daemon_log(daemon)


def count_lines_in_log(pattern, daemon_log):
    count = 0
    prog = re.compile(pattern)
    for line in daemon_log.open().read():
        print(line.message)
        if prog.search(line.message) is not None:
            count = count + 1
    return count


class TestBadCronJobLogging:

    line_template = "CronJob: '{}' \\(pid \\d+\\) exit_{}={}"


    def test_exit_zero_log(self, daemon_log):
        exit_zero_line = self.line_template.format("exit_zero", "status", "0")
        assert count_lines_in_log(exit_zero_line, daemon_log) == 0


    def test_exit_one_log(self, daemon_log):
        exit_one_line = self.line_template.format("exit_one", "status", "1")
        assert count_lines_in_log(exit_one_line, daemon_log) > 0


    def test_sig_kill_log(self, daemon_log):
        sig_kill_line = self.line_template.format("sig_kill", "signal", "9")
        assert count_lines_in_log(sig_kill_line, daemon_log) > 0
