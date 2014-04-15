#! /usr/bin/env perl

system("echo '  DAG_STATUS=$ARGV[2]'");
system("echo '  FAILED_COUNT=$ARGV[3]'");

system("echo '$ARGV[0]'");

exit($ARGV[1]);
