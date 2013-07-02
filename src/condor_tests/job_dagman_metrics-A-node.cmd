executable	= job_dagman_metrics-A-node.pl
arguments	= "$(nodename)"
universe	= scheduler
notification	= NEVER
output		= job_dagman_metrics-A-$(nodename).out
error		= job_dagman_metrics-A-$(nodename).err
queue
