#!/s/perl/bin/perl

use Class::Struct;
use Cwd;
use Getopt::Long;
use File::Copy;
use CSL::mysql;
use DBD::mysql;
use DBI;
use Time::Local;

my $db;


my %months = (
    "1" => "January",
    "2" => "February",
    "3" => "March",
    "4" => "April",
    "5" => "May",
    "6" => "June",
    "7" => "July",
    "8" => "August",
    "9" => "September",
    "10" => "October",
    "11" => "November",
    "12" => "December",
);

# history of some platforms desired for plotting increased tests

my %testtrackingplatforms = (
	"sun4u_sol_5.9" => 1, 
	"x86_rhas_3" => 1,
	"hppa_hpux_11" => 1,
	"x86_rh_9" => 1,
);

my %specialtests = (
	"lib_classads-1" => 329,
	"lib_classads" => 329,
	"lib_unit_tests-1" => 55,
	"lib_unit_tests" => 55,
);

my %metrotask = (
	"fetch.test_glue.src" => "1",
	"fetch.input_build_runid.src" => "1",
	"pre_all" => "1",
	"platform_pre" => "1",
	"platform_post" => "1",
	"platform_job" => "1",
	"common.put" => "1",
	"remote_pre_declare" => "1",
	"remote_declare" => "1",
	"remote_pre" => "1",
	"remote_task" => "1",
	"remote_post" => "1",
);

# Plot based date format is Day Month Year and the data we write out here
# will be in this format for plotting.

DbConnect();


my @buildruns;
my %platformlist;
my $platformlistfile = "durationfiles";
my %platform_btimes; # build times
my %platform_ttimes; # test times
my @testruns;
my @testresults;
my $type = "builds";

my $TodayDate = "";

SetMonthYear();

GetOptions (
		'branch=s' => \$branch,
        'date=s' => \$datestring,
		'type=s' => \$type,
		'project' => \$project,
        'help' => \$help,
		'testhist=s' => \$thistfile,
		'save=s' => \$savefile,
		'new' => \$newfile,
		'append' => \$appendfile,
        #'megs=i' => \$megs,
);

#print "project($project) type ($type)\n";
if( (defined($project)) && ($type eq "tests")) {
	if(!$thistfile) {
		$thistfile = "/scratch/bt/btplots/testhistory";
	}
	#print "Creating /scratch/bt/btplots/testhistory\n";
	open(THIST,">$thistfile") or die "Unable to open test history file $thistfile:$!\n";
}

if($datestring) {
	$TodayDate = $datestring;
}

my $dropfile = "";
if(!defined($project)) {
	$dropfile = "/scratch/bt/btplots/" . $branch . "auto" . $type;
} else {
	$dropfile = "/scratch/bt/btplots/" . "projectauto" . $type;
}
print "Drop file will be $dropfile\n";

open(DROP,">$dropfile") or die "Unable to open result file $dropfile:$!\n";

FindBuildRuns();
#SaveRunIds();

if( $type eq "builds") {
	ShowBuildRuns();

	foreach $buildrun (@buildruns) {
		# we want to massage the data if we are looking for long
		# term build and test results. So try to wean out framework
		# issues where we get multiple entries pre branch per day
		# but leave the zero entry if a better one does not exist
		# we know we are in that mode when $branch is unset.
		# third arg is branch..
		my @args = split /:/,$buildrun;
		FindBuildTasks($args[0],$args[1],$args[2]);
	}
} elsif($type eq "tests") {
	CollectTestResults();
} elsif($type eq "times") {
	CollectTimeResults();
} else {
	die "No such Database Crunching Method<<$type>>\n";
}


close(DROP);
if( (defined($project)) && ($type eq "tests")) {
	close(THIST);
}
DbDisconnect();	# all done..... :-)

exit(0);

