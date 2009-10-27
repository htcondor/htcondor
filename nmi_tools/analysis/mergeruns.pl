#!/usr/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;

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

GetOptions (
		'help' => \$help,
		'data=s' => \$datafile,
		'out=s' => \$outfile,
		'type=s' => \$type,
);

%ratehist;

if ( $help )    { help() and exit(0); }
if( !$datafile ) {
	die "Have to have an input file!\n";
}

if( !$type ) {
	die "Have to have a run type! <builds or tests>\n";
}

if( $datafile ) {
	#print "Merging daily data in <<$datafile>>\n";
}

my $domove = 0;
if( !$outfile ) {
	$outfile = "/tmp/tempdata";
	$domove=1;
	#print "Temp results going to <<$outfile>>\n";
}
#print "Temp results going to <<$outfile>>\n";

my $curday = "";
my $curyear = "";
my $curmonth = "";
my $line = "";

my $c1 = 0;
my $c2 = 0;
my $c3 = 0;
my $c4 = 0;
my $c5 = 0;

#allow for two branches and day
$branches = 1;
$branch = "";
$nonzero = 0;
$entries = 1;
$ratemarker = 0;
$previousrate = 0;
$previousratio = 0;
$t4 = 0;
$t5 = 0;
$t6 = 0;
$t7 = 0;
$t8 = 0;

