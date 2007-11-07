#!/bin/bash

##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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


#
# Xen Control Tool
# This program must be called by root user.
# V0.2 / 2007-Oct-19 / Jaeyoung Yoon / jyoon@cs.wisc.edu
#

usage() {
	echo $"Usage: $0 {xm|virsh} {start|stop|suspend|resume|pause|unpause|status|getvminfo|check|killvm|createiso|createconfig}" 1>&2
	exit 1
}

if [ -z "$1" ] ; then
	usage;
fi

XM=$1
CTRL_PROG=`basename $XM`
MKISOFS="mkisofs"
PROG="$0"
TOPDIR=`pwd`
DOMAINNAME=""
XM_STD_OUTPUT=xm_std_file
XM_ERROR_OUTPUT=xm_error_file

unalias rm 2>/dev/null
unalias cp 2>/dev/null

run_controller() {
	CTRL_RESULT=0
	if [ "${CTRL_PROG}" = "xm" ]; then
		run_xm_command $@	
		CTRL_RESULT=$?
	elif [ "${CTRL_PROG}" = "virsh" ]; then
		run_virsh_command $@	
		CTRL_RESULT=$?
	fi
	return $CTRL_RESULT
}

run_virsh_command() {
		rm -f "$XM_STD_OUTPUT" 2>/dev/null
		rm -f "$XM_ERROR_OUTPUT" 2>/dev/null
		# Because it is possible that some arguments have spaces,
		# We do like this.
		$XM $@ > "$XM_STD_OUTPUT" 2> "$XM_ERROR_OUTPUT"
		REALRESULT=$?
	return $REALRESULT
}

run_xm_command() {
	TMP_RESULT=0
	ERR_COUNT=0
	while [ $TMP_RESULT = 0 ] && [ $ERR_COUNT -lt 5 ]
	do
		rm -f "$XM_STD_OUTPUT" 2>/dev/null
		rm -f "$XM_ERROR_OUTPUT" 2>/dev/null
		# Because it is possible that some arguments have spaces,
		# We do like this.
		$XM $@ > "$XM_STD_OUTPUT" 2> "$XM_ERROR_OUTPUT"
		REALRESULT=$?
		if [ $REALRESULT != 0 ]; then
			# command failed
			grep "Device" "$XM_ERROR_OUTPUT" 2>/dev/null
			TMP_RESULT=$?

			if [ $TMP_RESULT = 0 ]; then
				grep "connected" "$XM_ERROR_OUTPUT" 2>/dev/null
				TMP_RESULT=$?
			fi
		else
			# command succeeded
			TMP_RESULT=1
		fi

		if [ $TMP_RESULT = 0 ]; then
			# Error happens due to temporary Xen connection error
			# So, we need to do again
			echo "xm command $@ error found" 1>&2
			ERR_COUNT=$((${ERR_COUNT}+1))
			sleep 3
		fi
	done

	return $REALRESULT
}

find_domain_name() {
	if [ "${CTRL_PROG}" = "xm" ]; then
		find_xm_domain_name $@	
	elif [ "${CTRL_PROG}" = "virsh" ]; then
		find_virsh_domain_name $@	
	fi
}

find_xm_domain_name() {

	TEMP_XEN_CONFIG="$1"
	DOMAINNAME=""

	if [ ! -f "$TEMP_XEN_CONFIG" ]; then
		echo "$TEMP_XEN_CONFIG doesn't exist" 1>&2
		return 1
	fi

	DOMAINNAME=`sed -n '/^name=/p' "$TEMP_XEN_CONFIG" | sed -e 's/name=//' | sed -e 's/\"//g'`
}

find_virsh_domain_name() {

	TEMP_XEN_CONFIG="$1"
	DOMAINNAME=""

	if [ ! -f "$TEMP_XEN_CONFIG" ]; then
		echo "$TEMP_XEN_CONFIG doesn't exist" 1>&2
		return 1
	fi

	DOMAINNAME=`sed -e "s#.*<name>\(.*\)</name>.*#\1#" $TEMP_XEN_CONFIG`
}

