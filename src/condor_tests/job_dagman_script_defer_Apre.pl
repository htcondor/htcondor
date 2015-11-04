#! /usr/bin/env perl

use CondorTest;
use CondorUtils;

runcmd("touch job_dagman_script_defer_Bpre.fail1");
runcmd("touch job_dagman_script_defer_Bpre.fail2");
runcmd("touch job_dagman_script_defer_Cpost.fail1");
runcmd("touch job_dagman_script_defer_Cpost.fail2");
