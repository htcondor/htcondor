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


// This file contains routines in common to both the qmgmt client
// and the qmgmt server (i.e. the schedd).

#include "condor_common.h"

#include <charconv>

#include "condor_daemon_core.h"
#include "dedicated_scheduler.h"
#include "scheduler.h"
#include "condor_qmgr.h"
#include "param_info.h" // for BinaryLookup

extern Scheduler scheduler;

int
SetAttributeInt(int cl, int pr, const char *name, int64_t val, SetAttributeFlags_t flags )
{
	char buf[24] = { 0 };
	int rval = 0;

	std::to_chars(buf, buf+sizeof(buf)-1, val);
	rval = SetAttribute(cl,pr,name,buf,flags);
	return(rval);
}

int
SetAttributeFloat(int cl, int pr, const char *name, double val, SetAttributeFlags_t flags )
{
	char buf[100];
	int rval = 0;

	snprintf(buf,100,"%f",val);
	rval = SetAttribute(cl,pr,name,buf,flags);
	return(rval);
}

int
SetAttributeString(int cl, int pr, const char *name, const char *val, SetAttributeFlags_t flags )
{
	std::string buf;
	int rval = 0;

	QuoteAdStringValue(val,buf);

	rval = SetAttribute(cl,pr,name,buf.c_str(),flags);
	return(rval);
}

int
SetAttributeExpr(int cl, int pr, const char *name, const ExprTree *val, SetAttributeFlags_t flags )
{
	std::string buf;
	int rval = 0;

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true, true );
	unp.Unparse( buf, val );

	rval = SetAttribute(cl,pr,name,buf.c_str(),flags);
	return(rval);
}

int
SetAttributeIntByConstraint(const char *con, const char *name, int64_t val, SetAttributeFlags_t flags)
{
	char buf[24] = { 0 };
	int rval = 0;

	std::to_chars(buf, buf+sizeof(buf)-1, val);
	rval = SetAttributeByConstraint(con,name,buf, flags);
	return(rval);
}

int
SetAttributeFloatByConstraint(const char *con, const char *name, float val, SetAttributeFlags_t flags)
{
	char buf[100];
	int rval = 0;

	snprintf(buf,100,"%f",val);
	rval = SetAttributeByConstraint(con,name,buf, flags);
	return(rval);
}

int
SetAttributeStringByConstraint(const char *con, const char *name,
							 const char *val,
							 SetAttributeFlags_t flags)
{
	std::string buf;
	int rval = 0;

	QuoteAdStringValue(val,buf);

	rval = SetAttributeByConstraint(con,name,buf.c_str(), flags);
	return(rval);
}

int
SetAttributeExprByConstraint(const char *con, const char *name,
                             const ExprTree *val,
                             SetAttributeFlags_t flags)
{
	std::string buf;
	int rval = 0;

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true, true );
	unp.Unparse( buf, val );

	rval = SetAttributeByConstraint(con, name, buf.c_str(), flags);
	return rval;
}

// struct for a table mapping attribute names to a flag indicating that the attribute
// must only be in cluster ad or only in proc ad, or can be either.
//
typedef struct attr_force_pair {
	const char * key;
	int          forced; // -1=forced cluster, 0=not forced, 1=forced proc
	// a LessThan operator suitable for inserting into a sorted map or set
	bool operator<(const struct attr_force_pair& rhs) const {
		return strcasecmp(this->key, rhs.key) < 0;
	}
} ATTR_FORCE_PAIR;

// table defineing attributes that must be in either cluster ad or proc ad
// force value of 1 is proc ad, -1 is cluster ad, 2 is proc ad, and sent first, -2 is cluster ad and sent first
// NOTE: this table MUST be sorted by case-insensitive attribute name
#define FILL(attr,force) { attr, force }
static const ATTR_FORCE_PAIR aForcedSetAttrs[] = {
	FILL(ATTR_CLUSTER_ID,         -2), // forced into cluster ad
	FILL(ATTR_JOB_SET_ID,         -1), // forced into cluster ad
	FILL(ATTR_JOB_SET_NAME,       -1), // forced into cluster ad
	FILL(ATTR_JOB_STATUS,         2),  // forced into proc ad (because job counters don't work unless this is set to IDLE/HELD on startup)
	FILL(ATTR_JOB_UNIVERSE,       -1), // forced into cluster ad
	FILL(ATTR_OWNER,              -1), // forced into cluster ad
	FILL(ATTR_PROC_ID,            2),  // forced into proc ad
};
#undef FILL