open(DATA,"<$datafile") || die "Can not open data file<<$datafile>>: $!\n";
open(OUTDATA,">$outfile") || die "Can not open data file<<$outfile>>: $!\n";
while(<DATA>) {
	$ratemarker = $ratemarker + 1;
	chomp($_);
	$line = $_;
	#if( $line =~ /^(\d+)\s+(\w+)\s(\d+),\s+(\d+),\s+(\d+),\s+(\d+),\s+(\d+),\s+(\d+)\s*,\s*(V\d+_\d+-(trunk|branch)).*$/ ) {
	if( $line =~ /^(\d+)\s+(\w+)\s(\d+),\s+(\d+),\s+(\d+),\s+(\d+),\s+(\d+),\s+(\d+)\s*,\s*([V]*\d*[_]*\d*[\-]*(trunk|branch)).*$/ ) {
		#print "$line\n";
		# check for weird data and toss
		# how many branches this day?
		if($branch eq "") {
			#first branch
			$branch = $9;
			#print "setting branch to $9\n";
			#print "Number of branches is<<$branches>>\n";
		} elsif($branch ne $9) {
			#second branch
			#print "Second branch is $9 upping $branches to 2<<$branches>>\n";
			$branches = 2;
			#print "Second branch is $9 upping $branches to 2<<$branches>>\n";
		}

		$t4 = $4;
		$t5 = $5;
		$t6 = $6;
		$t7 = $7;
		$t8 = $8;
		
		feedratehist($t8);

		if(($1 eq $curday) && ($2 eq $curmonth) && ($3 eq $curyear)) {
			# roll in second days data
			if($type eq "builds") {
				# dump bad build data - total builds >0 and <10   kludge
				if(($c5 != 0) && ($c5 < 10)) {
					#next;
				}
			}

			#print "repeat day up entry\n";
			$entries = $entries + 1;
			#how many none zero entries
			if( $t8 != 0 ) {
				$nonzero = $nonzero + 1;
				#print "Got non-zero Total <<$t8>>\n";
			}

			$c1 = $c1 + $t4;
			$c2 = $c2 + $t5;
			$c3 = $c3 + $t6;
			$c4 = $c4 + $t7;
			$c5 = $c5 + $t8;
		} else {
			#print "new day\n";
			#calculate an average - compromise
			#only if current day not "";
			if( $curday ne "") {
				if($nonzero == 0) {
					# well they were all zero so just print it.
					print OUTDATA "$curday $curmonth $curyear, $c1, $c2, $c3, $c4, $c5\n";
					$nonzero = 0;
				} else {
					#print "New day so print results prior\n";
					#print "barnches is <<$branches>> Running total is <<$c5>> and non-zero is <<$nonzero>>\n";
					#print "entries is <<$entries>>\n";
#					$c1 = (($c1 / $nonzero) * $branches);
#					$c2 = (($c2 / $nonzero) * $branches);
#					$c3 = (($c3 / $nonzero) * $branches);
#					$c4 = (($c4 / $nonzero) * $branches);
#					$c5 = (($c5 / $nonzero) * $branches);
					$c1 = (($c1));
					$c2 = (($c2));
					$c3 = (($c3));
					$c4 = (($c4));
					$c5 = (($c5));
					print OUTDATA "$curday $curmonth $curyear, $c1, $c2, $c3, $c4, $c5\n";
					#print "$curday $curmonth $curyear, $c1, $c2, $c3, $c4, $c5\n";
					$nonzero = 0;
				}
				# reset branch tracking variables
				#print "Clear branch and count branches\n";
				$branches = 1;
				$branch = $9;
				#print "New day and entry starting at 1\n";
				$entries = 1;
			}
			#how many none zero entries
			if( $t8 != 0 ) {
				$nonzero = $nonzero + 1;
				#print "Got non-zero Total <<$t8>>\n";
			}

			# start next day
			#print "Collect new day's data\n";
			$curday = $1;
			$curmonth = $2;
			$curyear = $3;
			$c1 = $t4;
			$c2 = $t5;
			$c3 = $t6;
			$c4 = $t7;
			$c5 = $t8;
		}
	} elsif( $line =~ /^(\d+)\s+(\w+)\s(\d+),\s+(\d+),\s+(\d+),\s+(\d+),\s+(\d+),\s+(\d+)\s*,\s*(Continuous\s*Build\s*-.*)$/ ) {
		#print "$line\n";
		# check for weird data and toss
		# how many branches this day?
		if($branch eq "") {
			#first branch
			$branch = $9;
			#print "setting branch to $9\n";
			#print "Number of branches is<<$branches>>\n";
		} elsif($branch ne $9) {
			#second branch
			#print "Second branch is $9 upping $branches to 2<<$branches>>\n";
			$branches = 2;
			#print "Second branch is $9 upping $branches to 2<<$branches>>\n";
		}

		$t4 = $4;
		$t5 = $5;
		$t6 = $6;
		$t7 = $7;
		$t8 = $8;

		feedratehist($t8);

		if(($1 eq $curday) && ($2 eq $curmonth) && ($3 eq $curyear)) {
			# roll in second days data
			if($type eq "builds") {
				# dump bad build data - total builds >0 and <10   kludge
				if(($c5 != 0) && ($c5 < 10)) {
					#next;
				}
			}

			#print "repeat day up entry\n";
			$entries = $entries + 1;
			#how many none zero entries
			if( $t8 != 0 ) {
				$nonzero = $nonzero + 1;
				#print "Got non-zero Total <<$t8>>\n";
			}

			$c1 = $c1 + $t4;
			$c2 = $c2 + $t5;
			$c3 = $c3 + $t6;
			$c4 = $c4 + $t7;
			$c5 = $c5 + $t8;
		} else {
			#print "new day\n";
			#calculate an average - compromise
			#only if current day not "";
			if( $curday ne "") {
				if($nonzero == 0) {
					# well they were all zero so just print it.
					print OUTDATA "$curday $curmonth $curyear, $c1, $c2, $c3, $c4, $c5\n";
					$nonzero = 0;
				} else {
					#print "New day so print results prior\n";
					#print "barnches is <<$branches>> Running total is <<$c5>> and non-zero is <<$nonzero>>\n";
					#print "entries is <<$entries>>\n";
					$c1 = (($c1));
					$c2 = (($c2));
					$c3 = (($c3));
					$c4 = (($c4));
					$c5 = (($c5));
					print OUTDATA "$curday $curmonth $curyear, $c1, $c2, $c3, $c4, $c5\n";
					#print "$curday $curmonth $curyear, $c1, $c2, $c3, $c4, $c5\n";
					$nonzero = 0;
				}
				# reset branch tracking variables
				#print "Clear branch and count branches\n";
				$branches = 1;
				$branch = $9;
				#print "New day and entry starting at 1\n";
				$entries = 1;
			}
			#how many none zero entries
			if( $t8 != 0 ) {
				$nonzero = $nonzero + 1;
				#print "Got non-zero Total <<$t8>>\n";
			}

			# start next day
			#print "Collect new day's data\n";
			$curday = $1;
			$curmonth = $2;
			$curyear = $3;
			$c1 = $t4;
			$c2 = $t5;
			$c3 = $t6;
			$c4 = $t7;
			$c5 = $t8;
		}
	} else {
		#print ".";
	}
}
close(OUTDATA);
close(DATA);
#system("cat $outfile");

if($domove == 1) {
	#merged files goes back to same file
	system("mv $outfile $datafile");
}

sub resetratehist
{
	for($cnt = 1;$cnt < 30;$cnt++) {
		$ratehist{$cnt} = 0;
	}
	return(0);
}

sub readratehist
{
	$gooddays = 0;
	$average = 0;
	$total = 0;
	for($cnt = 1;$cnt < 30;$cnt++) {
		if($ratehist{$cnt} != 0) {
			$total = $total + $ratehist{$cnt};
			$gooddays = $gooddays + 1;
		}
	}
	if($gooddays != 0) {
		$average = $total / $gooddays;
		#print "Rate hist return <<<$average>>>\n";
		return($average);
	} else {
		return(0);
	}
}

sub feedratehist
{
	$value = shift;
	#print "Feeding value is <<<<<$value>>>>>\n";
	my $day = $ratemarker % 30;
	$ratehist{$day} = $value;
}

# =================================
# print help
# =================================

sub help 
{
    print "Usage: writefile.pl --megs=#
Options:
	[-h/--help]             See this
	[-d/--data]             file holding test data
	[-o/--out]              temp file holding test data
	\n";
}
