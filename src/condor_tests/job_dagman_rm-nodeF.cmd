universe = scheduler
executable = ./job_dagman_rm-nodeF.pl
arguments = "job_dagman_rm-NodeF-job $(DAGManJobId)"
transfer_input_files = CondorTest.pm, Condor.pm, CondorUtils.pm, CondorPersonal.pm
log = job_dagman_rm-nodeF.log
output = job_dagman_rm-nodeF.out
error = job_dagman_rm-nodeF.err
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
notification = NEVER
queue
