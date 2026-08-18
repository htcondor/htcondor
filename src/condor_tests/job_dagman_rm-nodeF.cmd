universe = scheduler
executable = ./job_dagman_rm-nodeF.pl
arguments = "job_dagman_rm-NodeF-job $(DAGManJobId)"
transfer_input_files = CondorTest.pm, Condor.pm, CondorUtils.pm, CondorPersonal.pm
log = job_dagman_rm-nodeF.log
output = job_dagman_rm-nodeF.out
error = job_dagman_rm-nodeF.err
getenv = CONDOR*,PATH
notification = NEVER
queue
