##
## Skeleton for the CronTab Tests
## The perl script will insert what universe the job will be
## This file SHOULD NOT contain "Queue" at the end
##
Executable		= ./x_time.pl
Notification	= NEVER
Arguments		= 20

##
## The job will run every 3 minutes
##
CronHour		= *
CronMinute		= */2
CronDayOfMonth	= *
CronMonth		= *
CronDayOfWeek	= *

## 
## Preparation Time
##
CronPrepTime	= 20


#
# Deferral window
# Give it 5 seconds leeway to run, so if it gets there late,it
# is still ok
deferral_window = 5

##
## We only want to run it 2 times
##
OnExitRemove	= NumJobStarts >= 1
