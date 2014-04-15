executable   = ./job_dagman_vars-node.pl
arguments    = $(nodename) $$([DAGParentNodeNames]) $$([A]) $(noderetry)
# Note: this needs to be vanilla universe for the $$([DAGParentNodeNames])
# expansion to work above.
universe     = vanilla
output       = job_dagman_vars-$(nodename).out
error        = job_dagman_vars-$(nodename).err
log          = job_dagman_vars.log
Notification = NEVER
queue
