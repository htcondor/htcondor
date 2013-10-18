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
my $verbose = undef;

# Location of "vmrun" and "vmware-cmd" program
# If these programs are in $PATH, just use basename. Otherwise use a full path
my $vmrun_prog;
my $vmwarecmd_prog;

if (lc($^O) eq "mswin32") {
	# For MS Windows assume VMWare is in "C:\Program Files (x86)" if it exists
	# otherwise assume it's in "C:\Program Files", we find these directories
	# using environment variables.
	my $pf86 = "ProgramFiles(x86)";
	my $progfiles = $ENV{$pf86};
	if ( ! $progfiles) { $progfiles = $ENV{ProgramFiles}; }
	$vmrun_prog = $progfiles . '\VMware\VMware VIX\vmrun';
	$vmwarecmd_prog = $progfiles . '\VMware\VMware VmPerl Scripting API\vmware-cmd';
}else {
	# For Linux
	$vmrun_prog = 'vmrun';
	$vmwarecmd_prog = 'vmware-cmd';
}

# Location of "mkisofs" or "cdmake.exe" program
# If mkisofs program is in $PATH, just use basename. Otherwise use a full path
my $cdmake;
my $mkisofs;
if (lc($^O) eq "mswin32") {
	# For MS Windows 
	$cdmake = 'C:\condor\bin\cdmake.exe';
}else {
	# For Linux
	$mkisofs = 'mkisofs';
}

# Location of dhcpd.lease
# If dhcpd_lease is defined, we will use the file to find IP address of guest VM.
# Otherwise, we will send a request to vmware tool.
# If vmware tool is not installed inside VM, we can't get IP address of the VM.
my $dhcpd_lease = undef;
#$dhcpd_lease = '/etc/vmware/vmnet8/dhcpd/dhcpd.leases';
#$dhcpd_lease = 'C:\Documents and Settings\All Users\Application Data\VMware\vmnetdhcp.leases';


# stdout will be directed to stderr
#open STDOUT, ">&STDERR" or die "Can't dup STDERR: $!";
#select STDERR; $| = 1;  # make unbuffered
#select STDOUT; $| = 1;  # make unbuffered
open OUTPUT, ">&STDOUT";
open STDOUT, ">&STDERR";

my $tmpdir = undef;
my $progname = $0;

#
# Assume we're using Workstation or Player, but autodetect Server;
# only Server has vmware-cmd.
#
my $VMWARE_VERSION = "workstation";

# open()ing a pipe on Windows always succeeds, even if the binary in
# question doesn't exist, so just check if the binary exists.  (We can't
# do this on Linux because we don't have the full path to the binary.)
if( lc($^O) eq "mswin32" ) {
    if( -x $vmwarecmd_prog ) {
        $VMWARE_VERSION = "server";
    }
} else {
    my $childPID = open( VMV, '-|', $vmwarecmd_prog, '-l' );
    if( defined( $childPID ) ) {
	    $VMWARE_VERSION = "server";
    }
    close( VMV );
}

if( defined($verbose) ) { print( "Using \$VMWARE_VERSION '$VMWARE_VERSION'.\n" ); }

#
# The behavior of vmrun isn't consistent from platform to platform, so we
# have to examine the output to determine if it actually worked.  Note 
# that VMWare Server's implementation of vmrun dies if passed -T, so be
# sure never to do that.
#

my @VMRUN_CMD = ($vmrun_prog);
while( $VMWARE_VERSION eq 'workstation' ) {
    my $wsList = `"$vmrun_prog" -T ws list`;
    if( $wsList =~ m/^Total running VMs:/ ) {
	push( @VMRUN_CMD, '-T', 'ws' );
        last;
    }
    
    my $playerList = `"$vmrun_prog" -T player list`;
    if( $playerList =~ m/^Total running VMs:/ ) {
	push( @VMRUN_CMD, '-T', 'player' );
        last;
    }
    
    last;
}

my $VMRUNCMD = '"' . join( '"  "', @VMRUN_CMD ) . '"';
if( defined($verbose) ) { print( "Using \$VMRUNCMD '$VMRUNCMD'.\n" ); }

