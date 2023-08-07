executable   = job_dagman_jobstate_log-nodeA.pl
arguments    = $(ProcId)
universe     = local
output       = $(nodename).out
error        = $(nodename).err
log          = job_dagman_jobstate_log-nodeA.log
Notification = NEVER
+pegasus_site = "local"
queue 2