start() {
# $1: Config file 
# $2: error file
	if [ ! -f "$1" ]; then
		echo "Usage: $PROG start <configfile>" 1>&2
		return 1
	fi
	XEN_CONFIG_FILE="$1"

	if [ -n "$2" ]; then
		rm -f "$2" 2>/dev/null
	fi

	find_domain_name "$XEN_CONFIG_FILE"

	run_controller create "$XEN_CONFIG_FILE"
	RESULT=$?

	cat "$XM_STD_OUTPUT" 1>&2

	if [ $RESULT != 0 ]; then
		echo "$XM create $XEN_CONFIG_FILE error" 1>&2
		cat "$XM_ERROR_OUTPUT" 1>&2

		if [ -n "$2" ]; then
			cat "$XM_ERROR_OUTPUT" > "$2"
			if [ -f "$2" ]; then
				chown --reference="$XEN_CONFIG_FILE" "$2" 2>/dev/null
			fi
		fi

		return 1
	fi
}

stop() {
# $1: config file
	if [ ! -f "$1" ]; then
		echo "Usage: $PROG stop <configfile>" 1>&2
		return 1
	fi
	XEN_CONFIG_FILE="$1"

	find_domain_name "$XEN_CONFIG_FILE"

	if [ -z "$DOMAINNAME" ]; then
		echo "Can't find domain name in $XEN_CONFIG_FILE" 1>&2
		return 1
	fi

	run_controller list
	RESULT=$?

	if [ $RESULT != 0 ]; then
		echo "XM list error" 1>&2
		cat "$XM_ERROR_OUTPUT" 1>&2
		return 1
	fi

	n=`grep -w "$DOMAINNAME" "$XM_STD_OUTPUT" | wc -l`
	if [ $n -eq 1 ]; then
		run_controller destroy "$DOMAINNAME"
		RESULT=$?

		cat "$XM_STD_OUTPUT" 1>&2
		if [ $RESULT != 0 ]; then
			echo "$XM destroy $DOMAINNAME error" 1>&2
			cat "$XM_ERROR_OUTPUT" 1>&2
			return 1
		fi
	fi
}

suspend() {
# $1: config file
# $2: file to be saved
	if [ -z "$1" ] || [ -z "$2" ]; then
		echo "Usage: $PROG suspend <configfile> <filename>" 1>&2
		return 1
	fi
	XEN_CONFIG_FILE="$1"
	XEN_SUSPEND_FILE="$2"

	if [ ! -f "$XEN_CONFIG_FILE" ]; then
		echo "Can't find vm config file for suspend" 1>&2
		return 1
	fi

	find_domain_name "$XEN_CONFIG_FILE"

	if [ -z "$DOMAINNAME" ]; then
		echo "Can't find domain name in $XEN_CONFIG_FILE" 1>&2
		return 1
	fi

	run_controller list
	RESULT=$?

	if [ $RESULT != 0 ]; then
		echo "XM list error" 1>&2
		cat "$XM_ERROR_OUTPUT" 1>&2
		return 1
	fi

	n=`grep -w "$DOMAINNAME" "$XM_STD_OUTPUT" | wc -l`
	if [ $n -eq 1 ]; then
		run_controller save "$DOMAINNAME" "$XEN_SUSPEND_FILE"
		RESULT=$?

		cat "$XM_STD_OUTPUT" 1>&2
		if [ $RESULT != 0 ]; then
			echo "$XM save $DOMAINNAME $XEN_SUSPEND_FILE error" 1>&2
			rm -f $XEN_SUSPEND_FILE 2>/dev/null
			cat "$XM_ERROR_OUTPUT" 1>&2
			return 1
		fi

		# change UID/GID from root to condor's
		chown --reference="$XEN_CONFIG_FILE" "$XEN_SUSPEND_FILE" 2>/dev/null
		sync
	else
		echo "Domain($DOMAINNAME) is not running" 1>&2
		return 1
	fi
}

resume() {
# $1: saved file
	if [ ! -f "$1" ]; then
		echo "Usage: $PROG resume <filename>" 1>&2
		return 1
	fi
	XEN_CONFIG_FILE="$1"

	run_controller restore "$XEN_CONFIG_FILE"
	RESULT=$?

	cat "$XM_STD_OUTPUT" 1>&2
	if [ $RESULT != 0 ]; then
		echo "$XM restore $XEN_CONFIG_FILE error" 1>&2
		cat "$XM_ERROR_OUTPUT" 1>&2
		return 1
	fi
}

