#!/usr/bin/env perl
# batch_test.pl - Condor Test Suite Batch Tester
#
# V2.0 / 2000-May-31 / Peter Couvares / pfc@cs.wisc.edu
# V2.1 / 2004-Apr-29 / Becky Gietzel / bgietzel@cs.wisc.edu  
# Dec 03 : Added an xml output format, triggered by a command line switch, -xml
# Feb 04 : now you don't need to list all compilers to run/skip for a test, just add the test 
# Feb 05 : bt Explicit removal of . from the path and explicit addition
#	of test and sub test directories(during use only) in.
# make sure tests are called testname.run for skip and run files
# can use multiple command line options now

#require 5.0;
use FileHandle;
use POSIX "sys_wait_h";
use Cwd;

# configuration options
$test_dir = ".";            # directory in which to find test programs
$test_timeout = 60*60*18;   # seconds until we decide a test is hung

# setup
STDOUT->autoflush();   # disable command buffering of stdout
STDERR->autoflush();   # disable command buffering of stderr
$num_success = 0;
$num_failed = 0;
$isXML = 0;  # are we running tests with XML output
my $BaseDir = getcwd();

# remove . from path
CleanFromPath(".");
# yet add in base dir of all tests and compiler directories
$ENV{PATH} = $ENV{PATH} . ":" . $Basedir;

#
# the args:
# -d[irectory <dir>: just test this directory
# -f[ile] <filename>: use this file as the list of tests to run
# -s[kip] <filename>: use this file as the list of tests to skip
# -t[estname] <test-name>: just run this test
#
while( $_ = shift( @ARGV ) ) {
  SWITCH: {
        if( /-d.*/ ) {
                push(@compilers, shift(@ARGV));
                next SWITCH;
        }
        if( /-f.*/ ) {
                $testfile = shift(@ARGV);
                next SWITCH;
        }
        if( /-s.*/ ) {
                $skipfile = shift(@ARGV);
                next SWITCH;
        }
        if( /-t.*/ ) {
                push(@testlist, shift(@ARGV));
                next SWITCH;
        }
        if( /-xml.*/ ) {
                $isXML = 1;
                print "xml output format selected\n";
                next SWITCH;
        }
  }
}


%test_suite = ();

# figure out what tests to try to run.  first, figure out what
# compilers we're trying to test.  if that was given on the command
# line, we just use that.  otherwise, we search for all subdirectories
# in the current directory that might be compiler subdirs...
if( ! @compilers ) {
    # find all compiler subdirectories in the test directory
    opendir( TEST_DIR, $test_dir ) || die "error opening \"$test_dir\": $!\n";
    @compilers = grep -d, readdir( TEST_DIR );
    # filter out . and ..
    @compilers = grep !/^\.\.?$/, @compilers;
    # get rid of CVS entry for testing - won't hurt to leave it in.
    @compilers = grep !/CVS/, @compilers;
    closedir TEST_DIR;
    die "error: no compiler subdirectories found\n" unless @compilers;
}


# now we find the tests we care about.
if( @testlist ) {
    # we were explicitly given a # list on the command-line
    foreach $test (@testlist) {
	if( ! ($test =~ /(.*)\.run$/) ) {
	    $test = "$test.run";
	}
	foreach $compiler (@compilers)
	{
	    push(@{$test_suite{"$compiler"}}, $test);
	}
    }
} elsif( $testfile ) {
    # if we were given a file, let's read it in and use it.
    print "found a runfile: $testfile\n";
    open(TESTFILE, $testfile) || die "Can't open $testfile\n";
    while( <TESTFILE> ) {
	chomp();
	#//($compiler, $test) = split('\/');
	$test = $_;
	if( ! ($test =~ /(.*)\.run$/) ) {
	    $test = "$test.run";
	}
	foreach $compiler (@compilers)
	{
	    push(@{$test_suite{"$compiler"}}, $test);
	}
    }
    close(TESTFILE);
} else {
    # we weren't given any specific tests or a test list, so we need to 
    # find all test programs (all files ending in .run) for each compiler
    foreach $compiler (@compilers) {
	opendir( COMPILER_DIR, $compiler )
	    || die "error opening \"$compiler\": $!\n";
	@{$test_suite{"$compiler"}} = grep /\.run$/, readdir( COMPILER_DIR );
	closedir COMPILER_DIR;
	die "error: no test programs found for $compiler\n" 
	    unless @{$test_suite{"$compiler"}};
    }
}


# if we were given a skip file, let's read it in and use it.
# remove any skipped tests from the test list  
if( $skipfile ) {
    print "found a skipfile: $skipfile \n";
    open(SKIPFILE, $skipfile) || die "Can't open $skipfile\n";
    while(<SKIPFILE>) {
	chomp();
	$test = $_;
	foreach $compiler (@compilers) {
	    # $skip_hash{"$compiler"}->{"$test"} = 1;
	    #@{$test_suite{"$compiler"}} = grep !/$test\.run/, @{$test_suite{"$compiler"}};
	    @{$test_suite{"$compiler"}} = grep !/$test/, @{$test_suite{"$compiler"}};
	} 
    }
    close(SKIPFILE);
}

