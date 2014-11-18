#! /usr/bin/env perl

use CondorTest;
use CondorUtils;

runcmd("cp job_dagman_gt4727-lower-nodeA.cmd.ok job_dagman_gt4727-lower-nodeA.cmd");
# Force sub-DAG into recovery mode.
runcmd("touch job_dagman_gt4727-lower.dag.lock");
