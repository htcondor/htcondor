/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "proc.h"
#include "string_list.h"

extern "C" 
PROC_ID
getProcByString( const char* str )
{
	PROC_ID rval;
	StrToProcId(str,rval);
	return rval;
}

void
ProcIdToStr( const PROC_ID a, char *buf )
{
	ProcIdToStr( a.cluster, a.proc, buf );
}

void
ProcIdToStr( int cluster, int proc, char *buf ) {
	if ( proc == -1 ) {
		// cluster ad key
		sprintf(buf,"0%d.-1",cluster);
	} else {
		// proc ad key
		sprintf(buf,"%d.%d",cluster,proc);
	}
}

bool StrToProcId(char const *str, PROC_ID &id) {
	return StrToProcId(str,id.cluster,id.proc);
}

bool StrToProcId(char const *str, int &cluster, int &proc) {
	char const *tmp;

	// skip leading zero, if any
	if ( *str == '0' ) 
		str++;

	if ( !(tmp = strchr(str,'.')) ) {
		cluster = -1;
		proc = -1;
		return false;
	}
	tmp++;

	cluster = atoi(str);
	proc = atoi(tmp);
	return true;
}


bool operator==( const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}

// The str will be like this: "12.0,12.1,12.2,12.3...."
// The caller is responsible for freeing this memory.
ExtArray<PROC_ID>*
mystring_to_procids(MyString &str)
{
	StringList sl(str.Value());
	char *s = NULL;
	char *t = NULL;
	ExtArray<PROC_ID> *jobs = NULL;
	int i;

	jobs = new ExtArray<PROC_ID>;
	ASSERT(jobs);

	sl.rewind();

	i = 0;
	while((s = sl.next()) != NULL) {
		// getProcByString modifies the argument in place with strtok, since
		// s is actually held in the string list, I don't want to corrupt
		// that memory, so make a copy and do my task on that instead.
		t = strdup(s);
		ASSERT(t);
		(*jobs)[i++] = getProcByString(t);
		free(t);
	}

	return jobs;
}

// convert an ExtArray<PROC_ID> to a MyString suitable to construct a StringList
// out of.
void
procids_to_mystring(ExtArray<PROC_ID> *procids, MyString &str)
{
	MyString tmp;
	int i;

	str = "";

	// A null procids pretty much means an empty string list.
	if (procids == NULL) {
		return;
	}

	for(i = 0; i < procids->length(); i++) {
		tmp.sprintf("%d.%d", (*procids)[i].cluster, (*procids)[i].proc);
		str += tmp;
		// don't put a comma on the last one.
		if (i < (procids->length() - 1)) {
			str += ",";
		}
	}
}




