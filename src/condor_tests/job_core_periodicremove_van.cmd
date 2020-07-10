universe = vanilla 
executable = x_sleep.pl
output = job_core_periodicremove_van.out
error = job_core_periodicremove_van.err
log = job_core_periodicremove_van.log
periodic_remove = (((JobStatus == 2) && (JobUniverse == 5) && ((time() - JobCurrentStartDate) > 60)) =?= TRUE) 
arguments = 3600
notification = never
queue
