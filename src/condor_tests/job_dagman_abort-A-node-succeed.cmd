executable     	= $CHOICE(IsWindows,/bin/echo,appendmsg)
arguments		= job_dagman_abort-A-node.cmd $(nodename) OK
universe       	= local
log			= job_dagman_abort-A-node.log
notification   	= NEVER
getenv         	= true
output			= job_dagman_abort-A-node.$(nodename).out
error			= job_dagman_abort-A-node.err
queue

