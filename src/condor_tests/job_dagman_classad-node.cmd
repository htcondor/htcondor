executable   = job_dagman_classad-node.pl
arguments    = $(DAGManJobId) $(NODE)
universe     = local
output       = job_dagman_classad-node$(NODE).out
error        = job_dagman_classad-node$(NODE).err
# Note: we need getenv = true for the node job to talk to the schedd of
# the personal condor that's running the test.
getenv       = true
Notification = NEVER
queue
