#!/usr/bin/env perl

##/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
##
## Condor Software Copyright Notice
## Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
##
## This source code is covered by the Condor Public License, which can
## be found in the accompanying LICENSE.TXT file, or online at
## www.condorproject.org.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
## AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
## IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
## WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
## FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
## HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
## MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
## ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
## PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
## RIGHT.
##
##***************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#
# VMWare Control Tool
# V0.1 / 2007-Mar-01 / Jaeyoung Yoon / jyoon@cs.wisc.edu
#

use strict;
use File::Path;
use File::Copy;

# program version information
my $APPNAME = "VMWare Control Tool";
my $VERSION = "0.1";

# Location of "vmrun" and "vmware-cmd" program
# If these programs are in $PATH, just use basename. Otherwise use a full path
my $vmrun_prog;
my $vmwarecmd_prog;

if (lc($^O) eq "mswin32") {
	# For MS Windows 
	$vmrun_prog = 'C:\Program Files\VMware\VMware VIX\vmrun';
	$vmwarecmd_prog = 'C:\Program Files\VMware\VMware VmPerl Scripting API\vmware-cmd';
}else {
	# For Linux
	$vmrun_prog = 'vmrun';
	$vmwarecmd_prog = 'vmware-cmd';
}

# Location of "mkisofs" or "mkisofs.exe" program
# If mkisofs program is in $PATH, just use basename. Otherwise use a full path
my $mkisofs;
if (lc($^O) eq "mswin32") {
	# For MS Windows 
	$mkisofs = 'C:\condor\bin\mkisofs.exe';
}else {
	# For Linux
	$mkisofs = 'mkisofs';
}

# stdout will be directed to stderr
#open STDOUT, ">&STDERR" or die "Can't dup STDERR: $!";
#select STDERR; $| = 1;  # make unbuffered
#select STDOUT; $| = 1;  # make unbuffered
open STDOUT, ">&STDERR";

my $tmpdir = undef;
my $progname = $0;

sub usage()
{
print STDERR <<EOF;

$APPNAME (version $VERSION)

Usage: $progname command [parameters]

Command     Parameters         Description
list                           List all running VMs
check                          Check if vmware is installed
register    [vmconfig]         Register a VM
unregister  [vmconfig]         Unregister a VM
start       [vmconfig] [file]  Start a VM and save PID into file
stop        [vmconfig]         Shutdown a VM
killvm      [string]           Kill a VM
suspend     [vmconfig]         Suspend a VM
resume      [vmconfig] [file]  Resume a suspended VM and save PID into file
status      [vmconfig] [file]  Save the status of a VM into file
getpid      [vmconfig] [file]  Save PID of VM into file
getvminfo   [vmconfig] [file]  Save info about VM into file
snapshot    [vmconfig]         Create a snapshot of a VM
commit      [vmconfig]         Commit COW disks and delete the COW
revert      [vmconfig]         Set VM state to a snapshot
createiso   [listfile] [ISO]   Create an ISO image with files in a listfile

Note:
  vmconfig must be an absolute pathname, e.g. /vm/testvm1.vmx

Examples:
  $progname start /vm/testvm1.vmx
  $progname status /vm/testvm1.vmx output.txt

EOF

exit(1);
}

sub printerror
{
	if( defined($tmpdir) ) {
		rmtree("$tmpdir")
			or print STDERR "Cannot delete temporary directory($tmpdir) and files in it";
	}
	print STDERR "(ERROR) @_\n";
	exit(1);
}

sub printwarn
{
	print STDERR "(WARN) @_\n";
}

sub checkvmconfig
{
	my $vmconfig = $_[0];
	if( ! defined($vmconfig) ) {
		usage();
	}

	unless( -e $vmconfig ) {
		printerror "vmconfig $vmconfig doesnot exist";
	}

	unless ( -r $vmconfig ) {
		printerror "vmconfig $vmconfig is not readable";
	}

	return $vmconfig;
}

