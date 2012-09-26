#!/usr/bin/env perl

use strict;
use warnings;

use CondorTest;

package DagmanTest;

my $abnormal = sub {
    die "Want to see only submit, execute and successful completion\n";
};

my $aborted = sub {
    die "Abort event NOT expected\n";
};

my $held = sub {
    die "Held event NOT expected\n";
};

my $executed = sub {
    my %info = @_;
    CondorTest::debug("Dag is running.  Cluster: $info{cluster}\n",1);
};

my $submitted = sub {
    CondorTest::debug("submitted: This test will see submit, executing and successful completion\n",1);
};

sub register_event_handlers {
    my ($testname) = @_;
    CondorTest::RegisterExecute($testname, $executed);
    CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
    CondorTest::RegisterAbort( $testname, $aborted );
    CondorTest::RegisterHold( $testname, $held );
    CondorTest::RegisterSubmit( $testname, $submitted );
}
