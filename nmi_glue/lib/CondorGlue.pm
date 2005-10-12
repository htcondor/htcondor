######################################################################
# Perl module to handle a lot of shared functionality for the Condor
# build and test "glue" scripts for use with the NMI-NWO framework.
#
# Originally written by Derek Wright <wright@cs.wisc.edu> 2004-12-30
# $Id: CondorGlue.pm,v 1.1.2.34 2005-10-12 07:56:45 wright Exp $
#
######################################################################

package CondorGlue;


sub parseTag
{
    my $tag = shift;
    my $desc;
    my @vers;

    $tag =~ s/BUILD-//;
    my $desc = $tag;
    $tag =~ s/-branch-.*//;
    $tag =~ s/V//;
    if( $tag =~ /(\d+)(\D*)_(\d+)(\D*)_?(\d+)?(\D*)/ ) {
        $vers[0] = $1;
        $vers[1] = $3;
        if( $5 ) {
            $vers[2] = $5;
        } else {
            $vers[2] = "x";
        }
    }
    return ( $desc, @vers );
}


sub run
{
    my ($cmd, $fatal) = @_;
    my $ret;
    my $output = "";

    # if not specified, the command is fatal
    if (!defined($fatal) or ($fatal != 0 and $fatal != 1)) {
        $fatal = 1;
    }

    print "\n";
    print "#  $cmd\n";

    # run the command
    system("($cmd)  </dev/null 2>&1");
    $ret = $? / 256;

    # should we die here?
    if ($fatal && $ret != 0) {
        print "\n";
        print "FAILED COMMAND: $cmd\n";
        print "RETURN VALUE:  $ret\n";
        print "\n";
        exit(1);
    }

    # return the commands return value
    return $ret;
}


1;

