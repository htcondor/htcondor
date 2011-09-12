####################
##
## Test Condor command file
##
####################

universe = standard
executable      = job_ckpt_lfs_std.cndr.exe.$$(OPSYS).$$(ARCH)
error           = job_ckpt_lfs_std.err
output          = job_ckpt_lfs_std.out
log		= job_ckpt_lfs_std.log

coresize = -1

arguments = 5 -_condor_aggravate_bugs

buffer_size = 10000000
buffer_block_size = 256000

priority = 0

Notification = Never

queue
