accounting_group = fw
arguments =  0
concurrency_limits = XSW
copy_to_spool = false
coresize = 1000000
cron_day_of_the_month = 5
cron_day_of_the_week = 2
cron_hour = 4
cron_minute = 5
cron_month = 5
cron_prep_time = 60
cron_window = 60
dagman_log = dagman_log
deferral_time = 100
deferral_window = 1000
description = "my test job"
dont_encrypt_input_files = one,two
dont_encrypt_output_files = three,four
encrypt_input_files = five,six
encrypt_output_files = seven,eight
env = MEMORY=$$(memory)|MACHINE=$$(machine)|WONTBE=$$(wontbethere:/be/here/now)
environment = "food=good school=hard"
error = job_core_initialdir_van.err
executable = x_sleep.pl
getenv=true
initialdir = .
job_lease_duration = 3600
job_max_vacate_time = 120
kill_sig = 3
log = job_core_initialdir_van.log
log_xml = false
match_list_length = 5
max_job_retirement_time = (60 * 60 * 48)
max_transfer_input_mb = 1000
max_transfer_output_mb = 1000
nice_user = false
Notification = NEVER
output = job_core_initialdir_van.out
priority = 10
rank = Memory
request_Boats = 1
request_cpus = 1
request_memory = 25
request_disk = 10000
requirements = (Target.name =?= bonanza)
submit_event_notes = "just tracking time"
stack_size = 2048
universe   = vanilla
want_gracefule_removal = true
foo			= Foo1
foo			= Foo2
bar			= Bar
bar			= foo
foo			= bar
foobar		= $(bar)$(foo)
+foo		= "bar"
+bar		= "foo"
+last		= "first"
+done		= FALSE
queue