// returns non-zero attribute id and optional category flags for attributes
// that require special handling during SetAttribute.
int IsForcedClusterProcAttribute(const char *attr)
{
	const ATTR_FORCE_PAIR* found = nullptr;
	found = BinaryLookup<ATTR_FORCE_PAIR>(
		aForcedSetAttrs,
		COUNTOF(aForcedSetAttrs),
		attr, strcasecmp);
	if (found) {
		return found->forced;
	}
	return 0;
}

// send a cluster ad or proc ad as a series of SetAttribute calls.
// this function does a *shallow* iterate of the given ad, ignoring attributes in the chained parent ad (if any)
// since the chained parent attributes should be sent only once, and using a different key.
// To use this function to sent the cluster ad, pass a key with -1 as the proc id, and pass the cluster ad as the ad argument.
//
int SendJobAttributes(const JOB_ID_KEY & key, const classad::ClassAd & ad, SetAttributeFlags_t saflags, CondorError *errstack /*=NULL*/, const char * who /*=NULL*/)
{
	classad::ClassAdUnParser unparser;
	unparser.SetOldClassAd( true, true );
	std::string rhs; rhs.reserve(120);

	if ( !  who) who = "Qmgmt";

	int retval = 0;
	bool is_cluster = key.proc < 0;

	// first try and send the cluster id or proc id
	if (is_cluster) {
		if (SetAttributeInt (key.cluster, -1, ATTR_CLUSTER_ID, key.cluster, saflags) == -1) {
			if (errstack) {
				errstack->pushf(who, SCHEDD_ERR_SET_ATTRIBUTE_FAILED,
					"failed to set " ATTR_CLUSTER_ID "=%d (%d)",
					key.cluster, errno);
			}
			return -1;
		}
	} else {
		if (SetAttributeInt (key.cluster, key.proc, ATTR_PROC_ID, key.proc, saflags) == -1) {
			if (errstack) {
				errstack->pushf(who, SCHEDD_ERR_SET_ATTRIBUTE_FAILED,
					"job %d.%d failed to set " ATTR_PROC_ID "=%d (%d)",
					key.cluster, key.proc,
					key.proc, errno);
			}
			return -1;
		}

		// For now, we make sure to set the JobStatus attribute in the proc ad, note that we may actually be
		// fetching this from a chained parent ad.  this is the ONLY attribute that we want to pick up
		// out of the chained parent and push into the child.
		// we force this into the proc ad because the code in the schedd that calculates per-cluster
		// and per-owner totals by state doesn't work if this attribute is missing in the proc ads.
		int status = IDLE;
		if ( ! ad.EvaluateAttrInt(ATTR_JOB_STATUS, status)) { status = IDLE; }
		if (SetAttributeInt (key.cluster, key.proc, ATTR_JOB_STATUS, status, saflags) == -1) {
			if (errstack) {
				errstack->pushf(who, SCHEDD_ERR_SET_ATTRIBUTE_FAILED,
					"job %d.%d failed to set " ATTR_JOB_STATUS "=%d (%d)",
					key.cluster, key.proc,
					status, errno);
			}
			return -1;
		}
	}

	// (shallow) iterate the attributes in this ad and send them to the schedd
	//
	for (const auto & it : ad) {
		const char * attr = it.first.c_str();

		// skip attributes that are forced into the other sort of ad, or have already been sent.
		int forced = IsForcedClusterProcAttribute(attr);
		if (forced) {
			// skip attributes not forced into the cluster ad and not already sent
			if (is_cluster && (forced != -1)) continue;
			// skip attributes not forced into the proc ad and not already sent
			if ( ! is_cluster && (forced != 1)) continue;
		}

		if ( ! it.second) {
			if (errstack) {
				errstack->pushf(who, SCHEDD_ERR_SET_ATTRIBUTE_FAILED,
					"job %d.%d ERROR: %s=NULL", key.cluster, key.proc, attr);
			}
			retval = -1;
			break;
		}
		rhs.clear();
		unparser.Unparse(rhs, it.second);

		if (SetAttribute(key.cluster, key.proc, attr, rhs.c_str(), saflags) == -1) {
			if (errstack) {
				errstack->pushf(who, SCHEDD_ERR_SET_ATTRIBUTE_FAILED,
					"job %d.%d failed to set %s=%s (%d)", key.cluster, key.proc, attr, rhs.c_str(), errno);
			}
			retval = -1;
			break;
		}
	}

	return retval;
}

