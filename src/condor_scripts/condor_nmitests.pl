#!/usr/bin/env perl
use Class::Struct;
use Cwd;
use Getopt::Long;

struct Platform_info =>
{
	platform => '$',
	passed => '$',
	failed => '$',
	expected => '$',
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
        'gid=s' => \$gid,
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
);

%platformerrors;
@platformresults;

if($help) { help(); exit(0); }

if(!$singletest) {
	if(!$gid) {
		print "You must enter the build GID to check test data from!\n";
		help();
		exit(1);
	}
} else {
	if(!$arch) {
		print "Can not evaluate single test without platform type!\n";
		help();
		exit(1);
	}
}
	
$foruser = "cndrauto";

if($whoosetests) {
	$foruser = $whoosetests;
} 

if(!$base) {
	$base = "/nmi/run/" . $foruser . "_nmi-s001.cs.wisc.edu_";
}
$testbase = $foruser . "_nmi-s001.cs.wisc.edu_";

$basedir = $base . $gid;
#system("ls $basedir");

my $TopDir = getcwd();
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

if(!$singletest) {
	print "Opening build output for GID <$gid>\n";
	opendir DH, $basedir or die "Can not open Build Results<$basedir>:$!\n";
	foreach $file (readdir DH) {
		if($file =~ /platform_post\.(.*).out/) {
			$platformpost = $basedir . "/" . $file;
			#print "$platformpost is for platform $1\n";
			$platform = $1;
			open(FH,"<$platformpost") || die "Unable to scrape<$file> for GID:$!\n";
			$line = "";
			while(<FH>) {
				chomp($_);
				$line = $_;
				if($line =~ /^CNS:\s+gid\s+=\s+(.*)$/) {
					#print "Platform $platform has GID $1\n";
					$TestGIDs{"$platform"} = $1;
				} elsif ($line =~ /^ENV:\s+NMI_tag\s+=\s+(.*)$/) {
					$buildtag = $1;
				}
			}
			close(FH);
		} elsif($file =~ /platform_pre\.(.*).out/) {
			$platform = $1;
			$BuildTargets{"$platform"} = 1;
		}
	}
	closedir DH;
} else {
	$realtestgid = $testbase . $singletest;
	$TestGIDs{"$arch"} = $realtestgid;
	$buildtag = "TagUnknown";
	$histold = $buildtag . "-test_history";
	$histnew = $buildtag . "-test_history.new";
}

