####################
##
## Test Condor command file
##
####################

# You can only submit one executable at a time
executable	= loop.remote

# However, you can have Condor run each executable many times, each
# with different arguments and different input and output files.  This
# is called a "job cluster".  Each cluster has a "cluster ID" and
# within each cluster, each job has a "process ID".  The macro
# $(Process) corresponds to the process ID of each job in the cluster,
# and can be used to simplify the naming of output files, like this:

output		= loop.$(Process).out
error		= loop.$(Process).err

# For all of the individual jobs submitted in this cluster, stdout 
# will be written to loop.#.out and stderr to loop.#.err, where # is
# an integer specifying the job ID within the cluster.

# By specifying a "log" setting for your job, it will create a "user
# log", with details on important events in the life of the job, such
# as when it was submitted, where it runs, when it checkpoints and
# when it completes.  One log can be shared between all jobs in a
# cluster.  You could also specify a different log for each process,
# using the $(Process) macro like output and error above.

log		= loop.log

# The "arguments" line is used to specify command-line arguments for
# your job.  So, if you would normally run "loop 1000" from your
# shell, you specify:
#	executable = loop
#	arguments = 1000
# in this file and Condor will pass these arguments on to your job.  

arguments	= 200

# Once all the parameters have been specified for your job, you can
# actually submit the job using "queue"   If you have a number after
# the queue statement, that many jobs will be queued at once.

queue 2

# This will submit two versions of the loop.remote executable with the
# same arguments to Condor.  Notice that stdout and stderr will be
# different for each one, since they use the $(Process) macro.


# Now, you can change parameters like arguments, in, out and error
# files, etc, and queue more jobs to this cluster

arguments	= 300

queue 2

arguments	= 500
output		= loop.last.out
error		= loop.last.err
queue

