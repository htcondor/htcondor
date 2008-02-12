#!/usr/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;
use Class::Struct;

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

struct Stat_info =>
{
	date => '$',
	branch => '$',

	passed => '$',
	failed => '$',
	expected => '$',
	missing => '$',

	condor_errs => '$',
	test_errs => '$',
	unknown_errs => '$',
	platform_errs => '$',
	framework_errs => '$',

	condor_build_errs => '$',
	platform_build_errs => '$',
	framework_build_errs => '$',

	allbuilds => '$',
	goodbuilds => '$',
	failedbuilds => '$',

	count_errs => '$',
};

@monthlydata;

GetOptions (
		'help' => \$help,
		'data=s' => \$datafile,
		'out=s' => \$outfile,
		'view=s' => \$view, # production vs issues
		'branch=s' => \$targetbranch,
);

if ( $help )    { help() and exit(0); }

if( $datafile ) {
	#print "data file will be <<$datafile>>\n";
}

if( !$outfile ) {
	$outfile = "tempdata";
}

my $ttest = 0;
my $tcondor = 0;
my $tplatform = 0;
my $tframework = 0;
my $tunknown = 0;

my $tfailed = 0;
my $tmissing = 0;
my $tpassed = 0;
my $texpected = 0;

my $totbuildc = 0;
my $totbuildp = 0;
my $totbuildf = 0;

my $line = "";
my $mydate = "";
my $currentbranch = "";

my $passed = 0;
my $failed = 0;
my $expected = 0;
my $missing = 0;
my $test = 0;
my $condor = 0;
my $platform = 0;

my $cbuilderr = 0;
my $pbuilderr = 0;
my $fbuilderr = 0;

my $goodbuilds = 0;
my $allbuilds = 0;
my $failedbuilds = 0;

