/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef CONDOR_PROC_INCLUDED
#define CONDOR_PROC_INCLUDED

#include "condor_universe.h"
#include "condor_header_features.h"


// a handy little structure used in a lot of places
typedef struct {
	int		cluster;
	int		proc;
} PROC_ID;

#if defined(__cplusplus)
class MyString;
template <class Item> class ExtArray;
#endif


/*
**	Possible notification options
*/
#define NOTIFY_NEVER		0
#define NOTIFY_ALWAYS		1
#define	NOTIFY_COMPLETE		2
#define NOTIFY_ERROR		3

#define READER	1
#define WRITER	2
#define	UNLOCK	8

/*
** Warning!  Keep these consistent with the strings defined in the
** JobStatusNames array defined in condor_util_lib/proc.c
*/
#define UNEXPANDED	0
#define IDLE		1
#define RUNNING		2
#define REMOVED		3
#define COMPLETED	4
#define	HELD		5
#define SUBMISSION_ERR	6
#define JOB_STATUS_MAX  7 /* A placeholder, not a valid status. */

// Put C funtion definitions here
BEGIN_C_DECLS

PROC_ID getProcByString( const char* str );
const char* getJobStatusString( int status );
int getJobStatusNum( const char* name );

END_C_DECLS

// Put C++ definitions here
#if defined(__cplusplus)
bool operator==( const PROC_ID a, const PROC_ID b);
unsigned int hashFuncPROC_ID( const PROC_ID & );
void procids_to_mystring(ExtArray<PROC_ID> *procids, MyString &str);
ExtArray<PROC_ID>* mystring_to_procids(MyString &str);

// result MUST be of size PROC_ID_STR_BUFLEN
void ProcIdToStr(const PROC_ID a, char *result);
void ProcIdToStr(int cluster, int proc, char *result);
bool StrToProcId(char const *str, PROC_ID &id);
bool StrToProcId(char const *str, int &cluster, int &proc);
#endif

#define ICKPT -1

#define NEW_PROC 1

// maximum length of a buffer containing a "cluster.proc" string
// as produced by ProcIdToStr()
#define PROC_ID_STR_BUFLEN 35

#endif /* CONDOR_PROC_INCLUDED */
