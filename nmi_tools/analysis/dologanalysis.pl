#!/usr/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;
use Time::Local;

system("mkdir -p /scratch/bt/btplots");

my $dbtimelog = "/scratch/bt/btplots/dodownloads.log";
#print "Trying to open logfile... $dbtimelog\n";
open(OLDOUT, ">&STDOUT");
open(OLDERR, ">&STDERR");
open(STDOUT, ">$dbtimelog") or die "Could not open $dbtimelog: $!";
open(STDERR, ">&STDOUT");
select(STDERR);
 $| = 1;
 select(STDOUT);
  $| = 1;

chdir("/p/condor/public/html/developers/download_stats");

my $tooldir = "/p/condor/home/tools/build-test-plots/";
#my $tooldir = ".";
my $datalocation = "/p/condor/public/license/";
my $datacruncher = "filter_sendfile";
my $crunch = $datalocation . $datacruncher;
#my @datafiles = ( "sendfile-v6.0", "sendfile-v6.1", "sendfile-v6.2", "sendfile-v6.3", "sendfile-v6.4", "sendfile-v6.5", "sendfile-v6.6", "sendfile-v6.7", "sendfile-v6.8", "sendfile-v6.9","sendfile-v7.0", "sendfile-v7.1" );
#my @datafiles = ( "sendfile-v6.6", "sendfile-v6.7", "sendfile-v6.8", "sendfile-v6.9","sendfile-v7.0", "sendfile-v7.1" );
#my @datafiles = ( "sendfile-v6.8", "sendfile-v6.9", "sendfile-v7.0", "sendfile-v7.1" );
my @datafiles = ( "sendfile-v7.2", "sendfile-v7.3");
my $stable = "sendfile-v7.2";
my $developer = "sendfile-v7.3";
#my @datafiles = ( "sendfile-v6.8" );
my %datapoints;

GetOptions (
		'help' => \$help,
		'plot' => \$plot,
);

my %months = (
    "Jan" => "January",
    "Feb" => "February",
    "Mar" => "March",
    "Apr" => "April",
    "May" => "May",
    "Jun" => "June",
    "Jul" => "July",
    "Aug" => "August",
    "Sep" => "September",
    "Oct" => "October",
    "Nov" => "November",
    "Dec" => "December",
);

my %previousmonth = (
    "Feb" => "January",
    "Mar" => "February",
    "Apr" => "March",
    "May" => "April",
    "Jun" => "May",
    "Jul" => "June",
    "Aug" => "July",
    "Sep" => "August",
    "Oct" => "September",
    "Nov" => "October",
    "Dec" => "November",
    "Jan" => "December",
);

my %nextmonth = (
    "Dec" => "January",
    "Jan" => "February",
    "Feb" => "March",
    "Mar" => "April",
    "Apr" => "May",
    "May" => "June",
    "Jun" => "July",
    "Jul" => "August",
    "Aug" => "September",
    "Sep" => "October",
    "Oct" => "November",
    "Nov" => "December",
);

my %monthdays = (
    "Jan" => 31,
    "Feb" => 28,
    "Mar" => 31,
    "Apr" => 30,
    "May" => 31,
    "Jun" => 30,
    "Jul" => 31,
    "Aug" => 31,
    "Sep" => 30,
    "Oct" => 31,
    "Nov" => 30,
    "Dec" => 31,
);

my %nummonths = (
    "Jan" => 1,
    "Feb" => 2,
    "Mar" => 3,
    "Apr" => 4,
    "May" => 5,
    "Jun" => 6,
    "Jul" => 7,
    "Aug" => 8,
    "Sep" => 9,
    "Oct" => 10,
    "Nov" => 11,
    "Dec" => 12,
);

my %monthtoregnum = (
    "January" => 1,
	"February" => 2,
	"March" => 3,
    "April" => 4,
    "May" => 5,
    "June" => 6,
    "July" => 7,
    "August" => 8,
    "September" => 9,
    "October" => 10,
    "November" => 11,
    "December" => 12,
);

my %monthtonum = (
    "January" => 0,
	"February" => 1,
	"March" => 2,
    "April" => 3,
    "May" => 4,
    "June" => 5,
    "July" => 6,
    "August" => 7,
    "September" => 8,
    "October" => 9,
    "November" => 10,
    "December" => 11,
);

