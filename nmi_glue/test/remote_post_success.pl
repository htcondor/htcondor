#!/usr/bin/perl

######################################################################
# $Id: remote_post_success.pl,v 1.1.2.4 2004-06-24 21:35:35 wright Exp $
# post script for a successful Condor testsuite run   
######################################################################

my $BaseDir = $ENV{BASE_DIR};

######################################################################
# tar up test results
######################################################################

#note - tar with 2 options fails on a lot of the platforms (-cr)  
# we should be using gnu tar for all - make sure path is correct
$results = "results.tar.gz";
chdir("$BaseDir") || die "Can't chdir($BaseDir): $!\n";

my $etc_dir = "$BaseDir/results/etc";
my $log_dir = "$BaseDir/results/log";

if( ! -d $etc_dir ) {
    mkdir( "$etc_dir" ) || die "Can't mkdir($etc_dir): $!\n";
}
if( ! -d $log_dir ) {
    mkdir( "$log_dir" ) || die "Can't mkdir($log_dir): $!\n";
}

print "Copying config files to $etc_dir\n";
system( "cp $BaseDir/condor/etc/condor_config $etc_dir" );
if( $? >> 8 ) {
    die "Can't copy $BaseDir/condor/etc/condor_config to $etc_dir: $!\n";
}
system( "cp $BaseDir/local/condor_config.local $etc_dir" );
if( $? >> 8 ) {
    die "Can't copy $BaseDir/local/condor_config.local to $etc_dir: $!\n";
}

print "Copying log files to $log_dir\n";
system( "cp $BaseDir/local/log/* $log_dir" );
if( $? >> 8 ) {
    die "Can't copy $BaseDir/local/log/* to $log_dir: $!\n";
}

print "Tarring up debug stuff - condor logs, config files and daemons\n";
system( "tar zcf $BaseDir/$results results" );
if( $? >> 8 ) {
    die "Can't tar zcf $BaseDir/$results results\n";
}

