####################
##
## Test Condor command file
##
####################

executable      = env.remote
output          = env.out
error           = env.err
log		= env.log
arguments	= foo bar glarch
environment	= alpha=a;bravo=b;charlie=c
queue
