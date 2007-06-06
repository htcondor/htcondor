#!/usr/bin/env perl
use Class::Struct;
use Cwd;
use Getopt::Long;
use File::Copy;
use DBI;

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


GetOptions (
		'arch=s' => \$arch,
        'base=s' => \$base,
		'builds=s' => \$berror,
		'branch=s' => \$branch,
        'gid=s' => \$gid,
        'runid=s' => \$requestrunid,
        'help' => \$help,
        #'megs=i' => \$megs,
		# types of blame
		'platform=s' => \$perror,
		'framework=s' => \$ferror,
		'tests=s' => \$terror,
		'condor=s' => \$cerror,
		'unknown=s' => \$uerror,
		'single=s' => \$singletest,
		'who=s' => \$whoosetests,
		'month=s' => \$buildmonth,
);

%platformerrors;
@platformresults;
@buildresults;
$currentmonth = 0;
$monthstring = "";
$currentyear = 0;
$ymbase = "";
$histcache = "analysisdata";
$histverison = "";

if($help) { help(); exit(0); }

if(!$singletest && !$berror) {
	if(!($gid || $requestrunid)) {
		print "You must enter the build GID to check test data from!\n";
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

#SetMonthYear();
my $basedir;

if(!$base) {
	# test db access of base
	DbConnect();
	$basedir = FindBuildRunDir($requestrunid);
	print "Have <<$basedir>> for basedir\n";
	DbDisconnect();
} else {
	$basedir = $base . $gid;
}

my $TopDir = getcwd(); # current location hosting testhistory data and platform data cache
my $CacheDir = "";     # level where different platform data files go.

chomp($TopDir);
#print "Working dir is <$TopDir>\n";

my %TestGIDs;
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
	print "Opening build output for GID <$gid>\n";
	$builddir = $basedir;;
	#print "Testing <<$basedir>>\n";
	if(!(-d $basedir )) {
		die "<<$basedir>> Does not exist\n";
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
				if($line =~ /^CNS:\s+gid\s+=\s+(.*)$/) {
					#print "Platform $platform has GID $1\n";
					$TestGIDs{"$platform"} = "/nmi/run/" . $1;
				} elsif ($line =~ /^ENV:\s+NMI_tag\s+=\s+(.*)$/) {
					$buildtag = $1;
				} elsif($line =~ /^CNS:\s+Run\s+Directory:\s+(.*)$/) {
					#print "Platform $platform has GID $1\n";
					$TestGIDs{"$platform"} = $1;
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
	$TestGIDs{"$arch"} = $realtestgid;
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
		if( exists $TestGIDs{"$key"} ) {
			;
		} else {
			print "$key ";
		}
	}
	print "\n";
	print "***************************\n";
}

if($berror) {
	AddTestGids($berror);
}

SetupAnalysisCache($buildtag);

#print "These are what we will be looking at for test results:\n";
#while(($key, $value) = each %TestGIDs) {
	#print "$key => $value\n";
#}

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

AnalyseTestGids();

#
# Process error allocations
#
#'platform=s' => \$perror,
#'framework=s' => \$ferror,
#'tests=s' => \$terror,
#'condor=s' => \$cerror,
#'unknown=s' => \$uerror,
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

if($perror) {
	CrunchErrors( "platform", $perror );
}

if($uerror) {
	CrunchErrors( "unknown", $uerror );
}

#AnalyseTestGids();
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
        print "Runid <$runid> is <$url>\n";
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
					$totaltests = $totaltests + $pexpected;
					$p->expected($pexpected);
				}

				if(($pbad == 0) && ($pgood == 0)) {
					$pbad = $pexpected;
					$totalbad = $totalbad + $pbad;
					$p->failed($pbad);
					$totaltests = $totaltests + $pbad;
				}

				if($#partial > 0) {
					$blame = $partial[1];
					#print "partial blame $partial[1]\n";
				} else {
					$blame = ($pexpected - $pgood);
				}

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
					$totalplatformerr = $totalplatformerr + $blame;
					#print "Blame to <<$type>>\n";
				} elsif($type eq "framework") {
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
# 
# Fill @platformresults with these:
#
#	struct Platform_info =>
#	{
#	    platform => '$',
#	    passed => '$',
#	    failed => '$',
#	    expected => '$',
#	    condor_errs => '$',
#	    test_errs => '$',
#	    unknown_errs => '$',
#	    platform_errs => '$',
#	    framework_errs => '$',
#		count_errs => '$',
#	};

sub GetPlatformData
{
	my $goal = shift;

	my $pformcount = $#platformresults;
	my $contrl = 0;
	my $p;
	while($contrl <= $pformcount) {
		$p = $platformresults[$contrl];
		if($p->platform() eq $goal) {
			#printf "Found it.....%s \n",$p->platform(); 
			return($contrl);
		}
		$contrl = $contrl + 1;
	}
	return(-1);
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
# 
# Fill @platformresults with these:
#
#	struct Platform_info =>
#	{
#	    platform => '$',
#	    passed => '$',
#	    failed => '$',
#	    expected => '$',
#		build_errs => '$',
#	    condor_errs => '$',
#	    test_errs => '$',
#	    unknown_errs => '$',
#	    platform_errs => '$',
#	    framework_errs => '$',
#		count_errs => '$',
#	};

sub AddTestGids
{	
	my @evalplatforms;
	my $platforms = shift;
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

sub AnalyseTestGids
{
	my $platbasedir = "";
	my $platform;

	while(($key, $value) = each %TestGIDs) {
		my $good = 0;
		my $bad = 0;
		my $expected = 0;
		my $entry = Platform_info->new();
	
		$totalbuilds = $totalbuilds + 1;
		#$platbasedir = "/nmi/run/" . $value . "/userdir/$key";
		$platbasedir = $value . "/userdir/$key";
		opendir PD, $platbasedir or die "Can not open Test Results<$platbasedir>:$!\n";
		foreach $file (readdir PD) {
			chomp($file);
			$testresults = $platbasedir . "/" . $file;
			if($file =~ /^successful_tests_summary$/) {
				$good = CountLines($testresults);
				StoreInAnalysisCache($testresults,$key);
				#print "$key: $good passed\n";
			} elsif($file =~ /^failed_tests_summary$/) {
				$bad = CountLines($testresults);
				StoreInAnalysisCache($testresults,$key);
				#print "$key: $bad failed\n";
			} elsif($file =~ /^tasklist.nmi$/) {
				$expected = CountLines($testresults);
				StoreInAnalysisCache($testresults,$key);
				$totalexpected = $totalexpected + $expected;
				#print "$key: $expected failed\n";
			}
			#print "Platform $key file: $file\n";
		}
		#print "$key:	Passed = $good	Failed = $bad	Expected = $expected\n";
		if($expected == 0) { 
			# no tasklist.nmi
			$expected = GetHistExpected($key);
			$totalexpected = $totalexpected + $expected;
		}

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
		}
			
		AddHistExpected($key, $good, $bad, $expected);

		$entry->platform("$key");
		$entry->passed($good);
		$entry->failed($bad);
		$entry->expected($expected);
		$entry->condor_errs(0);
		$entry->test_errs(0);
		$entry->unknown_errs(0);
		$entry->platform_errs(0);
		$entry->framework_errs(0);

		push @platformresults, $entry;

		$totaltests = $totaltests + $good + $bad;
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

	#print "Looking for expetced for $platform\n";
	open(OLD,"<$histold") || die "Can not open history file<$histold>:$!\n";
	while(<OLD>) {
		chomp($_);
		$line = $_;
		if($line =~ /^$platform:\s*Passed\s*=\s*(\d+)\s*Failed\s*=\s*(\d+)\s*Expected\s*=\s*(\d+)*$/) {
			#print "Found $platform:";
			close(OLD);
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
		if($line =~ /^\s*$platform:\s*Passed\s*=\s*(\d+)\s+Failed\s=\s*([\-]*\d+)\s+Expected\s*=\s*(\d+).*$/) {
				$found = 1;
				#print "found $platform p=$1 f=$2 e=$3\n";
				if(($expected != $3) && ($expected != 0)) {
					#print "New expected changed from $3 to $expected for $platform\n";
					print NEW "$platform: Passed = $pass Failed = $fail Expected = $expected\n";
				} elsif(($expected <= 0) && ($newexpected > 0)) {
					#print "$platform expected from 0 to $newexpected\n";
					print NEW "$platform: Passed = $pass Failed = $fail Expected = $newexpected\n";
				} else {
					#print "Keeping old values, test not done\n";
					print NEW "$line\n"; # keep  old values
				}
		} else {
			#print "No match on platform line!!!!\n";
			#print "<<$line>>\n";
			print NEW "$line\n"; # well I don't expect anything else but who knows... save it
		}
	}

	# add it in if its not here yet and seed the file/platform
	if($found != 1) {
		
		#print "We did not find <<$platform>> it so add it in!\n";
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
			#print "$cerrs , $perrs , $merrs\n";
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

sub help
{
	print "Usage: condor_nmitests.pl --gid=<string>
	Options:
		[-h/--help]             See this
		[-a/--arch]             platform type for single test evaluation
		[-ba/--base]            Base location indexed by gid.
		[-br/--branch]          What branch had issues? 6_8, 6_9 ??
		[-bu/--builds]          Lost a test run to a build error assigned now.
		[-g/--gid]              ie: 1175825704_25640 { build gid tests came from }
		                            assumes{/nmi/run/cndrauto_nmi-s001.cs.wisc.edu_}
		[-s/--single]           ie: 1175825704_25640 { test gid to evaluate }
		[-p/--platform]         Assign blame to platforms.
		[-f/--framework]        Assign blame to framework.
		[-c/--condor]           Assign blame to Condor.
		[-t/--tests]            Assign blame to tests.
		[-u/--unknown]          Assign blame to Unknown.
		[-w/--who]              Whoose test run?

		Build assignment and test consequences(c=condor,p=platform,m=metronome)

		condor_nmitests.pl --gid=<string> --builds=ppc_ydl_3.0:c,sun4u_sol_5.9:p,ppc64_sles_9:f
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

	if($buildmonth) {
		$monthstring = $buildmonth;
	}
	$currentyear = $year + 1900;
	print "Month <<$monthstring>> Year <<$currentyear>>\n";
}

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
    $db = DBI->connect("DBI:mysql:database=nmi_history;host=nmi-db", "nmipublic", "nmiReadOnly!"
) || die "Could not connect to database: $!\n";
    #print "Connected to db!\n";
}

sub DbDisconnect
{
    $db->disconnect;
}
