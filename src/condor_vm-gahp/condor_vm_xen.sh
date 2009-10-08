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
	echo $"Usage: $0 {start|stop|suspend|resume|pause|unpause|status|getvminfo|check|killvm|createiso|createconfig}" 1>&2
	exit 1
}

if [ -z "$1" ] ; then
	usage;
fi

XM="virsh"
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
	run_virsh_command $@	
	CTRL_RESULT=$?
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

find_domain_name() {
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
	if [ ! -f "$1" ]; then
		echo "Usage: $PROG start <configfile>" 1>&2
		return 1
	fi
	XEN_CONFIG_FILE="$1"

	find_domain_name "$XEN_CONFIG_FILE"

	run_controller create "$XEN_CONFIG_FILE"
	RESULT=$?

	cat "$XM_STD_OUTPUT" 1>&2

	if [ $RESULT != 0 ]; then
		echo "$XM create $XEN_CONFIG_FILE error" 1>&2
		cat "$XM_ERROR_OUTPUT" 1>&2

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

		run_virsh_command suspend "$DOMAINNAME"

		RESULT=$?

		cat "$XM_STD_OUTPUT" 1>&2
		if [ $RESULT != 0 ]; then

			echo "$XM suspend $DOMAINNAME error" 1>&2

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
		run_virsh_command resume "$DOMAINNAME"
		RESULT=$?

		cat "$XM_STD_OUTPUT" 1>&2
		if [ $RESULT != 0 ]; then
			echo "$XM resume $DOMAINNAME error" 1>&2
			cat "$XM_ERROR_OUTPUT" 1>&2
			return 1
		fi
	else
		echo "Domain($DOMAINNAME) is not running" 1>&2
	fi
}

status() {
# $1: config file
	if [ ! -f "$1" ] ; then
		echo "Usage: $PROG status <configfile>" 1>&2
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
		# domain is running
		echo "STATUS=Running"

	else
		# domain is stopped
		echo "STATUS=Stopped"
	fi
}

getvminfo() {
# $1: config file
	if [ ! -f "$1" ] ; then
		echo "Usage: $PROG getvminfo <configfile>" 1>&2
		return 1
	fi

	status $1
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

case "$1" in
  start)
	start "$2"
	;;
  stop)
	stop "$2"
	;;
  suspend)
	suspend "$2" "$3"
	;;
  resume)
	resume "$2"
	;;
  pause)
	pause "$2"
	;;
  unpause)
	unpause "$2"
	;;
  status)
	status "$2"
	;;
  getvminfo)
	getvminfo "$2"
	;;
  check)
	check
	;;
  killvm)
	killvm "$2"
	;;
  createiso)
	createiso "$2" "$3"
	;;
  createconfig)
	createconfig "$2"
	;;
  virsh)
	check
	;;
   *)
	usage
esac

RESULT=$?

rm -f "$XM_STD_OUTPUT" 2>/dev/null
rm -f "$XM_ERROR_OUTPUT" 2>/dev/null
exit $RESULT
