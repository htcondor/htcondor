#!/usr/bin/env perl


sub WaitForChild {
	while( my $child = wait() ) {
		# if there are no more children, we're done
		last if $child == -1;

		print "Child pid $child has status $?\n";
	}
};

my $pid = fork(); 
my $test = "x_job_mem_checker.exe";

if($pid > 0) {
	sleep(10);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	sleep(2);
	CollectMemStats($pid);
	WaitForChild();
	print "$test done\n";
} else {
	system("./$test 128 5 60 10 60 5 60");
}

exit(0);

sub CollectMemStats {
	my $pid = shift;
	my $status = "/proc/$pid/status";
	my $line = "";

	print "******************< PID $pid>*************************\n";
	open(MS,"<$status") or die "Can not open <$status>:$!\n";
	while(<MS>) {
		chomp();
		$line = $_;
		if($line =~ /VmPeak/) {
			print "$line\n";
		} elsif($line =~ /VmSize/) {
			print "$line\n";
		} elsif($line =~ /VmHWM/) {
			print "$line\n";
		} elsif($line =~ /VmRSS/) {
			print "$line\n";
		}
	}
	close(MS);
	print "************************************************\n";
}
