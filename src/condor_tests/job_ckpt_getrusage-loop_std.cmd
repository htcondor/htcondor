universe     = standard
executable   = job_ckpt_getrusage-loop_std.cndr.exe.$$(OPSYS).$$(ARCH)
output       = job_ckpt_getrusage-loop_std.out
error        = job_ckpt_getrusage-loop_std.err
log          = job_ckpt_getrusage-loop_std.log
arguments    = 200 25 -_condor_aggravate_bugs
priority     = 0
notification = Never
queue
