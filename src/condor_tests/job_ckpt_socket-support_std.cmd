universe        = standard
executable      = job_ckpt_socket-support_std.cndr.exe.$$(OPSYS).$$(ARCH)
output          = job_ckpt_socket-support_std.out
error           = job_ckpt_socket-support_std.err
log             = job_ckpt_socket-support_std.log
priority        = 9
arguments       = http://www.cs.wisc.edu/condor 40 -_condor_aggravate_bugs
notification    = never
queue 1
