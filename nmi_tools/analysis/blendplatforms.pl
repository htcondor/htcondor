#! /usr/bin/env perl


system("hostname");
system("date");
system("df");

my $datalocation = "/tmp/btplots";
my $target = $ARGV[0];
print "Look at $target\n";
chdir($datalocation);

opendir DH, "." || die "Failed to open dir $target: $!\n";
foreach $file (readdir DH)
{
	next if $file =~ /^\.\.?$/;	# skip . and ..
	if(-f $file ) {
		# find only the platform data files
		if($file =~ /^(V\d_\d+-)(.*)$/) {
			next if $2 =~ /^nmi:.*$/;
			$newdata = $1 . "nmi:" . $2;
			if(! (-f $newdata)) { next; }
			open(NEW,"<$newdata") || die "Can not open new data<$newdata>: $!\n";
			$tmpfile = $file . ".new";
			open(OLD,"<$file") || die "Can not open old data<$file>: $!\n";
			open(TMP,">$tmpfile") || die "Can not open temp data<$tmpfile>: $!\n";
			# read the old data into the new file first
			print "joining $file and $newdata into $tmpfile\n";
			print "$file\n";
			while(<OLD>) {
				print TMP "$_";
				print "$_";
			}
			# read the newer data into the new file now
			print "$newdata\n";
			while(<NEW>) {
				print "$_";
				print TMP "$_";
			}
			print "Moving $tmpfile to $file\n";
			system("mv $tmpfile $file");
			#print "$1/<$2>\n";
			#print "processed $file - $newdata\n";
			close(OLD);
			close(NEW);
			close(TMP);
		}
	}
}
close(DH);

exit(0);

