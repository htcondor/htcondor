#TEMPTEMP -- do we need scheduler?
universe	= scheduler
executable	= ./job_dagman_set_attr-node.pl
arguments	= $(DAGManJobId)
output		= job_dagman_set_attr-nodeB.out
error		= job_dagman_set_attr-nodeB.err
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv		= true
queue