sub checkregister
{
	my $vmconfig = $_[0];
	
	chomp(my @vmstatus = `"$vmwarecmd_prog" -l`);
	# result must be like this
	# /home/condor/vmware/Centos.vmx
	# /home/condor/vmware/Centos2.vmx

	foreach( @vmstatus ) {
		# delete leading/trailing whitespace
		s/^\s+|\s+$//g;
		if( $_ eq $vmconfig ) {
			# this vm is already registered
			# return true
			return 1;
		}
	}
	
	# return false
	return 0;
}

sub list
{
#list                           List all running VMs
	!system $vmrun_prog, "list"
		or printerror "Can't execute the command: '$vmrun_prog' list";
}

sub check
{
#check       [vmconfig]         Check if vmware command line tools are installed
	!system $vmrun_prog, "list"
		or printerror "Can't execute $vmrun_prog";

	!system $vmwarecmd_prog, "-l"
		or printerror  "Can't execute $vmwarecmd_prog";

	!system $mkisofs, "-version"
		or printerror "Can't execute $mkisofs";
}

sub register
{
#register    [vmconfig]         Register a VM
	print STDERR "VMwareTool: register is called\n";
	my $vmconfig = checkvmconfig($_[0]);
	if( ! checkregister($vmconfig) ) {
		#!system $vmwarecmd_prog, "-s", "register", $vmconfig
		#	or printerror "Can't register a vm($vmconfig)";
		system $vmwarecmd_prog, "-s", "register", $vmconfig;
	}
}

sub unregister
{
#unregister    [vmconfig]         Unregister a VM
	print STDERR "VMwareTool: unregister is called\n";
	my $vmconfig = checkvmconfig($_[0]);
	#if( checkregister($vmconfig) ) {
		#!system $vmwarecmd_prog, "-s", "unregister", $vmconfig
		#	or printerror "Can't unregister a vm($vmconfig)";
	#}
	system $vmwarecmd_prog, "-s", "unregister", $vmconfig;
}

sub getvmpid
{
	my $vmconfig = $_[0];

	my $resultline = `"$vmwarecmd_prog" "$vmconfig" getpid`;
	chomp($resultline);
	if( ! $resultline ) {
		printerror "Can't execute getpid of a vm($vmconfig)";
	}

	# result must be like "getpid() = 18690"
	my @fields = split /=/, $resultline;
	shift @fields;
	my $pid_field = shift @fields;
	if( ! defined($pid_field) ) {
		printerror "Invalid format of getpid($resultline)";
	}
	# delete leading/trailing whitespace
	$pid_field =~ s/^\s+|\s+$//g;
	return $pid_field;
}

sub status
{
#status      [vmconfig] [file]  Show the status of a VM
	my $vmconfig = checkvmconfig($_[0]);

	my $resultfile = undef;
	if ( defined($_[1]) ) {
		$resultfile = $_[1];
		open VMSTATUS, "> $resultfile"
			or printerror "Cannot create the file($resultfile) : $!";
	}

	my $output_status = "Stopped";	# default status
	#if( checkregister($vmconfig) ) {
		my $vmstatus = `"$vmwarecmd_prog" "$vmconfig" getstate`;
		chomp($vmstatus);
		if( ! $vmstatus ) {
			printerror "Can't execute getstate of a vm($vmconfig)";
		}

		# result must be like "getstate() = on"
		my @fields = split /=/, $vmstatus;
		shift @fields;
		my $status_field = shift @fields;
		if( ! defined($status_field) ) {
			printerror "Invalid format of getstate($vmstatus)";
		}
		# delete leading/trailing whitespace
		$status_field =~ s/^\s+|\s+$//g;
		$status_field = lc($status_field);
		$output_status = 
		( $status_field eq "on") ? "Running" :
		( $status_field eq "suspended") ? "Suspended" :
		( $status_field eq "off") ? "Stopped":
		"Error";	# default value
	#}
	if( defined($resultfile) ) {
		print VMSTATUS "STATUS=$output_status\n";
		if( $output_status eq "Running") {
			my $vmpid = getvmpid($vmconfig);
			print VMSTATUS "PID=$vmpid\n";
		}
		close VMSTATUS;
	}
	return $output_status;
}

