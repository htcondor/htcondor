universe = vanilla
executable = ./x_cat.pl
arguments = $(filename)
output = job_dagman_submit_glob-A-nodeB.$(process).out
queue filename matching files job_dagman_submit_glob-A-*.dat
