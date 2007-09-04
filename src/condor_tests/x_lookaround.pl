#! /usr/bin/env perl

my $request = $ARGV[0];
($name, $pass, $uid, $gid, $quota, $comment, $gcos, $dir, $shell, $expire) = getpwuid($<);
print "My name is \"$name\"\n";

print "This is the time: ";
system("date");

print "This is the current directory: ";
system("pwd");

exit(0);
