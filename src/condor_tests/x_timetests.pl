#! /usr/bin/env perl

use CondorTest;

$timestamp  = time();

print "$timestamp\n";

sleep(30);

$greater = CondorTest::CheckTriggerTime($timestamp, 