sub CollectTimeResults
{
	# which platforms do we care about
	# Look at ones in all the runs and see if 
	# we have them in our hash yet
	#print "Collecting Time Results\n";
	ShowBuildRuns();
	foreach $run (@buildruns) {
		print "Platforms for $run\n";
		FindPlatforms($run);
	}
	print "Done looking for platform list\n";
	print "save it as we go....\n";
	if( !$savefile ) {
		die "--save must be set for collecting image list<$savefile>\n";
	}
	if($newfile) {
		open(LIST,">$savefile") || die "Failed to start datafile:<$datafile> $!\n";
	} elsif($appendfile) {
		open(LIST,">>$savefile") || die "Failed to start datafile:<$datafile> $!\n";
	} else {
		die "Either --new or --append must be set\n";
	}

	print "Here are the keys from hash platformlist\n";
	foreach $key (sort keys %platformlist) {
		print "Key-----<$key>\n";
		my $datafile = "/scratch/bt/btplots/" . $branch . "-" . $key;
		print LIST "$datafile\n";
		open(DATA,">$datafile") || die "Failed to start datafile:<$datafile> $!\n";
		foreach $run (@buildruns) {
			print "Looking at tests for<<$run>>\n";
			@testruns = ();
			%platform_btimes = (); # empty it....
			%platform_ttimes = (); # empty it....
			FindBuildAndTestTimes($run, $key);
		}
		close(DATA);
	}
	close(LIST);
}

sub CollectTestResults
{
	foreach $run (@buildruns) {
		#print "Looking at tests for<<$run>>\n";
		@testruns = ();
		FindTestRuns($run);
		FindTestTasks();
	}
}

sub FindPlatforms
{
	$runid = shift;

	print "Platforms for <$runid> are:\n";
	@args = split /:/, $runid;
	$extraction = $db->prepare("SELECT platform FROM Task WHERE  \
			runid = $args[0] \
			and name =  'platform_job' \
			");
	$extraction->execute();
	while( my $sumref = $extraction->fetchrow_hashref() ){
		$platform = $sumref->{'platform'};

		#print "FindPlatforms<<$platform>>\n";

		if( exists $platformlist{$platform}) {
			# we are good and have this already 
			#print "Have $platform in list already\n";
		} else {
			print "Add $platform as a platform\n";
			$platformlist{$platform} = 1;
		}
	}
}

sub FindBuildAndTestTimes
{
	my $runid = shift;
	my $desiredplatform = shift;

	my $nullresult = 0;
	my $goodresult = 0;
	my $badresult = 0;
	my $result = 0;
	my $extraction;

	print "FindBuildTimes: runid = $runid\n";

	@args = split /:/, $runid;
	$plotdate = $args[1];

	if($plotdate =~ /^(\d+)\s+[0]?(\d+)\s+(\d+).*$/) {
		# turn month into spelling
		$plotdate = $1 . " " . $months{$2} . " " . $3;
	}

	print "FindBuildTasks: args are <$runid,$plotdate>\n";
	#print "restricting look at platform_job only\n";
	$extraction = $db->prepare("SELECT result, name, start, finish, platform FROM Task WHERE  \
			runid = $args[0] \
			and name =  'platform_job' \
			and platform = '$desiredplatform' \
			");
    $extraction->execute();

	# we really only care about the results for the automatics
	# null(not defined()) means still in never never land, 0 is good and all else bad.

	#First fill build times for this runid

	my $time = 0.3;
	while( my $sumref = $extraction->fetchrow_hashref() ){
		$res = $sumref->{'result'};
		$name = $sumref->{'name'};
		$start = $sumref->{'start'};
		$done = $sumref->{'finish'};
		$platform = $sumref->{'platform'};
		$time = 0.0;
		#print "$name for platform $platform is <<$res>> started $start finished $done\n";
		if($done == NULL) {
			#print "Platform $platform is still pending\n";
			$platform_btimes{"$platform"} = -10;
		} else {
			$time = GetTaskDuration($start,$done);
			$platform_btimes{"$platform"} = $time;
			#print "Calculated build duration as <<$platform/$time>>\n";
		}
	}

	# now find the runids(test runs) which used this build id for data
	print "Calling FindTestRuns\n";
	FindTestRuns($run);
	print "Calling FindTestTimes\n";
	FindTestTimes($desiredplatform);

	# so we now have all the builds in one hash and existent tests in another
	# all we need to do now is drop this days data....
	foreach $key (sort keys %platform_btimes) {
		$btime = $platform_btimes{"$key"};
		if( exists $platform_ttimes{"$key"}) {
			$ttime = $platform_ttimes{"$key"};
		} else {
			$ttime = 0;
		}
		$together = ($ttime + $btime);
		# scale back large values and change color key by moving
		# where we record the data
		if($together > 50) {
			$ttime = $ttime / 10.0;
			$btime = $btime / 10.0;
			$together = ($ttime + $btime);
			$output = sprintf("%s, %s, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f",$plotdate, $key, 0, 0, 0, $btime, $ttime, $together);
		} else {
			$output = sprintf("%s, %s, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f",$plotdate, $key, $btime, $ttime, $together, 0, 0, 0);
		}

		print  DATA "$output\n";
	}
}

