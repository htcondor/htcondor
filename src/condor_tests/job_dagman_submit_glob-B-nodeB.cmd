universe = vanilla
executable = ./x_cat.pl
arguments = $(filename)
output = job_dagman_submit_glob-B-nodeB.$(process).out
queue filename matching files job_dagman_submit_glob-B-*.dat
