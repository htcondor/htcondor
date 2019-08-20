executable   = job_dagman_jobstate_log-nodeB.pl
arguments    = $(nodename)
universe     = local
output       = $(nodename).out
error        = $(nodename).err
log          = job_dagman_jobstate_log-nodeB.log
Notification = NEVER
+job_tag_name = "+job_tag_value"
+job_tag_value = "viz"
queue 2