pause() {
# $1: config file
	if [ ! -f "$1" ]; then
		echo "Usage: $PROG pause <configfile>" 1>&2
		return 1
	fi
	XEN_CONFIG_FILE="$1"

	find_domain_name "$XEN_CONFIG_FILE"

	if [ -z "$DOMAINNAME" ]; then
		echo "Can't find domain name in $XEN_CONFIG_FILE" 1>&2
		return 1
	fi

	run_controller list
	RESULT=$?

	if [ $RESULT != 0 ]; then
		echo "XM list error" 1>&2
		cat "$XM_ERROR_OUTPUT" 1>&2
		return 1
	fi

	n=`grep -w "$DOMAINNAME" "$XM_STD_OUTPUT" | wc -l`
	if [ $n -eq 1 ]; then

		if [ "${CTRL_PROG}" = "xm" ]; then
			run_xm_command pause "$DOMAINNAME"
		elif [ "${CTRL_PROG}" = "virsh" ]; then
			run_virsh_command suspend "$DOMAINNAME"
		fi

		RESULT=$?

		cat "$XM_STD_OUTPUT" 1>&2
		if [ $RESULT != 0 ]; then

			if [ "${CTRL_PROG}" = "xm" ]; then
				echo "$XM pause $DOMAINNAME error" 1>&2
			elif [ "${CTRL_PROG}" = "virsh" ]; then
				echo "$XM suspend $DOMAINNAME error" 1>&2
			fi

			cat "$XM_ERROR_OUTPUT" 1>&2
			return 1
		fi
	else
		echo "Domain($DOMAINNAME) is not running" 1>&2
		return 1
	fi
}

unpause() {
# $1: config file
	if [ ! -f "$1" ]; then
		echo "Usage: $PROG unpause <configfile>" 1>&2
		return 1
	fi
	XEN_CONFIG_FILE="$1"

	find_domain_name "$XEN_CONFIG_FILE"

	if [ -z "$DOMAINNAME" ]; then
		echo "Can't find domain name in $XEN_CONFIG_FILE" 1>&2
		return 1
	fi

	run_controller list
	RESULT=$?

	if [ $RESULT != 0 ]; then
		echo "XM list error" 1>&2
		cat "$XM_ERROR_OUTPUT" 1>&2
		return 1
	fi

	n=`grep -w "$DOMAINNAME" "$XM_STD_OUTPUT" | wc -l`
	if [ $n -eq 1 ]; then
		if [ "${CTRL_PROG}" = "xm" ]; then
			run_xm_command unpause "$DOMAINNAME"
		elif [ "${CTRL_PROG}" = "virsh" ]; then
			run_virsh_command resume "$DOMAINNAME"
		fi
		RESULT=$?

		cat "$XM_STD_OUTPUT" 1>&2
		if [ $RESULT != 0 ]; then
			if [ "${CTRL_PROG}" = "xm" ]; then
				echo "$XM unpause $DOMAINNAME error" 1>&2
			elif [ "${CTRL_PROG}" = "virsh" ]; then
				echo "$XM resume $DOMAINNAME error" 1>&2
			fi
			cat "$XM_ERROR_OUTPUT" 1>&2
			return 1
		fi
	else
		echo "Domain($DOMAINNAME) is not running" 1>&2
	fi
}

status() {
# $1: config file
# $2: result file
	if [ ! -f "$1" ] || [ -z "$2" ] ; then
		echo "Usage: $PROG status <configfile> <resultfile>" 1>&2
		return 1
	fi
	XEN_CONFIG_FILE="$1"
	XEN_RESULT_FILE="$2"

	rm -f "$XEN_RESULT_FILE" 2>/dev/null

	find_domain_name "$XEN_CONFIG_FILE"

	if [ -z "$DOMAINNAME" ]; then
		echo "Can't find domain name in $XEN_CONFIG_FILE" 1>&2
		return 1
	fi

	run_controller list
	RESULT=$?

	if [ $RESULT != 0 ]; then
		echo "XM list error" 1>&2
		cat "$XM_ERROR_OUTPUT" 1>&2
		return 1
	fi

	n=`grep -w "$DOMAINNAME" "$XM_STD_OUTPUT" | wc -l`
	if [ $n -eq 1 ]; then
		# domain is running
		echo "STATUS=Running" > "$XEN_RESULT_FILE"

		if [ "${CTRL_PROG}" = "xm" ]; then
			CPUTIME=`grep -w "$DOMAINNAME" "$XM_STD_OUTPUT" | awk '{print $6}'`
			if [ -n "$CPUTIME" ]; then
				echo "CPUTIME=$CPUTIME" >> "$XEN_RESULT_FILE"
			fi
		fi
	else
		# domain is stopped
		echo "STATUS=Stopped" > "$XEN_RESULT_FILE"
	fi
	
	chown --reference="$XEN_CONFIG_FILE" "$XEN_RESULT_FILE" 2>/dev/null
}

