universe   = parallel
executable = ./x_paralleljob.pl
output = job_core_basic_par21198.out$(NODE)
error = job_core_basic_par21198.err$(NODE)
log = job_core_basic_par21198.log
arguments = $(NODE) 4  21198 
Notification = NEVER
machine_count = 4
queue

