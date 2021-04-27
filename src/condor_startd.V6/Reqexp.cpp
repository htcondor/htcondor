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

/*
    This file implements the Reqexp class, which contains methods and
    data to manipulate the requirements expression of a given
    resource.

   	Written 9/29/97 by Derek Wright <wright@cs.wisc.edu>
*/

#include "condor_common.h"
#include "startd.h"
#include "consumption_policy.h"
#include <set>
using std::set;

Reqexp::Reqexp( Resource* rip )
{
	this->m_rip = rip;
	std::string tmp;

	tmp = "START";

	if( Resource::STANDARD_SLOT != rip->get_feature() ) {
		formatstr_cat( tmp, " && (%s)", ATTR_WITHIN_RESOURCE_LIMITS );
	}

	origreqexp = strdup( tmp.c_str() );
	origstart = NULL;
	rstate = ORIG_REQ;
	m_within_resource_limits_expr = NULL;
	drainingStartExpr = NULL;
}


char * Reqexp::param(const char * name) {
	if (m_rip) return SlotType::param(m_rip->r_attr, name);
	return param(name);
}

void
Reqexp::config( ) // formerly compute(A_STATIC)
{
	// note that this param call will pick up the slot_type param overrides
	if (origstart) { free(origstart); }
	origstart = param( "START" );
	if( !origstart ) {
		EXCEPT( "START expression not defined!" );
	}

	if (m_within_resource_limits_expr) { free(m_within_resource_limits_expr); }
	m_within_resource_limits_expr = param(ATTR_WITHIN_RESOURCE_LIMITS);
	if (m_within_resource_limits_expr) {
		// WithinResourceLimits set by param, unusual, but if it is, we are done here...
		dprintf(D_FULLDEBUG, "config " ATTR_WITHIN_RESOURCE_LIMITS " = %s\n", m_within_resource_limits_expr);
		return;
	}

	// The WithinResourceLimits expression can be set by param, but it usually isn't
	// instead we usually build it up by iterating the names of the resources
	// The clauses here a different when consumption policy is in place than when it isn't

	bool has_consumption_policy = m_rip->r_has_cp || (m_rip->get_parent() && m_rip->get_parent()->r_has_cp);
	if (has_consumption_policy) {
			// In the below, _condor_RequestX attributes may be explicitly set by
			// the schedd; if they are not set, go with the RequestX that derived from
			// the user's original submission.
                dprintf(D_FULLDEBUG, "Using CP variant of WithinResourceLimits\n");
                // a CP-supporting p-slot, or a d-slot derived from one, gets variation
                // that supports zeroed resource assets, and refers to consumption
                // policy attributes.

                // reconstructing this isn't a big deal, but I'm doing it because I'm 
                // afraid to randomly perterb the order of the resource initialization 
                // spaghetti, which makes kittens cry.
                std::set<std::string,  classad::CaseIgnLTStr> assets;
                assets.insert("Cpus");
                assets.insert("Memory");
                assets.insert("Disk");
                for (auto j(m_rip->r_attr->get_slotres_map().begin());  j != m_rip->r_attr->get_slotres_map().end();  ++j) {
                    if (MATCH == strcasecmp(j->first.c_str(),"swap")) continue;
                    assets.insert(j->first);
                }

                // first subexpression does not need && operator:
                bool need_and = false;
                string estr = "(";
                for (auto j(assets.begin());  j != assets.end();  ++j) {
                    //string rname(*j);
                    //*(rname.begin()) = toupper(*(rname.begin()));
                    string te;
                    // The logic here is that if the target job ad is in a mode where its RequestXxx have
                    // already been temporarily overridden with the consumption policy values, then we want
                    // to use RequestXxx (note, this will include any overrides by _condor_RequestXxx).
                    // Otherwise, we want to refer to ConsumptionXxx.
                    formatstr(te, "ifThenElse(TARGET._cp_orig_Request%s isnt UNDEFINED, TARGET.Request%s <= MY.%s, MY.Consumption%s <= MY.%s)", 
                        /*Request*/j->c_str(), /*Request*/j->c_str(), /*MY.*/j->c_str(),
                        /*Consumption*/j->c_str(), /*MY.*/j->c_str());
                    if (need_and) estr += " && ";
                    estr += te;
                    need_and = true;
                }
                estr += ")";

                m_within_resource_limits_expr = strdup(estr.c_str());
	} else {
			static const char * climit_full =
				"("
				 "ifThenElse(TARGET._condor_RequestCpus =!= UNDEFINED,"
					"MY.Cpus > 0 && TARGET._condor_RequestCpus <= MY.Cpus,"
					"ifThenElse(TARGET.RequestCpus =!= UNDEFINED,"
						"MY.Cpus > 0 && TARGET.RequestCpus <= MY.Cpus,"
						"1 <= MY.Cpus))"
				" && "
				 "ifThenElse(TARGET._condor_RequestMemory =!= UNDEFINED,"
					"MY.Memory > 0 && TARGET._condor_RequestMemory <= MY.Memory,"
					"ifThenElse(TARGET.RequestMemory =!= UNDEFINED,"
						"MY.Memory > 0 && TARGET.RequestMemory <= MY.Memory,"
						"FALSE))"
				" && "
				 "ifThenElse(TARGET._condor_RequestDisk =!= UNDEFINED,"
					"MY.Disk > 0 && TARGET._condor_RequestDisk <= MY.Disk,"
					"ifThenElse(TARGET.RequestDisk =!= UNDEFINED,"
						"MY.Disk > 0 && TARGET.RequestDisk <= MY.Disk,"
						"FALSE))"
				")";

			// This one assumes job._condor_Request* attributes never present
			//  and job.Request* is always set to some value.  If 
			//  if job.RequestCpus is undefined, job won't match, instead of defaulting to one Request cpu
			static const char *climit_simple = 
			"("
				"MY.Cpus > 0 && TARGET.RequestCpus <= MY.Cpus && "
				"MY.Memory > 0 && TARGET.RequestMemory <= MY.Memory && "
				"MY.Disk > 0 && TARGET.RequestDisk <= MY.Disk"
			")"; 

			static const char *climit = nullptr;
	
			// We can build the WithinResourceLimits expression with or without the
			// JOB._condor_request* sub expressions.  We did it with them for many years
			// but they were never used, and added a lot to negotiation overhead, so now
			// these we build by default without them.
			const bool job_has_request_attrs = param_boolean("STARTD_JOB_HAS_REQUEST_ATTRS", false);
			if (job_has_request_attrs) {
				climit = climit_full;
			} else {
				climit = climit_simple;
			}

			const CpuAttributes::slotres_map_t& resmap = m_rip->r_attr->get_slotres_map();
			if (resmap.empty()) {
				m_within_resource_limits_expr = strdup(climit);
			} else {
				// start by copying all but the last ) of the pre-defined resources expression
				std::string wrlimit(climit,strlen(climit)-1);
				// then append the expressions for the user defined resource types
				CpuAttributes::slotres_map_t::const_iterator it(resmap.begin());
				for ( ; it != resmap.end();  ++it) {
					const char * rn = it->first.c_str();
					if (job_has_request_attrs) {
							formatstr_cat(wrlimit,
							" && "
							 "(TARGET.Request%s is UNDEFINED ||"
								"MY.%s >= ifThenElse(TARGET._condor_Request%s is UNDEFINED,"
									"TARGET.Request%s,"
									"TARGET._condor_Request%s)"
							 ")",
							rn, rn, rn, rn, rn);
					} else {
							formatstr_cat(wrlimit,
							" && "
							 "(TARGET.Request%s is UNDEFINED ||"
								"MY.%s >= TARGET.Request%s)",
							rn, rn, rn);
					}
				}
				// then append the final closing )
				wrlimit += ")";
				m_within_resource_limits_expr = strdup(wrlimit.c_str());
			}
	}
	dprintf(D_FULLDEBUG, ATTR_WITHIN_RESOURCE_LIMITS " = %s\n", m_within_resource_limits_expr);
}


