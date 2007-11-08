#! /usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************


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
