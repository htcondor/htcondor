universe     = standard
executable   = job_rsc_hello_std.cndr.exe.$$(OPSYS).$$(ARCH)
input        = x_hello.in
output       = job_rsc_hello_std.out
error        = job_rsc_hello_std.err
log          = job_rsc_hello_std.log
arguments    = -_condor_aggravate_bugs
priority     = 0
notification = Never
queue