Reqexp::~Reqexp()
{
	if( origreqexp ) free( origreqexp );
	if( origstart ) free( origstart );
	if( m_within_resource_limits_expr ) free( m_within_resource_limits_expr );
	if( drainingStartExpr ) { delete drainingStartExpr; }
}

extern ExprTree * globalDrainingStartExpr;

bool
Reqexp::restore()
{
	if (m_rip->isSuspendedForCOD()) {
		if( rstate != COD_REQ ) {
			rstate = COD_REQ;
			publish();
			return true;
		} else {
			return false;
		}
	} else {
		caDeleteThruParent(m_rip->r_classad, ATTR_RUNNING_COD_JOB );
	}
	if( resmgr->isShuttingDown() || m_rip->isDraining() ) {
		if( rstate != UNAVAIL_REQ ) {
			unavail( m_rip->isDraining() ? globalDrainingStartExpr : NULL );
			return true;
		}
		return false;
	}
	if( rstate != ORIG_REQ) {
		rstate = ORIG_REQ;
		publish();
		return true;
	}
	return false;
}

void
Reqexp::unavail( ExprTree * start_expr )
{
	if( m_rip->isSuspendedForCOD() ) {
		if( rstate != COD_REQ ) {
			rstate = COD_REQ;
			publish();
		}
		return;
	}
	rstate = UNAVAIL_REQ;

	if( start_expr ) {
		drainingStartExpr = start_expr->Copy();
	} else {
		drainingStartExpr = NULL;
	}
	publish();
}


