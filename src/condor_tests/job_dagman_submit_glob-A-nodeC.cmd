universe = vanilla
executable = ./x_cat.pl
output = job_dagman_submit_glob-A-nodeC.$(process).out
queue arguments from (
	job_dagman_submit_glob-A-1.dat job_dagman_submit_glob-A-3.dat
	job_dagman_submit_glob-A-2.dat
)
