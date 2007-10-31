#! /usr/bin/env perl

#use strict;

use POSIX "sys_wait_h";
use Cwd;

my $pid;

my $opsys = $ARGV[0];
my $sleeptime = $ARGV[1];
my $testname = $ARGV[2];

my $shortsleep = $sleeptime - 60;

my $LogFile = $testname . ".data.log";
system("rm -f $LogFile");
system("touch $LogFile");

system("date");

print "OPSYS = $opsys\n";
print "SLEEP = $sleeptime\n";

my $path = getcwd();
my $count;
my $innercount = 0;
my $innerpid = 0;

#my $childcmd = "/usr/bin/strace -ostrace$$ ./x_tightloop.exe $sleeptime $LogFile\n";
my $childcmd = "./x_tightloop.exe $sleeptime $LogFile\n";
my $shortchildcmd = "./x_tightloop.exe $shortsleep $LogFile 1\n";

system("rm -rf $testname.data.kids");


# spin out three levels of tasks each of which
# runs the tightloop code.

$toppid = fork();
if($toppid == 0) {
	# second level spawns two tasks

	$pid = fork();
	if ($pid == 0) {
		# child 1 code....

		$innerpid = fork();
		if ($innerpid == 0) {
			spin
			system("$childcmd");
			exit(0);
		}

		$innerpid = fork();
		if ($innerpid == 0) {
			spin
			system("$shortchildcmd");
			exit(0);
		}

		#spin
		system("$childcmd");

		# clean up the kids
		while( $child = wait() ) {
			last if $child == -1;
		}

		exit 0;
	} 

	$pid = fork();
	if ($pid == 0) {
		# child 1 code....

		$innerpid = fork();
		if ($innerpid == 0) {
			#spin
			system("$childcmd");
			exit(0);
		}

		$innerpid = fork();
		if ($innerpid == 0) {
			#spin
			system("$shortchildcmd");
			exit(0);
		}

		#spin
		system("$childcmd");

		# clean up the kids
		while( $child = wait() ) {
			last if $child == -1;
		}

		exit 0;
	} 

	#spin
	system("$shortchildcmd");


	# clean up the kids
	while( $child = wait() ) {
		last if $child == -1;
	}

	exit 0;
} 
	
system("$childcmd");

# clean up the kids
while( $child = wait() ) {
	last if $child == -1;
}

#system("date");

exit 0;

