universe   = java
executable = x_java_CondorEcho.class
output = job_core_macros_java.out
error = job_core_macros_java.err
log = job_core_macros_java.log
# test late bindings
foo			= Foo1
foo			= Foo2
bar			= Bar
bar			= foo
foo			= bar
foobar		= $(bar)$(foo)
# with only one job fired up this should be process 0
process 	= $(Process)
# what cluster are we ?
mycluster 	= $(Cluster)
# what about our existing path
mypath = $ENV(PATH)
# out put file has herader line making these $lines[1], $lines[2] ...
arguments = x_java_CondorEcho $(foobar) $(process) $(mycluster) $(mypath) $RANDOM_CHOICE(2,3,4,5) $(hostname) $(full_hostname) $(tilde) $(subsystem) $(DOLLAR) $(ip_address) $(pid) $(ppid)  $(real_uid) $(real_gid)

Notification = NEVER
queue

