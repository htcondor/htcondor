universe = vanilla
executable = ./x_cat.pl
output = job_dagman_submit_glob-B-nodeA.$(process).out
queue 1 input in job_dagman_submit_glob-B-1.dat, \
	job_dagman_submit_glob-B-2.dat, \
	job_dagman_submit_glob-B-3.dat
