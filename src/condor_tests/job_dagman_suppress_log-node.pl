#! /usr/bin/env perl

$id = $ARGV[0];

$user_log = `condor_q $id -af UserLog`;
chomp $user_log;
print "UserLog <$user_log>\n";