if(!$singletest) {
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


#print "These are what we will be looking at for test results:\n";
#while(($key, $value) = each %TestGIDs) {
	#print "$key => $value\n";
#}

my $totalgood = 0;;
my $totalbad = 0;;
my $totaltests = 0;
my $totalexpected = 0;

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

PrintResults();

my $missing = $totalexpected - $totaltests;


print "********\n"; 
print "	Build GID<$gid>:\n";
print "	Totals Passed = $totalgood, Failed = $totalbad, ALL = $totaltests Expected = $totalexpected Missing = $missing\n";
print "	Test = $totaltesterr, Condor = $totalcondorerr, Platform = $totalplatformerr, Framework = $totalframeworkerr\n";
print "	Unknown = $totalunknownerr\n";
print "********\n"; 

if( $totalcounterr  > 0 ) {
	print "\n";
	print "*** NOTE: $totalcounterr platform(s) had Passed plus Failed != Expected\n";
	print "\n";
	print "********\n"; 
}
exit 0;

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
		print "All test failure allocated to<<$type>>!!!!\n";
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

			print "Current host is $phost\n";

			if($pexpected == 0) {
				$pexpected = GetHistExpected($phost);
				$totalexpected = $totalexpected + $pexpected;
				$totaltests = $totaltests + $pexpected;

				$host->expected($pexpected);
			}

			$blame = $pexpected;

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
					$blame = $pbad;
				}

				if($type eq "tests") {
					$p->test_errs($blame);
					$totaltesterr = $totaltesterr + $blame;
				} elsif($type eq "condor") {
					$p->condor_errs($blame);
					$totalcondorerr = $totalcondorerr + $blame;
				} elsif($type eq "platform") {
					$p->platform_errs($blame);
					$totalplatformerr = $totalplatformerr + $blame;
				} elsif($type eq "framework") {
					$p->framework_errs($blame);
					$totalframeworkerr = $totalframeworkerr + $blame;
				} elsif($type eq "unknown") {
					$p->unknown_errs($blame);
					$totalunknownerr = $totalunknownerr + $blame;
				} else {
					print "CLASS of problem unknown!!!!!!<$type>!!!!!!!\n";
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
		printf "%-11s (%d):		Passed = %d	Failed = %d	",$p->platform(),$p->expected(),
			$p->passed(),$p->failed();
		if($p->count_errs( ) > 0) {
			printf " *** ",$p->test_errs();
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
		print "\n";;
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
#	    condor_errs => '$',
#	    test_errs => '$',
#	    unknown_errs => '$',
#	    platform_errs => '$',
#	    framework_errs => '$',
#		count_errs => '$',
#	};

sub AnalyseTestGids
{
	my $platbasedir = "";
	my $platform;

	while(($key, $value) = each %TestGIDs) {
		my $good = 0;
		my $bad = 0;
		my $expected = 0;
		my $entry = Platform_info->new();
	
		$platbasedir = "/nmi/run/" . $value . "/userdir/$key";
		opendir PD, $platbasedir or die "Can not open Test Results<$platbasedir>:$!\n";
		foreach $file (readdir PD) {
			chomp($file);
			$testresults = $platbasedir . "/" . $file;
			if($file =~ /^successful_tests_summary$/) {
				$good = CountLines($testresults);
				$totalgood = $totalgood + $good;
				#print "$key: $good passed\n";
			} elsif($file =~ /^failed_tests_summary$/) {
				$bad = CountLines($testresults);
				$totalbad = $totalbad + $bad;
				#print "$key: $bad failed\n";
			} elsif($file =~ /^tasklist.nmi$/) {
				$expected = CountLines($testresults);
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
		print "No history file....\n";
		return(-1);
	}

	#print "Looking for expetced for $platform\n";
	open(OLD,"<$histold") || die "Can not open history file<$histold>:$!\n";
	while(<OLD>) {
		chomp($_);
		$line = $_;
		if($line =~ /^(.*):\s*Passed\s*=\s*(\d+)\s*Failed\s*=\s*(\d+)\s*Expected\s*=\s*(\d+)*$/) {
			#print "Found $1:";
			if($platform eq $1) {
				#print "Like it!\n";
				close(OLD);
				return($4);
			}
			#print "Do not like it!\n";
		} else {
			#print "Bad line: $line\n";
		}
	}
	close(OLD);
	return(-1);
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
		if($line =~ /^(.*):\s*Passed\s=\s*(\d+)\s*Failed\s=\s*(\d+)\s*Expected\s=\s*(\d+)*$/) {
			if($1 eq $platform) {
				$found = 1;
				if(($expected != $4) && ($expected != 0)) {
					#print "New expected changed from $4 to $expected for $platform\n";
					print NEW "$platform: Passed = $pass Failed = $fail Expected = $expected\n";
				} elsif(($expected == 0) && ($newexpected > 0)) {
					print NEW "$platform: Passed = $pass Failed = $fail Expected = $newexpected\n";
				} else {
					#print "Keeping old values, test not done\n";
					print NEW "$line\n"; # keep  old values
				}
			} else {
				print NEW "$line\n"; # keep the other platforms
			}
		} else {
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
		[-b/--base]             Base location indexed by gid.
		[-g/--gid]              ie: 1175825704_25640 { build gid tests came from }
		                            assumes{/nmi/run/cndrauto_nmi-s001.cs.wisc.edu_}
		[-s/--single]           ie: 1175825704_25640 { test gid to evaluate }
		[-p/--platform]         Assign blame to platforms.
		[-f/--framework]        Assign blame to framework.
		[-c/--condor]           Assign blame to Condor.
		[-t/--tests]            Assign blame to tests.
		[-u/--unknown]          Assign blame to Unknown.
		[-w/--who]              Whoose test run?
		\n";
}