sub pidofvm
{
#getpid      [vmconfig] [file]  Save PID of VM to file
	print STDERR "VMwareTool: pidofvm is called\n";
	my $vmconfig = checkvmconfig($_[0]);
	if( ! defined($_[1]) ) {
		usage();
	}
	my $pidfile = $_[1];

	# Get status
	my $vmstatus = status($vmconfig);
	if( $vmstatus ne "Running" ) {
		printerror "vm($vmconfig) is not running";
	}

	# Get pid of main process of this VM
	open VMPID, "> $pidfile" 
		or printerror "Cannot create the file($pidfile) : $!";

	my $vmpid = getvmpid($vmconfig);
	print VMPID "$vmpid\n";
	close VMPID;
}

sub getvminfo
{
#getvminfo     [vmconfig] [file]  Save info about VM to file
	print STDERR "VMwareTool: getvminfo is called\n";
	my $vmconfig = checkvmconfig($_[0]);
	if( ! defined($_[1]) ) {
		usage();
	}
	my $infofile = $_[1];

	# Get status
	my $vmstatus = status($vmconfig, $infofile);

	if( $vmstatus ne "Running" ) {
		return;
	}

	open VMINFO, ">> $infofile" 
		or printerror "Cannot create the file($infofile) : $!";

	# Get mac address of VM
	my $resultline = `"$vmwarecmd_prog" "$vmconfig" getconfig ethernet0.generatedAddress`;
	chomp($resultline);
	if( $resultline ) {
		# result must be like 
		# "getconfig(ethernet0.generatedAddress) = 00:0c:29:cb:40:bb"
		my @fields = split /=/, $resultline;
		shift @fields;
		my $second_field = shift @fields; 
		# delete leading/trailing whitespace
		$second_field =~ s/^\s+|\s+$//g;
		if( ! defined($second_field) ) {
			printwarn "Invalid format of getconfig($resultline)";
		}else {
			print VMINFO "MAC=$second_field\n";
		}
	}
	# Get IP address of VM
	$resultline = `"$vmwarecmd_prog" "$vmconfig" getguestinfo ip`;
	chomp($resultline);
	if( $resultline ) {
		# result must be like "getguestinfo(ip) = 172.16.123.143"
		my @fields = split /=/, $resultline;
		shift @fields;
		my $second_field = shift @fields;
		# delete leading/trailing whitespace
		$second_field =~ s/^\s+|\s+$//g;
		if( ! defined($second_field) ) {
			printwarn "Invalid format of getguestinfo ip($resultline)";
		}else {
			print VMINFO "IP=$second_field\n";
		}
	}
	close VMINFO;
}

sub start
{
#start       [vmconfig] [file]	Start a VM
	print STDERR "VMwareTool: start is called\n";
	my $vmconfig = checkvmconfig($_[0]);

	if( ! checkregister($vmconfig) ) {
		#!system $vmwarecmd_prog, "-s", "register", $vmconfig
		#	or printerror "Can't register a new vm with $vmconfig";
		system $vmwarecmd_prog, "-s", "register", $vmconfig;
	}

	# Now, a new vm is registered
	# Try to create a new vm
	!system $vmwarecmd_prog, $vmconfig, "start", "trysoft"
		or printerror "Can't create vm with $vmconfig";

	sleep(5);

	if ( defined($_[1]) ) {
		# Get pid of main process of this VM
		my $pidfile = $_[1];
		open VMPID, "> $pidfile" 
			or printerror "Cannot create the file($pidfile) : $!";

		my $vmpid = getvmpid($vmconfig);
		print VMPID "$vmpid\n";
		close VMPID;
	}
}

sub stop
{
#stop        [vmconfig]         Shutdown a VM
	print STDERR "VMwareTool: stop is called\n";
	my $vmconfig = checkvmconfig($_[0]);

	# Get status
	my $vmstatus = status($vmconfig);
	if( $vmstatus eq "Running" ) {
		# Try to stop vm
		!system $vmwarecmd_prog, $vmconfig, "stop", "hard"
			or printerror "Can't stop vm with $vmconfig";

		sleep(2);
	}
	unregister($vmconfig);
}

