####################
##
## Test Condor command file
##
####################

executable      = env.remote
output          = env.out
error           = env.err
arguments	= foo bar glarch
environment	= alpha=a;bravo=b;charlie=c
queue
