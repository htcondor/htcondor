universe = vanilla
executable = job_starter_script-A-job.pl
arguments = "job""a1 'job a2' 'jo''ba3'"
output = job_starter_script-A.out
error = job_starter_script-A.err
log = job_starter_script-A.log

should_transfer_files = YES
when_to_transfer_output = ON_EXIT
transfer_input_files = job_starter_script-A-pre.pl, job_starter_script-A-post.pl

+PreCmd = "job_starter_script-A-pre.pl"
+PreArgs = "pre\"a1 pre'a2"

+PostCmd = "job_starter_script-A-post.pl"
+PostArguments = "post\"a1 'post''a 2'"

queue
