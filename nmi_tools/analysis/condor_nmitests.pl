#!/usr/bin/env perl
use Class::Struct;
use Cwd;
use Getopt::Long;
use File::Copy;
#use CSL::mysql;
use DBI;


# This is the base storage unit filled in for each
# platform from lost builds to losts tests to ongoing tests

struct Platform_info =>
{
	platform => '$',
	passed => '$',
	failed => '$',
	expected => '$',
	build_errs => '$',
	condor_errs => '$',
	test_errs => '$',
	unknown_errs => '$',
	platform_errs => '$',
	framework_errs => '$',
	count_errs => '$',
};

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

GetOptions (
		'arch=s' => \$arch,
		'lost=s' => \$losttests,
		'branch=s' => \$branch,
        'runid=s' => \$requestrunid,
        'help' => \$help,
		# types of blame
		'builds=s' => \$berror,
		'platform=s' => \$perror,
		'framework=s' => \$ferror,
		'tests=s' => \$terror,
		'condor=s' => \$cerror,
		'unknown=s' => \$uerror,
		'single=s' => \$singletest,
		'who=s' => \$whoosetests,
);

my $gid;
@platformresults;
@buildresults;
$histcache = "analysisdata";
$histverison = "";

my $totalgood = 0;;
my $totalbad = 0;;
my $totaltests = 0;
my $totalexpected = 0;
my $totalbuilds = 0;

my $totaltesterr = 0;
my $totalcondorerr = 0;
my $totalframeworkerr = 0;
my $totalplatformerr = 0;
my $totalunknownerr = 0;
my $totalcounterr = 0;

if($help) { help(); exit(0); }

if(!$singletest && !$berror) {
	if(!$requestrunid) {
		print "You must enter the build Runid to check test data from!\n";
		help();
		exit(1);
	}
} else {
	if(!$arch && !$berror) {
		print "Can not evaluate single test without platform type!\n";
		help();
		exit(1);
	}
}
	
$foruser = "cndrauto";

if($whoosetests) {
	$foruser = $whoosetests;
} 

my $basedir;

# test db access of base
DbConnect();
$basedir = FindBuildRunDir($requestrunid);
print "Have <<$basedir>> for basedir\n";
DbDisconnect();

my $TopDir = getcwd(); # current location hosting testhistory data and platform data cache
my $CacheDir = "";     # level where different platform data files go.

chomp($TopDir);
#print "Working dir is <$TopDir>\n";

my %TestPlatforms;
my %RunPlatforms;
my %ProblemTestRun;
my @TestRuns;
my %BuildTargets;
my $line = "";
my $platform = "";
my $platformpost = "";
my $buildtag = "";

my $histold = "";
my $histnew = "";


# if we are assigning losses to the Builds and then saying why
# push a marker into our storage for later listing and crunching.

if((!$singletest) && (!($berror =~ /all/))) {
	print "Opening build output for RunId <$requestrunid>\n";
	$builddir = $basedir;;
	print "Testing <<$basedir>>\n";
	if(!(-d $basedir )) {
		print "<<$basedir>> Does not exist\n";
	}
	opendir DH, $builddir or die "Can not open Build Results<$builddir>:$!\n";
	foreach $file (readdir DH) {
		if($file =~ /platform_post\.(.*).out/) {
			$platformpost = $builddir . "/" . $file;
			#print "$platformpost is for platform $1\n";
			$platform = $1;
			open(FH,"<$platformpost") || die "Unable to scrape<$file> for GID:$!\n";
			$line = "";
			while(<FH>) {
				chomp($_);
				$line = $_;
				if ($line =~ /^ENV:\s+NMI_tag\s+=\s+(.*)$/) {
					$buildtag = $1;
				} elsif ($line =~ /^CNS:\s+Working\s+on\s+(.*)$/) {
					$buildtag = $1;
				} else {
					#print "SKIP: $line\n";
				}
			}
			close(FH);
		} elsif($file =~ /platform_pre\.(.*).out/) {
			$platform = $1;
			$BuildTargets{"$platform"} = 1;
		}
	}
	closedir DH;
} elsif($singletest) {
	print "Single Test Analysis\n";
	$realtestgid = $testbase . $singletest;
	$buildtag = "TagUnknown";
	$histold = $buildtag . "-test_history";
	$histnew = $buildtag . "-test_history.new";
} else {
	print "$berror = all <all builds failed>\n";
	if(!$branch) {
		die "Need branch to determine historic platforms for build failures\n";
	} else {
		print "Basing history file on branch<<$branch>>\n";
		$histold = "V" . $branch . "-test_history";
		$histnew = "V" . $branch . "-test_history.new";
	}
}