sub usage()
{
print STDERR <<EOF;

$APPNAME (version $VERSION)

Usage: $progname command [parameters]

Command      Parameters         Description
list                            List all running VMs
check                           Check if vmware is installed
register     [vmconfig]         Register a VM
unregister   [vmconfig]         Unregister a VM
start        [vmconfig]         Start a VM and print PID
stop         [vmconfig]         Shutdown a VM
killvm       [string]           Kill a VM
suspend      [vmconfig]         Suspend a VM
resume       [vmconfig]         Resume a suspended VM and print PID
status       [vmconfig]         Print the status of a VM
getpid       [vmconfig]         Print PID of VM
getvminfo    [vmconfig]         Print info about VM
snapshot     [vmconfig]         Create a snapshot of a VM
commit       [vmconfig]         Commit COW disks and delete the COW
revert       [vmconfig]         Set VM state to a snapshot
createiso    [listfile] [ISO]   Create an ISO image with files in a listfile
createconfig [configfile]       Modify configuration file created by vm-gahp

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

sub printverbose
{
	if( defined($verbose) ) {
		print STDERR "VMwareTool: @_\n";
	}
}

sub checkvmconfig
{
	my $vmconfig = $_[0];
	if( ! defined($vmconfig) ) {
		usage();
	}

	unless( -e $vmconfig ) {
		printerror "vmconfig $vmconfig does not exist";
	}

	unless ( -r $vmconfig ) {
		printerror "vmconfig $vmconfig is not readable";
	}

	return $vmconfig;
}

sub checkregister
{
	if ($VMWARE_VERSION eq "workstation") {
		# return true
		return 1;
	}
	else {
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
}

sub list
{
#list                           List all running VMs
	!system @VMRUN_CMD, "list"
		or printerror "Can't execute the command: '$vmrun_prog' list";
}

sub check
{
#check       [vmconfig]         Check if vmware command line tools are installed
	!system @VMRUN_CMD, "list"
		or printerror "Can't execute $vmrun_prog";

	if ($VMWARE_VERSION ne "workstation") {
		!system $vmwarecmd_prog, "-l"
			or printerror  "Can't execute $vmwarecmd_prog";
	}

	if (lc($^O) ne "mswin32") {
		!system $mkisofs, "-version"
	
			or printerror "Can't execute $mkisofs";
	}
}

sub register
{
#register    [vmconfig]         Register a VM
	if ($VMWARE_VERSION ne "workstation") {
		printverbose "register is called";
		my $vmconfig = checkvmconfig($_[0]);
		if( ! checkregister($vmconfig) ) {
			#!system $vmwarecmd_prog, "-s", "register", $vmconfig
			#	or printerror "Can't register a vm($vmconfig)";
			system $vmwarecmd_prog, "-s", "register", $vmconfig;
		}
	}
}

sub unregister
{
#unregister    [vmconfig]         Unregister a VM
	if ($VMWARE_VERSION ne "workstation") {
		printverbose "unregister is called";
		my $vmconfig = checkvmconfig($_[0]);
		#if( checkregister($vmconfig) ) {
			#!system $vmwarecmd_prog, "-s", "unregister", $vmconfig
			#	or printerror "Can't unregister a vm($vmconfig)";
		#}
		system $vmwarecmd_prog, "-s", "unregister", $vmconfig;
	}
}

sub getvmpid
{
	if ($VMWARE_VERSION eq "workstation") {
		return 0;
	} 
    
	else {	
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
}

sub status
{
#status      [vmconfig]   Show the status of a VM
	my $vmconfig = checkvmconfig($_[0]);

	# If a second argument is passed, then we should print our results
	# to stdout. Otherwise, just return the current status.
	my $print_result = undef;
	if ( defined($_[1]) ) {
		$print_result = $_[1];
	}

	my $output_status = "Stopped";	# default status
	if ($VMWARE_VERSION eq "workstation") {
		# Just check to see if $vmconfig is listed in the output
		my @vmstatus =  `${VMRUNCMD} list`;
		foreach my $i (@vmstatus) {	
			# delete newline
			chomp($i);
			if( $i eq $vmconfig ) {
				$output_status = "Running";
			}
		}
	}

	else {
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
	}
	if( defined($print_result) ) {
		print OUTPUT "STATUS=$output_status\n";
		if( $output_status eq "Running") {
			my $vmpid = getvmpid($vmconfig);
			print OUTPUT "PID=$vmpid\n";
		}
	}
	return $output_status;
}

sub pidofvm
{
#getpid      [vmconfig]   Print PID of VM
	printverbose "pidofvm is called";
	my $vmconfig = checkvmconfig($_[0]);

	# Get status
	if ($VMWARE_VERSION ne "workstation") {
		my $vmstatus = status($vmconfig);
		if( $vmstatus ne "Running" ) {
			printerror "vm($vmconfig) is not running";
		}
	}

	# Get pid of main process of this VM
	my $vmpid = getvmpid($vmconfig);
	print OUTPUT "PID=$vmpid\n";
}

sub getvminfo
{
#getvminfo     [vmconfig]         Print info about VM
	printverbose "getvminfo is called";
	my $vmconfig = checkvmconfig($_[0]);

	# Get status
	my $vmstatus = status($vmconfig, 1);

	if( $vmstatus ne "Running" ) {
		return;
	}

	if ($VMWARE_VERSION eq "workstation") {
		return;
	}

	else {
		# Get mac address of VM
		my $mac_address = undef;
		my $resultline = `"$vmwarecmd_prog" "$vmconfig" getconfig ethernet0.generatedAddress`;
		chomp($resultline);
		if( $resultline ) {
			# result must be like 
			# "getconfig(ethernet0.generatedAddress) = 00:0c:29:cb:40:bb"
			my @fields = split /=/, $resultline;
			shift @fields;
			$mac_address = shift @fields; 
			# delete leading/trailing whitespace
			$mac_address =~ s/^\s+|\s+$//g;
			if( ! defined($mac_address) ) {
				printwarn "Invalid format of getconfig for mac($resultline)";
				$mac_address = undef;
			}else {
				print OUTPUT "MAC=$mac_address\n";
			}
		}

		my $ip_address = undef;
		if( defined($mac_address) ) {
			# Get IP address of VM
	
			# If dhcpd_lease file is defined, use it.
			# Otherwise, use vmware tool.
			if( defined($dhcpd_lease) && -r $dhcpd_lease ) {
				my $tmp_lease_file = "dhcpd_lease";
				copy( "$dhcpd_lease", "$tmp_lease_file") 
					or printwarn "Cannot copy file($dhcpd_lease) into working directory: $!";
	
				if( -r $tmp_lease_file && open(LEASEFILE, $tmp_lease_file) ) {
					my $tmp_ip;
					while (<LEASEFILE>) {
						next if /^#|^$/;
						if( /^lease (\d+\.\d+\.\d+\.\d+)/) {
							$tmp_ip = $1;
						}
						if( /$mac_address/ ) {
							$ip_address = $tmp_ip;
						}
					}
					close LEASEFILE;
				}
				unlink $tmp_lease_file;
			}
			if( defined($ip_address) ) {
				printverbose "getting IP address using lease file($dhcpd_lease)";
			}else {
				# We failed to get IP address of guest VM
				# We will retry to get it by using vmware tool
	
				$resultline = `"$vmwarecmd_prog" "$vmconfig" getguestinfo ip`;
				chomp($resultline);
				if( $resultline ) {
					# result must be like "getguestinfo(ip) = 172.16.123.143"
					my @fields = split /=/, $resultline;
					shift @fields;
					$ip_address = shift @fields;
					# delete leading/trailing whitespace
					$ip_address =~ s/^\s+|\s+$//g;
					if( ! defined($ip_address) ) {
						$ip_address = undef;
						printwarn "Invalid format of getguestinfo ip($resultline)";
					}
				}
			}
	
			if( defined($ip_address) ) {
				print OUTPUT "IP=$ip_address\n";
			}
		}
	}
}

sub start
{
#start       [vmconfig]	Start a VM
	printverbose "start is called";

	my $vmconfig = checkvmconfig($_[0]);

	if ($VMWARE_VERSION eq "workstation") {
		!system @VMRUN_CMD, "start", $vmconfig, "nogui"
			or printerror "Can't create vm with $vmconfig";
	}

        else {
		if( ! checkregister($vmconfig) ) {
			#!system $vmwarecmd_prog, "-s", "register", $vmconfig
			#	or printerror "Can't register a new vm with $vmconfig";
			system $vmwarecmd_prog, "-s", "register", $vmconfig;
		}

		# Now, a new vm is registered
		# Try to create a new vm
		!system $vmwarecmd_prog, $vmconfig, "start", "trysoft"
			or printerror "Can't create vm with $vmconfig";
        }

        sleep(5);
	# Get pid of main process of this VM
	my $vmpid = getvmpid($vmconfig);
	print OUTPUT "PID=$vmpid\n";
}

sub stop
{
#stop        [vmconfig]         Shutdown a VM
	printverbose "stop is called";
	my $vmconfig = checkvmconfig($_[0]);

	if ($VMWARE_VERSION eq "workstation") {
		# Try to stop vm
		!system @VMRUN_CMD, "stop", $vmconfig, "hard"
		    or printerror "Can't stop vm with $vmconfig";
		sleep(2);
	}	  

	else {
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
}

sub killvm
{
#killvm        [vmconfig]         kill a VM
	printverbose "killvm is called";
	my $matchstring = $_[0];

	if ($VMWARE_VERSION eq "workstation") {
		#
		# Rather than error out and cause ugly warnings in the log,
		# we'd like to try stopping the VM again.  Unfortunately,
		# the starter has already removed the execute directory by
		# the time it calls VMwareType::killVMFast(), so we don't
		# have a .vmx to pass to vmrun any longer.  (The .vmx must
		# exist; we can't just pass the path.)
		#

        my $processList = `ps uxw`;
        my @processes = split( /\n/, $processList );
        foreach my $process (@processes) {
            if( $process =~ m/vmware-vmx.*$matchstring/ ) {
                my( $user, $pid ) = split( /\s+/, $process );
                printwarn( "Found '$process', trying to kill...\n" );
                my $numSignalled = kill( 9, $pid );
                if( $numSignalled < 1 ) {
                    printerror( "Failed to signal process '$process'.\n" );
                } else {
                    printwarn( "Sent signal to process '$process'.\n" );
                    # Otherwise, the VM GAHP won't log the warnings.  Change
                    # back to zero once this problem is corrected.
                    exit 1;
                }
            }
        }
        printwarn( "Found no process matching '$matchstring', assuming succesful prior termination." );
        exit( 0 );
	}

	if( ! defined($matchstring) ) {
		usage();
	}

	printverbose "matching string is '$matchstring'";

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
				printverbose "Killing process(pid=$vmpid)";
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
					printverbose "Killing process(pid=$pidarr[1])";
					kill 9, $pidarr[1];
				}
			}
		}
	}
}

