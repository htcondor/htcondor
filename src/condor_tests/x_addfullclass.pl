#! /usr/bin/env perl

foreach $name (@ARGV) {
	print "Process full class on $name\n";

	my $new = $name . ".new";
	my $line = "";
	open(OLD,"<$name") || die "Can not open $name:$!\n";
	open(NEW,">$new") || die "Can not open $new:$!\n";
	while(<OLD>) {
		chomp();
		$line = $_;
		if($line =~ /^TESTCLASS\((.*),quick\)$/) {
			#print "Found quick test $1\n";
			print NEW "$line\n";
			print NEW "TESTCLASS($1,full)\n";
		} elsif($line =~ /^TESTCLASS\((.*),long\)$/) {
			#print "Found long test $1\n";
			print NEW "$line\n";
			print NEW "TESTCLASS($1,full)\n";
		} else {
			#print "Keep this $line\n";
			print NEW "$line\n";
		}
	}
	close(OLD);
	close(NEW);
	system("mv $name $name.old");
	system("mv $name.new $name");
}
