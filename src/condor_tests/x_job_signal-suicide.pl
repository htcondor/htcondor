#! /usr/bin/env perl
BEGIN {$^W=1}  #warnings enabled

# I'm adding 1 below because this will be called with an argument of
# condor job ids, which start at 0. A signal of 0 will not kill a
# process.
kill 1+$ARGV[0], $$;
sleep 2 # just so it won't quit before the signal hits
