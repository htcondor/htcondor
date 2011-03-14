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
	echo $"Usage: $0 TBD" 1>&2
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

#### Program starts from here #####

if [ -z "$MKISOFS" ]; then
	echo "Should define 'MKISOFS' for mkisofs program" 1>&2
	exit 1
fi

createiso "$2" "$3"

RESULT=$?

rm -f "$XM_STD_OUTPUT" 2>/dev/null
rm -f "$XM_ERROR_OUTPUT" 2>/dev/null
exit $RESULT