sub GetTaskDuration
{
	my $start = shift;
	my $done = shift;
	my $diff = 0;
	if( !defined($start)) { return(-5) }; # just plain wrong
	if( !defined($done)) { return(-10) }; # pending
	my $begin = MysqlDate2Seconds($start);
	my $end = MysqlDate2Seconds($done);
	$diff = $end - $begin;
	$time = Seconds2Days($diff);
	return($time);
}

sub FindTestTimes
{
	$desiredplatform = shift;
	print "Find Test Times\n";
	for $run (@testruns) {
		@args = split /:/, $run;

		#print "FindTestTimes: runid <<$args[0]>>\n";
		my $extraction = $db->prepare("SELECT runid, platform, start, finish FROM Task WHERE  \
				runid = $args[0] \
				and name = 'platform_job' \
				and platform = '$desiredplatform' \
				");
    	$extraction->execute();
		while( my $sumref = $extraction->fetchrow_hashref() ){
        	$trid = $sumref->{'runid'};
			$platform = $sumref->{'platform'};
			$start = $sumref->{'start'};
			$finish = $sumref->{'finish'};
			$time = GetTaskDuration($start,$finish);
			$platform_ttimes{"$platform"} = $time;
			#print "Calculated first duration as <<$platform/$time>>\n";
		}
	}
}

