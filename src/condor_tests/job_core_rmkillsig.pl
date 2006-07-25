#! /usr/bin/env perl

kill 6, $$;
sleep 6 # just so it won't quit before the signal hits
