#! /usr/bin/env perl

use CondorTest;
use CondorUtils;

$fname = "job_dagman_script_defer_Bpre.fail1";
if (-e $fname) {
	runcmd("rm $fname");
	exit(4);
}
$fname = "job_dagman_script_defer_Bpre.fail2";
if (-e $fname) {
	runcmd("rm $fname");
	exit(4);
}
exit(0);

