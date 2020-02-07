#! /bin/bash
##**************************************************************
##
## Copyright (C) 1990-2013, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

# Original Author Michael Hanke <michael.hanke@gmail.com> (v0.1)
# Updated by Samuel Friedman (03/13)

#
# To generate a manpage, simply run through help2man.

# play safe
set -u
set -e

# define variable defaults
scriptname="condor_qsub"
version="0.2"
error_file=""
accounting_group=""
binary_flag=0
checkpoint_interval=""
cluster_id=0
cwd_flag=0
deferral_date_time=""
environment=""
hold_jid=""
hold_flag=0
init_dir=""
input_file=""
interactive_option=""
mail_config=""
mail_user=""
merge_streams=""
job_name=""
no_output_batch_id_flag=0
number_specs=""
output_file=""
print_flag=0
priority=0
requirements_string=""
rerun_flag=""
resource_list=""
shell=${SHELL:-/bin/sh}
sync_flag=0
taskid_first=""
taskid_last=""
taskid_step=""
test_qsub_flag=0
total_procs=1
transfer_files=""
qsub_cmd=""
qsub_args=""
qsub_file=""
queue_name=""
export_env_flag=0
condor_keep_files=0

print_description()
{
cat << EOT
Minimalistic emulation of SGE's qsub for Condor.

EOT
}

print_version()
{
cat << EOT
$scriptname $version

This script is released under the Apache V2.0 License. It was
written by Michael Hanke <michael.hanke@gmail.com> and 
Samuel Friedman <samf@cs.wisc.edu>

EOT
}

print_help()
{
cat << EOT
Usage:  $scriptname [OPTIONS] [<command> [<command_args>]]

The primary purpose of this emulation is to allow PBS/Torque-style and 
SGE-style submission of dependent jobs without the need to specify the full
dependency graph at submission time. This implementation is neither as efficient
as HTCondor's DAGMan, nor as functional as SGE's qsub/qalter. It merely serves 
as a minimal adaptor to be able to use software original written to interact 
with PBS/Torque/SGE in an HTCondor pool. In Version 0.2, many additional 
features have been added.

In general $scriptname behaves just like qsub. However, only a fraction of the
original functionality is available (~1/3). The following list of options only
describes the differences in the behavior of PBS/Torque/SGE's qsub and this 
emulation. Qsub options not listed here are not supported.

For a full listing of options, please see
POSIX: http://pubs.opengroup.org/onlinepubs/9699919799/utilities/qsub.html
SGE: http://gridscheduler.sourceforge.net/htmlman/htmlman1/qsub.html
PBS/Torque: http://docs.adaptivecomputing.com/torque/4-1-3/Content/topics/commands/qsub.htm

Options:

-a date_time
  We follow the PBS/Torque implementation of this option. The date_time argument
  is in the form:

  [[[[CC]YY]MM]DD]hhmm[.SS]

  where MM and DD are potentially optional. SGE does not permit MM or DD to be 
  optional.

-A account_string
  Creates an attribute specifying what accounting group should list the time 
  associated with this job. Unlike SGE, we do not give a default group of "sge".

-b <y|n>
  If 'y', command and arguments given on the command line are wrapped into a
  shell script which is then submitted to HTCondor. SGE only. PBS/Torque version
  not supported.

-c checkpoint_options
  There is some differences between POSIX, SGE, and PBS/Torque. This area could
  use some quick and helpful improvement. Currently, checkpointing can be turned
  off entirely or have periodic checkpointing turned off. Otherwise, the code
  assumes that the executable is standard universe and will do automatic
  checkpointing as HTCondor currently implements it.

--condor-keep-files
  This is a non-SGE option. If given, it will prevent $scriptname from deleting
  temporary files (generated submit files, sentinel jobs). This is mostly useful
  for debugging.

-cwd
  If given, this option will cause the 'initialdir' value in the Condor submit
  file to be set to the current directory.

-d path
-wd working_dir
  For -d, the default working directory is not the home directory as speicifed 
  by PBS/Torque. Otherwise, works as expected for SGE-style (-wd).

-e <filename|path>
  Name of the file to contain the STDERR output of the job. By default this will
  be job_name.ejob_id[.task_id]. If an existing directory is specified, the file
  will be placed inside this directory using the default schema for the
  filename.

-f qsub_file
  Non-PBS/Torque/SGE option. condor_qsub was originally intended to accept
  command line arguments. However, submissions to qsub are often done in a batch
  file with commands like #PBS or #SGE. condor_qsub will parse the batch file
  listed as qsub_file. Note, this may be similar to the SGE -@ option.

-h
  Implemented as listed.

--help
  Print usage summary and option list. As of Version 0.2, no longer will -h be
  an option for usage summary and option list as it is a feature. We may 
  consider changing this option to -help as this is a valid SGE option.

-hold_jid <jid>
  If given, the job will be submitted in 'hold' state. Along with the actual job
  a 'sentinel' job will be submitted to Condor's local universe. This sentinel
  watches the specified job and releases the submitted job whenever the job has
  completed. The sentinel observes SGE's behavior to detect job exiting with
  code 100 and not start depedent job in this case. If a cluster id of an array
  job is given the dependent job will only be released after all individual jobs
  of a cluster have completed.

-l <resource spec>
  This option is currently under development. We support from PBS/Torque the 
  following options: arch, file, host, mem, nodes (partial), opsys, pmem, procs.
  We do not support parallel jobs at the moment.

-m <a|e|n><...>
  SGE's notification labels will be translated (approximately) into Condor's
  notifications states (Never, Error, Complete).

-M <email>
  Added as 'notify_user' to the submit file.

-N <jobname>
  Determines the default name of logfile (stdout, stderr).

-o <filename|path>
  See -e option, but for a job's stdout.

-p <int>
  Added a 'priority' to the submit file.

--print
  Non-PBS/Torque/SGE option. Print to STDOUT the contents of the HTCondor Submit
  File.

-q <queue name>
  This option is permanently ignored, as HTCondor doesn't have multiple queues.

-r <y|n>
  Defaults to 'y' as this is the default value for HTCondor, PBS/Torque, and the
  POSIX standard. HTCondor likes to try to rerun jobs, so we've done as best of
  a job as possible to ensure that reruns do not occur.

-S <shell>
  Path to a shell binary for script execution.

-shell <y|n>
  This option is currently ignored.

-t <start>[-<stop>[:<step>]]
  Task ID specification for array job submissions.

--test
  This non-PBS/Torque/SGE option makes condor_qsub not submit the job. Useful
  for testing purposes, especially when used in conjunction with --print and/or
  --condor-keep-files.

-v variable_list
  This option does not handle whitespace well. When used on the command line,
  single and double quotes need to be escaped by backslashes. Also, HTCondor
  does not accept <variable> instead of <variable>=<value>. If just <variable>
  is used, =True is added. It also ingores the environment Variable_List 
  attribute.

-V
  If given, 'getenv = True' is added to the submit file.

-W attr_name=attr_value[,attr_name=attr_value...]
  PBS/Torque supports a number of attributes. Only stagein and stageout are
  supported for attr_name. The format of attr_value for stagein and stageout is
  local_file@hostname:remote_file[,...] and we strip it to remote_file[,...].
  HTCondor's file transfer mechanism is then used if needed.

--version
  Print version information and exit.

EOT
}