my %monthnums = (
    1 => "Jan",
    2 => "Feb",
    3 => "Mar",
    4 => "Apr",
    5 => "May",
    6 => "Jun",
    7 => "Jul",
    8 => "Aug",
    9 => "Sep",
    10 => "Oct",
    11 => "Nov",
    12 => "Dec",
);

if ( $help )    { help() and exit(0); }

$line;
$totalgood = 0;
$totalbad = 0;
$totaluniquegood = 0;
$totsrc = 0;
$negbad = 0;
$total = 0;
$scale = 0;
$month;
$day = 0;
$year;
$skip = "true";
$trimdata = "no";
$firstoutput;
%unique;
%sigevents;
$currentrelease = "";
$currenttitle = "";
my @queue = ();

#print "$crunch\n";
foreach $log (@datafiles) {
	$firstoutput = 0;
	$datafile = $datalocation . $log;
	print "$crunch $datafile $log\n";
	$outfile = $log . ".data";
	$weeklyoutfile = $log . ".weekly.data";
	$uniquefile = $log . ".unique.data";
	$totalsfile = $log . ".total.data";
	$image = $log . ".png";
	$uniqueimage = $log . ".unique.png";
	$totalsimage = $log . ".total.png";
	$events = $tooldir . $log . ".events";
	$saveevents =  $log . ".xtics";
	$trimdata = "no";
	$skip = "true";
	@queue = ();
	%unique = ();
	%sigevents = ();
	%uniquefiles = ();

	#ScanDownloadErrors ("problem-v7.0",\%datapoints);
	if($log =~ /^.*v(\d+)\.(\d+).*$/) {
		$currentrelease = "V$1.$2";
		#print "***************** $currentrelease ************************\n";
	} else {
		die "Failed to parse $log\n";
	}

	open(LOGDATA,"$crunch $datafile 2>&1|") || die "logdata bad: $!\n";
	open(OUT,">$outfile") || die "Failed to open $outfile: $!\n";
	open(UNIQUE,">$uniquefile") || die "Failed to open $uniquefile: $!\n";
	while(<LOGDATA>) {
		chomp;
        unshift @queue, $_;
    }
    reorderQueue();
	while( $line  = pop(@queue)) {
		#print "-----<$line>-----\n";
		# NOTE if you change matching criteria make SURE to
		# adjust matching criteria in reorderQueue().
		if($line =~ /^\s+[\?MBR]+\s+(\w+)\s+(\w+)\s+(\d+)\s+(\d+:\d+:\d+)\s+(\d+):\s(condor[-_])([\d\w\.\-]+).*\s+([^\s]+@[^\s]+).*$/) {
			if( $skip eq "true" )  {
				$skip = "false";
				#$firstdate = $3 . " " . $months{$2} . " " . $5;
				$firstdate = $months{$2} . " " . $3 . " " . $5;
				$month = $2;
				#print "Starting: $firstdate\n";
			} else {
				if( $day ne $3){ 
					$scale = ScaleData();
					DoOutput($scale);
					ResetCounters();
				}
				$trimdata = CheckForEnd( $month, $2, $3, $5 );
			}
			$day = $3;
			$month = $2;
			$year = $5;
			$filename = $6 . $7;
			$useremail = $8;
			# filter our multicounted good downloads
			my $datenow = $month . "/" . $day . " " . $year;
			my $gooduserfile = $useremail . "-" . $filename;
			#print "Consider $gooduserfile $datenow\n";
			if( !(exists $uniquefile{$gooduserfile})) {
				$uniquefile{$gooduserfile} = 1;
				#print "Adding $gooduserfile\n";
				$totalgood = $totalgood + 1;
				if(!(exists $unique{$useremail})) {
					$totaluniquegood += 1;
					$unique{$useremail} = 1;
					#print "Adding unique email <$useremail> for $2/$3\n";
				}
				if($filename =~ /^condor_.*/) {
					$totalsrc = $totalsrc + 1;
				}
			} else {
				#print "skip $gooduserfile\n";
			}
		#print "Good $2 $3 $5 <$totalgood|$line>\n";
		} elsif($line =~ /^\s+[\?MBR]+\s+(\w+)\s+(\w+)\s+(\d+)\s+(\d+:\d+:\d+)\s+(\d+):\s+condordebug.*$/) {
			#print "Ignore debugsysmbols|$l;ine\n";
		} elsif($line =~ /^\*[\?MBR]+\s+(\w+)\s+(\w+)\s+(\d+)\s+(\d+:\d+:\d+)\s+(\d+):.*/) {
			if( $skip eq "true" )  {
				$skip = "false";
				$firstdate = $3 . " " . $months{$2} . " " . $5;
				$month = $2;
				#print "Starting: ************** $firstdate ********************************\n";
			} else {
				if( $day ne $3){ 
					$scale = ScaleData();
					DoOutput($scale);
					ResetCounters();
				}
				$trimdata = CheckForEnd( $month, $2, $3, $5 );
			}
			$day = $3;
			$month = $2;
			$year = $5;
			$totalbad = $totalbad + 1;
			#print "Bad $2 $3 $5 <$totalbad|$line>\n";
		} else {
			#print "Mismatch:<$line>\n";
		}

		if($trimdata eq "yes") {
			last;
		}
	}
	
	# Did we skip a month and therefore the downloads have basically ended
	if( $trimdata ne "yes" ) {
		$scale = ScaleData();
		DoOutput($scale);
		ResetCounters();
	}

	#close the date file and generate image
	close(LOGDATA);
	close(UNIQUE);
	close(OUT);

	# create running total data file
	$line = "";
	my $runningtotalsrc = 0;
	my $runningtotalbin = 0;
	open(DATA,"<$outfile") || die "Failed to open $outfile: $!\n";
	open(TOT,">$totalsfile") || die "Failed to open $totalsfile: $!\n";
	while(<DATA>) {
		chomp();
		$line = $_;
		if($line =~ /^(\s*\w*\s+\d+\s+\d+),\s+(\d+),\s(\d+),\s+(.*)$/) {
			$runningtotalsrc += $2;
			$runningtotalbin += $3;
			#print "$totalsfile src $2 bin $3 tsrc $runningtotalsrc tbin $runningtotalbin\n";
			print TOT "$1, $runningtotalsrc, $runningtotalbin, $4\n";
		}
	}
	close(DATA);
	close(TOT);

	# look for significant events for plot x axis labeling
	if( -f $events) {
		$line = "";
		#print "**** Found $events ******\n";		
		open(EVENTS,"<$events") || print "failed to open $events:$!\n";
		while(<EVENTS>) {
			chomp();
			$line = $_;
			if($line =~ /^(\s*\w*\s+\d+\s+\d+),\s*\"(.*)\".*$/) {
				#print "***** Found  and added $2 on the $1\n";
				$sigevents{$1} = $2;
			}
		}
		close(EVENTS);
	}

	# convert the the download log to a weekly total for the non- 2 month plot
	# create a version of the data file which tracks weekly totals
	# and we may switch to using the first column to label the X axis
	# later. If we had a list of release dates, we could label the week
	# of the release by changing the date in the first column to the release
	# date string.....
	my $wdate = "";
	my $wsrc = 0;
	my $wgood = 0;
	my $wbad = 0;
	my $wcount = 0;
	my $datapoint = 0;
	my $timestamp = 0;
	my $xticslabels = "";
	my $last7totaldate = "";
	my $shortdate = "";
	open(DATA,"<$outfile") || die "Failed to open $outfile: $!\n";
	#print "Reading $outfile to produce $weeklyoutfile\n";
	open(WEEKLY,">$weeklyoutfile") || die "Failed to open $weeklyoutfile: $!\n";
	while(<DATA>) {
		chomp();
		$shortdate = "";
		$line = $_;
		#print "Outfile line: $line\n";
		if($line =~ /^(\s*\w*\s+\d+\s+\d+),\s*(\d+),\s*(\d+),\s*[-]?(\d+),\s*(\d+),\s*[-]?(\d+),\s*(\d+),\s*[-]?(\d+)\s*.*$/) {
			#print "Working on weekly totals for week containing $1\n";
			$wdate = $1;
			$wsrc += $2;
			$wgood += $3;
			$wbad += $4;
			$wcount += 1;
			# look for events for the week to replace date as label
			# last one per 7 days will be used
			if( exists $sigevents{$wdate} ) {
				$foundevent = $sigevents{$wdate};
			}
			if($wcount == 7) {
				print WEEKLY "$wdate, $wsrc, $wgood, -$wbad, 0, 0, 0, 0\n";
				$last7totaldate = $wdate; # save for calc of partial week
				if( $wdate =~ /^(\w+)\s+(\d+)\s+(\d+)$/) {
					#print "turning $wdate to a timestamp\n";
					my $mon = $monthtonum{$1};
					my $year = $3;
					$timestamp = timelocal("00","00","00",$2,$mon,$year);
					#print "Month $mon Day $2 Year $year, $timestamp \n";
					my $regmon =  $monthtoregnum{$1};
					$shortdate = $regmon . "/" . $2 . "/" . $year;
				} else {
					die "Can not parse date <$wdate>\n";
				}
				if($foundevent ne "") {
					#print "Found event <<$foundevent>>\n";
					if($xticslabels eq "") {
						#print "$xticslabels is currently <<$xticslabels>>\n";
						$xticslabels = $xticslabels  . "\"$foundevent\" \"$wdate\" ";
						#print "made xticslabels <<$xticslabels>>\n";
						$xticslabels
					} else {
						#print "$xticslabels is currently <<$xticslabels>>\n";
						$xticslabels = $xticslabels  . ",\"$foundevent\" \"$wdate\" ";
						#print "made xticslabels <<$xticslabels>>\n";
					}
				} else {
					#print "Found event <<$foundevent>>\n";
					if($xticslabels eq "") {
						#print "$xticslabels is currently <<$xticslabels>>\n";
						$xticslabels = $xticslabels  . "\"$shortdate\" \"$wdate\" ";
						#print "made xticslabels <<$xticslabels>>\n";
					} else {
						#print "$xticslabels is currently <<$xticslabels>>\n";
						$xticslabels = $xticslabels  . ",\"$shortdate\" \"$wdate\" ";
						#print "made xticslabels <<$xticslabels>>\n";
					}
				}
				$datapoint = $datapoint + 1;
				$wsrc = 0;
				$wgood = 0;
				$wbad = 0;
				$wcount = 0;
				$foundevent = "";
			}
		}
	}
	close(DATA);

	#partial week?
	#print "Looking for partial week...... wgood now <$wgood>\n";
	if($wgood != 0) {

		#print "Adding partial week.......\n";
		#print "$wdate, $wsrc, $wgood, -$wbad, 0, 0, 0, 0\n";
		if( $wdate =~ /^(\w+)\s+(\d+)\s+(\d+)$/) {
			#print "turning $wdate to a timestamp\n";
			my $mon = $monthtonum{$1};
			my $year = ($3 - 1900);
			#print "Month $mon Year $year\n";
			$timestamp = timelocal("00","00","00",$2,$mon,$year);
			#print "Month $mon Day $2 Year $year, $timestamp \n";
			my $regmon =  $monthtoregnum{$1};
			$shortdate = $regmon . "/" . $2 . "/" . $year;
		} else {
			die "Can not parse date <$wdate>\n";
		}

		$newdate = MakeWeekSince($last7totaldate);
		print WEEKLY "$newdate, $wsrc, $wgood, -$wbad, 0, 0, 0, 0\n";

		if($foundevent ne "") {
			#print "foundevent not empty<<<$foundevent>>>\n";
			if($xticslabels eq "") {
				#print "xticslabels empty\n";				
				#print "Set to <<<$xticslabels>>> based on new date <<<$newdate>>>\n";
				$xticslabels = $xticslabels  . "\"$foundevent\" \"$newdate\" ";
				#print "xticslabels now <<<$xticslabels>>>\n";
			} else {
				#print "xticslabels NOT empty<<<$xticslabels>>>\n";			
				#print "Set to <<<$xticslabels>>> based on new date <<<$newdate>>>\n";
				$xticslabels = $xticslabels  . ",\"$foundevent\" \"$newdate\" ";
				#print "xticslabels now <<<$xticslabels>>>\n";
			}
		} else {
			#print "foundevent empty<<<$foundevent>>>\n";
			if($xticslabels eq "") {
				#print "xticslabels empty\n";
				#print "Set to <<<$xticslabels>>> based on new date <<<$newdate>>>\n";
				$xticslabels = $xticslabels  . "\"$shortdate\" \"$newdate\" ";
				#print "xticslabels now <<<$xticslabels>>>\n";
			} else {
				#print "xticslabels NOT empty<<<$xticslabels>>>\n";
				#print "Set to <<<$xticslabels>>> based on new date <<<$newdate>>>\n";
				$xticslabels = $xticslabels  . ",\"$shortdate\" \"$newdate\" ";
				#print "xticslabels now <<<$xticslabels>>>\n";
			}
		}
		$datapoint = $datapoint + 1;
	}
	close(WEEKLY);

	# save xtics for the plot
	#print "XTICS = $xticslabels\n";
	open(XTICS,">$saveevents") || die "Need XTICS failed to open<$saveevents>:$!\n";
	print XTICS "( $xticslabels )\n";
	close(XTICS);

	# make plots for accumulated plot
	$currenttitle = $currentrelease . " Accumulated Downloads";
	$graphcmd = "$tooldir/make-graphs-dwnloads --firstdate \"$firstdate\" --detail totaldownloadlogs --daily --input $totalsfile --output $totalsimage --title \"$currenttitle\"";
	system("$graphcmd");

	# make plots for this series
	$currenttitle = $currentrelease . " Weekly Totals";
	$weeklygraphcmd = "$tooldir/make-graphs-dwnloads --firstdate \"$firstdate\" --detail weeklydownloadlogs --weekly --input $weeklyoutfile --output $image --xtics $saveevents --title \"$currenttitle\"";
	system("$weeklygraphcmd");

	$currenttitle = $currentrelease . " Unique Totals";
	$uniquegraphcmd = "$tooldir/make-graphs-dwnloads --firstdate \"$firstdate\" --detail uniquedownloadlogs --daily --input $uniquefile --output $uniqueimage --title \"$currenttitle\"";
	system("$uniquegraphcmd");

	# prepare a 2 month plot for key downloads

	$shortdate = mk2month_strtdate($day, $month, $year);

	if($log eq $developer) {
		$currenttitle = $currentrelease . "(Developer) Daily Totals";
		$graphcmd = "$tooldir/make-graphs-dwnloads --firstdate \"$shortdate\" --detail downloadlogs --daily --input $outfile --output developer.png --title \"$currenttitle\"";
	} elsif ($log eq $stable) {
		$currenttitle = $currentrelease . "(Stable) Daily Totals";
		$graphcmd = "$tooldir/make-graphs-dwnloads --firstdate \"$shortdate\" --detail downloadlogs --daily --input $outfile --output stable.png --title \"$currenttitle\"";
	}
	if(($log eq $developer) || ($log eq $stable)) {
		#print "About to execute this:\n";
		#print "$graphcmd\n";
		system("$graphcmd");
	}

}
exit(0);

