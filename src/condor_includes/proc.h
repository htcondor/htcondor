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
#include <string>

// parse a string of the form X.Y as a PROC_ID.
// return true the input string was a valid proc id and ended with \0 or whitespace.
// a pointer to the first unparsed character is optionally returned.
// input may be X  or X.  or X.Y.  if no Y is specified then proc will be set to -1
bool StrIsProcId(const char *str, int &cluster, int &proc, const char ** pend);

// a handy little structure used in a lot of places it has to remain a c style struct
// because some c code (I'm looking at you std-u) depends on it.
typedef struct PROC_ID {
	int		cluster;
	int		proc;
	bool operator<(const PROC_ID& cp) const {
		int diff = this->cluster - cp.cluster;
		if ( ! diff) diff = this->proc - cp.proc;
		return diff < 0;
	}
	bool set(const char * job_id_str) {
		if ( ! job_id_str) { cluster =  proc = 0; return false; }
		return StrIsProcId(job_id_str, this->cluster, this->proc, NULL);
	}

	// The schedd uses the PROC_ID as the key holder for ads in the job_queue.log
	// not all of these ads are jobs, various ranges of cluster and proc values are
	// 0,0     is the header ad.
	// >0,>=0  is a job ad
	// >0,-1   is a cluster ad
	// >0,-100 is a jobset ad
	PROC_ID() : cluster( -1 ), proc( -1 ) {}
	PROC_ID( int c, int p ) : cluster(c), proc(p) {}
	bool isJobKey() const { return cluster > 0 && proc >= 0; }
	bool isClusterKey() const { return cluster > 0 && proc == -1; }
	bool isJobsetKey() const { return cluster > 0 && proc == -100; }
	void invalidate() { cluster = proc = -1; }
} PROC_ID;

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
#define JOB_STATUS_MIN		1 /* Smallest valid job status value */
#define IDLE				1
#define RUNNING				2
#define REMOVED				3
#define COMPLETED			4
#define	HELD				5
#define	TRANSFERRING_OUTPUT	6
#define SUSPENDED			7
#define JOB_STATUS_FAILED	8  /* possible future use */
#define JOB_STATUS_BLOCKED	9  /* possible future use */
#define JOB_STATUS_MAX  	9 /* Largest valid job status value */

// define more searchable aliases for old job status defines
#define JOB_STATUS_IDLE                IDLE
#define JOB_STATUS_RUNNING             RUNNING
#define JOB_STATUS_REMOVED             REMOVED
#define JOB_STATUS_COMPLETED           COMPLETED
#define JOB_STATUS_HELD                HELD
#define JOB_STATUS_TRANSFERRING_OUTPUT TRANSFERRING_OUTPUT
#define JOB_STATUS_SUSPENDED           SUSPENDED
//#define JOB_STATUS_FAILED
//#define JOB_STATUS_BLOCKED


const char* getJobStatusString( int status );
int getJobStatusNum( const char* name );

bool operator==( const PROC_ID a, const PROC_ID b);
size_t hashFuncPROC_ID( const PROC_ID & );
size_t hashFunction(const PROC_ID &);

namespace std {
  template <> struct hash<PROC_ID>
  {
    size_t operator()(const PROC_ID & x) const
    {
		return hashFuncPROC_ID(x);
    }
  };
}

// result MUST be of size PROC_ID_STR_BUFLEN
void ProcIdToStr(const PROC_ID a, char *result);
void ProcIdToStr(int cluster, int proc, char *result);

PROC_ID getProcByString(const char* str);

// functions that call this should be fixed to either use StrIsProcId directly and deal with parse errors
// or to use the PROC_ID for native storage instead of trying to store jobid's in strings.
inline bool StrToProcIdFixMe(const char * str, PROC_ID& jid) {
	const char * pend;
	if (StrIsProcId(str, jid.cluster, jid.proc, &pend) && *pend == 0) {
		return true;
	}
	jid.cluster = jid.proc = -1;
	return false;
}

// maximum length of a buffer containing a "cluster.proc" string
// as produced by ProcIdToStr()
#define PROC_ID_STR_BUFLEN 35

typedef struct JOB_ID_KEY {
	int		cluster;
	int		proc;
	// a LessThan operator suitable for inserting into a sorted map or set
	bool operator<(const JOB_ID_KEY& cp) const {
		int diff = this->cluster - cp.cluster;
		if ( ! diff) diff = this->proc - cp.proc;
		return diff < 0;
	}
	bool operator<(const PROC_ID& cp) const {
		int diff = this->cluster - cp.cluster;
		if ( ! diff) diff = this->proc - cp.proc;
		return diff < 0;
	}
	JOB_ID_KEY operator+(int i) const { return {cluster, proc + i}; }
	JOB_ID_KEY operator-(int i) const { return {cluster, proc - i}; }
	JOB_ID_KEY &operator++() { ++proc; return *this; }
	JOB_ID_KEY &operator--() { --proc; return *this; }
	JOB_ID_KEY() : cluster(0), proc(0) {}
	JOB_ID_KEY(int c, int p) : cluster(c), proc(p) {}
	JOB_ID_KEY(const PROC_ID & rhs) : cluster(rhs.cluster), proc(rhs.proc) {}
	// constructing JOB_ID_KEY(NULL) ends up calling this constructor because there is no single int constructor - ClassAdLog depends on that...
	JOB_ID_KEY(const char * job_id_str) : cluster(0), proc(0) { if (job_id_str) set(job_id_str); }
	JOB_ID_KEY(const std::string& job_id_str) : cluster(0), proc(0) { set(job_id_str.c_str()); }
	operator const PROC_ID&() const { return *((const PROC_ID*)this); }
	operator std::string() const;
	void sprint(std::string &s) const;
	bool set(const char * job_id_str) { return StrIsProcId(job_id_str, this->cluster, this->proc, NULL); }
	bool set(const std::string& job_id_str) { return StrIsProcId(job_id_str.c_str(), this->cluster, this->proc, NULL); }
	static size_t hash(const JOB_ID_KEY &) noexcept;
} JOB_ID_KEY;

inline bool operator==( const JOB_ID_KEY a, const JOB_ID_KEY b) { return a.cluster == b.cluster && a.proc == b.proc; }
size_t hashFunction(const JOB_ID_KEY &);

// Macros used to indicate how a job was submitted to htcondor
#define JOB_SUBMIT_METHOD_MIN 0
#define JSM_CONDOR_SUBMIT 0
#define JSM_DAGMAN 1
#define JSM_PYTHON_BINDINGS 2
#define JSM_HTC_JOB_SUBMIT 3
#define JSM_HTC_DAG_SUBMIT 4
#define JSM_HTC_JOBSET_SUBMIT 5
#define JSM_USER_SET 100
#define JOB_SUBMIT_METHOD_MAX 100

const char* getSubmitMethodString(int method);

#define ICKPT -1

#define NEW_PROC 1


#endif /* CONDOR_PROC_INCLUDED */
