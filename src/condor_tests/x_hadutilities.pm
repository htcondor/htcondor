#!/usr/bin/env perl
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


use strict;
use Socket;
use POSIX     qw(strftime);

use constant MAX_INT       => 999999999999;
use constant POSTPONE      => 9999999;
use constant SUSPENDED     => -1;
use constant FAILED        => 0;
use constant RAISED        => 1;
use constant YES           => 1;
use constant NO            => 0;
use constant MORE_THAN_ONE => "More than one";
use constant PROCESSED     => 2;
use constant UPDATED       => 1;

use constant HAD_STARTING_PORT         => 50100;
use constant REPLICATION_STARTING_PORT => 51100;
use constant SCHEDD_STARTING_PORT      => 52100;
use constant NEGOTIATOR_STARTING_PORT  => 53100;
use constant COLLECTOR_STARTING_PORT   => 54100;
use constant STARTD_STARTING_PORT      => 55100;


sub LoadTable
{
    (my $tableFile, my %table) = @_;
    open(TABLE, "< $tableFile") or die("open: $!");

    foreach my $line (<TABLE>)
    {
        fullchomp($line);
    	$line =~ s/\s//g;
        my ($key, $value) = split(/=/, $line) if $line;
        $table{$key} = $value;
    }

    close(TABLE);

    return %table;
}

sub LogError
{
    my ($logsDirectory, $message) = @_;
    WriteToLog("$logsDirectory/Error.log", $message);
}

sub LogSuccess
{
    my ($logsDirectory, $message) = @_;
    WriteToLog("$logsDirectory/Success.log", $message);
}

sub LogControl
{
	my ($logsDirectory, $message) = @_;
    WriteToLog("$logsDirectory/Control.log", $message);
}

sub WriteToLog
{
	my ($logFilePath, $message) = @_;
  
  	open LOG_FILE, ">> $logFilePath" or 
  		die "Cannot open $logFilePath for append :$!";
  	print LOG_FILE "$message\n";
  	close LOG_FILE;
}

sub GetModificationTime
{
	my $filePath = shift;

	my ($dev  , $ino  , $mode , $nlink  , $uid   , $gid, $rdev, $size,
	    $atime, $mtime, $ctime, $blksize, $blocks) = stat($filePath);

	return $mtime;
}

sub GetLogicalClock
{
	my $filePath = shift;

	open(ACCOUNTANT_VERSION, "< $filePath") or die("open: $!");
	my @accountantVersionContents = <ACCOUNTANT_VERSION>;
	close(ACCOUNTANT_VERSION);

	fullchomp(@accountantVersionContents);

	return $accountantVersionContents[1];	
}

sub CondorConfigVal
{
	my ($address, $parameter) = @_;
	my $commandString = "condor_config_val  -address \'<$address>\' " . 
			    "$parameter";
	my $parameterValue = `$commandString`;

	fullchomp($parameterValue);

	return $parameterValue;
}

sub ConvertSinfulString
{
	my $sinfulString = shift;
	
	$_ = $sinfulString;
	tr/<>//d;
	my ($ip, $port) = split(':', $_);

	return ($ip, $port);
}

# Function    : FormatTime
# Parameters  : timeStamp - timestamp to format
# Return value: scalar - in format [MON] [DD] [HH]:[MI]:[SS]
# Description : returns formatted timestamp in human-readable format
sub FormatTime
{
	(my $timeStamp) = @_;
	
	return strftime("%b %e %H:%M:%S", localtime($timeStamp));
}

sub GetMyIpAddress
{
	my $protocol = getprotobyname("udp");

	socket(SOCKET, PF_INET, SOCK_DGRAM, $protocol);
	connect(SOCKET, sockaddr_in(0, inet_aton('10.10.10.10')));

	my ($port, @address) = unpack_sockaddr_in(getsockname(SOCKET));

	@address = map {inet_ntoa($_)} @address;

	return "@address";
}

sub GetAddress
{
	(my $filePath) = (@_);

	open(ADDRESS_FILE, "< $filePath");
	my $sinfulString = <ADDRESS_FILE>;
	fullchomp($sinfulString);
	close(ADDRESS_FILE);

	return $sinfulString;
}

sub IsDaemonAlive 
{
	my ($daemonName, $port) = @_;
    my $isAlive 			= NO; 
	
	$isAlive = `ps -ef | grep \"$daemonName.*-p $port\" | grep -v grep`;

    fullchomp($isAlive);

	return YES if $isAlive ne "";
	return NO;
}

sub GetCondorLocalConfigurationFile
{
	open(CONFIGURATION_FILE, "condor_config_val -config | ");

	my @configurationFiles = <CONFIGURATION_FILE>;
	
	close(CONFIGURATION_FILE);
	fullchomp(@configurationFiles);
	$_ = $configurationFiles[1];
	tr/\t\ //d;
	$configurationFiles[1] = $_;
	
	return $configurationFiles[1];
}

sub SubstituteLine
{
    (my $file, my $startingPhrase, my $substitution) = @_;
    my $newFile = "";
    
	open(FILE, "< $file");
    my @fileLines = <FILE>;
    close(FILE);

    foreach my $line (@fileLines)
    {
    	if($line =~ /^$startingPhrase=/)
        {
        	$newFile .= "$startingPhrase=" . $substitution . "\n";
            next;
        }
    	$newFile .= $line;
    }

    open(NEW_FILE, "> $file") or die("open: $!");
    print NEW_FILE $newFile;
    close(NEW_FILE);
}

sub ImplantLine
{
	(my $file, my $startingPhrase, my $substitution) = @_;
    
	open(LOCAL_CONFIG_FILE, "< $file");
    my @localConfigFile = <LOCAL_CONFIG_FILE>;
    close(LOCAL_CONFIG_FILE);

    if(grep(/^$startingPhrase=/, @localConfigFile))
    {
    	&SubstituteLine($file, $startingPhrase, $substitution);
    }
    else
    {
    	open(LOCAL_CONFIG_FILE, ">> $file");
        print LOCAL_CONFIG_FILE "$startingPhrase=$substitution\n";
        close(LOCAL_CONFIG_FILE);
    }
}

#
# Cygwin's perl chomp does not remove cntrl-m but this one will
# and linux and windows can share the same code. The real chomp
# totals the number or changes but I currently return the modified
# array. bt 10/06
#

sub fullchomp
{
	push (@_,$_) if( scalar(@_) == 0);
	foreach my $arg (@_) {
		$arg =~ s/\012+$//;
		$arg =~ s/\015+$//;
	}
	return(0);
}

1;