sub ScanDownloadErrors 
{
    my $log = shift;
    my $hashref = shift;
    $datafile = $datalocation . $log;
    #print "examine $datafile\n";
    $newrecord = 0;
    
    open(LOGDATA,"<$datafile") || die "logdata bad: $!\n";
    my $line = "";
    my @error = ();
    my $skiprecord = 1;
    my $firstreset = 1;
    
    my $date = "";
    while(<LOGDATA>) {
        chomp;
        $line = $_;
        #unshift @queue, $_;
        #print "$_\n";
        if($line =~ /^[\-]+----$/) {
            #print "New record\n";
            if($skiprecord == 0) {
                #print "Process Last Record\n";
            } else {
                if($firstreset == 0) {
                    # remove count when first seen
                    ${$hashref}{"$date"} -= 1;
                } else {
                    $firstreset = 0;
                }
            }
            @error = ();
            $skiprecord = 1;
        } elsif($line =~ /^(\d+)\s+(\d+\.\d+\.\d+\.\d+).*$/) {
            #print "$1 $2\n";
            my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($1);
            $mon += 1;
            $year += 1900;
            #print "$months{$monthnums{$mon}} $mday $year\n";
            $date = "$months{$monthnums{$mon}} $mday $year";
            ${$hashref}{"$date"} += 1;
        } elsif($line =~ /^UM:(.*)$/) {
            # some people sent a blank form - do not count as error
            if($1 eq "") {
                #print "Content not defined\n";
            } else {
                $skiprecord = 0;
            }
        }
        push @error, $line;
    }
}


