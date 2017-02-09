#!/usr/bin/perl -w
#
use strict;
use feature ':5.10';
use Fcntl;
use Getopt::Std;

#use warnings;

#Global variables
my @fileHashes;
my %selectedItems;
my %opts;

sub usage()
{
    print "Usage: condor_top.pl [-nhs num] file1 file2
    Calculate runtime statistics from two classads and display the top
    results. The two input classads should be from the same daemon at
    different times.
    -h:	Display this usage statement
    -c num:	Sort by column number 'num'\n";
	exit;
}

sub HELP_MESSAGE()
{
	usage();
}

sub VERSION_MESSAGE()
{
	usage();
}

sub checkArgs()
{
	my $num_args = $#ARGV + 1;
	if ($num_args != 2)
	{
		usage();
	}
}

sub parseKey
{
	#Begin magical wizard voodoo (aka regular expressions)...

	# Check to see that we have the proper number of arguements
	if( (scalar (@_)) != 1)
	{
		die "ERROR: Unexpected number of arguments in 'parseKey()'\n";
	}
	#Store the key as a string
	my $string = $_[0];
	#Use reg ex to see if the key has "DC" at the beginning AND "Runtime" 
	#at the end
	#if($string =~ /^DC/ && $string =~ /Runtime$/)
	if($string =~ /Runtime$/)
	{
		#Remove "Runtime" from the end of the key
		$string =~ s/Runtime//g;
		return $string;
	}
	#If there isn't a match return an empty string
	else
	{
		return "";
	}
}

# Calculate the duty cycle used to determine the "health" of the system
sub dutyCycle
{
	my $selectWaitTime = $fileHashes[1]{"DCSelectWaittime"} - 
		$fileHashes[0]{"DCSelectWaittime"}; 
	
	my $pumpCycle_Count = $fileHashes[1]{"DCPumpCycleCount"} - 
		$fileHashes[0]{"DCPumpCycleCount"};
		
	my $pumpCycle_Sum = $fileHashes[1]{"DCPumpCycleSum"} - 
		$fileHashes[0]{"DCPumpCycleSum"};

	my $dDutyCycle = 0.0;


	if($pumpCycle_Count)
	{
		if($pumpCycle_Sum > 1e-9)
		{
			$dDutyCycle = 1.0 - ($selectWaitTime / $pumpCycle_Sum);
		}
	}
	return $dDutyCycle;
}

sub sortColumns
{
	my ($col_num, %keyTable) = @_;

	my @keys;
	#$col_num--;
	if($col_num == 6)
	{
		@keys = sort { $selectedItems{$b}[$col_num] cmp 
			$selectedItems{$a}[$col_num] } keys %keyTable;
	}
	else
	{
		@keys = sort { $selectedItems{$b}[$col_num] <=> 
			$selectedItems{$a}[$col_num] } keys %keyTable;
	}

	return @keys;
}

sub loadFiles()
{
	#Variable declarations
	my $handle;

	checkArgs();

	#Load file names
	my @files = ($ARGV[0], $ARGV[1]);


	#Load data into 2 hashtables
	for(my $i = 0; $i < (scalar @files); $i++)
	{
		#Open file with read-only permissions
		sysopen($handle,$files[$i], O_RDONLY) or 
			die "Couldn't open file $files[$i]";
		#For each line, split components into key and value and 
		#store in hashtable
		my @lines = <$handle>;
		for(my $j = 2; $j < (scalar @lines); $j++)
		{
			my @strings = split(" = ",$lines[$j]);
            #print "$strings[0] = $strings[1]";
			$fileHashes[$i]{$strings[0]} = $strings[1];
		}
		close($handle);
	}

}

sub generateHash()
{
	#Initialize variables
	my $countB = 0;   
	my $countA = 0;
	my $runtimeB = 0;
	my $runtimeA = 0;			
	my $max = 0;
	my $overall_avg = 0;
	my $inst_avg = 0;
	my $delta_count = 0;
	my $key = 0;
	my $timeB = 0;
	my $timeA = 0;

	#For every key in the hash table...
	foreach $key (keys(%{$fileHashes[0]}))
	{
		$key = parseKey($key);
        #print "key=$key\n";
		
		#If a key is not usable... or the key is not one of the 
		#following...
		if($key ne "" || !($key eq "DCSocket" || $key eq "DCTimer" 
			|| $key eq "DCSignal" || $key eq "DCPipe"))
		{	
			#Reset variables		
			$countB = 0;   
			$countA = 0;
			$runtimeB = 0;
			$runtimeA = 0;			
			$max = 0;
			$overall_avg = 0;
			$inst_avg = 0;
			$delta_count = 0;

			#Check to see if the item exists in the
			#hashtable, set it to its var and remove
			#any newline characters

			if(defined($fileHashes[1]{$key}))
			{
				$countB = $fileHashes[1]{$key};
				chomp($countB);
			}

            #print "key=$key count=$countB\n";


			#Skip those which have 0 as a count as they don't
			#have much useful info
			if($countB == 0)
			{
				next;
			}

            #print "key=$key count=$countB\n";

			if(defined($fileHashes[0]{$key}))
			{
				$countA = $fileHashes[0]{$key};
				chomp($countA);
			}

			if(defined($fileHashes[0]{$key."Runtime"}))
			{
				$runtimeA  = $fileHashes[0]{$key."Runtime"};
				chomp($runtimeA);
			}

			if(defined($fileHashes[1]{$key."Runtime"}))
			{
				$runtimeB = $fileHashes[1]{$key."Runtime"};
				chomp($runtimeB);
			}

			if(defined($fileHashes[1]{$key."RuntimeMax"}))
			{
				$max = $fileHashes[1]{$key."RuntimeMax"};
				chomp($max);
			}

			if(defined($fileHashes[1]{$key."RuntimeAvg"}))
			{
				$overall_avg = 
					$fileHashes[1]{$key."RuntimeAvg"};
				chomp($overall_avg);
			}

			if(defined($fileHashes[1]{"RecentStatsTickTime"}))
			{
				$timeB = 
					$fileHashes[1]{"RecentStatsTickTime"};
				chomp($timeB);
			}

			if(defined($fileHashes[0]{"RecentStatsTickTime"}))
			{
				$timeA = 
					$fileHashes[0]{"RecentStatsTickTime"};
				chomp($timeA);
			}
			
			#Check that we have the appropriate coponents and
			#compute the instantaneous average
			if($countA ne "" && $countB ne "" && $runtimeA ne "" 
				&& $runtimeB ne "")
			{
				my $denom = $countB - $countA;
				if($denom != 0)
				{
					$inst_avg = ($runtimeB - $runtimeA) 
						/ ($countB - $countA);
				}
			}

			#Check that we have the appropriate components
			#and compute the normalized difference between the 
			#two data points
			if($countA ne "" && $countB ne "")
			{	
				my $minutes = ($timeB - $timeA) / 60;
				if ($minutes) {
				$delta_count= ($countB - $countA) / $minutes; 
				}
			}

			#Remove the first occurrence of "DC" from the 
			#item name			
			$key =~ s/^DC//g;
			
			$selectedItems{$key} = [$runtimeB, $inst_avg, $overall_avg, 
				$delta_count, $countB, $max, $key];			
		}
	}
}