$framework = 0;
$limitloop = 0;
open(DATA,"<$datafile") || die "Can not open <<$datafile>>: $!\n";
while(<DATA>) {
	#print "$_";
	chomp($_);
	$line = $_;
	$gotdate = 0;

	if( $line =~ /^(\d+)\-(\d+)\-(\d+)\s*$/) {
		#print "got a date <<$1 $2 $3>>\n";
		$gotdate = 1;
		$mydate = $2 . " " . $months{$1} . " " . $3;
		#print "New style Date <<$mydate>>\n";
	} elsif( $line =~ /^\s*Tests V(\d+_\d+).*$/) {
		#print "found a branch <<$1>>\n";
		$currentbranch = $1;
	} elsif( $line =~ /^\s*Totals Passed\s*=\s*(\d+),\s*Failed\s*=\s*(\d+),.*Expected\s*=\s*(\d+)[,]*\s*Missing\s*=\s*(\d+).*$/ ) {
		if($targetbranch eq $currentbranch) {
		#print "**************************** hit Lost Tests ************************************\n";
			#print "Branch hit <$currentbranch><$targetbranch>\n";
			$passed = $1;
			$failed = $2;
			$expected = $3;
			$missing = $4;
			$tpassed = $passed + $tpassed;
			$tfailed = $failed + $tfailed;
			$texpected = $expected + $texpected;
			$tmissing = $missing + $tmissing;
		}
		#print "$mydate, $branch, $passed, $failed, $expected, $missing, ";
	} elsif( $line =~ /^\s*Test\s*=\s*(\d+),\s*Condor\s*=\s*(\d+),\s*Platform\s*=\s*(\d+),\s*Framework\s*=\s*(\d+).*$/) {
		if($targetbranch eq $currentbranch) {
		#print "**************************** hit Lost Tests ************************************\n";
			#print "Branch hit <$currentbranch><$targetbranch>\n";
			$test = $1;
			$condor = $2;
			$platform = $3;
			$framework = $4;
			$ttest = $test + $ttest;
			$tcondor = $condor + $tcondor;
			$tplatform = $platform + $tplatform;
			$tframework = $framework + $tframework;
		}
		#print "$test, $condor, $platform, $framework\n";
		#Store_stats($mydate, $currentbranch, $passed, $failed, $expected, $missing, $test, $condor, $platform, $framework);
	} elsif( $line =~ /^\s*Lost\s+Tests\((\d+)\)\s+Condor\s*=\s*(\d+)\s*Platform\s*=\s*(\d+)\s*Framework\s*=\s*(\d+).*$/) {
		if($targetbranch eq $currentbranch) {
			$cbuilderr = $2;
			$pbuilderr = $3;
			$fbuilderr = $4;
		#print "**************************** hit Lost Tests ************************************\n";
			#print "Branch hit <$currentbranch><$targetbranch>\n";
			#print "framework:$4, Platform:$3, Condor:$2\n";
			$tcondor = $cbuilderr + $tcondor;
			$tplatform = $pbuilderr + $tplatform;
			$tframework = $fbuilderr + $tframework;
			$totbuildc = $totbuildc + $cbuilderr;
			$totbuildp = $totbuildp + $pbuilderr;
			$totbuildf = $totbuildf + $fbuilderr;
		}
		#print "c = $cbuilderr, p = $pbuilderr, f = $fbuilderr\n";
		Store_stats($mydate, $currentbranch, $passed, $failed, $expected, $missing, $test, $condor, $platform, $framework, $cbuilderr, $pbuilderr, $fbuilderr, $allbuilds, $goodbuilds, $failedbuilds);

		#$limitloop = $limitloop + 1;
		#print "bump limit to <<$limitloop>>\n";
		#if($limitloop == 3) { last; }

	} elsif( $line =~ /^\s*Lost\s+Builds\((\d+)\/(\d+)\)\s+Condor\s*=\s*(\d+)\s*Platform\s*=\s*(\d+)\s*Framework\s*=\s*(\d+).*$/) {
		if($targetbranch eq $currentbranch) {
		#print "**************************** hit Lost Tests ************************************\n";
			#print "Branch hit <$currentbranch><$targetbranch>\n";
			$goodbuilds = ($2 - $1);
			$allbuilds = $2;
			$failedbuilds = $1;
		}
	} else {
	}
}

#print "Total Passed = $tpassed Missing = $tmissing\n";
#print "Total Test = $ttest Condor = $tcondor\n";
#print "Total Platform = $tplatform Framework = $tframework\n";
#print "Total Expected = $texpected\n";
#print "Total Lost from Condor Build Issues($totbuildc)\n";
#print "Total Lost from Platform Build Issues($totbuildp)\n";
#print "Total Lost from Framework Build Issues($totbuildf)\n";

Write_stats($targetbranch,$view);

exit(0);

# =================================
# Store_stats
#struct Stat_info =>
#{
#	date => '$',
#	branch => '$',
#	passed => '$',
#	failed => '$',
#	expected => '$',
#	missing => '$',
#	condor_errs => '$',
#	test_errs => '$',
#	unknown_errs => '$',
#	platform_errs => '$',
#	framework_errs => '$',
#	condor_build_errs => '$',
#	platform_build_errs => '$',
#	framework_build_errs => '$',
#	allbuilds => '$',
#	goodbuilds => '$',
#	failedbuilds => '$',
#	count_errs => '$',
#};
#
#};
		#Store_stats($mydate, $currentbranch, $passed, $failed, $expected, $missing, $test, $condor, $platform, $framework, $cbuilderr, $pbuilderr, $fbuilderr, $allbuilds, $goodbuilds, $failedbuilds);
# =================================