create_dep_sentinel() {
	cat << EOT > $1
#!/bin/sh

hold_jids="$2"
dep_job="$3"

clean_error() {
	printf "\$1\n"
	condor_rm \$dep_job
	exit 1
}

# as long as there are relevant job in the queue wait and try again
counter=3
while [ \$(condor_q -long -attributes Owner \$hold_jids | grep "$USER" | wc -l) -ge 1 -o \$counter -ge 1 ]; do
	job_count=\$(condor_q -long -attributes Owner \$hold_jids | grep "$USER" | wc -l)
	if [ \$job_count -eq 0 ]; then
		counter=\$((counter-1))
	else
		counter=3
	fi
	sleep 5
done

# now check whether all deps have exited properly (i.e. not with code 100)
for jid in \$hold_jids; do
	   job_status="\$(condor_history \$jid |tail -n 1 | awk '{print \$6}')"
	   case "\$job_status" in
		   C) if [ "\$(condor_history -long \$jid |grep ExitCode | cut -d= -f2,2)" = " 100" ]; \
			  then clean_error "Error: Job dependency \$jid completed but exited with code 100."; fi;;
			X) clean_error "Error: Job dependency \$jid has been removed from the queue" ;;
			*) clean_error "Error: Job dependency \$jid doesn't exist" ;;
		esac
done

# all good -- let the job go
condor_release \$dep_job

# in the end we can safely clean this sentinel script
[ $condor_keep_files -eq 0 ] && rm \$0 || true

EOT
	chmod +x $1

cat << EOT > $1.submit
universe = local
executable = $1
Queue
EOT
	chmod +x $1.submit

}

# PBS/Torque: http://docs.adaptivecomputing.com/torque/4-1-3/help.htm#topics/commands/qsub.htm
# SGE/Oracle Grid Engine: http://docs.oracle.com/cd/E24901_01/doc.62/e21978/configuration.htm#autoId60
parse_args() {
	###############################################################
	# cmdline args
	###############################################################

	# Parse commandline options
	# Note that we use `"$@"' to let each command-line parameter expand to a
	# separate word. The quotes around `$@' are essential!
	# We need CLOPTS as the `eval set --' would nuke the return value of getopt.
	command_line_flags="-o A:,a:,b:,c:,d:,e:,f:,h,i:,I,j:,l:,m:,M:,N:,o:,p:,q:,r:,S:,t:,v:,V,W:,z --long condor-keep-files,cwd,help,hold_jid:,print,shell:,test,verbose,version,wd:"
	CLOPTS=`getopt -a $command_line_flags -n "$scriptname" -- "$@"`

	if [ $? != 0 ] ; then
	  echo "Terminating..." >&2
	  exit 1
	fi

	# If we have #PBS commands or #SGE commands
	# Parse the file for PBS or SGE commands
	if [ "$1" = "-f" ]; then
		# Read through the file and "create" commands line options
		shift
		qsub_file=$1
		qsub_cmd=$qsub_file
		qsub_new_command_line=""
		pbs_regex='^#PBS '
		sge_regex='^#\$ '
		qsub_new_command_line=""
		while read qsub_file_line; do
			# Check each line in the file to see if they start with #PBS or #SGE
			line_has_command=0
			if [[ $qsub_file_line =~ $pbs_regex ]]; then
				line_qsub_option=${qsub_file_line#*#PBS}
				line_has_command=1
			fi
			if [[ $qsub_file_line =~ $sge_regex ]]; then
				line_qsub_option=${qsub_file_line#*#\$}
				line_has_command=1
			fi
			if [ "$line_has_command" = 1 ]; then
				qsub_new_command_line="$qsub_new_command_line $line_qsub_option"
			fi
		done < $qsub_file
#		echo New command line options: "$qsub_new_command_line"
		qsub_new_command_line="$@ $qsub_new_command_line"
#		echo New command line: $qsub_new_command_line
		set -- $qsub_new_command_line
		CLOPTS=`getopt -a $command_line_flags -n "$scriptname" -- "$@"`
	fi
		
	# Note the quotes around `$CLOPTS': they are essential!
	eval set -- "$CLOPTS"

	while true ; do
	  case "$1" in
		-A) shift; accounting_group=$1; shift;;
		-a) shift; deferral_date_time=$1; shift;;
		-b) shift; if [ "$1" = "y" ]; then binary_flag=1; fi; shift;;
		-c) shift; checkpoint_interval=$1;; # if [ ]; then asdf; fi; shift;; #Figure out how to get optional argument
		--cwd) shift; cwd_flag=1;;
		--condor-keep-files) shift; condor_keep_files=1;;
		-d|--wd) shift; init_dir=$1; shift;;
