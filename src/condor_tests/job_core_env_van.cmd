universe = vanilla
executable = /usr/bin/env
log = job_core_env_van.log
output = job_core_env_van.out
error = job_core_env_van.err
environment = env01="a;env02=&;env03=>/dev/null;env04=\" | echo;env05=-a environment = bad=true;env06=-----;env07=`foo`;env08=\n;env09=!@#$%^&*()><-_+=|;env10=;env11=	;
getenv=true
Notification = NEVER
queue


