universe = vanilla 
executable = x_sleep.pl
output = job_core_periodicremove_van.out
error = job_core_periodicremove_van.err
log = job_core_periodicremove_van.log
periodic_remove = ((WallClockCheckpoint > 60) =?= TRUE)
arguments = 0
notification = never
queue