# set up base directory for storing test results
if ($isXML){
      system ("mkdir -p $BaseDir/results");
      $ResultDir = "$BaseDir/results";
      open( XML, ">$ResultDir/ncondor_testsuite.xml" ) || die "error opening \"ncondor_testsuite.xml\": $!\n";
      print XML "<\?xml version=\"1.0\" \?>\n<test_suite>\n";
}

# Now we'll run each test.
foreach $compiler (@compilers)
{
    if ($isXML){
      system ("mkdir -p $ResultDir/$compiler");
    } 
    chdir $compiler || die "error switching to directory $compiler: $!\n";
	my $compilerdir = getcwd();
	# add in compiler dir to the current path
	$ENV{PATH} = $ENV{PATH} . ":" . $compilerdir;

    # fork a child to run each test program
    print "submitting $compiler tests";
    foreach $test_program (@{$test_suite{"$compiler"}})
    {
        print ".";

        #next if $skip_hash{$compiler}->{$test_program};
        $pid = fork();
        die "error calling fork(): $!\n" unless defined $pid;

        # if we're the parent, record the child's pid and go on
        if( $pid > 0 )
        {
            $test{$pid} = "$test_program";
            # wait a moment so as not to overwhelm condor_submit
            sleep 1;
            next;
        }

        # if we're the child, start test program

        # kill ourselves with SIGALARM if we run for more than 18 hours
        alarm $test_timeout;

        # exec the test itself
	exec( "perl $test_program > $test_program.out 2>&1" );
    }

    # wait for each test to finish and print outcome
    print "\n";
    while( $child = wait() ) {
        # if there are no more children, we're done
        last if $child == -1;

        # record the child's return status
        $status = $?;

        ($test_name) = $test{$child} =~ /(.*)\.run$/;

        if ($isXML){
          print XML "<test_result>\n<name>$compiler.$test_name</name>\n<description></description>\n";
          printf( "%-40s ", $test_name );
        } else {
          printf( "%-40s ", $test_name );
        }

        if( WIFEXITED( $status ) && WEXITSTATUS( $status ) == 0 )
        {
                if ($isXML){
                  print XML "<status>SUCCESS</status>\n";
                  print "succeeded\n";
                } else {
                  print "succeeded\n";
                }
                $num_success++;
                @successful_tests = (@successful_tests, "$compiler/$test_name");
        }
        else
        {
                $failure = `grep 'FAILURE' $test{$child}.out`;
                $failure =~ s/^.*FAILURE[: ]//;
                chomp $failure;
                $failure = "failed" if $failure =~ /^\s*$/;

                if ($isXML){
                  print XML "<status>FAILURE</status>\n";
                  print "$failure\n";
                } else {
                  print "$failure\n";
                }
                $num_failed++;
                @failed_tests = (@failed_tests, "$compiler/$test_name");
        }

        if ($isXML){
          print "Copying to $ResultDir/$compiler ...\n";
        
          # possibly specify exact files in future - for now bring back all 
          #system ("cp $test_name.run.out $ResultDir/$compiler/.");
          system ("cp $test_name.* $ResultDir/$compiler/.");

          # uncomment this when we decide to have each test tar itself up when it finishes
          #system ("cp $test_name.tar $ResultDir/$compiler/.");
       
          print XML "<data_file>$compiler.$test_name.run.out</data_file>\n<error>";
          print XML "</error>\n<output>";
          print XML "</output>\n</test_result>\n";
        }
    } # end while

    print "\n";
    chdir ".." || die "error switching to directory ..: $!\n";
	# remove compiler directory from path
	CleanFromPath("$compilerdir");
} # end foreach compiler dir

if ($isXML){
    print XML "</test_suite>\n";
    close (XML);
}


print "$num_success successful, $num_failed failed\n";

open( OUTF, ">successful_tests" )
    || die "error opening \"successful_tests\": $!\n";
for $test_name (@successful_tests)
{
    print OUTF "$test_name\n";
}
close OUTF;

open( OUTF, ">failed_tests" )
    || die "error opening \"failed_tests\": $!\n";
for $test_name (@failed_tests)
{
    print OUTF "$test_name\n";
}
close OUTF;

exit $num_failed;

sub CleanFromPath
{
	my $pulldir = shift;
	my $path = $ENV{PATH};
	my $newpath = "";
	my @pathcomponents = split /:/, $path;
	foreach my $spath ( @pathcomponents)
	{
		if($spath ne "$pulldir")
		{
			$newpath = $newpath . ":" . $spath;
		}
	}
}
