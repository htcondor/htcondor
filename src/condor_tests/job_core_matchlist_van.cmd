universe   = vanilla
executable = job_core_matchlist.pl
output = job_core_matchlist_van.out
error = job_core_matchlist_van.err
log = job_core_matchlist_van.log
match_list_length = 5
#requirements = (TARGET.Name =!= LastMatchName1 || LastMatchName1 =?= UNDEFINED )
rank = ((TARGET.Name != LastMatchName0) * 100) 
on_exit_remove = (CurrentTime - QDate) > 10000
should_transfer_files = ALWAYS
transfer_input_files = CondorTest.pm
when_to_transfer_output = ON_EXIT_OR_EVICT
environment = MYNAME = $$(name)
input = job_core_matchlist.data
arguments = 10 job_core_matchlist.data $$(name)
Notification = NEVER
queue

