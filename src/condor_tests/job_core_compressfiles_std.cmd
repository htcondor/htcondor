universe   = standard
executable = job_core_compressfiles.cndr.exe.$$(OPSYS).$$(ARCH)
output = job_core_compressfiles_std.out
error = job_core_compressfiles_std.err
log = job_core_compressfiles_std.log
compress_files = job_core_compressfiles_std.datain.gz job_core_compressfiles_std.dataout.gz
arguments = job_core_compressfiles_std.datain.gz job_core_compressfiles_std.dataout.gz
Notification = NEVER
queue

