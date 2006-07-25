#!/usr/bin/env perl
my $file_target = $ARGV[0];
my $new_name = $ARGV[1];

#my $dbtimelog = "x_change.log";
#print "Trying to open logfile... $dbtimelog\n";
#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">>$dbtimelog") or die "Could not open $dbtimelog: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR);
 #$| = 1;
#select(STDOUT);
 #$| = 1;

my $targetdir = '.';
print "Directory to change is $targetdir\n";
opendir DH, $targetdir or die "Can not open $dir:$!\n";
foreach $file (readdir DH)
{	
	my $line = "";
	next if $file =~ /^\.\.?$/;
	next if $file eq $dbtimelog;
	next if $file eq "x_renametool.pl";
	#print "Currently scanning $file for name $file_target being renamed $new_name\n";
	open(FILE,"<$file") || die "Can not open $file for reading: $!\n";
	open(NEW,">$file.tmp") || die "Can not open Temp file: $!\n";
	#print "$file opened OK!\n";
	while(<FILE>)	
	{		
		chomp($_);
		$line = $_;
		s/$file_target/$new_name/g;
		print NEW "$_\n";
	}	
	close(FILE);
	close(NEW);
	system("mv $file.tmp $file");
}
closedir DH;
system("mv $file_target $new_name");

