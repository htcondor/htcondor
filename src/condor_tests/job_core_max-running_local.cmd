##
## Max Local Universe Job Submission
## We set the requirements so that only one local job is allowed to
## to run at a time. We pass the perl script the # of seconds it
## should sleep after it prints out the time. This way we can check
## to make sure that each time is that many seconds after each other
## If two jobs have times that are less than this time then we know
## that the jobs weren't blocked properly
##
Universe				= local
Executable				= ./x_time.pl
Notification			= NEVER
Log                     = job_core_max-running_local.log
Error                   = job_core_max-running_local.error
ShouldTransferFiles		= yes
WhenToTransferOutput	= ON_EXIT_OR_EVICT
Requirements            = TotalLocalJobsRunning < 1
Arguments				= 20

##
## Queue a bunch of jobs 
##
BaseFile				= job_core_max-running_local.out
Output                  = $(BaseFile).$(Cluster).$(Process)
Queue
Output                  = $(BaseFile).$(Cluster).$(Process)
Queue
Output                  = $(BaseFile).$(Cluster).$(Process)
Queue
Output                  = $(BaseFile).$(Cluster).$(Process)
Queue
Output                  = $(BaseFile).$(Cluster).$(Process)
Queue
