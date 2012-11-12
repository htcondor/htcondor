Executable           	= /bin/echo
Arguments            	= "Notification = $$([ifThenElse(JobNotification==0,""Never"",ifThenElse(JobNotification==1,""Always"",ifThenElse(JobNotification==2,""Complete"",ifThenElse(JobNotification==3,""Error"",""Unknown""))))])"
Universe             	= vanilla
log                  	= job_dagman_suppress_notification-cmd_line-node.$(cluster).log
Notification         	= Always
getenv               	= true
output               	= job_dagman_suppress_notification-cmd_line-node.$(cluster).out
error                	= job_dagman_suppress_notification-cmd_line-node.$(cluster).err
Queue

