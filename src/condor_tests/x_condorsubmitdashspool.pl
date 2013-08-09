#! /usr/bin/env perl

my $cmd = $ARGV[0];

if($cmd eq "1") {
	system("touch dashspool.file1");
	system("touch dashspool.file2");
}

if($cmd eq "3") {
	system("cp realdata dashspool.file1");
	system("touch dashspool.file2");
}