getvminfo() {
# $1: config file
# $2: result file
	if [ ! -f "$1" ] || [ -z "$2" ] ; then
		echo "Usage: $PROG getvminfo <configfile> <resultfile>" 1>&2
		return 1
	fi

	status $1 $2
	RESULT=$?
	return $RESULT
}

check() {
	$MKISOFS -version >/dev/null
	if [ $? -ne 0 ]; then
   		echo "Cannot execute $MKISOFS" 1>&2
		return 1
	fi

	run_controller list
	RESULT=$?

	if [ $RESULT != 0 ]; then
		echo "XM list error" 1>&2
		cat "$XM_ERROR_OUTPUT" 1>&2
		return 1
	fi
}

killvm() {
# $1: vmname
	if [ -z "$1" ]; then
		echo "Usage: $PROG killvm <vmname>" 1>&2
		return 1
	fi

	DOMAINNAME="$1"

	run_controller destroy "$DOMAINNAME"

	return 0;
}

createiso() {
# $1: listfile
# $2: iso name
	if [ ! -f "$1" ] || [ -z "$2" ] ; then
		echo "Usage: $PROG createiso <listfile> <isoname>" 1>&2
		return 1
	fi

	ISOCONFIG="$1"
	ISONAME="$2"

	# Create temporary directory
	TMPFILE="${ISONAME}.dir"
	rm -rf $TMPFILE
	mkdir $TMPFILE
	if [ $? -ne 0 ]; then
   		echo "Cannot create $TMPFILE" 1>&2
		return 1
	fi
	chown --reference="$ISOCONFIG" "$TMPFILE" 2>/dev/null

	# Read file list for ISO
	# Copy all files in the file list into the temporary directory
	while read ONEFILE
	do
		if [ -n "$ONEFILE" ]; then
			cp -f "$ONEFILE" "$TMPFILE" >/dev/null
			if [ $? -ne 0 ]; then
				echo "Cannot copy file($ONEFILE) into directory($TMPFILE)" 1>&2
				return 1
			fi
		fi
	done < $ISOCONFIG

	$MKISOFS -quiet -o "$ISONAME" -input-charset iso8859-1 -J -A "CONODR" -V "CONDOR" "$TMPFILE" >/dev/null
	if [ $? -ne 0 ]; then
		echo "Cannot create an ISO file($ISONAME)" 1>&2
		return 1
	fi
	chown --reference="$ISOCONFIG" "$ISONAME" 2>/dev/null

	rm -rf "$TMPFILE" 2>/dev/null 
	sync
}

createconfig() {
# $1: configuration file
	if [ ! -f "$1" ]; then
		echo "Usage: $PROG createconfig <configfile>" 1>&2
		return 1
	fi
}

#### Program starts from here #####

#if [ ! -x "$XM" ]; then
#	echo "Xen Utility($XM) does not exist" 1>&2
#	exit 1
#fi

if [ -z "$MKISOFS" ]; then
	echo "Should define 'MKISOFS' for mkisofs program" 1>&2
	exit 1
fi

ID=`id -u`
if [ $ID != 0 ]; then
	echo "Should be a root user" 1>&2
	exit 1
fi

case "$2" in
  start)
	start "$3" "$4"
   	;;
  stop)
   	stop "$3"
   	;;
  suspend)
  	suspend "$3" "$4"
	;;
  resume)
  	resume "$3"
	;;
  pause)
  	pause "$3"
	;;
  unpause)
  	unpause "$3"
	;;
  status)
  	status "$3" "$4"
	;;
  getvminfo)
  	getvminfo "$3" "$4"
	;;
  check)
  	check
	;;
  killvm)
    killvm "$3"
	;;
  createiso)
  	createiso "$3" "$4"
	;;
  createconfig)
  	createconfig "$3"
	;;
   *)
   	usage
esac

RESULT=$?

rm -f "$XM_STD_OUTPUT" 2>/dev/null
rm -f "$XM_ERROR_OUTPUT" 2>/dev/null
exit $RESULT