#		--wd) shift; init_dir=$1; shift;;
		-e) shift; error_file=$1; shift;;
		-h) shift; hold_flag=1;;
		--help) print_description; print_help; exit 0;;
		--hold_jid) shift; hold_jid=$1; shift;;
		# we would be only interested in the arch spec -- right now it is ignored
		-i) shift; input_file=$1; shift;;
		-I) shift; interactive_option="-interactive";;
		-j) shift; merge_streams=$1; shift;;
		-l) shift
			if [ -z "$resource_list" ]; then
				resource_list=$1
			else
				resource_list="$resource_list,$1"
			fi 
			shift;;
		-m) shift; mail_config=$1; shift;;
		-M) shift; mail_user=$1; shift;;
		-N) shift; job_name=$1; shift;;
		-o) shift; output_file=$1; shift;;
		-p) shift; priority=$1; shift;;
		--print) shift; print_flag=1;;
		-q) shift; queue_name=$1; shift;;
		-r) shift; rerun_flag=$1; shift;;
		-S) shift; shell=$1; shift;;
		--shell) shift; shift;;
#		--sync) shift; sync_flag=1;; #Needs work.
		-t) shift
			taskid_first="$(echo "$1" | awk -F- '{print $1}')"
			taskid_last="$(echo "$1" | awk -F- '{print $2}' | awk -F: '{print $1}')"
			taskid_step="$(echo "$1" | awk -F- '{print $2}' | awk -F: '{print $2}')"
			shift;;
		# needs to handle SGE_TASK_ID, SGE_TASK_FIRST, SGE_TASK_LAST and SGE_TASK_STEPSIZE
		# log goes to: <jobname>.['e'|'o']<job_id>'.'<task_id>
		--test) shift; test_qsub_flag=1;;
		-v) shift; environment=$1; shift;;
		-V) shift; export_env_flag=1;;
		--version) print_version; exit 0;;
		-W) shift; transfer_files="$transfer_files $1"; shift;;
		-z) shift; no_output_batch_id_flag=1;;
		--) shift ; break ;;
		*) echo "Internal error! ($1)"; exit 1;;
	  esac
	done
	# the first arg is the command the rest are its arguments
	if [ $# -ge 1 ] && [ "$1" != "XXcondor_sub_scriptmodeXX" ]; then
		qsub_cmd="$1"
		shift
		qsub_args="$@"
	fi
}

# parse all commandline args
parse_args $@

# no arguments -> need to read from stdin
if [ -z "$qsub_cmd" ]; then
	# redirect STDIN into a file, place it into the current dir to increase the
	# likelihood of being available on the execute host too (NFS-mounted FS)
	# unfortunately the author cannot think of a way to clean this tempfile up
	# as it need to be available until the last job in the cluster actually
	# started running -- yet another sentinel could do it ....
	if [ -z "$job_name" ]; then
		stdin_script_file=$(mktemp --tmpdir=$(pwd) STDIN_qsub.XXXXXXXXXXXXX)
	else
		stdin_script_file=$(mktemp --tmpdir=$(pwd) ${job_name}_qsub.XXXXXXXXXXXXX)
	fi
	cat < /dev/stdin > $stdin_script_file
	chmod +x $stdin_script_file
	# use same default name as SGE
	if [ -z "$job_name" ]; then job_name="STDIN"; fi
	qsub_cmd="$stdin_script_file"
	qsub_args=""
fi

# if we're not in "binary" mode, we also need to parse the submitted script for
# additional arguments -- in an ideal world this would be done before parsing
# the actual commandline args to give them a higher priority
if [ $binary_flag -ne 1 ]; then
	script_args=`grep '^\#\$ ' < $qsub_cmd |cut -d' ' -f 2- | tr "\n" " "`
	if [ $? = 0 ] ; then
		# found some args
		parse_args $script_args XXcondor_sub_scriptmodeXX
	fi
fi

# have a meaningful job name in any case
if [ -z "$job_name" ]; then
	job_name="$(basename $qsub_cmd)"
fi

# handle job dependency
# store effective job deps
job_deps=""
if [ -n "$hold_jid" ]; then
	# loop over potentially multiple job deps
	for jid in $(echo "$hold_jid" |tr "," " ") ; do
		if [ $(condor_q -long -attributes Owner $jid | grep "$USER" | wc -l) -ge 1 ]; then
			# job is currently in the queue and owned by $USER -> store
			job_deps="$job_deps $jid"
		else
			# job not owned by this user or not in the queue, but maybe already
			# done?
		   job_status="$(condor_history $jid |tail -n 1 | awk '{print $6}')"
		   echo "JOBSTATUS: $job_status" >&2
		   case "$job_status" in
			   C) if [ "$(condor_history -long $jid |grep ExitCode | cut -d= -f2,2)" = " 100" ]; \
			      then printf "Error: Job dependency $jid completed but exited with code 100.\n"; exit 100; fi;;
				X) printf "Error: Job dependency $jid has been removed from the queue\n" ; exit 1;;
				*) printf "Error: Job dependency $jid doesn't exist\n" ; exit 1;;
			esac
		fi
	done
