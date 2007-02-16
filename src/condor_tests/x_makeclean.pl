#! /usr/bin/env perl

opendir( POSTG, ".") || die "Error opening version dir<$mypostgressworld>: $!\n";
@everything = readdir( POSTG );
# filter out . and .. and CVS
@everything = grep !/^\.\.?$/, @everything;
@everything = grep !/.run/, @everything;
@everything = grep !/g77/, @everything;
@everything = grep !/gcc/, @everything;
@everything = grep !/g\+\+/, @everything;
@everything = grep !/.pl/, @everything;
foreach $thing (@everything) {
	print "$thing\n";
	if( -d $thing ) {
		print "rm directory\?\n";
		if( $thing =~ /.*saveme/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		} elsif($thing =~ /^Expect.*$/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		} elsif($thing =~ /^perllib.*$/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		} elsif($thing =~ /^IO-Tty.*$/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		} elsif($thing =~ /^\d+$/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		}
	} else {
		if($thing =~ /^\d+$/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		} elsif($thing =~ /^.*\d+\.log.*$/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		} elsif($thing =~ /^.*\d+\.out.*$/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		} elsif($thing =~ /^.*\d+\.err.*$/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		} elsif($thing =~ /^pgsql\d+$/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		} elsif($thing =~ /^runCTool.*$/) {
			system("rm -rf $thing");
			print "Removed:$thing\n";
		}
	}
}
	

