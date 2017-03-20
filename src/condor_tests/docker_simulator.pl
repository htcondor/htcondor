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

###########################################################################
# Update:
# the script can now simulate docker functionalities:
#   version, info, images: with fixed fake data
#   run: where it sleeps for 60s, echos job id and name, and returns
#   rm, kill, pause, unpause: right now they just prints job name to stdout
###########################################################################

use strict;
use warnings;

my $logfile = "docker_simulator.log";
my $arg;
my $version_log;
my $info_log;
my $images_log;

# attach to the end of the log file.
open(FH,">>$logfile") || print "FAILED opening file $logfile\n";

my $parsed_ref = parse_args(\@ARGV);
my @parsed;
if ($parsed_ref ne '0') {
	@parsed = @{$parsed_ref};
}
my $name;
if (defined $parsed[0]) {
	$name = $parsed[0];
}

# rules of processing the arguments.
my %rules_arg = 
(
	'version' => sub {
		print FH gettime(time), " @ARGV\n";
		$version_log = version();
		print "$version_log";
	},
	'info' => sub {
		print FH gettime(time), " @ARGV\n";
		$info_log = info();
		print "$info_log";
	},
	'images' => sub {
		print FH gettime(time), " @ARGV\n";
		$images_log = images();
		print "$images_log";
	},
	'run' => sub {
		print FH gettime(time), " @ARGV\n";
		run($name);
	},
	'inspect' => sub {
		print FH gettime(time), " @ARGV\n";
		print inspect($ARGV[1]);
	},
	"rm" => sub {
		print FH gettime(time), " @ARGV\n";
		print rm($ARGV[1]);
	},
	'kill' => sub {                                             ## right now kill and the others returns the same std out as rm does
		print FH gettime(time), " @ARGV\n";     ## use rm() for now but might need to change later
		print rm($ARGV[1]);
	},
	'pause' => sub {
		print FH gettime(time), " @ARGV\n";
		print rm($ARGV[1]);
	},
	'unpause' => sub {
		print FH gettime(time), " @ARGV\n";
		print rm($ARGV[1]);
	}
);

if (defined $rules_arg{$ARGV[0]}) {
	$rules_arg{$ARGV[0]}();
} 
else {
	print FH gettime(time), " Invalid command\n";
	usage();
}

# functions related to different arguments
sub version {
	my $line =  "client version: 1.7.1
Client API version: 1.19
Go version (client): go1.4.2
Git commit (client): 786b29d/1.7.1
OS/Arch (client): linux/amd64
Server version: 1.7.1
Server API version: 1.19
Go version (server): go1.4.2
Git commit (server): 786b29d/1.7.1
OS/Arch (server): linux/amd64
";
	return $line;
}

sub info {
	my $line = "Containers: 6
Images: 4
Storage Driver: devicemapper
 Pool Name: docker-253:0-522536-pool
 Pool Blocksize: 65.54 kB
 Backing Filesystem: extfs
 Data file: /dev/loop0
 Metadata file: /dev/loop1
 Data Space Used: 317.4 MB
 Data Space Total: 107.4 GB
 Data Space Available: 30.48 GB
 Metadata Space Used: 1.069 MB
 Metadata Space Total: 2.147 GB
 Metadata Space Available: 2.146 GB
 Udev Sync Supported: true
 Deferred Removal Enabled: false
 Data loop file: /var/lib/docker/devicemapper/devicemapper/data
 Metadata loop file: /var/lib/docker/devicemapper/devicemapper/metadata
 Library Version: 1.02.89-RHEL6 (2014-09-01)
Execution Driver: native-0.2
Logging Driver: json-file
Kernel Version: 2.6.32-642.11.1.el6.x86_64
Operating System: <unknown>
CPUs: 1
Total Memory: 1.826 GiB
Name: localhost.localdomain
ID: 4QHG:JPN3:H2UL:MANT:VUWJ:4J2G:WI6N:TASX:NWPF:G2J3:SVL6:CRDV
";
	return $line;
}

sub images {
	my $line = "REPOSITORY          TAG                 IMAGE ID            CREATED             VIRTUAL SIZE
centos              latest              d4350798c2ee        5 weeks ago         191.8 MB
";
	return $line;
}

sub run {
	my $jobname = $_[0];
	print "Job: $jobname, Id: ", int(rand(1000)), "\n";
	sleep(60);
	print FH gettime(time), " Job $jobname finished\n";
}

