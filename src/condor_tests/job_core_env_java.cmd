universe = java
executable =  x_java_Property.class
log = job_core_env_java.log
output = job_core_env_java.out
error = job_core_env_java.err
environment = env01="a;env02=&;env03=>/dev/null;env04=\" | echo;env05=-a environment = bad=true;env06=-----;env07=`foo`;env08=\n;env09=!@#$%^&*()><-_+=|;env10=;env11=	;
getenv=true
arguments = x_java_Property env01 env02 env03
Notification = NEVER
queue


