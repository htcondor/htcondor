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
queue