sub DropData 
{
    foreach  $key (sort keys %datapoints) {
        if(exists $datapoints{$key}) {
            #print "$key $datapoints{$key} \n";
        }
    }
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
	\n";
}

# =================================
# reorderQueue will go through the queue
# looking for day changes. If it pops back
# within 2 more items it saves the data points
# finds them in the queue and places them with 
# their kin
# =================================

sub reorderQueue
{
    @outoforder = ();
    $outmax = 2; # look for two matching
    $outcount = 0;
    @reorder = ();
    $outvalue = 0;
    $thisday = 0;
    $first = "true";
    while( $line = pop(@queue)) {
        if($line =~ /^\s*[\*\?MBR]+\s+(\w+)\s+(\w+)\s+(\d+)\s+.*$/) {
            # either good or bad
            if($irst eq "true") {
                # first
                $thisday = $3;
                $first = "false";
                push @reorder, $line;
            } else {
                # rest
                if($3 == $thisday) {
                    # match
                    push @reorder, $line;
                } else {
                    # mismatch
                    if($outcount == 0) {
                        # first of new value
                        $outcount = $outcount + 1;
                        push @outoforder, $line;
                        $outvalue = $3;
                    } else {
                        #second assumed same value
                        $requeue = pop(@outoforder);
                        push  @reorder, $requeue;
                        push @reorder, $line;
                        $outcount = 0;
						$thisday = $3;
                    }
                }
            }
        }
    }
	# what if we have the last value be first of a day?
	# If we are holding one at the end , drop it out
	if($outcount != 0){
		$requeue = pop(@outoforder);
		push  @reorder, $requeue;
	}

    while( $line = pop(@reorder)) {
        push @queue, $line;
    }
}

