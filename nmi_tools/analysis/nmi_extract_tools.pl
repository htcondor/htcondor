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

my %platformlist;


# Plot based date format is Day Month Year and the data we write out here
# will be in this format for plotting.


GetOptions (
		'platform' => \$platforms,
		'runid=i' =>\$runid,
);

DbConnect();

if(defined $runid) {
	print "Want info on runid $runid\n";

	FindPlatforms($runid);
	FindBuildTasks($runid);
	FindBuildRunDetails($runid);
} else {
	print "Usage: ./nmi_extract_tools.pl --runid=12345\n";
}



DbDisconnect();	# all done..... :-)

exit(0);

sub FindPlatforms
{
	$runid = shift;

	#print "Platforms for <$runid> are:\n";
	$extraction = $db->prepare("SELECT * FROM Task WHERE  \
			runid = $runid \
			and name =  'platform_job' \
			");
	$extraction->execute();
	while( my $sumref = $extraction->fetchrow_hashref() ){
		$platform = $sumref->{'platform'};

		print "$platform\n";

		if( exists $platformlist{$platform}) {
			# we are good and have this already 
			#print "Have $platform in lest already\n";
		} else {
			#print "Add $platform as a platform\n";
			$platformlist{$platform} = 1;
		}
	}
}

sub FindBuildTasks
{
	my $runid = shift;

	my $line;
	my $extraction;

	$extraction = $db->prepare("SELECT * FROM Task WHERE  \
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
		$start = $sumref->{'start'};
		$finish = $sumref->{'finish'};
		$line = "";

		if( defined($finish)) {
			$line = $platform . "|" . $start . "|" . $finish . "|";
		} else {
			$line = $platform . "|" . $start . "|-----|";
		}

		if( defined($res)) { 
			#print "<$res>\n";
			if($res == 0) {
					$line = $line . "success\n";
			} else {
				# we are left with the positive and negative failures
				$line = $line . "fail\n";
			}
		} else {
			$line = $line . "pending\n";
		}
		print "$line";
	}
}

sub FindBuildRunDetails
{
	my $runid = shift;
	my $gid = "";
	my $host = "";
	my $url = "http//";
	my $line = "";
	
	# looking for project use or nightly use?
	#print "Looking for runs since <$TodayDate>\n";
	my $extraction;
		$extraction = $db->prepare("SELECT * FROM Run WHERE  \
				 runid = $runid");

    $extraction->execute();
	while( my $sumref = $extraction->fetchrow_hashref() ){
		$gid = $sumref->{'gid'};
		$host = $sumref->{'host'};
		$path = $sumref->{'filepath'};
	}

	foreach $key (sort keys %platformlist) {
        $line = $key . "|" . $url . $host . $path . "/" . $gid . "/userdir/" . $key;
		print "$line\n";
	}
}

sub DbConnect
{
    $db = DBI->connect("DBI:mysql:database=nmi_history;host=nmi-build25", "nmipublic", "nmiReadOnly!") || die "Could not connect to database: $!\n";
    #print "Connected to db!\n";
}

sub DbDisconnect
{
    $db->disconnect;
}
