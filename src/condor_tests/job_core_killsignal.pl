#! /usr/bin/env perl

kill 4, $$;
sleep 6 # just so it won't quit before the signal hits
