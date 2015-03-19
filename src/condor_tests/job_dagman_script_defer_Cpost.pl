#! /usr/bin/env perl

use CondorTest;
use CondorUtils;

$fname = "job_dagman_script_defer_Cpost.fail1";
if (-e $fname) {
	runcmd("rm $fname");
	exit(5);
}
$fname = "job_dagman_script_defer_Cpost.fail2";
if (-e $fname) {
	runcmd("rm $fname");
	exit(5);
}
exit(0);