sub FindBuildTasks
{
	my $runid = shift;
	my $plotdate = shift;
	my $branch = shift;

	my $nullresult = 0;
	my $goodresult = 0;
	my $badresult = 0;
	my $result = 0;
	my $extraction;

	if($plotdate =~ /^(\d+)\s+[0]?(\d+)\s+(\d+).*$/) {
		# turn month into spelling
		$plotdate = $1 . " " . $months{$2} . " " . $3;
	}

	#print "FindBuildTasks: args are <$runid,$plotdate>\n";
	#print "restricting look at platform_job only\n";
	$extraction = $db->prepare("SELECT result, name, platform FROM Task WHERE  \
			runid = $runid \
			and name =  'platform_job' \
			");
    $extraction->execute();

	# we really only care about the results for the automatics
	# null(not defined()) means still in never never land, 0 is good and all else bad.

	while( my $sumref = $extraction->fetchrow_hashref() ){
		$res = $sumref->{'result'};
		$name = $sumref->{'name'};
		$platform = $sumref->{'platform'};
		#print "<$res>\n";
		if( defined($res)) { 
			#print "<$res>\n";
			if($res == 0) {
				#print "GOOD\n";
				$goodresult = $goodresult + 1;
			} else {
				# we are left with the positive and negative failures
				#print "BAD\n";
				$badresult = $badresult + 1;
			}
		} else {
			#print "NULL\n";
			$nullresult = $nullresult + 1;
		}
	}
	my $pandb = $nullresult + $badresult;
	my $pandbandf = $pandb + $goodresult;
	 
	if( defined($branch) ) {
		print DROP "$plotdate, $nullresult, $badresult, $pandb, $goodresult, $pandbandf, $branch\n"; 
	} else {
		print DROP "$plotdate, $nullresult, $badresult, $pandb, $goodresult, $pandbandf\n"; 
	}
}

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
	
	# looking for project use or nightly use?
	#print "Looking for runs since <$TodayDate>\n";
	my $extraction;
	if(!defined($project)) {
		$extraction = $db->prepare("SELECT runid, result, project_version, start, \
				component_version, description FROM Run WHERE  \
				user = 'cndrauto' \
				and project = 'condor' \
				and start >=  '$TodayDate' \
				and run_type = 'BUILD'");
	} else {
		$extraction = $db->prepare("SELECT runid, result, project_version, start, \
				component_version, description FROM Run WHERE  \
				#user = 'cndrauto' \
				#and project = 'condor' \
				project = 'condor' \
				and start >=  '$TodayDate' \
				and run_type = 'BUILD'");
	}

    $extraction->execute();
	while( my $sumref = $extraction->fetchrow_hashref() ){
        $rid = $sumref->{'runid'};
		$res = $sumref->{'result'};
		$pv = $sumref->{'project_version'};
		$starttime = $sumref->{'start'};
		$cv = $sumref->{'component_version'};
		$des = $sumref->{'description'};

		if($starttime =~ /^(\d+)\-(\d+)\-(\d+)\s*.*$/) {
			#print "Plat date is $3 $2 $1\n";
			$plotdateform = $3 . " " . $2 . " " . $1;
		} else {
			die "can not create plot date from starttime!!!!\n";
		}


		#print "Runs since: $TodayDate on $starttime builds $rid pv:$pv cv:$cv\n";
		if( !defined($res)) {
			#print "this one is currently pending!\n";
			#FindBuildTasks($rid, $plotdateform, "all");
		}
		if($branch) {
			if( $des =~ /^$branch.*$/ ) {
				print "adding $rid and $plotdateform to array buildruns\n";
				print "Described as $des\n";
				push(@buildruns,$rid . ":" . $plotdateform);
			} else {
				#print "1: Did not find <$branch> in <$des>\n";
			}
		} else {
			if(!defined($project)) {
				if( $des =~ /^.*([V]*\d*[_]*\d*[\-]*(trunk|branch)).*$/ ) {
					#changed to find trunk as a branch 12/08 bt
					push(@buildruns,$rid . ":" . $plotdateform . ":" . $1);
				} elsif( $des =~ /^.*(Continuous\s*Build\s*-.*)$/ ) {
					#changed to find trunk as a branch 12/08 bt
					push(@buildruns,$rid . ":" . $plotdateform . ":" . $1);
				} else {
					print "2: Did not find <$branch> in <$des>\n";
				}
			} else {
				# when collecting for project take ALL branches
				push(@buildruns,$rid . ":" . $plotdateform . ":" . $des);
			}
		}
	}
}

sub FindTestRuns
{
	my $runid = shift;
	my $trid = "";
	my @args = split /:/, $runid;

	print "******************* FindTestRuns *******************\n";
	my $extraction = $db->prepare("SELECT runid FROM Method_nmi WHERE  \
				input_runid = '$args[0]'");
    $extraction->execute();
	$testruns = ();
	print "*********************************\n";
	while( my $sumref = $extraction->fetchrow_hashref() ){
        $trid = $sumref->{'runid'};
		# if a branch were not called out we pass the branch along.
		push(@testruns,$trid . ":" . $args[1] . ":" . $args[2]);
		print "$trid:$args[1]:$args[2]\n";
	}
	print "*********************************\n";
}