sub suspend
{
#suspend     [vmconfig]         Suspend a VM
	printverbose "suspend is called";
	my $vmconfig = checkvmconfig($_[0]);

	if ($VMWARE_VERSION eq "workstation") {
		!system @VMRUN_CMD, "suspend", $vmconfig, "soft"
			or printerror "Can't suspend vm with $vmconfig";
	}
	else {
		# Get status
		my $vmstatus = status($vmconfig);
		if( $vmstatus ne "Running" ) {
			printerror "vm($vmconfig) is not running";
		}

		!system $vmwarecmd_prog, $vmconfig, "suspend", "trysoft"
			or printerror "Can't suspend vm with $vmconfig";
	}

	# We need to guarantee all memory data into a file
	# VMware does some part of suspending in background mode
	# So, we give some time to VMware
	sleep(5);
}

sub resume
{
#resume      [vmconfig]         Restore a suspended VM
	printverbose "resume is called";
	my $vmconfig = checkvmconfig($_[0]);

	# Get status
	my $vmstatus = status($vmconfig);
	if( $vmstatus ne "Running" ) {
		start($vmconfig);
	}
}

sub snapshot
{
#snapshot    [vmconfig]         Create a snapshot of a VM
	printverbose "snapshot is called";
	my $vmconfig = checkvmconfig($_[0]);

	if ($VMWARE_VERSION eq "workstation") {
		!system @VMRUN_CMD, "snapshot", $vmconfig, "condor-snapshot"
			or printerror "Can't create snapshot for vm($vmconfig)";	
	}
	else {
		if( ! checkregister($vmconfig) ) {
			#!system $vmwarecmd_prog, "-s", "register", $vmconfig
			#	or printerror "Can't register a vm with $vmconfig";
			system $vmwarecmd_prog, "-s", "register", $vmconfig;
		}

		!system $vmrun_prog, "snapshot", $vmconfig
			or printerror "Can't create snapshot for vm($vmconfig)";
	}

	sleep(1);
}