DbConnect();
FindTestRuns($requestrunid);
FindTestPlatforms();
DbDisconnect();

#die "done looking for test runs\n";

if((!$singletest) && (!($berror =~ /all/))) {
	my $bv = "";
	if($buildtag =~ /^BUILD-(V\d_\d)-.*$/) {
		$bv = $1;
	}
	$histold = $bv . "-test_history";
	$histnew = $bv . "-test_history.new";

	print "Build Tag: $buildtag Release: $bv\n";
	print "***************************\n";
	print "Failed or Pending Builds: ";
	foreach  $key (sort keys %BuildTargets) {
		if( exists $TestPlatforms{"$key"} ) {
			;
		} else {
			print "$key ";
		}
	}
	print "\n";
	print "***************************\n";
}

if($berror) {
	AddTestGids($berror, "build");
}

if($losttests) {
	AddTestGids($losttests, "test");
}

SetupAnalysisCache($buildtag);

DbConnect();
AnalyseTestRuns();
DbDisconnect();


#
# Process error allocations
#
#'platform=s' => \$perror,
#'framework=s' => \$ferror,
#'tests=s' => \$terror,
#'condor=s' => \$cerror,
#'unknown=s' => \$uerror,
# etc etc
#

if($terror) {
	CrunchErrors( "tests", $terror );
}

if($cerror) {
	CrunchErrors( "condor", $cerror );
}

if($ferror) {
	CrunchErrors( "framework", $ferror );
}

if($losttests) {
	CrunchErrors( "losttests", $losttests );
}

if($perror) {
	print "Call CrunchErrors for a platform issue<$perror>\n";
	CrunchErrors( "platform", $perror );
}

if($uerror) {
	CrunchErrors( "unknown", $uerror );
}

PrintResults();
SweepTotals();


my $missing = $totalexpected - $totaltests;


print "********\n"; 
print "	Build GID<$gid>:\n";
print "	Totals Passed = $totalgood, Failed = $totalbad, ALL = $totaltests Expected = $totalexpected Missing = $missing\n";
print "	Test = $totaltesterr, Condor = $totalcondorerr, Platform = $totalplatformerr, Framework = $totalframeworkerr\n";
print "	Unknown = $totalunknownerr\n";
print "********\n"; 
GetBuildResults();
print "********\n"; 

if( $totalcounterr  > 0 ) {
	print "\n";
	print "*** NOTE: $totalcounterr platform(s) had Passed plus Failed != Expected\n";
	print "\n";
	print "********\n"; 
}

exit 0;

# So we can have a test run which fails prior to when there
# is a platform associated with it