sub FindTestTasks
{
	my $result = 0;
	my $extraction;
	my $plotdateform = "";

	my $pandb = 0;
	my $pandbandf = 0;

	# accumulate from all test runs
	my $nullresult = 0;
	my $goodresult = 0;
	my $badresult = 0;
	my $builddate = "";
	my $platform = "";
	#my $off = 1;

	print "******************* FindTestTasks *******************\n";
	print "*********************************\n";
	foreach $runid (@testruns) {

		my @args = split /:/, $runid;
		$builddate = $args[1];
		$thisbranch = $args[2];

		if($builddate =~ /^(\d+)\s+[0]?(\d+)\s+(\d+).*$/) {
			#print "Plat date is $3 $2 $1\n";
			$plotdateform = $1 . " " . $months{$2} . " " . $3;
		} else {
			die "can not create plot date from starttime!!!!\n";
		}

		print "************************** Optimized **************************\n";
		# new more optimized
		# verify that the user is cndrauto unless another is called out
		# I for one am known to run entire sets of tests on nightly builds
		# and we do not want to mix these results.

		$extraction = $db->prepare("\
					SELECT \
					Task.result AS result, \
					Task.name AS name, \
					Task.start AS start, \
					Task.platform AS platform, \
					Run.user AS user \
					FROM Run, Task WHERE  \
					Run.runid = $args[0] \
					AND Task.runid = $args[0]");

	    $extraction->execute();

		my $ownref = $extraction->fetchrow_hashref();
		$testuser = $ownref->{'user'};

		print "Run for user<$testuser>\n";

		# looking at all project use vs nightly use
		if(!defined($project)) {
			if($testuser ne "cndrauto") {
				# do not add these results in cause its a different user
				next;
			}
		}

		# we really only care about the results for the automatics
		# null(not defined()) means still in never never land, 0 is good and all else bad.
		
		# track individual runs and then add to accumulators
		$thisgood = 0;
		$thisbad = 0;
		$thisnull = 0;
		while( my $ownref = $extraction->fetchrow_hashref() ){
			$res = $ownref->{'result'};
			$name = $ownref->{'name'};
			$start = $ownref->{'start'};
			$platform = $ownref->{'platform'};
			if(!(exists $metrotask{"$name"})) {
				#print "TestRun $runid Task $name Time $start Platform $platform<$res>\n";
				my $value = 1;
				if( exists $specialtests{"$name"} ) {
					$value = $specialtests{"$name"};
					#print "Special Test <$name>\n";
				} else {
					#print "Nothing special about test <$name>\n";
				}
				if( defined($res)) { 
					#print "<$res>\n";
					if($res == 0) {
						#print "GOOD\n";
						$thisgood = $thisgood + $value;
					} else {
						# we are left with the positive and negative failures
						#print "BAD\n";
						$thisbad = $thisbad + $value;
					}
				} else {
					#print "NULL\n";
					$thisnull = $thisnull + $value;
				}
			}
		}
	
		# track a few platforms of test count to see effort
		if( (exists $testtrackingplatforms{"$platform"} ) && ( $testuser eq "cndrauto" )){
			if( $thisbranch =~ /^.*(([V]*\d*[_]*\d*[\-]*)(trunk|branch)).*$/) {
				my $sum = $thisnull + $thisgood + $thisbad;
				if($sum != 0) {
					print THIST "$plotdateform, $platform, $1, $sum\n";
				}
			}
		}

		# add results to accumulators
		$badresult = $badresult + $thisbad;
		$goodresult = $goodresult + $thisgood;
		$nullresult = $nullresult + $thisnull;

		$pandb = $nullresult + $badresult;
		$pandbandf = $pandb + $goodresult;
		 
	}
	print "*********************************\n";
	#print "Runs for tests = <<$#testruns>>\n";
	# more then 2 platforms
	if($#testruns > 1) {
		print DROP "$plotdateform, $nullresult, $badresult, $pandb, $goodresult, $pandbandf, $thisbranch\n"; 
	}
}

sub ShowTestRuns
{
	foreach $test (@testruns) {
		print "$test\n";
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
    print "Usage: condor_nmitests.pl --gid=<string>
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

sub MysqlDate2Seconds
{
	$date = shift;
	my $year = 0;
	my $mon = 0;
	my $mday = 0;
	my $hour = 0;
	my $min = 0;
	my $sec = 0;
	#print "Parse this date <<$date>>\n";
	if($date =~ /^(\d+)\-(\d+)\-(\d+)\s+(\d+):(\d+):(\d+)$/) {
		#print "OK\n";
		$year = $1 - 1900;
		$mon = $2 - 1;
		$mday = $3;
		$hour = $4;
		$min = $5;
		$sec = $6;
		#print "$sec,$min,$hour,$mday,$mon,$year";
		$timeseconds = timelocal($sec,$min,$hour,$mday,$mon,$year);
		#print "seconds returned is <<$timeseconds>>\n";
		return($timeseconds);
	}
	else
	{
		die "Failed to parse MysqlDate:$date\n";
	}
}

sub Seconds2Days
{
	$seconds = shift;
	$hours = ($seconds / 3600.00);
	#print "$seconds is $hours hours\n";
	return($hours);
}