sub killvm
{
#killvm        [vmconfig]         kill a VM
	print STDERR "VMwareTool: killvm is called\n";
	my $matchstring = $_[0];

	if( ! defined($matchstring) ) {
		usage();
	}

	print STDERR "VMwareTool: matching string is '$matchstring'\n";

	my $os = $^O;

	if (lc($os) eq "mswin32") {
		# replace \ with \\ for regular expression
		$matchstring =~ s/\\/\\\\/g;
		# replace / with \\ for regular expression
		$matchstring =~ s/\//\\\\/g;
	}

	# Get the list of registered VMs
	chomp(my @vmstatus = `"$vmwarecmd_prog" -l`);
	# result must be like this
	# /home/condor/vmware/Centos.vmx
	# /home/condor/vmware/Centos2.vmx

	my $ori_line;
	foreach( @vmstatus ) { 
		# delete leading/trailing whitespace
		s/^\s+|\s+$//g;

		$ori_line = $_;
		if (lc($os) eq "mswin32") {
			# replace / with \
			$_ =~ s/\//\\/g;
		}
		if( $_ =~ m{$matchstring} ) {
			# this registed vm is matched
			# Get pid of this vm 	
			my $vmpid = getvmpid($ori_line);

			if( defined($vmpid) ) {
				print STDERR "VMwareTool: Killing process(pid=$vmpid)\n";
				kill "KILL", $vmpid;
			}

			!system $vmwarecmd_prog, "-s", "unregister", $ori_line
				or printwarn "Can't unregister a vm($ori_line)";
		}
	}

	if (lc($os) eq "linux") {
		# On Linux machines, we make certain no process for VM again.
		my @pidarr;
		my $psline;
		my @psarr = `ps -ef`;
		foreach $psline (@psarr)
		{
			if( $psline =~ m{$matchstring} && $psline =~ m/vmware-vmx/i && $psline !~ m{$0})
			{
				@pidarr = split (/ +/, $psline);
				if( defined($pidarr[1]) ) {
					print STDERR "VMWareTool: Killing process(pid=$pidarr[1])\n";
					kill 9, $pidarr[1];
				}
			}
		}
	}
}

sub suspend
{
#suspend     [vmconfig]         Suspend a VM
	print STDERR "VMwareTool: suspend is called\n";
	my $vmconfig = checkvmconfig($_[0]);

	# Get status
	my $vmstatus = status($vmconfig);
	if( $vmstatus ne "Running" ) {
		printerror "vm($vmconfig) is not running";
	}

	!system $vmwarecmd_prog, $vmconfig, "suspend", "trysoft"
		or printerror "Can't suspend vm with $vmconfig";

	# We need to guarantee all memory data into a file
	# VMware does some part of suspending in background mode
	# So, we give some time to VMware
	sleep(5);
}

sub resume
{
#resume      [vmconfig]         Restore a suspended VM
	print STDERR "VMwareTool: resume is called\n";
	my $vmconfig = checkvmconfig($_[0]);
	my $pidfile = $_[1];

	# Get status
	my $vmstatus = status($vmconfig);
	if( $vmstatus ne "Running" ) {
		start($vmconfig, $pidfile);
	}
}

sub snapshot
{
#snapshot    [vmconfig]         Create a snapshot of a VM
	print STDERR "VMwareTool: snapshot is called\n";
	my $vmconfig = checkvmconfig($_[0]);
	if( ! checkregister($vmconfig) ) {
		#!system $vmwarecmd_prog, "-s", "register", $vmconfig
		#	or printerror "Can't register a vm with $vmconfig";
		system $vmwarecmd_prog, "-s", "register", $vmconfig;
	}

	!system $vmrun_prog, "snapshot", $vmconfig
		or printerror "Can't create snapshot for vm($vmconfig)";

	sleep(1);
}

sub commit
{
#commit      [vmconfig]         Commit COW disks and delete the COW
	print STDERR "VMwareTool: commit is called\n";
	my $vmconfig = checkvmconfig($_[0]);
	if( ! checkregister($vmconfig) ) {
		#!system $vmwarecmd_prog, "-s", "register", $vmconfig
		#	or printerror "Can't register a vm with $vmconfig";
		system $vmwarecmd_prog, "-s", "register", $vmconfig;
	}

	!system $vmrun_prog, "deleteSnapshot", $vmconfig
		or printerror "Can't combine snapshot disk with base disk for vm($vmconfig)";

	sleep(5);
}

