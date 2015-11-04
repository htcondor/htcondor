universe = vanilla
executable = ./x_cat.pl
output = job_dagman_submit_glob-B-nodeC.$(process).out
queue arguments from (
	job_dagman_submit_glob-B-1.dat job_dagman_submit_glob-B-3.dat
	job_dagman_submit_glob-B-2.dat
)
