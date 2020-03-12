universe = scheduler
executable = ./job_dagman_rm-nodeA.pl
arguments = "job_dagman_rm-NodeA-job"
log = job_dagman_rm-nodeA.log
transfer_input_files = CondorTest.pm, Condor.pm, CondorUtils.pm, CondorPersonal.pm
output = job_dagman_rm-nodeA.out
error = job_dagman_rm-nodeA.err
notification = NEVER
queue
