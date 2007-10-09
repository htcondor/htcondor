####################
##
## Test Condor command file
##
####################

universe        = standard
executable      = job_ckpt_env_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_ckpt_env_std.err
output          = job_ckpt_env_std.out
log             = job_ckpt_env_std.log
# These cmdline argumnets MUST be distinct
arguments       = foo bar glarch -_condor_aggravate_bugs
# The env var assignments MUST be distinct
environment     = alpha=a;bravo=b;charlie=c;
priority        = 0
Notification    = never
queue