// internal publish
// when reverting to ORIG_REQ, we populate the config classad and clear the r_classad of the associated Resource
// for other states, we publish into the r_classad, which hides but does not remove things from the config classad
void
Reqexp::publish()
{
	ClassAd * cb = m_rip->r_config_classad;
	ClassAd * ca = m_rip->r_classad;

	ClassAd * parent = ca->GetChainedParentAd();
	ca->Unchain();

	switch (rstate)
	{
	case ORIG_REQ:
		ca->Delete(ATTR_START);
		ca->Delete(ATTR_REQUIREMENTS);
		ca->Delete(ATTR_WITHIN_RESOURCE_LIMITS);
		ca->Delete(ATTR_RUNNING_COD_JOB);
		publish_external(cb);
		break;

	case UNAVAIL_REQ:
	case COD_REQ:
		publish_external(ca);
		break;
	}

	if (parent) ca->ChainToAd(parent);
}

void
Reqexp::publish_external( ClassAd* ca )
{
	switch( rstate ) {
	case ORIG_REQ:
		ca->AssignExpr( ATTR_START, origstart );
		ca->AssignExpr( ATTR_REQUIREMENTS, origreqexp );
		if( Resource::STANDARD_SLOT != m_rip->get_feature() ) {
			ca->AssignExpr( ATTR_WITHIN_RESOURCE_LIMITS,
							m_within_resource_limits_expr );
		}
		break;
	case UNAVAIL_REQ:
		if(! drainingStartExpr) {
			ca->AssignExpr( ATTR_REQUIREMENTS, "False" );
		} else {
			// Insert()ing an ExprTree transfers ownership for some reason.
			ExprTree * sacrifice = drainingStartExpr->Copy();
			ca->Insert( ATTR_START, sacrifice );

			ca->AssignExpr( ATTR_REQUIREMENTS, origreqexp );
			if( Resource::STANDARD_SLOT != m_rip->get_feature() ) {
				ca->AssignExpr( ATTR_WITHIN_RESOURCE_LIMITS,
								m_within_resource_limits_expr );
			}
		}
		break;
	case COD_REQ:
		ca->Assign(ATTR_RUNNING_COD_JOB, true);
		ca->AssignExpr( ATTR_REQUIREMENTS, "False && " ATTR_RUNNING_COD_JOB );
		break;
	default:
		EXCEPT("Programmer error in Reqexp::publish()!");
		break;
	}
}


void
Reqexp::dprintf( int flags, const char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	m_rip->dprintf_va( flags, fmt, args );
	va_end( args );
}