# =================================
# make the partial weekly total have a 
# full week range for the plot
# =================================

sub MakeWeekSince
{
	my $lastdate = shift;
	my $monthnum =  0;
	my $shortmonth = "";
	my $monthdays = 0;
	my $newday = 0;
	#print "MakeWeekSince $lastdate\n";
	if( $lastdate =~ /^(\w+)\s+(\d+)\s+(\d+)$/) {
		#print "Month $1 Day $2 Year $3\n";
		$monthnum = $monthtoregnum{$1};
		$shortmonth = $monthnums{$monthnum};
		$monthdays = $monthdays{$shortmonth};
		#print "MakeWeekSince Month # $monthnum Short Month $shortmonth\n";
		$newday = $2 + 7;
		#print "MakeWeekSince New Day $newday  Month Days $monthdays\n";
		if( $newday <= $monthdays) {
			$newdate =  $1 . " " . $newday . " " . $3;
			#print "MakeWeekSince $lastdate\n";
			#print "MakeWeekSince Done $newdate\n";
			return($newdate);
		} else {
			#print "MakeWeekSince New Day $newday  Month Days $monthdays\n";
			#print "MakeWeekSince Complicated......\n";
			$daysthismonth =  $monthdays - $2;
			$daysnextmonth = 7 - $daysthismonth;
			my $year = $3;
			if( $monthnum == 12 ) {
				$monthnum = 1;
				$year += 1;
			} else {
				$monthnum = $monthnum + 1;
			}
			$shortmonth = $monthnums{$monthnum};
			$regularmonth = $months{$shortmonth};
			$newdate =  $regularmonth . " " . $daysnextmonth . " " . $year;
			#print "MakeWeekSince $lastdate\n";
			#print "MakeWeekSince Done $newdate\n";
			return($newdate);
		}
	} else {
		#print "MakeWeekSince not parsing date\n";
		# return todays date
		my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
        $mon += 1;
        $year += 1900;
        #print "$months{$monthnums{$mon}} $mday $year\n";
        my $date = "$months{$monthnums{$mon}} $mday $year";
		return($date);
	}
	#print "MakeWeekSince $lastdate\n";
	#print "MakeWeekSince Done\n";
}

