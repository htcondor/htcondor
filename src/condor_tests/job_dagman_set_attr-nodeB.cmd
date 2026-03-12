# Note:  Maybe change this to local universe once gittrac #5299 is
# fixed.  wenger 2015-12-11
universe	= scheduler
executable	= ./job_dagman_set_attr-node.pl
arguments	= $(DAGManJobId)
output		= job_dagman_set_attr-nodeB.out
error		= job_dagman_set_attr-nodeB.err
getenv = CONDOR*,PATH
queue