sub Store_stats
{
	my $entry = Stat_info->new();
	my $date = shift;
	my $branch = shift;
	my $passed = shift;
	my $failed = shift;
	my $expected = shift;
	my $missing = shift;
	my $test = shift;
	my $condor = shift;
	my $platform = shift;
	my $framework = shift;
	my $condorbt = shift;
	my $platformbt = shift;
	my $frameworkbt = shift;
	my $allbuilds = shift;
	my $goodbuilds = shift;
	my $failedbuilds = shift;

#print "store tuple with expected = <<$expected>>\n";

#	date => '$',
#	branch => '$',
#
#	passed => '$',
#	failed => '$',
#	expected => '$',
#	missing => '$',

	$entry->date("$date");
	$entry->branch("$branch");
	$entry->passed("$passed");
	$entry->failed("$failed");
	$entry->expected("$expected");
	$entry->missing("$missing");

#	condor_errs => '$',
#	test_errs => '$',
#	unknown_errs => '$',
#	platform_errs => '$',
#	framework_errs => '$',

	$entry->condor_errs("$condor");
	$entry->test_errs("$test");
	$entry->platform_errs("$platform");
	$entry->framework_errs("$framework");
	$entry->missing("$missing");

#	condor_build_errs => '$',
#	platform_build_errs => '$',
#	framework_build_errs => '$',

	$entry->condor_build_errs("$condorbt");
	$entry->platform_build_errs("$platformbt");
	$entry->framework_build_errs("$frameworkbt");
	#print "Store lost test from builds c:p:f($condorbt:$platformbt:$frameworkbt)\n";

#	allbuilds => '$',
#	goodbuilds => '$',
#	failedbuilds => '$',

	$entry->allbuilds($allbuilds);
	$entry->goodbuilds($goodbuilds);
	$entry->failedbuilds($failedbuilds);

	push @monthlydata, $entry;
}

#struct Stat_info =>
#{
#	date => '$',
#	branch => '$',
#	passed => '$',
#	failed => '$',
#	expected => '$',
#	missing => '$',
#	condor_errs => '$',
#	test_errs => '$',
#	unknown_errs => '$',
#	platform_errs => '$',
#	framework_errs => '$',
#	condor_build_errs => '$',
#	platform_build_errs => '$',
#	framework_build_errs => '$',
#	allbuilds => '$',
#	goodbuilds => '$',
#	failedbuilds => '$',
#	count_errs => '$',
#};

