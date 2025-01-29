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

void
Resource::reqexp_config( ) // formerly compute(A_STATIC)
{
	// note that this param call will pick up the slot_type param overrides
	if (r_reqexp.origstart) { free(r_reqexp.origstart); }
	r_reqexp.origstart = param( "START" );

	if (r_reqexp.within_resource_limits) { free(r_reqexp.within_resource_limits); }
	r_reqexp.within_resource_limits = param(ATTR_WITHIN_RESOURCE_LIMITS);
	if (r_reqexp.within_resource_limits) {
		// WithinResourceLimits set by param, unusual, but if it is, we are done here...
		dprintf(D_FULLDEBUG, "config " ATTR_WITHIN_RESOURCE_LIMITS " = %s\n", r_reqexp.within_resource_limits);
		return;
	}

	// The WithinResourceLimits expression can be set by param, but it usually isn't
	// instead we usually build it up by iterating the names of the resources
	// The clauses here a different when consumption policy is in place than when it isn't

	bool has_consumption_policy = r_has_cp || (get_parent() && get_parent()->r_has_cp);
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
                for (auto j(r_attr->get_slotres_map().begin());  j != r_attr->get_slotres_map().end();  ++j) {
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

                r_reqexp.within_resource_limits = strdup(estr.c_str());
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

			const CpuAttributes::slotres_map_t& resmap = r_attr->get_slotres_map();
			if (resmap.empty()) {
				r_reqexp.within_resource_limits = strdup(climit);
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
				r_reqexp.within_resource_limits = strdup(wrlimit.c_str());
			}
	}
	dprintf(D_FULLDEBUG, ATTR_WITHIN_RESOURCE_LIMITS " = %s\n", r_reqexp.within_resource_limits);
}

bool
Resource::reqexp_restore()
{
	if (isSuspendedForCOD()) {
		if (r_reqexp.rstate != COD_REQ ) {
			reqexp_set_state(COD_REQ);
			return true;
		} else {
			return false;
		}
	} else {
		caDeleteThruParent(r_classad, ATTR_RUNNING_COD_JOB );
	}
	if (resmgr->isShuttingDown() || isDraining()) {
		if (r_reqexp.rstate != UNAVAIL_REQ) {
			reqexp_unavail( isDraining() ? getDrainingExpr() : nullptr );
			return true;
		}
		return false;
	}
	if (r_reqexp.rstate != NORMAL_REQ) {
		reqexp_set_state(NORMAL_REQ);
		return true;
	}
	return false;
}

void
Resource::reqexp_unavail(const ExprTree * start_expr)
{
	if (isSuspendedForCOD()) {
		if (r_reqexp.rstate != COD_REQ) {
			reqexp_set_state(COD_REQ);
		}
		return;
	}

	if (r_reqexp.drainingStartExpr) delete r_reqexp.drainingStartExpr;
	if (start_expr) {
		r_reqexp.drainingStartExpr = start_expr->Copy();
	} else {
		r_reqexp.drainingStartExpr = nullptr;
	}
	reqexp_set_state(UNAVAIL_REQ);
}


// internal publish
// when reverting to ORIG_REQ, we populate the config classad and clear the r_classad of the associated Resource
// for other states, we publish into the r_classad, which hides but does not remove things from the config classad
void
Resource::reqexp_set_state(reqexp_state rst)
{
	ClassAd * cb = r_config_classad;
	ClassAd * ca = r_classad;

	// unchain so that edits to r_classad don't alter the parent ad
	ClassAd * parent = ca->GetChainedParentAd();
	ca->Unchain();

	// If the requirements state is normal, we use the config requirements
	// otherwise, we use some sort of override in the resource classad
	r_reqexp.rstate = rst;
	if (r_reqexp.rstate == NORMAL_REQ) {
		ca->Delete(ATTR_START);
		ca->Delete(ATTR_REQUIREMENTS);
		ca->Delete(ATTR_WITHIN_RESOURCE_LIMITS);
		ca->Delete(ATTR_RUNNING_COD_JOB);
		publish_requirements(cb);
	} else {
		publish_requirements(ca);
	}

	if (parent) ca->ChainToAd(parent);
}

void
Resource::publish_requirements( ClassAd* ca )
{
	const char * static_slot_requirements = ATTR_START;
	const char * pslot_requirements = ATTR_START " && (" ATTR_WITHIN_RESOURCE_LIMITS ")";
	const char * cod_requirements = "False && " ATTR_RUNNING_COD_JOB;

	// For cod, we don't publish START at all, and set Requirements to "False && Cod"
	if (r_attr->is_broken()) {
		// broken slots set Requirements equal to the broken reason
		// this prevents matching while also making the reason visible to -analyze
		std::string broken_reason;
		r_attr->is_broken(&broken_reason);
		if (broken_reason.empty()) { broken_reason = "Broken"; }
		ca->Assign(ATTR_REQUIREMENTS, broken_reason);
	} else if (r_reqexp.rstate == COD_REQ) {
		ca->Assign(ATTR_RUNNING_COD_JOB, true);
		ca->AssignExpr(ATTR_REQUIREMENTS, cod_requirements);
	} else if (r_reqexp.rstate == UNAVAIL_REQ && ! r_reqexp.drainingStartExpr) {
		// if draining without a draining start expression Requirements is also false
		ca->Assign(ATTR_REQUIREMENTS, false);
	} else {
		// for all others, START controls matching
		if (r_reqexp.rstate == UNAVAIL_REQ) {
			ca->Insert(ATTR_START, r_reqexp.drainingStartExpr->Copy());
		} else if (r_reqexp.origstart) {
			if ( ! ca->AssignExpr(ATTR_START, r_reqexp.origstart)) {
				ca->AssignExpr(ATTR_START, "error");
			}
		} else {
			ca->Assign(ATTR_START, true);
		}

		// and Requirements is based on the slot type
		if (Resource::STANDARD_SLOT != get_feature()) {
			ca->AssignExpr(ATTR_REQUIREMENTS, pslot_requirements);
			ca->AssignExpr(ATTR_WITHIN_RESOURCE_LIMITS, r_reqexp.within_resource_limits);
		} else {
			ca->AssignExpr(ATTR_REQUIREMENTS, static_slot_requirements);
		}
	}
}