sub inspect {
	my $jobname =$_[0];
	return "[
{
    \"Id\": \"a0b69664b42fc84b7bb46f4f51275efd3c81c687647d5c4e98018f2413a429c4\",
    \"Created\": \"2017-01-23T22:49:18.611954836Z\",
    \"Path\": \"/bin/bash\",
    \"Args\": [],
    \"State\": {
        \"Running\": false,
        \"Paused\": false,
        \"Restarting\": false,
        \"OOMKilled\": false,
        \"Dead\": false,
        \"Pid\": 0,
        \"ExitCode\": 0,
        \"Error\": \"\",
        \"StartedAt\": \"2017-01-23T22:49:20.253395177Z\",
        \"FinishedAt\": \"2017-01-23T22:49:20.961450116Z\"
    },
    \"Image\": \"fb434121fc77c965f255cbb848927f577bbdbd9325bdc1d7f1b33f99936b9abb\",
    \"NetworkSettings\": {
        \"Bridge\": \"\",
        \"EndpointID\": \"\",
        \"Gateway\": \"\",
        \"GlobalIPv6Address\": \"\",
        \"GlobalIPv6PrefixLen\": 0,
        \"HairpinMode\": false,
        \"IPAddress\": \"\",
        \"IPPrefixLen\": 0,
        \"IPv6Gateway\": \"\",
        \"LinkLocalIPv6Address\": \"\",
        \"LinkLocalIPv6PrefixLen\": 0,
        \"MacAddress\": \"\",
        \"NetworkID\": \"\",
        \"PortMapping\": null,
        \"Ports\": null,
        \"SandboxKey\": \"\",
        \"SecondaryIPAddresses\": null,
        \"SecondaryIPv6Addresses\": null
    },
    \"ResolvConfPath\": \"/var/lib/docker/containers/a0b69664b42fc84b7bb46f4f51275efd3c81c687647d5c4e98018f2413a429c4/resolv.conf\",
    \"HostnamePath\": \"/var/lib/docker/containers/a0b69664b42fc84b7bb46f4f51275efd3c81c687647d5c4e98018f2413a429c4/hostname\",
    \"HostsPath\": \"/var/lib/docker/containers/a0b69664b42fc84b7bb46f4f51275efd3c81c687647d5c4e98018f2413a429c4/hosts\",
    \"LogPath\": \"/var/lib/docker/containers/a0b69664b42fc84b7bb46f4f51275efd3c81c687647d5c4e98018f2413a429c4/a0b69664b42fc84b7bb46f4f51275efd3c81c687647d5c4e98018f2413a429c4-json.log\",
    \"Name\": \"/$jobname\",
    \"RestartCount\": 0,
    \"Driver\": \"devicemapper\",
    \"ExecDriver\": \"native-0.2\",
    \"MountLabel\": \"\",
    \"ProcessLabel\": \"\",
    \"Volumes\": {},
    \"VolumesRW\": {},
    \"AppArmorProfile\": \"\",
    \"ExecIDs\": null,
    \"HostConfig\": {
        \"Binds\": null,
        \"ContainerIDFile\": \"\",
        \"LxcConf\": [],
        \"Memory\": 0,
        \"MemorySwap\": 0,
        \"CpuShares\": 0,
        \"CpuPeriod\": 0,
        \"CpusetCpus\": \"\",
        \"CpusetMems\": \"\",
        \"CpuQuota\": 0,
        \"BlkioWeight\": 0,
        \"OomKillDisable\": false,
        \"Privileged\": false,
        \"PortBindings\": {},
        \"Links\": null,
        \"PublishAllPorts\": false,
        \"Dns\": null,
        \"DnsSearch\": null,
        \"ExtraHosts\": null,
        \"VolumesFrom\": null,
        \"Devices\": [],
        \"NetworkMode\": \"bridge\",
        \"IpcMode\": \"\",
        \"PidMode\": \"\",
        \"UTSMode\": \"\",
        \"CapAdd\": null,
        \"CapDrop\": null,
        \"RestartPolicy\": {
            \"Name\": \"no\",
            \"MaximumRetryCount\": 0
        },
        \"SecurityOpt\": null,
        \"ReadonlyRootfs\": false,
        \"Ulimits\": null,
        \"LogConfig\": {
            \"Type\": \"json-file\",
            \"Config\": {}
        },
        \"CgroupParent\": \"\"
    },
    \"Config\": {
        \"Hostname\": \"a0b69664b42f\",
        \"Domainname\": \"\",
        \"User\": \"\",
        \"AttachStdin\": false,
        \"AttachStdout\": true,
        \"AttachStderr\": true,
        \"PortSpecs\": null,
        \"ExposedPorts\": null,
        \"Tty\": false,
        \"OpenStdin\": false,
        \"StdinOnce\": false,
        \"Env\": [
            \"PATH=/usr/local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\"
        ],
        \"Cmd\": [
            \"/bin/bash\"
        ],
        \"Image\": \"docker/test\",
        \"Volumes\": null,
        \"VolumeDriver\": \"\",
        \"WorkingDir\": \"/test\",
        \"Entrypoint\": null,
        \"NetworkDisabled\": false,
        \"MacAddress\": \"\",
        \"OnBuild\": null,
        \"Labels\": {}
    }
}
]
";
}

sub rm {
	my $jobname = $_[0];
	return "$jobname\n";
}

# parse the command line arguments: 0->name 1->.....
sub parse_args {
	my @args = @{$_[0]};
	my @parsed_elements;
	for my $i (0..(scalar @args)-1) {
		if ($args[$i] =~ /--name/) {
			$name = $args[$i+1];
		}
#		if ......    # other arguments

	}
	return @parsed_elements;
}

sub gettime {
	my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime(time);
	$mon = sprintf("%d",$mon + 1);
	$mday = sprintf("%d", $mday);
	$hour = sprintf("%02d", $hour);
	$min = sprintf("%02d", $min);
	$sec = sprintf("%02d", $sec);
	return ("$mon/$mday ", "$hour:$min:$sec");
}

sub usage {
	print "Invalid command!\nUsage:
  version: ./docker_simulator.pl version
  info:    ./docker_simulator.pl info
  images:  ./docker_simulator.pl images
  run:     ./docker_simulator.pl --name <JOBNAME>
  inspect: ./docker_simulator.pl inspect <JOBNAME>
  rm:      ./docker_simulator.pl rm <JOBNAME>
  kill:    ./docker_simulator.pl kill <JOBNAME>
  pause:   ./docker_simulator.pl pause <JOBNAME>
  unpause: ./docker_simulator.pl unpause <JOBNAME>
";
	exit;
}
