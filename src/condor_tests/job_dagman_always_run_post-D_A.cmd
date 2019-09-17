executable = $CHOICE(IsWindows,/bin/echo,appendmsg)
output = job_dagman_always_run_post-D_A.out
error = job_dagman_always_run_post-D_A.err
arguments = Not OK that we are running
queue