sub revert
{
#revert      [vmconfig]         Set VM state to a snapshot
	print STDERR "VMwareTool: revert is called\n";
	my $vmconfig = checkvmconfig($_[0]);

	if( ! checkregister($vmconfig) ) {
		#!system $vmwarecmd_prog, "-s", "register", $vmconfig
		#	or printerror "Can't register a vm with $vmconfig";
		system $vmwarecmd_prog, "-s", "register", $vmconfig;
	}

	!system $vmrun_prog, "revertToSnapshot", $vmconfig
		or printerror "Can't revert VM state to a snapshot for vm($vmconfig)";

	sleep(5);
}

sub createiso
{
#createiso   [listfile] [ISO]   Create an ISO image with files in a listfile
	print STDERR "VMwareTool: createiso is called\n";

	my $isoconfig = $_[0];
	my $isofile = $_[1];

	if( ! defined($isofile) || ! defined($isoconfig)) {
		usage();
	}

	unless( -e $isoconfig ) {
		printerror "File($isoconfig) does not exist";
	}

	unless( -r $isoconfig ) {
		printerror "File($isoconfig) is not readable";
	}

	# Create temporary directory
	$tmpdir = $isofile.".dir";
	mkdir("$tmpdir") || printerror "Cannot mkdir newdir";

	# Read config file
	open(ISOFILES, "$isoconfig") 
		or printerror "Cannot open the file($isoconfig) : $!";

	# Copy all files in config into the temporary directory 
	while( <ISOFILES> ) {
		chomp;
		if( $_ ) {
			copy( "$_", "$tmpdir")
				or printerror "Cannot copy file($_) into directory($tmpdir) : $!";
		}
	}
	close ISOFILES;

	my $cdlabel = "CONDOR";

	# Using volume ID, application Label, Joliet
	!system $mkisofs, "-quiet", "-o", $isofile, "-input-charset", "iso8859-1", "-J", "-A", $cdlabel, "-V", $cdlabel, $tmpdir
		or printerror "Cannot create an ISO file($isofile)";

	rmtree("$tmpdir")
		or printwarn "Cannot delete temporary directory($tmpdir) and files in it";

	sleep(1);
}

if ($#ARGV < 0 || $ARGV[0] eq "--help") { usage(); }
elsif ($ARGV[0] eq "list")	{ list(); }
elsif ($ARGV[0] eq "check")	{ check(); }
elsif ($ARGV[0] eq "register")	{ register($ARGV[1]); }
elsif ($ARGV[0] eq "unregister"){ unregister($ARGV[1]); }
elsif ($ARGV[0] eq "start")	{ start($ARGV[1],$ARGV[2]); }
elsif ($ARGV[0] eq "stop")	{ stop($ARGV[1]); }
elsif ($ARGV[0] eq "killvm")	{ killvm($ARGV[1]); }
elsif ($ARGV[0] eq "suspend")	{ suspend($ARGV[1]); }
elsif ($ARGV[0] eq "resume")	{ resume($ARGV[1], $ARGV[2]); }
elsif ($ARGV[0] eq "status")	{ status($ARGV[1], $ARGV[2]); }
elsif ($ARGV[0] eq "getpid")	{ pidofvm($ARGV[1], $ARGV[2]); }
elsif ($ARGV[0] eq "getvminfo")	{ getvminfo($ARGV[1], $ARGV[2]); }
elsif ($ARGV[0] eq "snapshot")	{ snapshot($ARGV[1]); }
elsif ($ARGV[0] eq "commit")	{ commit($ARGV[1]); }
elsif ($ARGV[0] eq "revert")	{ revert($ARGV[1]); }
elsif ($ARGV[0] eq "createiso")	{ createiso($ARGV[1], $ARGV[2]); }
else { printerror "Unknown command \"$ARGV[0]\". See $progname --help."; }
