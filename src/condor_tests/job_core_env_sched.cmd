universe = scheduler
executable = /usr/bin/env
log = job_core_env_sched.log
output = job_core_env_sched.out
error = job_core_env_sched.err
environment = env01="a;env02=&;env03=>/dev/null;env04=\" | echo;env05=-a environment = bad=true;env06=-----;env07=`foo`;env08=\n;env09=!@#$%^&*()><-_+=|;env10=    ;env11=;
getenv=true
Notification = NEVER
queue


