executable	= ./job_dagman_halt-A-node.pl
arguments	= $(args)
universe	= scheduler
output		= job_dagman_halt-A-$(nodename).out
error		= job_dagman_halt-A-$(nodename).err
notification = NEVER
queue
