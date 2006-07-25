#! /usr/bin/env perl

my $limit = $ARGV[0];
my $corefile = "core";

#system("rm -rf core");
system("chmod 755 x_dumpcore.exe");
#system("gcc -o dumpcore dumpcore.c");

defined(my $pid = fork) or die "Cannot fork: $!";
unless ($pid)
{
	#child runs and ceases to exist
	exec "./x_dumpcore.exe";
	die "can not exec dumpcore!\n";
}
#main process continues here
waitpid($pid,0);

my $corefilepid = "core"."\." . "$pid";
print "File with pid is $corefilepid\n";
system("ls;pwd");


if(-s $corefile)
{
	my $size = -s $corefile;
	print "Core exists and is $size \n";
}
elsif( -s $corefilepid)
{
	my $size = (-s $corefilepid);
	print "Core -$corefile- exists and is $size \n";
	if($size > $limit)
	{
		print "core file too big\n";
		exit 2;
	}
	else
	{
		print "core file OK\n";
		exit 0;
	}
}
else
{
	print "Can not find core.....\n";
	exit 1;
}

sleep 10;
