#! /usr/bin/env perl

$nodename = shift;
$parentid = shift;

open (OUTPUT, "condor_hold $parentid 2>&1 |") or die "Can't fork: $!";
while (<OUTPUT>) {
	print "$_";
}
close (OUTPUT) or die "Condor_hold failed: $?";

sleep 60;

open (OUTPUT, "condor_release $parentid 2>&1 |") or die "Can't fork: $!";
while (<OUTPUT>) {
	print "$_";
}
close (OUTPUT) or die "Condor_release failed: $?";

print "$nodename succeeded\n";