#Print formatted health information
sub generateHealthTable()
{
    if (exists $fileHashes[1]{"MonitorSelfSysCpuTime"}) {
       my $str = sprintf("SysCpu:   %9d        UserCpu: %8d",
                      $fileHashes[1]{"MonitorSelfSysCpuTime"},
                      $fileHashes[1]{"MonitorSelfUserCpuTime"});
       print("$str\n");
    }
    if (exists $fileHashes[1]{"MonitorSelfImageSize"}) {
       my $str = sprintf("Memory:   %9d        RSS:     %8d",
                      $fileHashes[1]{"MonitorSelfImageSize"},
                      $fileHashes[1]{"MonitorSelfResidentSetSize"});
       print("$str\n");
    }

	my $dutyCycle = dutyCycle();
	my $opsPerSec = "0";
	if (exists $fileHashes[1]{"DCCommandsPerSecond_4m"}) { $opsPerSec = $fileHashes[1]{"DCCommandsPerSecond_4m"}; }
	if (exists $fileHashes[1]{"DCCommandsPerSecond_20m"}) { $opsPerSec = $fileHashes[1]{"DCCommandsPerSecond_20m"}; }
	if (exists $fileHashes[1]{"DCCommandsPerSecond_4h"}) { $opsPerSec = $fileHashes[1]{"DCCommandsPerSecond_4h"}; }
	my $str = sprintf("Duty Cycle:    %7.2f %%   Ops/second: %9.3f", $dutyCycle*100.0,$opsPerSec);
	print("$str\n\n");
}

#Print a formatted table listing statistics from 2 condor status data sets
sub generateTable()
{
	my @keys;
	my %notable_keys;
	my $sort_col = 0;
	my @cols = ("Runtime","Inst Avg","Avg","Count/Min","Count","Max","Item");
	my $size = @cols;

	#Verify arguments and apply column to sort by
	if(exists($opts{'c'}))
	{
		if($opts{'c'} <= ((scalar @cols))  && $opts{'c'} <= 7)
		{
			$sort_col = $opts{'c'};
		}
	}

	#If the -n command line argument has been given, display only the most
	#notable items
	if(exists($opts{'n'}))
	{
		#For each column except for "Item", add the top 5 items to a 
		#list to be printed and do not add an item if it already 
		#exists in the list.
		for(my $i = 0; $i < $size - 1; $i++)
		{
			my @keys = sort { $selectedItems{$b}[$i] <=> 
				$selectedItems{$a}[$i] } keys %selectedItems;
			for(my $k = 0; $k < 5; $k++)
			{
				if(!exists($notable_keys{$keys[$k]}))
				{
					$notable_keys{$keys[$k]} = "";
				}
			}
		}
		# Sorts hashtable by value in descending order
		@keys = sortColumns($sort_col,%notable_keys);
	}
	else
	{
		# Sorts hashtable by value in descending order
		@keys = sortColumns($sort_col,%selectedItems);
	}

	#Print column headers
	my $str = sprintf("%9s%10s%7s%13s%11s%9s  %s\n\n",$cols[0],$cols[1],
		$cols[2],$cols[3],$cols[4],$cols[5],$cols[6]);
	print($str);

	#Print all "interesting" items in the generated list
	foreach my $key (@keys)
	{
		$str = sprintf("%9.1f%10.3f%7.3f%13.2f%11.0f%9.3f  %s\n",
			$selectedItems{$key}[0],$selectedItems{$key}[1],
			$selectedItems{$key}[2],$selectedItems{$key}[3],
			$selectedItems{$key}[4],$selectedItems{$key}[5],
			$selectedItems{$key}[6]);
		print($str);
	}
	
}

getopts('nc:h', \%opts);

if(exists($opts{'h'}))
{
	usage();
}

loadFiles();

generateHash();

generateHealthTable();

generateTable();