sub commit
{
#commit      [vmconfig]         Commit COW disks and delete the COW
	printverbose "commit is called";
	my $vmconfig = checkvmconfig($_[0]);

	if ($VMWARE_VERSION eq "workstation") {
		!system @VMRUN_CMD, "deleteSnapshot", $vmconfig, "condor-snapshot"
			or printerror "Can't combine snapshot disk with base disk for vm($vmconfig)";
	}
	else {
		if( ! checkregister($vmconfig) ) {
			#!system $vmwarecmd_prog, "-s", "register", $vmconfig
			#	or printerror "Can't register a vm with $vmconfig";
			system $vmwarecmd_prog, "-s", "register", $vmconfig;
		}

		!system $vmrun_prog, "deleteSnapshot", $vmconfig
			or printerror "Can't combine snapshot disk with base disk for vm($vmconfig)";
	}

	sleep(5);
}

sub revert
{
#revert      [vmconfig]         Set VM state to a snapshot
	printverbose "revert is called";
	my $vmconfig = checkvmconfig($_[0]);

	if ($VMWARE_VERSION eq "workstation") {
		!system @VMRUN_CMD, "revertToSnapshot", $vmconfig, "condor-snapshot"
			or printerror "Can't revert VM state to a snapshot for vm($vmconfig)";
	}
	else {
		if( ! checkregister($vmconfig) ) {
			#!system $vmwarecmd_prog, "-s", "register", $vmconfig
			#	or printerror "Can't register a vm with $vmconfig";
			system $vmwarecmd_prog, "-s", "register", $vmconfig;
		}

		!system $vmrun_prog, "revertToSnapshot", $vmconfig
			or printerror "Can't revert VM state to a snapshot for vm($vmconfig)";
	}

	sleep(5);
}

