universe = vanilla
executable = ./x_cat.pl
output = job_dagman_submit_glob-A-nodeA.$(process).out
queue 1 input in job_dagman_submit_glob-A-3.dat, \
	job_dagman_submit_glob-A-2.dat, \
	job_dagman_submit_glob-A-1.dat
