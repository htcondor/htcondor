# Note:  Maybe change this to local universe once gittrac #5299 is
# fixed.  wenger 2015-12-11
universe	= scheduler
executable	= ./job_dagman_set_attr-node.pl
arguments	= $(DAGManJobId)
output		= job_dagman_set_attr-nodeA.out
error		= job_dagman_set_attr-nodeA.err
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv		= true
queue