sub FindTestPlatforms
{
	my $platformknown = 0;
	foreach $run (@TestRuns) {
		$platformknown = 0;
		my $extraction = $db->prepare("SELECT * FROM Task where 
			runid = '$run'\
			and name = 'platform_job'");
    	$extraction->execute();
    	while( my $sumref = $extraction->fetchrow_hashref() ){
			$dbplatform = $sumref->{'platform'};
			$TestPlatforms{"$dbplatform"}= 1;
			$RunPlatforms{"$run"}= $dbplatform;
			$platformknown = 1;
			#print "Runid $run is platform <<$dbplatform>>\n";
		}
		if($platformknown != 1) {
			# something failed prior to the platform job
			# and without looking in the run dir we do not
			# know what the platform is OR if its early in the
			# run and will be fine later. We could verify an error
			# by looking up all the tasks and we can check
			# in the run directory for platforms in "platform_list".
			my $platform = GetPlatformFromRun($run);
			$ProblemTestRun{"$run"} = $platform;
			$TestPlatforms{"$platform"}= 1;
			$RunPlatforms{"$run"}= $platform;
			if(HaveRunError("$run") eq "") {
				#print "No real error\n";
				#print "Early in run....\n";
				$ProblemTestRun{"$run"} = 1;
			}
		}
	}
}

# Do we actually have an error or an early stage run
# with no platform data soon.

sub HaveRunError
{
	$targetrun = shift;
	my $extraction = $db->prepare("SELECT * FROM Task where 
		runid = '$run'\
		");
    $extraction->execute();
    while( my $sumref = $extraction->fetchrow_hashref() ){
		$result = $sumref->{'result'};
		$name = $sumref->{'name'};
		if($result ne 0) {
			print "**************** Error on Test run stage <<$name>>*****************\n";
			return ($name);
		}
	}
	return("");
}

# run failed prior to a platform task. Go to
# rundir and get platform from platform_list

sub GetPlatformFromRun
{
	$targetrun = shift;
	$plats = "";
	$pathtorun = "";
	$pathtorun = FindBuildRunDir($targetrun);
	$pathtorun = $pathtorun . "/platform_list";
	open(PLATFORMLIST,"<$pathtorun") || die "Can not open $pathtorun:$! \n";
	while(<PLATFORMLIST>) {
		chomp();
		$plats = $_;
		print "**************** Error on Test run platform <<$plats>>*****************\n";
	}
	return($plats);
}

# this only means that there was a test run attempted

sub FindTestRuns
{
    my $runid = shift;
    my $trid = "";
	my @temprunids;

    my $extraction = $db->prepare("SELECT * FROM Method_nmi WHERE  \
                input_runid = '$runid'");
    $extraction->execute();
    $TestRuns = ();
    while( my $sumref = $extraction->fetchrow_hashref() ){
        $trid = $sumref->{'runid'};
		# now we have to screen these runs for being cndrauto test
		# runs and not independent tests by me or someone else.
		#print "Test Run: $trid\n";
        push(@temprunids,$trid);
	}
	foreach $run (@temprunids) {
    	my $extraction = $db->prepare("SELECT * FROM Run WHERE  \
                runid = '$run'");
    	$extraction->execute();
		my $user = "";
    	while( my $sumref = $extraction->fetchrow_hashref() ){
        	$user = $sumref->{'user'};
			#print "User for run $run is $user\n";
			if($user eq "cndrauto") {
				push(@TestRuns,$run);
			} else {
				print "Skipping test run by $user\n";
			}
		}
    }
}

sub FindBuildRunDir
{
    my $url = "";
    my $runid = shift;
    my $extraction = $db->prepare("SELECT * FROM Run WHERE  \
                runid = '$runid' \
                ");
    $extraction->execute();
    while( my $sumref = $extraction->fetchrow_hashref() ){
        $filepath = $sumref->{'filepath'};
        $gid = $sumref->{'gid'};
        #print "<<<$filepath>>><<<$gid>>>\n";
        $url = $filepath . "/" . $gid;
        #print "Runid <$runid> is <$url>\n";
		return($url);
    }
}

sub CrunchErrors
{
	my $type = shift;
	my $params = shift;
	my @partial;
	my $p;
	my $index = -1;
	my $pexpected = 0;
	my $pgood = 0;
	my $pbad = 0;
	my $phost = "";
	my $blame = 0;

	#print "CrunchErrors called for type $type\n";

	# how many platforms with test issues
	my @tests = split /,/, $params;

	if($tests[0] eq "all") {
		# all of the expected results for all platforms are being 
		# assessed to a single party
		#print "All test failure allocated to<<$type>>!!!!\n";
		foreach $host (@platformresults) {
			$pexpected = 0;
			$pgood = 0;
			$pbad = 0;
			$blame = 0;
			$phost = "";

			$pexpected = $host->expected();
			$pgood = $host->passed();
			$pbad = $host->failed();
			$phost = $host->platform();

			#print "Current host is $phost\n";

			if($pexpected == 0) {
				$pexpected = GetHistExpected($phost);
				$totalexpected = $totalexpected + $pexpected;
				$totaltests = $totaltests + $pexpected;

				$host->expected($pexpected);
			}

			$blame = $pexpected;
			$pbad = $blame;
			$pgood = 0;

			$host->passed(0);
			$host->failed($blame);
			if($type eq "tests") {
				$host->test_errs($blame);
				$totaltesterr = $totaltesterr + $blame;
			} elsif($type eq "condor") {
				$host->condor_errs($blame);
				$totalcondorerr = $totalcondorerr + $blame;
			} elsif($type eq "platform") {
				$host->platform_errs($blame);
				$totalplatformerr = $totalplatformerr + $blame;
			} elsif($type eq "framework") {
				$host->framework_errs($blame);
				$totalframeworkerr = $totalframeworkerr + $blame;
			} elsif($type eq "unknown") {
				$host->unknown_errs($blame);
				$totalunknownerr = $totalunknownerr + $blame;
			} else {
				print "CLASS of problem unknown!!!!!!<$type>!!!!!!!\n";
			}
		}
	} else {
		# this is normal processing where we are assigning blame (full or
		# partial) on a single platform
		foreach $test (@tests) {
			$index = -1;
			$pexpected = 0;
			$pgood = 0;
			$pbad = 0;
			$blame = 0;
			$phost = "";

			#print "Parsing $test\n";
			# entire platform or some tests
			@partial = split /:/, $test;
			$index = GetPlatformData($partial[0]);

				$p = $platformresults[$index];
				$blame = 0;

				#print "size 0f partial array is $#partial\n";
				#print "crunch errors dedicated to $type\n";

				$pexpected = $p->expected();
				$pgood = $p->passed();
				$pbad = $p->failed();
				$phost = $p->platform();

				#print "Current host is $phost\n";

				if($pexpected == 0) {
					$pexpected = GetHistExpected($phost);
					$totalexpected = $totalexpected + $pexpected;
					#$totaltests = $totaltests + $pexpected;
					$p->expected($pexpected);
					#print "CrunchErrors: platform $phost expected still 0 adding $pexpected\n";
				}

				if(($pbad == 0) && ($pgood == 0)) {
					$pbad = $pexpected;
					$totalbad = $totalbad + $pbad;
					$p->failed($pbad);
					$totaltests = $totaltests + $pbad;
				}

				if(($#partial > 0) && ($type ne "losttests")) {
					$blame = $partial[1];
					#print "partial blame $partial[1]\n";
					#print "blame $blame\n";
				} else {
					$blame = ($pexpected - $pgood);
					#print "Full blame $blame\n";
				}

				#print "Blame amount now <<<$blame>>>\n";
				if($type eq "tests") {
					$p->test_errs($blame);
					$totaltesterr = $totaltesterr + $blame;
					#print "Blame to <<$type>>\n";
				} elsif($type eq "condor") {
					$p->condor_errs($blame);
					$totalcondorerr = $totalcondorerr + $blame;
					#print "Blame to <<$type>>\n";
				} elsif($type eq "platform") {
					$p->platform_errs($blame);
					#print "Platform errors before <<$totalplatformerr>>\n";
					$totalplatformerr = $totalplatformerr + $blame;
					#print "Blame to <<$type>>\n";
					#print "Platform errors after <<$totalplatformerr>>\n";
				} elsif(($type eq "framework") || ($type eq "losttests")) {
					$p->framework_errs($blame);
					$totalframeworkerr = $totalframeworkerr + $blame;
					#print "Blame to <<$type>>\n";
				} elsif($type eq "unknown") {
					$p->unknown_errs($blame);
					$totalunknownerr = $totalunknownerr + $blame;
					#print "Blame to <<$type>>\n";
				} else {
					print "CLASS of problem unknown!!!!!!<$type>!!!!!!!\n";
					#print "Blame to <<$type>>\n";
				}
		}
	}
}

##########################################
#
# Find the position of a particular platforms data
# 

sub GetPlatformData
{
	my $goal = shift;

	my $pformcount = $#platformresults;
	my $contrl = 0;
	my $result = -1;
	my $p;
	my $host = "";
	while($contrl <= $pformcount) {
		$p = $platformresults[$contrl];
		$host = $p->platform();
		if($host eq $goal) {
			#printf "Found it.....%s<<%d>> \n",$host; 
			$result = $contrl;
			#return($contrl);
		} else {
			#print "Looking for $goal current choice $host\n";

		}
		$contrl = $contrl + 1;
	}
	if($result == -1) {
		print "GetPlatformData failed to find host <<<$goal>>>!!!\n";
	}
	return($result);
}

sub PrintResults() 
{
	my $pformcount = $#platformresults;
	my $contrl = 0;
	my $p;




	print "Problem Codes: T(test) C(condor) U(unknown) P(platform) F(framework)\n";
	print "--------------------------------------------------------------------\n";
	print "Platform (expected): 						Problem codes\n";
	while($contrl <= $pformcount) {
		$p = $platformresults[$contrl];
	my $framecount = $p->framework_errs( );
	#print "Into PrintResults and framework errors currently <<$framecount>>\n";

		my $Platformerrors = $p->platform_errs( );
	#print "Into PrintResults and platform errors currently <<$Platformerrors>>\n";
		printf "%-16s (%d):		Passed = %d	Failed = %d	",$p->platform(),$p->expected(),
			$p->passed(),$p->failed();
		if($p->build_errs( ) > 0) {
			printf " BUILD ";
		}
		if($p->count_errs( ) > 0) {
			printf " *** ";
		}
		if($p->test_errs( ) > 0) {
			printf " T(%d) ",$p->test_errs();
		}
		if($p->condor_errs( ) > 0) {
			printf " C(%d) ",$p->condor_errs();
		}
		if($p->framework_errs( ) > 0) {
			printf " F(%d) ",$p->framework_errs();
		}
		if($p->platform_errs( ) > 0) {
			printf " P(%d) ",$p->platform_errs();
		}
		if($p->unknown_errs( ) > 0) {
			printf " U(%d) ",$p->unknown_errs();
		}
		print "\n";
		$contrl = $contrl + 1;
	}
}

##########################################
#
# This routine is passed a list of platforms
# with blame codes. The second arg allows
# either adding this set of platforms to lost builds
# or simply a total assignment of blame for test runs.
# Missing test runs come in with "test" and build
# issues "build".
# 
# Fill @platformresults with these:
# or  @buildresults
#
#	struct Platform_info =>
#	{
#	};
#

sub AddTestGids
{	
	my @evalplatforms;
	my $platforms = shift;
	my $testorbuild = shift;
	my $entry;
	my $expected;

	@evalplatforms = split /,/, $platforms;
	foreach $platform (@evalplatforms) {
		if($platform =~ /all/) {
			LoadAll4BuildResults($platform);
		} else {
			@buildblame = split /:/, $platform;
			$entry = Platform_info->new();
			$expected = GetHistExpected($buildblame[0]);
			$entry->platform("$buildblame[0]");
			#print "Crunch errors for \"$buildblame[0] & $buildblame[1]\"\n";
			if($testorbuild eq "build") {
				$entry->build_errs(1);
				$entry->expected($expected);
				#print "We were expecting $expected tests\n";
			} else {
				$entry->expected(0);
			}
			$entry->passed(0);
			$entry->failed(0);
			$entry->condor_errs(0);
			$entry->test_errs(0);
			$entry->unknown_errs(0);
			$entry->platform_errs(0);
			$entry->framework_errs(0);
			my $blamecode = $buildblame[1];
			#print "Blame code is $blamecode\n";
			if($blamecode eq "c") {
				$entry->condor_errs($expected);
			} elsif($blamecode eq "p") {
				$entry->platform_errs($expected);
			} elsif($blamecode eq "f") {
				$entry->framework_errs($expected);
			} else {
				die "UNKNOWN blame code for build <<$blamecode>>\n";
			}
	
			if($testorbuild eq "build") {
				push @buildresults, $entry;
			} else {
				push @platformresults, $entry;
			}
		}
	}
}

sub LoadAll4BuildResults
{
	my $platform = shift;
	@buildblame = split /:/, $platform;
	#print "Blame to $buildblame[1]\n";
	my $line = "";
	my $entry;
	my $expected;

	if(!(-f "$histold")) {
		print "No history file....<<$histold>>\n";
		return(0);
	}

	#print "Looking for expetced for $platform\n";
	open(OLD,"<$histold") || die "Can not open history file<$histold>:$!\n";
	while(<OLD>) {
		chomp($_);
		$line = $_;
		if($line =~ /^\s*([\w\.]+):\s*Passed\s*=\s*(\d+)\s*Failed\s*=\s*(\d+)\s*Expected\s*=\s*(\d+)*$/) {
			#print "adding $1:\n";
			$entry = Platform_info->new();
			$expected = $4;
			$entry->platform("$1");
			$entry->build_errs(1);
			$entry->passed(0);
			$entry->failed(0);
			$entry->expected($expected);
			$entry->condor_errs(0);
			$entry->test_errs(0);
			$entry->unknown_errs(0);
			$entry->platform_errs(0);
			$entry->framework_errs(0);
			my $blamecode = $buildblame[1];
			#print "Blame code is $blamecode\n";
			if($blamecode eq "c") {
				$entry->condor_errs($expected);
			} elsif($blamecode eq "p") {
				$entry->platform_errs($expected);
			} elsif($blamecode eq "f") {
				$entry->framework_errs($expected);
			} else {
				die "UNKNOWN blame code for build <<$blamecode>>\n";
			}
			push @buildresults, $entry;
		}
	}
	close(OLD);
}

sub AnalyseTestRuns
{
	my $runcount = 0;
	my $rungood = 0;
	my $runbad = 0;
	my $runnull = 0;
	my $platform = "";
	foreach $run (@TestRuns) {
		my $extraction = $db->prepare("SELECT * FROM Run WHERE  \
                runid = '$run' \
				and user = 'cndrauto'\
                ");
    	$extraction->execute();
		my $expected = 0;
		my $good = 0;
		my $bad = 0;
    	my $sumref = $extraction->fetchrow_hashref() ;
        $filepath = $sumref->{'filepath'};
        $gid = $sumref->{'gid'};
		if(exists $ProblemTestRun{"$run"}) {
			print "Whats with this run????\n";
			if($ProblemTestRun{"$run"} == 1) {
				# just not far enough along yet
				next;
			}
		} 
		my $entry = Platform_info->new();

		$runcount = 0;
		$rungood = 0;
		$runbad = 0;
		$runnull = 0;
		#print "Find tests for runid $args[0] builddate <<$builddate>>\n";
        $testextraction = $db->prepare("SELECT * FROM Task WHERE  \
                    runid = $run
                    ");

        $testextraction->execute();

		# we really only care about the results for the automatics
        # null(not defined()) means still in never never land, 0 is good and all else bad.

		$platform = "";
        while( my $sumref = $testextraction->fetchrow_hashref() ){
            $res = $sumref->{'result'};
            $name = $sumref->{'name'};
            $start = $sumref->{'start'};
            if(!(exists $metrotask{"$name"})) {
                #print "TestRun $runid Task $name Time $start Platform $platform<$res>\n";
            	$platform = $sumref->{'platform'};
                if( defined($res)) {
                    #print "<$res>\n";
                    if($res == 0) {
                        #print "GOOD\n";
                        $rungood = $rungood + 1;
                    } else {
                        # we are left with the positive and negative failures
                        #print "BAD\n";
                        $runbad = $runbad + 1;
                    }
                } else {
                    #print "NULL\n";
                    $runnull = $runnull + 1; # pending
                }
				$runcount = $runcount + 1;
            } else {
				$plat = $sumref->{'platform'};
				if($platform eq "") {
					#print "looking for platform: $name & $plat\n";
				}
				if( $plat ne "local") {
            		$platform = $plat;
				}
			}

        }

		$good = $rungood;
		$bad = $runbad;
		# with our now accumulating during test runs we can be partially done
		# and need to look at the REAL historic expected and use that.
		$histexpected = GetHistExpected($platform); # 
		$expected = $runcount;

		#print "$platform:	Passed = $good	Failed = $bad	Expected = $expected\n";
		if($expected < $histexpected ) { 
			# no tasklist.nmi
			$expected = $histexpected;
		}
		$totalexpected = $totalexpected + $expected;

		# Did we get what we expected??????
		# We have to have expected and the tests need to have
		# either $bad or $good not zero. They both are 0 while

		# the tests are still running.
		if(($expected > 0) && (($bad > 0) || ($good > 0))) {
			$counttest = $bad + $good;
			if($expected != $counttest) {
				$entry->count_errs(1);
				$totalcounterr = $totalcounterr + 1;
			} else {
				$entry->count_errs(0);
			}
		} else {
			$entry->count_errs(0);
			if(exists $ProblemTestRun{"$run"}) {
				$bad = $expected;
			}
		}
			
		AddHistExpected($platform, $good, $bad, $expected);

		$entry->platform("$platform");
		$entry->passed($good);
		$entry->failed($bad);
		$entry->expected($expected);
		$entry->condor_errs(0);
		$entry->test_errs(0);
		$entry->unknown_errs(0);
		$entry->platform_errs(0);
		$entry->framework_errs(0);

		push @platformresults, $entry;

		$totaltests = $totaltests + $runcount;
		close PD;
	}
}

##########################################
#
# 
#

sub CountLines {
	$target = shift;
	$count = 0;
	#print "Opening <<<$target>>>\n";

	open(FILE,"<$target") || die "CountLines can not open $target:$!\n";
	while(<FILE>) {
		#print $_;
		$count = $count + 1;
	}
	close(FILE);
	#print "Returning $count\n";
	return($count);
}

##########################################
#
# Lookup expected value for a platform
#

sub GetHistExpected {
	my $platform = shift;
	my $line = "";

	if(!(-f "$histold")) {
		print "No history file....<<$histold>>\n";
		return(0);
	}

	#print "Want history expaected value for << $platform>>\n";

	open(OLD,"<$histold") || die "Can not open history file<$histold>:$!\n";
	while(<OLD>) {
		chomp($_);
		$line = $_;
		if($line =~ /^$platform:\s*Passed\s*=\s*(\d+)\s*Failed\s*=\s*(\d+)\s*Expected\s*=\s*(\d+)*$/) {
			close(OLD);
			#print "GetHistExpected: returning $3\n";
			return($3);
		}
	}
	close(OLD);
	return(0);
}

##########################################
#
# Add, seed and update our history for a platform
# We should only increase the expected number of tests
# except there may be some variance so we will allow
# it to change. If we have no entry yet we will
# sum the passed and the failed for an expected value
#

sub AddHistExpected {
	my $platform = shift;
	my $pass = shift;
	my $fail = shift;
	my $expected = shift;
	my $line = "";
	my $found = 0;
	my $newexpected = $pass + $fail;

	#print "Entering AddHistExpected with $platform: p=$pass f=$fail e=$expected Newe=$newexpected\n";

	if(!(-f "$histold")) {
		print "Create history file....\n";
		system("touch $histold");
	}

	open(OLD,"<$histold") || die "Can not open history file<$histold>:$!\n";
	open(NEW,">$histnew") || die "Can not new history file<$histnew>:$!\n";
	while(<OLD>) {
		chomp($_);
		$line = $_;
		if($line =~ /^\s*$platform:\s*Passed\s*=\s*(\d+)\s+Failed\s*=\s*([\-]*\d+)\s+Expected\s*=\s*(\d+).*$/) {
				$found = 1;
				if(($expected > $3) && ($expected != 0)) {
					#print "Old expected was $3\n";
					#print "$platform: Passed = $pass Failed = $fail Expected = $expected\n";
					print NEW "$platform: Passed = $pass Failed = $fail Expected = $expected\n";
				} elsif(($expected <= 0) && ($newexpected > 0)) {
					#print "Replaced HistExpected\n";
					print NEW "$platform: Passed = $pass Failed = $fail Expected = $newexpected\n";
				} else {
					print NEW "$line\n"; # keep  old values
				}
		} else {
			print NEW "$line\n"; # well I don't expect anything else but who knows... save it
		}
	}

	 #add it in if its not here yet and seed the file/platform
	if($found != 1) {
		
		if(($expected == 0) && ($newexpected > 0)) {
			print NEW "$platform: Passed = $pass Failed = $fail Expected = $newexpected\n";
		} else {
			print NEW "$platform: Passed = $pass Failed = $fail Expected = $expected\n";
		}
	}
	close(OLD);
	close(NEW);
	system("mv $histnew $histold");

}

sub SweepTotals
{
	$totalgood = 0;
	$totalbad = 0;
	$pbad = 0;
	$pgood = 0;
	foreach $host (@platformresults) {
		if($host->build_errs != 1) {
			$totalgood = $totalgood + $host->passed();
			$totalbad = $totalbad + $host->failed();
		}
	}
}

my $cfailedbuilds = 0;
my $pfailedbuilds = 0;
my $ffailedbuilds = 0;
my $cfailedbtests = 0;
my $pfailedbtests = 0;
my $ffailedbtests = 0;
my $totalfailedbtests = 0;
my $totalfailedbuilds = 0;

sub GetBuildResults
{
	my $foundblame = 0;
	print "Lost Builds:				Problem Code\n";
	foreach $host (@buildresults) {
		$totalbuilds = $totalbuilds + 1;
		if($host->build_errs() == 1) {
			my $platform = $host->platform();
			my $expected = $host->expected();
			$cerrs = $host->condor_errs();
			$perrs = $host->platform_errs();
			$ferrs = $host->framework_errs();
			#print "Crunch build issues for $platform\n";
			if($cerrs > 0) {
				$cfailedbuilds = $cfailedbuilds + 1;
				$cfailedbtests = $cfailedbtests + $expected;
				printf ("%-16s(%d)			Condor\n",$platform,$expected);
			} elsif($ferrs > 0) {
				$ffailedbuilds = $ffailedbuilds + 1;
				$ffailedbtests = $ffailedbtests + $expected;
				printf ("%-16s(%d)			Framework\n",$platform,$expected);
			} elsif($perrs > 0) {
				$pfailedbuilds = $pfailedbuilds + 1;
				$pfailedbtests = $pfailedbtests + $expected;
				printf ("%-16s(%d)			Platform\n",$platform,$expected);
			}
		}
	}
	$totalfailedbtests = $cfailedbtests + $pfailedbtests + $ffailedbtests;
	$totalfailedbuilds = $cfailedbuilds + $pfailedbuilds + $ffailedbuilds;
	print "********\n"; 
	printf ("	Lost Builds(%d/%d) Condor = %d Platform = %d Framework = %d\n",$totalfailedbuilds,$totalbuilds,$cfailedbuilds,$pfailedbuilds,$ffailedbuilds);
	printf ("	Lost Tests(%d) Condor = %d Platform = %d Framework = %d\n",$totalfailedbtests,$cfailedbtests,$pfailedbtests,$ffailedbtests);

}

##########################################
#
# print help
#

GetOptions (
		'arch=s' => \$arch,
		'lost=s' => \$losttests,
		'branch=s' => \$branch,
        'runid=s' => \$requestrunid,
        'help' => \$help,
		# types of blame
		'builds=s' => \$berror,
		'platform=s' => \$perror,
		'framework=s' => \$ferror,
		'tests=s' => \$terror,
		'condor=s' => \$cerror,
		'unknown=s' => \$uerror,
		'single=s' => \$singletest,
		'who=s' => \$whoosetests,
);
sub help
{
	print "Usage: condor_nmitests.pl --runid=<string>
	Options:
		[-h/--help]             See this
		[-a/--arch]             platform type for single test evaluation
		[-r/ --runid]           Find build and test info from runid
		[-br/--branch]          What branch had issues? 6_8, 6_9 ??
		[-bu/--builds]          Lost a test run to a build error assigned now.
		[-l/--lost]             Lost a test run due to framework or other cause
		[-s/--single]           ie: 1175825704_25640 { test gid to evaluate }
		[-p/--platform]         Assign blame to platforms.
		[-f/--framework]        Assign blame to framework.
		[-c/--condor]           Assign blame to Condor.
		[-t/--tests]            Assign blame to tests.
		[-u/--unknown]          Assign blame to Unknown.
		[-w/--who]              Whoose test run?

		Build assignment and test consequences(c=condor,p=platform,m=metronome)
		Numerous examples in test blog as the arguements are recorded at the
		beginning of each section of output.

		condor_nmitests.pl --runid=<string> --builds=ppc_ydl_3.0:c,sun4u_sol_5.9:p,ppc64_sles_9:f
		\n";
}

# still to write code to search in cache for when these
# key files have been cleaned off the submit node.

sub StoreInAnalysisCache
{
	$file = shift;
	$pdir = shift;
	$platformdir = $histcache . "/" .  $pdir;
	if(!(-d "$platformdir")) {
		#print "Test Cache needs paltformdir<<$platformdir>>\n";
		system("mkdir -p $platformdir");
	}
	if(!(-d "$platformdir")) {
		die "Could not cache test data for platform <<$pdir>> in <<$histcache>>\n";
	}
	copy($file, $platformdir);
}

sub SetupAnalysisCache
{
	$tag = shift;
	$CacheDir = getcwd();

	#print "working in  $CacheDir looking for <<$histcache>>\n";
	#print "Tag for locating cache <<$tag>>\n";
	if($tag =~ /^BUILD-(V\d_\d).*$/) {
		$histversion = $1; 
		#print "Found <<$histversion>>\n";
	}
	if(!(-d "$histcache")) {
		#print "Test Cache needs creating<<$histcache>>\n";
		system("mkdir  -p $histcache");
	}
	$histcache = $CacheDir . "/" . $histcache . "/" . $histversion;
	if(!(-d "$histcache")) {
		#print "Test Cache needs creating<<$histcache>>\n";
		system("mkdir -p $histcache");
	}

	$histcache = $histcache . "/" . $tag;
	if(!(-d "$histcache")) {
		print "Test Cache needs creating<<$histcache>>\n";
		system("mkdir -p $histcache");
	}
	if(!(-d "$histcache")) {
		die "Could not establish result cache <<$histcache>>\n";
	} else {
		print "Test Cache is <<$histcache>>\n";
	}
}


sub DbConnect
{
    $db = DBI->connect("DBI:mysql:database=nmi_history;host=mysql.batlab.org", "nmipublic", "nmiReadOnly!") 	
    #$db = DBI->connect("DBI:mysql:database=nmi_history;host=nmi-build25", "nmipublic", "nmiReadOnly!") 	
		|| die "Could not connect to database: $!\n";
    #print "Connected to db!\n";
}

sub DbDisconnect
{
    $db->disconnect;
}