#
# testsum positions are:
# date, expected, passed, (terrs + uerrs + cerrs + perrs + ferrs), platform_build, 
#	-platform_build, condor_build, -(platform_build + condor_build), framework_build, 
#	-(platform_build + condor_build + framework_build)
# testdetail positions are:
# date, (terrs + uerrs + cerrs + perrs + ferrs), terrs, cerrs, (terrs + cerrs), perrs,
#	(terrs + cerrs + perrs), ferrs, (terrs + cerrs + perrs + ferrs)
#
sub Write_stats
{
	my $branch = shift;
	my $mode = shift; # production vs issues
	my $subjectbranch = "";

print "write mode = $mode\n";
	my $date = 0;
	my $databranch = 0;
	my $passed = 0;
	my $failed = 0;
	my $expected = 0;
	my $missing = 0;
	my $condor_errs = 0;
	my $test_errs = 0;
	my $unknown_errs = 0;
	my $platform_errs = 0;
	my $framework_errs = 0;
	my $condor_build_errs = 0;
	my $platform_build_errs = 0;
	my $framework_build_errs = 0;
	my $allbuilds = 0;
	my $goodbuilds = 0;
	my $failedbuilds = 0;
	my $count_errs = 0;

	open(OUTDATA,">$outfile") || die "Can not open <<$outfile>>: $!\n";

	if($mode eq "testsum") {
		print "Date, Expected, Passed, Failed\n";
	} elsif($mode eq "details") {
		print "Date, Test, Condor, Platform, Framework\n";
	} else {
		print "Date, Passed, Failed, Expected, Missing, Test, Condor, Platform, Framework\n";
	}
	foreach $datapoint (@monthlydata) {

		#print "looking for $branch\n";
		$subjectbranch = $datapoint->branch();
		#print "Subject branch: $subjectbranch \n";
		if($subjectbranch eq $branch) {
			$date = $datapoint->date();
			$branch = $datapoint->branch();
			$passed = $datapoint->passed();
			$failed = $datapoint->failed();
			$expected = $datapoint->expected();
			$missing = $datapoint->missing();
			$condor_errs = $datapoint->condor_errs();
			$test_errs = $datapoint->test_errs();
			$unknown_errs = $datapoint->unknown_errs();
			$platform_errs = $datapoint->platform_errs();
			$framework_errs = $datapoint->framework_errs();
			$condor_build_errs = $datapoint->condor_build_errs();
			$platform_build_errs = $datapoint->platform_build_errs();
			$framework_build_errs = $datapoint->framework_build_errs();
			$allbuilds = $datapoint->allbuilds();
			$goodbuilds = $datapoint->goodbuilds();
			$failedbuilds = $datapoint->failedbuilds();
			$count_errs = $datapoint->count_errs();

			if($mode eq "testsum") {
#
# testsum positions are:
# date, expected, passed, (terrs + uerrs + cerrs + perrs + ferrs), platform_build, 
#	-platform_build, condor_build, -(platform_build + condor_build), framework_build, 
#	-(platform_build + condor_build + framework_build)
				#print "Dump testsum\n";
				printf OUTDATA "%s, ",$date ;
				printf OUTDATA "%d, ",$expected;
				printf OUTDATA "%d, ",$passed;
				printf OUTDATA "%d, ",($test_errs + $unknown_errs + $platform_errs + $condor_errs  + $framework_errs);
				printf OUTDATA "%d, ",$platform_build_errs;
				printf OUTDATA "%d, ",(0 - $platform_build_errs);
				printf OUTDATA "%d, ",$condor_build_errs;
				printf OUTDATA "%d, ",(0 - ($condor_build_errs + $platform_build_errs));
				printf OUTDATA "%d, ",$framework_build_errs;
				printf OUTDATA "%d\n",(0 - ($condor_build_errs + $platform_build_errs + $framework_build_errs));
			} elsif($mode eq "testdetail") {
# testdetail positions are:
# date, (terrs + uerrs + cerrs + perrs + ferrs), terrs, cerrs, (terrs + cerrs), perrs,
#	(terrs + cerrs + perrs), ferrs, (terrs + cerrs + perrs + ferrs)
#
				printf OUTDATA "%s, ",$date ;
				printf OUTDATA "%d, ",($test_errs + $unknown_errs + $platform_errs + $condor_errs  + $framework_errs);
				printf OUTDATA "%d, ",$test_errs;
				printf OUTDATA "%d, ",$condor_errs;
				printf OUTDATA "%d, ",($test_errs + $condor_errs);
				printf OUTDATA "%d, ",$platform_errs;
				printf OUTDATA "%d, ",($test_errs + $condor_errs + $platform_errs);
				printf OUTDATA "%d, ",$framework_errs;
				printf OUTDATA "%d\n",($test_errs + $condor_errs + $platform_errs + $framework_errs);
			} else {
				printf OUTDATA "%s, ",$datapoint->date();
				printf OUTDATA "%d, ",$datapoint->passed();
				printf OUTDATA "%d, ",$datapoint->failed();
				printf OUTDATA "%d, ",$datapoint->expected();
				printf OUTDATA "%d, ",$datapoint->missing();
				printf OUTDATA "%d, ",$datapoint->test_errs();
				printf OUTDATA "%d, ",$datapoint->condor_errs();
				printf OUTDATA "%d, ",$datapoint->platform_errs();
				printf OUTDATA "%d \n",$datapoint->framework_errs();
			}
		}
	}
	close(OUTDATA);
}

# =================================
# print help
# =================================

sub help 
{
    print "Usage: writefile.pl --megs=#
Options:
	[-h/--help]				See this
	[-d/--data]				file holding test data
	[-v/--view]				testsum, testdetail
	[-b/--branch]           6_8 or 6_9
	\n";
}