fi

# compose the names of logfiles
log_file="${log_file:-$job_name.log\$(Cluster)}"

# SGE also accepts directories for stdout and stderr. in this case it would
# place a file with the default name in this directory. This is probably
# evaluated at execution time. for condor we can only easily do this at
# submission time -- this is not the same -- let's hope it works nevertheless

if [ -d "$error_file" ]; then
	error_file="${error_file}/$job_name.e\$(Cluster)"
else
	error_file="${error_file:-$job_name.e\$(Cluster)}"
fi
if [ -d "$output_file" ]; then
	output_file="${output_file}/$job_name.o\$(Cluster)"
else
	output_file="${output_file:-$job_name.o\$(Cluster)}"
fi

# main submit file
submit_file=$(mktemp --tmpdir condor_qsub.XXXXXXXXXXXXX)

# write submit file header
cat << EOT > $submit_file
# condor_qsub call: $@
universe = vanilla
#should_transfer_files = NO
should_transfer_files = IF_NEEDED
#log = $log_file
EOT

# handle 'binary mode'
if [ $binary_flag = 1 ]; then
	## go with current shell or POSIX if undefined
	cat << EOT >> $submit_file
executable = $shell
arguments = "-c '$qsub_cmd $qsub_args'"
EOT
	qsub_cmd="$shell"
else
	cat << EOT >> $submit_file
executable = $shell
arguments = $qsub_cmd $qsub_args
EOT
fi

if [ $cwd_flag = 1 ]; then
	printf "initialdir = $(pwd)\n" >> $submit_file
fi
if [ -n "$init_dir" ]; then
	printf "initialdir = $init_dir\n" >> $submit_file
fi
if [ -n "$merge_streams" ]; then
	# Check to see if "e" is in the string
#	echo $merge_streams
	error_flag=0
	if [ `echo ${merge_streams} | grep -c "e"` -gt 0 ]; then
		error_flag=1
	fi
	# Check to see if "o" is in the string
	output_flag=0
	if [ `echo ${merge_streams} | grep -c "o"` -gt 0 ]; then
		output_flag=1
	fi
	# If we are to merge the two streams, merge them into output
	if [ "$output_flag" -eq 1 ] && [ "$error_flag" -eq 1 ]; then
		# Make sure we have a file name for the output
		if [ -n "$output_file" ]; then
			output_file="$job_name.o\$(Cluster)"
		fi
		error_file=$output_file
	fi
fi
if [ -n "$error_file" ]; then
	printf "error = $error_file\n" >> $submit_file
fi
if [ -n "$output_file" ]; then
	printf "output = $output_file\n" >> $submit_file
fi
if [ -n "$input_file" ]; then
	printf "input = $input_file\n" >> $submit_file
fi
if [ $export_env_flag = 1 ]; then
	printf "getenv = True\n" >> $submit_file
fi
if [ -n "$mail_user" ]; then
	printf "notify_user = $mail_user\n" >> $submit_file
fi
if [ -n "$mail_config" ]; then
	if [ "$mail_config" != "$(echo "$mail_config" | sed -e 's/n//')" ]; then
		printf "notification = Never\n" >> $submit_file
	elif [ "$mail_config" != "$(echo "$mail_config" | sed -e 's/e//')" ]; then
		printf "notification = Complete\n" >> $submit_file
	elif [ "$mail_config" != "$(echo "$mail_config" | sed -e 's/a//')" ]; then
		printf "notification = Error\n" >> $submit_file
	fi
else
	printf "notification = Never\n" >> $submit_file
fi
if [ -n "$priority" ]; then
	printf "priority = $priority\n" >> $submit_file
fi
if [ -n "$job_deps" ]; then
	# in case of job deps, submit the job held and release it via a sentinel
	printf "hold = True\n" >> $submit_file
