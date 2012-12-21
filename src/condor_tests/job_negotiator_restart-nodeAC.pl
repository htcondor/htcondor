#! /usr/bin/env perl

# Figure out our "HTCondor" username.
$username = `whoami`;
chomp($username);
$machine = `hostname`;
chomp($machine);
$user = $username . "@" . $machine;
print "HTCondor username is $user\n";

# Get our priority factor.
$priofactor = "";
open (OUTPUT, "condor_userprio -allusers 2>&1 |") or die "Can't fork: $!";
while (<OUTPUT>) {
	if ($_ =~ /$user/) {
		@fields = split;
		$priofactor = $fields[2];
	}
}
close (OUTPUT) or die "condor_userprio failed: $?";

if ($priofactor eq "") {
	die "Unable to determine prio factor";
}

print "Priofactor is $priofactor\n";

$factorfile = "job_negotiator_restart.priofactor";


if ($ARGV[0] eq "A") {
	system("touch job_negotiator_restart-nodeB.do_restart");

	# Set to a new priority factor.
	$priofactor *= 3;
	print "Setting priofactor to $priofactor\n";

	open (OUTPUT, "condor_userprio -setfactor $user $priofactor 2>&1 |") or die "Can't fork: $!";
	while (<OUTPUT>) {
		print "$_";
	}
	close (OUTPUT) or die "condor_userprio failed: $?";

	# Save the priority factor so we can check it after the condor_restart.
	if (-e $factorfile) {
		system("rm $factorfile");
	}
	system("echo $priofactor > $factorfile");

} elsif ($ARGV[0] eq "C") {

	# Make sure the current priority factor matches what we set it to
	# before the restart.
	$expectedfactor = `cat $factorfile`;
	chomp($expectedfactor);

	if ($priofactor == $expectedfactor) {
		print "Priority factor matches expected priority factor\n";
	} else {
		die "Priority factor ($priofactor) does NOT match expected priority factor ($expectedfactor)\n";
	}

	# Reset to the original priority factor.
	$priofactor /= 3;
	print "Setting priofactor to $priofactor\n";

	open (OUTPUT, "condor_userprio -setfactor $user $priofactor 2>&1 |") or die "Can't fork: $!";
	while (<OUTPUT>) {
		print "$_";
	}
	close (OUTPUT) or die "condor_userprio failed: $?";

} else {
	print "Node $ARGV[0] job fails -- unexpected node name!\n";
	exit(1);
}

print "Node $ARGV[0] job succeeds\n";
