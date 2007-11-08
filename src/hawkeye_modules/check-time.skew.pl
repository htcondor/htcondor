#!/s/std/bin/perl
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


######################################################################
# This script finds out the time skew between a given remote machine #
# and the local machine from which the job is submitted.It uses the  #
# globus-job-run command                                             #
######################################################################

use English;
require "getopts.pl";

# Get options for the script
&Getopts ("hr:g:");

# Process options
if ($opt_h) {
  die "Usage: check-time-skew.pl [-h] [-r <remote_machine_name>] [-g <globus location>]\n\n\n";
}  

unless ($opt_r) {
  die ("Error: Remote host name not specified. -h for usage\n");
}
$remote_host = $opt_r;

# Get local date, hostname
unless ($_ = `/bin/date -u`) {
  die ("Error: Command /bin/date not found\n");
}
chomp $_;
@_ = split (/\s+/, $_);
@local_time = split (/:/, $_[3]);
chomp ($local_host = `hostname`);
print ("Time on $local_host : $_[3]\n");

# Ensure that globus path is known
unless ($ENV{GLOBUS_LOCATION}) {
  unless ($opt_g) {
    print "Error: Globus location not specified. Specify with -g option\n";
    print "       or set environment variable GLOBUS_LOCATION\n";
    die ("\n");
  }
  $ENV{GLOBUS_LOCATION} = $opt_g;
  $globus_location = $opt_g; 
}
else {
  $globus_location = $ENV{GLOBUS_LOCATION};
}

# Get remote date
unless ($_ = `${globus_location}/bin/globus-job-run $remote_host /bin/date -u`) {
  die ("Error in running remote job\n");
}
chomp $_;

# Calculate and print time skew
@_ = split (/\s+/, $_);
@remote_time = split (/:/, $_[3]);
print ("Time on $remote_host : $_[3]\n");

$time_skew = $local_time[0]*3600 + $local_time[1]*60 + $local_time[2];
$time_skew -= $remote_time[0]*3600 + $remote_time[1]*60 + $remote_time[2];

printf ("Time skew between $local_host and $remote_host = %d seconds\n", $time_skew);
exit 0;
