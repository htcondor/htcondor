#!/usr/bin/env perl

my $target = $ARGV[0];
print "Sorting $target\n";

my @sortarray = ();
my $line = "";
#open(TA,"<$target") or die "Failed to open $target:$!\n";
#while(<TA>) {
while(<>) {
	$line = $_;
	#print "Consider:$line\n";
	chomp($line);
	if($line =~ /^\s*$/) {
		SortMyArray(\@sortarray);
		@sortarray = ();
		print "\n";
	} else {
		push @sortarray, $line;
	}
}
SortMyArray(\@sortarray);
exit(0);

sub SortMyArray {
	#print "Sorting\n";
	my $arrayref = shift;
	#foreach my $thing (@{$arrayref}) {
		#print "$thing\n";
	#}
	my @sorted = sort {lc $a cmp lc $b }  @{$arrayref};
	foreach my $ad (@sorted) {
		print "$ad\n";
	}
}

