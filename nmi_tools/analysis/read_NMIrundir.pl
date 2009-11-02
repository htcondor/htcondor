#!/usr/bin/env perl

use Class::Struct;
use Cwd;
use Getopt::Long;
use File::Copy;
use DBI;

my $db;


# Plot based date format is Day Month Year and the data we write out here
# will be in this format for plotting.

DbConnect();


my @buildruns;
my $type = "builds";

my $TodayDate = "";

SetMonthYear();

GetOptions (
		'branch=s' => \$branch,
        'date=s' => \$datestring,
		'type=s' => \$type,
		'httphost=s' => \$webserver,
        'help' => \$help,
);

if($help) { help(); exit(0); }

if(!$webserver) {
	$webserver = "nmi-s003.cs.wisc.edu";
}

if($datestring) {
	$TodayDate = $datestring;
}


FindBuildRuns();

if( $type eq "builds") {
	ShowBuildRuns();

	foreach $buildrun (@buildruns) {
		my @args = split /:/,$buildrun;
		FindBuildRunDir($args[0]);
	}
} else {
	die "No such Database Crunching Method<<$type>>\n";
}


DbDisconnect();	# all done..... :-)

exit(0);

sub FindBuildRunDir
{
	my $url = "http://";
	my $runid = shift;
	my $extraction = $db->prepare("SELECT * FROM Run WHERE  \
				runid = '$runid' \
				");
    $extraction->execute();
	while( my $sumref = $extraction->fetchrow_hashref() ){
		$filepath = $sumref->{'filepath'};
		$gid = $sumref->{'gid'};
		#print "<<<$filepath>>><<<$gid>>>\n";
		$url = $url . $webserver . $filepath . "/" . $gid;
		print "$url\n";
	}
}

###################################################
#
#	Find 'builds" on a particular date and store
#
#	Request is in form  of year-month-day
#   
#	We can not ask for the exact time so we'll
#	get all the ones after midnight and cut any
#	newer then that day.
#

sub FindBuildRuns
{
	my $runid = shift;
	my $nm = "";
	my $res = "";
	my $pv = "";
	my $rid = "";
	my $cv = "";
	my $plotdateform = "";
	my $starttime = "";

	my $inyear;
	my $inmon;
	my $inday;
	

	print "Looking for build runs on this date: $TodayDate\n";

	if($TodayDate =~ /^(\d+)\-(\d+)\-(\d+)\s*.*$/) {
		$inyear = $1;
		$inmon = $2;
		$inday = $3;
	} else {
		die "Can not parse date:$TodayDate. Experted year-month-day.\n";
	}
	
	my $extraction = $db->prepare("SELECT * FROM Run WHERE  \
				user = 'cndrauto' \
				and project = 'condor' \
				and start >=  '$TodayDate' \
				and run_type = 'BUILD'");
    $extraction->execute();
	while( my $sumref = $extraction->fetchrow_hashref() ){
		my $dbyear;
		my $dbmon;
		my $dbday;
        $rid = $sumref->{'runid'};
		$res = $sumref->{'result'};
		$pv = $sumref->{'project_version'};
		$starttime = $sumref->{'start'};
		$cv = $sumref->{'component_version'};
		$des = $sumref->{'description'};

		if($starttime =~ /^(\d+)\-(\d+)\-(\d+)\s*.*$/) {
			#print "Plat date is $3 $2 $1\n";
			$dbyear = $1;
			$dbmon = $2;
			$dbday = $3;
		} else {
			die "can not create plot date from starttime!!!!:$starttime\n";
		}

		if( !(($dbyear eq $inyear) && ($dbmon eq $inmon) && ($dbday eq $inday))) {
			next;
		}

		#print "Runs since: $TodayDate on $starttime builds $rid pv:$pv cv:$cv\n";
		if( !defined($res)) {
			#print "this one is currently pending!\n";
			#FindBuildTasks($rid, $plotdateform, "all");
		}
		if($branch) {
			if( $des =~ /^.*$branch-(trunk|branch).*$/ ) {
				push(@buildruns,$rid . ":" . $plotdateform);
			} else {
				#print "Did not findd <$branch> in <$des>\n";
			}
		} else {
			push(@buildruns,$rid  . ":" . $plotdateform);
		}
	}
}

sub ShowBuildRuns
{
	foreach $run (@buildruns) {
		print "$run\n";
	}
}

sub DbConnect
{
	$db = DBI->connect("DBI:mysql:database=nmi_history;host=mysql.batlab.org", "nmipublic", "nmiReadOnly!") || die "Could not connect to database: $!\n";
	#print "Connected to db!\n";
}

sub DbDisconnect
{
	$db->disconnect;
}

##########################################
#
# print help
#

sub help
{
    print "Usage: xxxxxxxxx.pl
    Options:
        [-h/--help]             See this
        [-b/--branch]           Limit runs by branch checks for pattern in NMI DB Run description
        [-d/--date]             Process all builds after dates in this form:'2007-05-30'
        [-t/--type]             What type of data are we after: builds, tests, duration ...
									\n";
}

sub SetMonthYear
{
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();

    $currentmonth = $mon + 1;

    if($currentmonth < 10 ) {
        $monthstring = "0$currentmonth";
    } else {
        $monthstring = "$currentmonth";
    }
    $currentyear = $year + 1900;
	$TodayDate = $currentyear . "-" . $monthstring . "-" . $mday;
	print "Today is $TodayDate\n";
}
