####################
##
## Test Condor command file
##
####################

universe	= vanilla
executable	= sh_loop
output		= sh_loop.out
error		= sh_loop.err
log		= sh_loop.log
arguments	= 60
should_transfer_files = IF_NEEDED
when_to_transfer_output = ON_EXIT
queue
