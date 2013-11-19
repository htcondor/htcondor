executable   = ./job_dagman_vars-retry.pl
arguments    = $(nodename) $(noderetry) $$([DagNodeRetry])
# Note: this needs to be vanilla universe for the $$([DagNodeRetry])
# expansion to work above.
universe     = vanilla
output       = job_dagman_vars-$(nodename).out
error        = job_dagman_vars-$(nodename).err
log          = job_dagman_vars.log
Notification = NEVER
queue