sub createiso
{
#createiso   [listfile] [ISO]   Create an ISO image with files in a listfile
	printverbose "createiso is called";

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
	if (lc($^O) eq "mswin32") {
		!system $cdmake, "-q", "-j", "-m", $tmpdir, $cdlabel, $isofile
			or printerror "Cannot create an ISO file($isofile)";
	} else {
		!system $mkisofs, "-quiet", "-o", $isofile, "-input-charset", "iso8859-1", "-J", "-A", $cdlabel, "-V", $cdlabel, $tmpdir
			or printerror "Cannot create an ISO file($isofile)";
	}

	rmtree("$tmpdir")
		or printwarn "Cannot delete temporary directory($tmpdir) and files in it";

	sleep(1);
}

sub createconfig {}

if ($#ARGV < 0 || $ARGV[0] eq "--help") { usage(); }
elsif ($ARGV[0] eq "list")	{ list(); }
elsif ($ARGV[0] eq "check")	{ check(); }
elsif ($ARGV[0] eq "register")	{ register($ARGV[1]); }
elsif ($ARGV[0] eq "unregister"){ unregister($ARGV[1]); }
elsif ($ARGV[0] eq "start")	{ start($ARGV[1]); }
elsif ($ARGV[0] eq "stop")	{ stop($ARGV[1]); }
elsif ($ARGV[0] eq "killvm")	{ killvm($ARGV[1]); }
elsif ($ARGV[0] eq "suspend")	{ suspend($ARGV[1]); }
elsif ($ARGV[0] eq "resume")	{ resume($ARGV[1]); }
elsif ($ARGV[0] eq "status")	{ status($ARGV[1], 1); }
elsif ($ARGV[0] eq "getpid")	{ pidofvm($ARGV[1]); }
elsif ($ARGV[0] eq "getvminfo")	{ getvminfo($ARGV[1]); }
elsif ($ARGV[0] eq "snapshot")	{ snapshot($ARGV[1]); }
elsif ($ARGV[0] eq "commit")	{ commit($ARGV[1]); }
elsif ($ARGV[0] eq "revert")	{ revert($ARGV[1]); }
elsif ($ARGV[0] eq "createiso")	{ createiso($ARGV[1], $ARGV[2]); }
elsif ($ARGV[0] eq "createconfig")	{ createconfig($ARGV[1]); }
else { printerror "Unknown command \"$ARGV[0]\". See $progname --help."; }