# =================================
# Make a zero amount starting date a day earlier
# =================================

sub mk2month_strtdate
{
	my $day = shift;
	my $month = shift;
	my $intmonth = 0;
	$intmonth = $nummonths{$month};
	my $year = shift;
	my $intyear = 0; 
	$intyear = $year;
	#print " Day $day Month $intmonth Year $intyear\n";
	if(($intmonth - 2) > 0) {
		$intmonth = $intmonth - 2;
	} elsif($intmonth == 2) {
		$intmonth = 12;
		$intyear = $intyear - 1;
	} elsif($intmonth == 1) {
		$intmonth = 11;
		$intyear = $intyear - 1;
	}

	$month = $monthnums{$intmonth};
	$month = $months{$month};
	#print "shortdate: Day $day Month $month Year $intyear\n";
	$newyear = "";
	$newyear = $intyear;
	$newdate = $day . " " . $month . " " . $newyear;
	#print "Returning new date $newdate\n";
	return($newdate);
}

# =================================
# Scale the data and return position info. Which
# data location is correct for color coding of data.
# =================================

sub ScaleData
{
	$total = $totalgood + $totalbad;
	if($total > 1000) {
		$total = $total / 200;
		$totalgood = $totalgood / 200;
		$totalbad = $totalbad / 200;
		$negbad = ( 0 - $totalbad);
		return(2);
	} elsif($total > 200) {
		$total = $total / 20;
		$totalgood = $totalgood / 20;
		$totalbad = $totalbad / 20;
		$negbad = ( 0 - $totalbad);
		return(1);
	}
	$negbad = ( 0 - $totalbad);
	return(0);
}

