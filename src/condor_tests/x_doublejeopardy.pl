#! /usr/bin/env perl
BEGIN {$^W=1}  #warnings enabled

kill 9, $$;
sleep 6 # just so it won't quit before the signal hits