fi
if [ -n "$deferral_date_time" ]; then
	# Convert [[[[CC]YY]MM]DD]hhmm[.SS] into a format for UNIX's date
	# Calculate length of string before period
	pre_period=${deferral_date_time%%.*}
	original_pre_period_length="${#pre_period}"
	post_period=${deferral_date_time##*.}
	#First, check to make sure that lengths are valid
	if [ "${#pre_period}" -gt 12 ] || [ "${#pre_period}" -lt 4 ] || [ "$((${#pre_period} % 2))" -eq 1 ]; then
		echo "Invalid Deferral Date. Incorrect lengths. Ignoring..."
	else # We have a (semi-)valid date. We need to parse [[[[CC]YY]MM]DD]hhmm[.SS] so that the Unix date command can use it.		
		# No need to add_year
		if [ "${#pre_period}" = 12 ]; then
			date_year=${pre_period:0:4}
			pre_period=${pre_period:4:((${#pre_period}-4))} # Chop off CCYY
		else # Need to add year
			current_year="$(date +%Y)"
			if [ "${#pre_period}" = 10 ]; then
				# Need to add century
				current_century=${current_year:0:2}
				date_year="$current_century${pre_period:0:2}"
				pre_period=${pre_period:2:((${#pre_period}-2))} #Chop off YY
			else
				date_year=$current_year
			fi
		fi
		# Need to add month
		current_month="" # We need this variable for testing to see if MM is present
		if [ "${#pre_period}" = 8 ]; then
			date_month=${pre_period:0:2}
			pre_period=${pre_period:2:((${#pre_period}-2))} # Chop off MM
		else
			current_month="$(date +%m)"
			date_month=$current_month
		fi
		# Need to add day
		current_day="" # We need this variable for testing to see if DD is present
		if [ "${#pre_period}" = 6 ]; then
			date_day=${pre_period:0:2}
			pre_period=${pre_period:2:((${#pre_period}-2))} # Chop off MM
		else
			current_day="$(date +%d)"
			date_day=$current_day
		fi
		# We will have a time and minute
		date_hour=${pre_period:0:2}
		date_minute=${pre_period:2:2}
		# Check to see if we have seconds with correct length
		if [ "${#post_period}" = 2 ]; then
			date_seconds=$post_period
		else
			date_seconds="00"
		fi
		current_date="$(date +%s)"

		date_raw_string="$date_year-$date_month-$date_day"
		time_raw_string="$date_hour:$date_minute:$date_seconds"
#		echo Date Raw String: $date_raw_string
#		echo Time Raw String: $time_raw_string
		date_from_command="$(date --date="$date_raw_string $time_raw_string" +%s)"
		if [ $? = 0 ]; then
#			echo Date from command: $date_from_command

			# We have an valid date
			update_day=0
			update_month=0
			if [ -n "$current_month" ]; then # If MM is not specified
			# Check to see if we need to run tomorrow
				if [ -n "$current_day" ]; then # If DD is not specified as well
					# Get the epoch for time specified
					if [ "$date_from_command" -lt "$current_date" ]; then
						update_day=1
					fi
				else # DD is specified
					# Get the epoch for the day and time specified
					if [ "$date_from_command" -lt "$current_date" ]; then
						update_month=1
					fi
					
				fi
			fi

			if [ "$update_day" = 1 ]; then
#				echo Plus one day "$(date --date="$date_raw_string + 1 day $time_raw_string")"
				deferral_unix_epoch="$(date --date="$date_raw_string + 1 day $time_raw_string" +%s)"
			elif [ "$update_month" = 1 ]; then
#				echo Plus one month "$(date --date="$date_raw_string + 1 month $time_raw_string")"
				deferral_unix_epoch="$(date --date="$date_raw_string + 1 month $time_raw_string" +%s)"
			else
				deferral_unix_epoch=$date_from_command
			fi			

############################################################
# We have two paths we can take here. Either we can use the
# Periodic Release Route (which has no maximum time) or we
# can use the Deferral Time Route (which does not have to 
# deal with any sort of periodic release issues). We are 
# going with the Periodic Release Route because of the lack
# of a maximum time.
############################################################
# The Periodic Release Route
############################################################
#	printf "hold = True\n" >> $submit_file
#	printf "+HoldReason = \"Job held until job becomes eligable for execution.\"\n" >> $submit_file
#	printf "+HoldReasonCode = 15\n" >> $submit_file
#	# We want to make sure that the reason why the job would be released from being on hold is because we put the job
#	# on hold on submission and the current time is after the specified eligible execution time.
#	printf "periodic_release = ( ( time() >= $deferral_unix_epoch ) && ( HoldReasonCode == 15 ) )\n" >> $submit_file
############################################################
# The Deferral Time Route
############################################################
			printf "deferral_time = $deferral_unix_epoch\n" >> $submit_file
			printf "deferral_window = 3153600000\n" >> $submit_file #100 years
			printf "deferral_prep_time = 0\n" >> $submit_file
		else
			echo "Invalid Deferral Date. Incorrect values for date and/or time. Ignoring..."
		fi
	fi
fi
if [ -n "$accounting_group" ]; then
	printf "+AccountingGroup = \"$accounting_group\"\n" >> $submit_file
fi
if [ -n "$environment" ]; then
	# The environment variables for SGE/PBS/Torque are comma separated
	IFS=',' read -ra environment_variables <<< "$environment"
	environment_string=""
#	echo "$environment"
#	echo "${#environment_variables[@]}"
	environment_variable=""
	for environment_variable in "${environment_variables[@]}"; do
#		echo $environment_variable
		# We want to escape spaces, single quotes, and double quotes
		# First, escape double quotes
		# Second, append =True if there is no equal sign
		# Third, escape single quotes
		# Fourth, place single quotes around string if there is whitespace or single quotes

		# First, escape double quotes
		environment_variable="$(echo $environment_variable | sed s/\"/\"\"/g)"
		# Second, append =True if there is no equal sign
		if [ "$environment_variable" = "${environment_variable/=/}" ]; then
			# We need to have <name>=<variable>
			# Assume that the variable should be set to true. Not a great assumption, but it's something
			environment_variable="$environment_variable=True"
		fi

		# Third, escape single quotes
		environment_variable="$(echo $environment_variable | sed s/\'/\'\'/g)"

		# Fourth, place single quotes around string if there is whitespace or single quotes
		if [ "$environment_variable" != "${environment_variable/\'/}" ]; then
			environment_single_quote=1
		else
			environment_single_quote=0
		fi

		environment_no_space="$(echo $environment_variable | sed s/\\s//g)"
		if [ "$environment_variable" != "$environment_no_space" ]; then
			environment_whitespace=1
		else
			environment_whitespace=0
		fi

		if [[  "$environment_single_quote" -eq 1  || "$environment_whitespace" -eq 1 ]]; then
			# Split it up to pre and post equal sign
			envname=${environment_variable%=*}
			envvalue=${environment_variable#*=}
			environment_variable="$envname='$envvalue'"
		fi

		if [ -z "$environment_string" ]; then
			environment_string="$environment_variable"
		else
			environment_string="$environment_string $environment_variable"
		fi		
	done
	printf "environment = \"$environment_string\"\n" >> $submit_file
fi
if [ -n "$rerun_flag" ]; then
	if [ "$(echo $rerun_flag | cut -c 1 | tr a-z A-Z)" = "N" ]; then 
# https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToAvoidJobRestarts
		rerun_string="( NumJobStarts == 0 )"
# TJ says that JobCurrenStartExecutingDate might be better. Sam is inclind to agree, but needs to do more investigation
#		rerun_string="( ( CompletionDate =?= 0 ) && ( JobCurrentStartExecutingDate =?= UNDEFINED ) )"
		if [ -z "$requirements_string" ]; then
			requirements_string="$rerun_string"
		else
			requirements_string="$requirements_string && $rerun_string"
		fi
		# on_exit_remove only evaluates after a job has started, which means that NumShadowStarts must be already defined.
		printf "periodic_remove = ( JobStatus == 1 && NumJobStarts > 0)\n" >> $submit_file
#		printf "periodic_remove = ( ( CompletionDate =?= 0 ) && ( JobCurrentStartExecutingDate =!= 0 ) )\n" >> $submit_file 
	fi
fi
if [ -n "$checkpoint_interval" ]; then
	if [ "$(echo $checkpoint_interval | cut -c 1 | tr A-Z a-z)" = "n" ]; then
		printf "+WantCheckpoint = False\n" >> $submit_file
	fi
	if [ "$(echo $checkpoint_interval | cut -c 1 | tr A-Z a-z)" = "s" ]; then
		printf "PERIODIC_CHECKPOINT = False\n" >> $submit_file
	fi
#  Try to support better checkpointing.
#	if [ "$checkpoint_interval" = "periodic" ]; then
#		printf "PERIODIC_CHECKPOINT = 10\n" >> $submit_file
##  http://research.cs.wisc.edu/htcondor/manual/v7.9/7_2Setting_up.html#SECTION00827000000000000000
#	fi
#	number_regex='^[0-9]+$'
#	number_interval_regex='^interval=[0-9]+$'
#	if [ "$checkpoint_interval" =~ $number_regex ] || [ "$checkpoint_interval" =~ $number_interval_regex ]; then
#		number_of_checkpoint_minutes=${checkpoint_interval#*=}
#		printf "PERIODIC_CHECKPOINT = $number_of_checkpoint_minutes\n" >> $submit_file
#	fi
	# If we have any checkpointing, we need to be in Standard Universe
	if [ "$(echo $checkpoint_interval | cut -c 1 | tr A-Z a-z)" != "n" ]; then
		printf "UNIVERSE = standard\n" >> $submit_file
	fi
fi
if [ $sync_flag = 1 ]; then
	printf "log = $log_file\n" >> $submit_file
fi
if [ $hold_flag = 1 ]; then
	printf "hold = True\n" >> $submit_file
	printf "+HoldReason = \"Job held because job was submitted on hold.\"\n" >> $submit_file
	printf "+HoldReasonCode = 15\n" >> $submit_file
fi
if [ -n "$resource_list" ]; then
	#Parse list.
	IFS=',' read -ra resources <<< "$resource_list"
	requirements_string=""
	num_nodes=0
	for resource_type in "${resources[@]}"; do
		case "$resource_type" in
# PBS/Torque
			arch=*) arch_value=${resource_type#*=}
				arch_string="\"Arch == $arch_value\""
				if [ -z "$requirements_string" ]; then
					requirements_string="$arch_string"
				else
					requirements_string="$requirements_string && $arch_string"
				fi;;
#			cput=*) shift;; #ignore
#			epilogue=*) shift;; #NEED DAGMan or PostCmd
			file=*) file_value=${resource_type#*=}
				file_value=`echo $file_value | tr a-z A-Z` #Uppercase
				printf "request_disk = $file_value\n" >> $submit_file;;
			host=*) host_value=${resource_type#*=}
				host_string="\"Machine == $host_value\""
				if [ -z "$requirements_string" ]; then
					requirements_string="$host_string"
				else
					requirements_string="$requirements_string && $host_string"
				fi;;
#				printf "requirements = \"Machine == $host_value\"\n" >> $submit_file;;
			mem=*) mem_value=${resource_type#*=}
				mem_value=`echo $mem_value | tr a-z A-Z`
				printf "request_memory = $mem_value\n" >> $submit_file;;
#			nice=*) nice_value=(((${resource_type#*=})*(-1))) #issues with *-1
#				printf "JobPriority = $nice_value\n" >> $submit_file;;
			nodes=*) IFS='+' read -ra node_specs <<< "${resource_type#*=}"
				number_specs=$(( number_specs + ${#node_specs[@]} ))
				node_spec_index=0
				total_procs=0
				for node_spec in "${node_specs[@]}"; do
					IFS=':' read -ra node_properties <<< "${node_spec}"
#					echo Node spec: $node_spec
					# Check to see if we have a number of nodes or a name
					#node_number=${node_info%%*[!0-9]*}
					node_num_regex='^[0-9]+$'
					if [[ ${node_properties[0]} =~ $node_num_regex ]]; then
						num_nodes[$node_spec_index]=${node_properties[0]}
						node_name_string[$node_spec_index]=""
					else
						node_type_name=${node_properties[0]}
						num_nodes[$node_spec_index]=1 #Assume that the number of nodes on a named node is 1.
						node_name_string[$node_spec_index]="( Machine == $node_type_name )"
					fi

					ppn_value=""
					gpu_value=""
					ppn_array[$node_spec_index]=1
#					echo Length of node properties: "${#node_properties[@]}"
					if [ "${#node_properties[@]}" \> 1 ]; then
#						echo Entering Node Loop
						for node_info in "${node_properties[@]:1}"; do
#							echo "Node Info: ${node_info}"
							case "$node_info" in 
								#*[!0-9]*) node_number=${node_info##*[!0-9]*};;
								ppn=*) ppn_value=${node_info#*=}
									ppn_array[$node_spec_index]=$ppn_value;;
								gpus=*) gpu_value=${node_info#*=}
									gpu_array[$node_spec_index]=$gpu_value;;
								*) 
								if [ -z "${node_name_string[$node_spec_index]}" ]; then
									node_name_string[$node_spec_index]=" ( Machine == $node_info )"
								else
									node_name_string[$node_spec_index]="${node_name_string[$node_spec_index]} && ( Machine == $node_info )"
								fi;;
								# Ignore all property cases for now
								*) other_value=${node_info#*=}
									other_array[$node_spec_index]=$gpu_value;;
							esac
						done						
					fi
					echo PPN Array: "${ppn_array[$node_spec_index]}"
					# If there is no value for ppn given, assume that 1 cpu is needed on each node
					if [ -z "$ppn_value" ]; then
						ppn_array[$node_spec_index]=1
					fi
					# If there is no value for gpu given, assume that no gpus are needed on a node
					if [ -n "$gpu_value" ]; then
						gpu_array[$node_spec_index]=0
					fi

					total_procs=$(( total_procs + num_nodes[$node_spec_index]*${ppn_array[$node_spec_index]} ))
					if [ -n "${node_name_string[$node_spec_index]}" ]; then
						node_name_string[$node_spec_index]="\"${node_name_string[$node_spec_index]}\""
					fi
#					echo Total Procs: $total_procs
					node_spec_index=$node_spec_index+1
				done

				if [ "$total_procs" -gt 1 ]; then
					printf "request_cpus = $total_procs\n" >> $submit_file
# The next few lines are for the start of trying to support MPI commands/parallel universe.
#					printf "machine_count = $total_procs\n" >> $submit_file
#					printf "Universe = parallel" >> $submit_file #Easy way to detect vanilla vs. parallel universe
				fi;;
			opsys=*) opsys_value=${resource_type#*=}
				opsys_string="\"OpSys == $opsys_value\""
				if [ -z "$requirements_string" ]; then
					requirements_string="$opsys_string"
				else
					requirements_string="$requirements_string && $opsys_string"
				fi;;
#			other=*) shift;; #ignore
#			pcput=*) shift;; #ignore
#			pmem=*) pmem_value=${resource_type#*=}
#				printf "RequestMemory = $pmem_value\n" >> $submit_file;;
			procs=*) procs_value=${resource_type#*=}
				printf "request_cpus = $procs_value\n" >> $submit_file;;
# More potential support for MPI commands/parallel universe.
#				printf "machine_count = $procs_value\n" >> $submit_file
#				if [ "$procs_value" -gt 1 ]; then
#					printf "Universe = parallel" >> $submit_file #Easy way to detect vanilla vs. parallel universe
#				fi;;

#			probs_bitmap=*) shift;; #ignore
#			prologue=*) shift;; #NEED DAGMan or PreCmd
#			pvmem=*) pvmem_value=${resource_type#*=} #ImageSize?
#				printf "RequestMemory = $pvmem_value\n" >> $submit_file;;
#			size=*) shift;;#
#			software=*) shift;;
#			vmem=*) shift;;
#			walltime=*) walltime_value=${resource_type#*=} #ignore
#				print "maxjobretirementtime = $walltime_value" >> $submit_file;;

# SGE
# Some of these SGE options may overlap with PBS/Torque options. Further investigation is needed.
# We have added comments about what each option is supposed to do. the "shift" commands here are just place holders.
#			qname=*) shift;; # name of the queue. The default value is Template.
#			hostlist=*) shift;; # A list of host names or host group names on which the queue runs. The default value is NONE.
#			hostname=*) shift;; #???
#			notify=*) shift;; # ???
#			calendar=*) shift;; # Specifies the calendar associated with this queue, if there is one. The default value for this parameter is NONE.
#			min_cpu_interval=*) shift;; # time between two automatic checkpoints in the case of transparent checkpointing jobs.
#			ckpt_list=*) shift;; # The list of administrator-defined checkpointing interface names. The default is NONE.
#			tmpdir=*) shift;; # temporary directory path. The default value is /tmp.
#			seq_no=*) shift;; # sequence number of the queue. The default value is 0.
#			slots=*) shift;; # maximum number of concurrently executing jobs allowed in the queue.
## http://docs.oracle.com/cd/E24901_01/doc.62/e21978/configuration.htm#autoId30
#			s_rt=*) shift;; # soft runtime limit
#			h_rt=*) shift;; # hard runtime limit
#			s_cpu=*) shift;; # per-process CPU time limit in seconds
#			h_cpu=*) shift;; # per-job CPU time limit in seconds
#			s_data=*) shift;; # per-process maximum memory limit in bytes
#			h_data=*) shift;; # per-job maximum memory limit in bytes
#			s_stack=*) shift;; # soft limit for the stack size
#			h_stack=*) shift;; # hard limit for the stack size
#			s_core=*) shift;; # soft limit for the per-process maximum core file size in bytes.
#			h_core=*) shift;; # hard limit for the per-process maximum core file size in bytes.
#			s_rss=*) shift;; # soft resident set size limit.
#			h_rss=*) shift;; # hard resident set size limit.
#			s_vmem=*) shift;; # per-process maximum memory limit in bytes. The same as s_data. If both are set, the minimum is used.
#			h_vmem=*) shift;; # per-job maximum memory limit in bytes. The same as h_data. If both are set, the minimum is used.
#			s_fsize=*) shift;; # soft limit for the number of disk blocks that this job can create.
#			h_fsize=*) shift;; # hard limit for the number of disk blocks that this job can create.
		esac
	done
fi
# Make printing the requirements string one of the last commands before entering the queue command
if [ -n "$requirements_string" ]; then
	printf "requirements = $requirements_string\n" >> $submit_file
fi
# Transfer files
if [ -n "$transfer_files" ]; then
	read -ra stages <<< "$transfer_files"
	set_should_transfer_files=0
	for particular_staging in "${stages[@]}"; do
		case "$particular_staging" in
			stagein=*) #Transfer files in. First remove quotes if present
				input_file_transfer_list=${particular_staging#*stagein=}
				input_file_transfer_list="$(echo $input_file_transfer_list | sed s/\'//g)"
				input_file_list=""
#				echo Input File Transfer List: $input_file_transfer_list
				IFS=',' read -ra input_file_array <<< "$input_file_transfer_list"
				# Iterate over each file
				for input_file_full_path in "${input_file_array[@]}"; do
					input_file_name=${input_file_full_path#*:} # Remove everything before the :
					if [ -z "$input_file_list" ]; then
						input_file_list=$input_file_name
					else
						input_file_list="$input_file_list,$input_file_name"
					fi
				done
				printf "transfer_input_files = $input_file_list\n" >> $submit_file;;
			stageout=*) #Transfer files out. First remove quotes if present
				output_file_transfer_list=${particular_staging#*stageout=}
				output_file_transfer_list="$(echo $output_file_transfer_list | sed s/\'//g)"
				output_file_list=""
#				echo Output File Transfer List: $output_file_transfer_list
				IFS=',' read -ra output_file_array <<< "$output_file_transfer_list"
				# Iterate over each file
				for output_file_full_path in "${output_file_array[@]}"; do
					output_file_name=${output_file_full_path#*:} # Remove everything before the :
					if [ -z "$output_file_list" ]; then
						output_file_list=$output_file_name
					else
						output_file_list="$output_file_list,$output_file_name"
					fi
				done
				printf "when_to_transfer_output = ON_EXIT \n" >> $submit_file
				printf "transfer_output_files = $output_file_list \n" >> $submit_file;;
		    esac
	done
fi


# transfer_executable = False for not transfering executable
# -k 

# handle array jobs
if [ -n "$taskid_first" ]; then
	for taskid in $(seq $taskid_first $taskid_step $taskid_last); do
		printf "environment = \"$environment SGE_TASK_ID=$taskid SGE_TASK_FIRST=$taskid_first SGE_TASK_STEPSIZE=$taskid_step SGE_TASK_LAST=$taskid_last\"\n" >> $submit_file
		if [ -n "$error_file" ]; then
			printf "error = $error_file.$taskid\n" >> $submit_file
		fi
		if [ -n "$output_file" ]; then
			printf "output = $output_file.$taskid\n" >> $submit_file
		fi
		# queue this job
		printf "Queue\n" >> $submit_file
	done
else
	if [ "$total_procs" -gt 1 ] && [ "$number_specs" -gt 1 ]; then
		number_specs_max=$(( number_specs - 1))
		for node_spec_index in $(seq 0 $number_specs_max ); do
			echo Node Specification Index: $node_spec_index
			if [ -n "${node_name_string[$node_spec_index]}" ]; then
				#echo ${node_name_string[$node_spec_index]}
				#host_string="\"Machine == ${node_name_string[$node_spec_index]}\""
				host_string=${node_name_string[$node_spec_index]}
				if [ -z "$requirements_string" ]; then
					queued_requirements_string="$host_string"
				else
					queued_requirements_string="$requirements_string && $host_string"
				fi
				# This will change the requirements for each collection of nodes 
				printf "requirements = $queued_requirements_string\n" >> $submit_file				
			fi
			num_queued_procs=$(( num_nodes[$node_spec_index] * ppn_array[$node_spec_index] ))
			printf "Queue $num_queued_procs\n" >> $submit_file
		done
	else
		# unconditional queuing
		printf "Queue\n" >> $submit_file
	fi
fi

# Done with creating to submission file
if [ $test_qsub_flag = 0 ]; then
	# actually submit the job, printing only the cluster id
	#printf "Trying to submit job.\n"
	cluster_id_with_period="$(condor_submit $interactive_option < $submit_file | grep 'cluster' | awk '{print $6}')"
	cluster_id=${cluster_id_with_period%.}
	# Hanke's original (V0.1) submit line is below
	#cluster_id="$(condor_submit -verbose $interactive_option < $submit_file |grep '^** Proc' || true)"
	#printf "Post condor_submit\n"
	if [ -z "$cluster_id" ]; then
		printf "Job submission failed.\n"
		[ $condor_keep_files -eq 0 ] && rm $submit_file || true
		exit 1
	fi
	cluster_id="$(echo "$cluster_id" | head -n1 | cut -d ' ' -f 3,3 | cut -d. -f1,1)"
#	if [ $sync_flag = 1 ]; then
#		echo "Attempting to sync"
##		# condor_q needs to be called to make sure to wait that the job has been scheduled to run.
#		condor_wait $log_file $cluster_id
#	fi
	#cleanup
	[ $condor_keep_files -eq 0 ] && rm $submit_file || true

	# deal with local job sentinel
	if [ -n "$job_deps" ]; then
		sentinel_file="$(mktemp --tmpdir cluster${cluster_id}_sentinel.XXXXXXXXXXXXX)"
		create_dep_sentinel $sentinel_file "$job_deps" "$cluster_id"
		condor_submit < $sentinel_file.submit > /dev/null
		[ $condor_keep_files -eq 0 ] && rm $sentinel_file.submit || true
	fi
else
	if [ $print_flag = 1 ]; then
		echo "$(cat $submit_file)"
	else
		echo "$submit_file"
	fi
	[ $condor_keep_files -eq 0 ] && rm $submit_file || true
fi


if [ $no_output_batch_id_flag = 0 ]; then 
	printf "Your job $cluster_id (\"$job_name\") has been submitted\n"
fi


exit 0