sub ResetCounters
{
	$total = 0;
	$totalgood = 0;
	$totaluniquegood = 0;
	$totalbad = 0;
	$totalsrc = 0;
	%unique = ();
	%uniquefiles = ();
}

sub DoOutput
{
	my $scale = shift;

	if($firstoutput == 0) {
		#ok if we start a data set on the 1st of the month we "should"
		# roll the data to the previous month....
		#print "starting 0 data point\n";
		if($day == 1) {
            $dayearly = $monthdays{$month};
            print OUT "$previousmonth{$month} $dayearly $year, 0, 0, 0, 0, 0, 0\n";
        } else {
            $dayearly = $day - 1;
            print OUT "$months{$month} $dayearly $year, 0, 0, 0, 0, 0, 0\n";
        }
		$firstoutput = 1;
	}
	print UNIQUE "$months{$month} $day $year, 0, $totaluniquegood, 0, 0, 0, 0, 0\n";
	if( $scale == 0 ) {
		print OUT "$months{$month} $day $year, $totalsrc, $totalgood, $negbad, 0, 0, 0, 0\n";
	} elsif( $scale == 1 ) {
		print OUT "$months{$month} $day $year, 0, 0, 0, $totalgood, $negbad, 0, 0\n";
	} else { # 2 returned for scaling
		print OUT "$months{$month} $day $year, 0, 0, 0, 0, 0, $totalgood, $negbad\n";
	}
}

sub CheckForEnd
{
	# $trimdata = CheckForEnd( $month, $2 );
	#print "last month was $month and now $2\n";
	$lastmonth = shift;
	$thismonth = shift;
	$thisday = shift;
	$thisyear = shift; 
	# We have a bad data point where a Sept 2005 day falls in the middle
	# of january 2004 messing up my approximate trimming. Not worth
	# finding why as most of the data flows very smoothly
	if(($thismonth eq "Sep") && ($thisday eq "8") && ($thisyear eq "2005")) {
		return("no");
	}

	if(($thismonth eq "Nov") && ($thisday eq "25") && ($thisyear eq "2006")) {
		return("no");
	}

	if($lastmonth eq $thismonth) {
		return("no");
	}

	$lastmonth = $nummonths{$lastmonth};
	$thismonth = $nummonths{$thismonth};

	if( $lastmonth == 12 ) {
		if($thismonth == 1) {
			#print "January after December\n";
			return("no");
		} else {
			#print "NOT January after December\n";
			# end of month reversion to last month?
			# sometimes we have previous day events after
			# we change days....... assume this is the case 
			# and it is not an 11 month skip....
			if( $thismonth == 11 ) {
				# assume nov 30/Dec 1
				#print "Late  november\n";
				return("no");
			} else {
				#print "stopping on last <$lastmonth> this <$thismonth>\n";
				return("yes");
			}
		}
	} else {
		if( $thismonth != ($lastmonth + 1)) {
			#print "Increment of NOT 1 $lastmonth/$thismonth\n";
			# end of month reversion to last month?
			# sometimes we have previous day events after
			# we change days....... assume this is the case 
			# and it is not an 11 month skip....
			if(($lastmonth == 1) && ($thismonth == 12)) {
				# dec 31/Jan 1
				#print "dec 31/Jan 1\n";
				return("no");
			} elsif( $thismonth < $lastmonth ) {
				return("no");
				#print "This less then last\n";
			} else {
				#print "stopping on last <$lastmonth> this <$thismonth>\n";
				return("yes");
			}
		} else {
			#print "Increment of 1 $lastmonth/$thismonth\n";
			return("no");
		}
	}
}
