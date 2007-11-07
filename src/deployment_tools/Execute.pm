##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

##*****************************************************************
## Functions to help execute other programs.
## Author: Joe Meehean
## Date: 6/7/05
##*****************************************************************

package Execute;
require      Exporter;
use File::Temp qw/ tempfile /;
use File::Spec;

our @ISA       = qw(Exporter);
our @EXPORT    = qw(ExecuteAndCapture);    # Symbols to be exported by default

our $VERSION   = 1.00;         # Version number


#***
# Constant Static Variables
#***
my $TEMPFILE_BASE_NAME = 'REDIRECTXXXX';
my $TEMPFILE_OUTPUT_SUFFIX = '.out';
my $TEMPFILE_ERROR_SUFFIX = '.err';

##*****************************************************************
## This function executes the given executable with the given 
## paramaters.  It differs from standard exec in that it captures
## both the input, output, and the return value in seperate 
## variables.  This function returns a hash that contains the 
## following entries:
##   EXIT_VALUE:  The exit value of the executable
##   SIGNAL_NR:  The signal number (if any) that stopped this 
##               executable
##   DUMPED_CORE:  True if the executable dumped core.
##   OUTPUT: A list of the output (each line as a separate entry)
##   ERRORS: A list of the errors (each line as a separate entry)
##*****************************************************************
sub ExecuteAndCapture{
    my($executable, @args) = @_;  #Name the parameters

    # maintain a pointer to the old streams
    local(*OLDSTDOUT, *OLDSTDERR);
    open OLDSTDOUT, ">&STDOUT"
	or die "Failed to capture old stdout: $!";
    open OLDSTDERR, ">&STDERR"
	or die "Failed to capture old stderr: $!";


    # Create a pair of temporary files for the redirected output
    my ($outFh, $outFile) = tempfile( $TEMPFILE_BASE_NAME, 
				      DIR => File::Spec->tmpdir(), 
				      SUFFIX => $TEMPFILE_OUTPUT_SUFFIX, 
				      UNLINK => 0);
    my ($errFh, $errFile) = tempfile( $TEMPFILE_BASE_NAME, 
				      DIR => File::Spec->tmpdir(), 
				      SUFFIX => $TEMPFILE_ERROR_SUFFIX, 
				      UNLINK => 0);

    # redirect the standard streams
    open STDOUT, ">$outFile"
	or die "Failed to redirect standard out: $!";

    open STDERR, ">$errFile"
	or die "Failed to redirect standard error: $!";

    select STDERR; $| = 1;      # make unbuffered
    select STDOUT; $| = 1;      # make unbuffered

    # execute the program
    my $systemReturn = system $executable, @args;

    # Put the streams back to normal
    open STDOUT,">&OLDSTDOUT"
	or die "Failed to reset stdout";
    open STDERR, ">&OLDSTDERR"
	or die "Failed to reset stderr";
    
    # Put the output into an array
    my @output = ();
    while(<$outFh>){
	chomp($_);
	if( $_ ){
	    push @output, $_;
	}
    }

    # Put the errors into an array
    my @errors = ();
    while(<$errFh>){
	chomp($_);
	if( $_ ){
	    push @errors, $_;
	}
    }

    # Remove the temporary files
    unlink $outFile, $errFile
	or warn "WARNING: Failed to remove temporary output/error redirection files";

    #Build the result
    my %result = (
	EXIT_VALUE => $systemReturn >> 8,
	SIGNAL_NR => $systemReturn & 127,
	DUMPED_CORE => $systemReturn & 128,
	OUTPUT => [@output],
	ERRORS => [@errors],
	);
	
    return \%result;    
}

# loading module was a success
1;
