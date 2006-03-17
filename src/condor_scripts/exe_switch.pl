#!/usr/bin/env perl

#eval 'use warnings;' unless $platform =~ /hpux/;
#use strict; 

use Cwd;

my $base = getcwd();
#print "Basedir is $base\n";

my $execbase = "";

if($base =~ /(.*)\/condor\/execute\/.*$/) {
	$execbase = $1;
}
my $win_perl_wrapper, $win_sh_wrapper;
my $cygwinperl, $cygwinsh;
my $exe = shift @ARGV;


################ HACK !!!!!!!!!!!!!!!!!!!!!



$win_perl_wrapper = "$execbase/local/bin/win32toperl.bat";
$win_sh_wrapper = "$execbase/local/bin/win32tosh.bat";
$cygwinperl = `cygpath -u $win_perl_wrapper`;
$cygwinsh = `cygpath -u $win_sh_wrapper`;

#print "Before exec $win_perl_wrapper and $win_sh_wrapper are set...\n";
#foreach $file ( @ARGV ) {
#print " $file";
#}
#print "\n";

my $rval = 0;

# which if any wrapper?
my $answer = check_which_wrapper($exe);
if($answer eq "perl") {
	#print "Perl\n";
	$rval = system_wrapper( $win_perl_wrapper, $exe, @ARGV );
} elsif($answer eq "sh") {
	#print "sh\n";
	$rval = system_wrapper( $win_sh_wrapper, $exe, @ARGV );
} else {
	#print "other\n";
	$rval = system_wrapper( $exe, @ARGV );
}
if( $rval == 0) {
	#sweet...
	exit(0);
}

print STDERR "Execution failed system return <<$rval>>\n";
exit($rval);

##########


# returns exit code if child exited
# returns negative signal number if child died by signal
# returns -32 if child failed to execute at all
# prints debugging to STDERR
sub system_wrapper 
{
    my @args = @_;

    #foreach $arg (@args)
    #{
    #print "System_wrapper: $arg\n";
    #}
    #print "\n";

    my $rc = system( @args );

    if( $rc == -1 ) {
	print STDERR "error executing $args[0]: $!\n";
	return -32;
    }

    my $exit_value  = $rc >> 8;
    my $signal_num  = $rc & 127;
    my $dumped_core = $rc & 128;

    if( $signal_num  ) {
	print STDERR "error: $args[0] died by signal $signal_num\n";
	return (0 - $signal_num);
    }

    return $exit_value;
}

# On windows we have to be careful and get the correct environment
# without the normal automatic execing we are used to.
sub check_which_wrapper
{
    my $script = shift;
    my $line = "";
    open(FILE,"<$script") || die "Can not open $script for reading: $!\n";
    while(<FILE>)
    {
        chomp();
        $line = $_;
        if( $line =~ /^#!\s*(\/bin\/sh).*/) {
            return("sh");
        }
        elsif( $line =~ /^#!\s*(\/usr\/bin\/env\s+perl).*/) {
            return("perl");
        }
        else {
            return("none");
        }
    }
    close(FILE);
}
