universe   = parallel
executable = ./x_paralleljob.pl
output = job_core_basic_par6968.out$(NODE)
error = job_core_basic_par6968.err$(NODE)
log = job_core_basic_par6968.log
arguments = $(NODE) 4  6968 
Notification = NEVER
machine_count = 4
queue

