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
	r_reqexp.origstart.set(param("START"));

	r_reqexp.normal_clauses.clear();

	classad::References std_clauses{ATTR_START, ATTR_WITHIN_RESOURCE_LIMITS};
	bool include_healthy_clause = resmgr->config_classad->Lookup(ATTR_HEALTHY);
	if (include_healthy_clause) std_clauses.insert(ATTR_HEALTHY);

	auto_free_ptr gen_clauses(param("SLOT_REQUIREMENTS_CLAUSES"));
	const char * slot_param_name = m_parent ? "DSLOT_REQUIREMENTS_CLAUSES" : "PSLOT_REQUIREMENTS_CLAUSES";
	auto_free_ptr slot_clauses(param(slot_param_name));
	if (gen_clauses || slot_clauses || include_healthy_clause) {
		r_reqexp.normal_clauses.reserve(size(std_clauses)+4);

		if (include_healthy_clause) { r_reqexp.normal_clauses.push_back(ATTR_HEALTHY); }

		// include the general sub clauses
		if (gen_clauses) {
			// when the admin specifies the slot clauses, we expect them to include Healthy in the list.
			for (auto & attr : StringTokenIterator(gen_clauses)) {
				const char * type_expr = SlotType::type_param(r_attr, attr.c_str());
				auto_free_ptr expr(::param(attr.c_str()));
				if (expr || type_expr) {
					const char * slot_expr = type_expr ? type_expr : expr.ptr();
					if (slot_expr && ! IsValidClassAdExpression(slot_expr)) {
						dprintf(D_ERROR, "No jobs will match because of parse error in SLOT_REQUIREMENTS_CLAUSE %s = %s\n", attr.c_str(), slot_expr);
					}
				} else if ( ! resmgr->config_classad->Lookup(attr)) {
					dprintf(D_ERROR, "ignoring undefined SLOT_REQUIREMENTS_CLAUSE %s\n", attr.c_str());
					continue; // don't add the clause to the clause list
				}
				// add to clause list, unless its one of the standard clauses that we add explicitly
				if (std_clauses.count(attr)) continue;
				r_reqexp.normal_clauses.push_back(attr);
				std_clauses.insert(attr);
			}
		}

		// always include these subclauses
		r_reqexp.normal_clauses.push_back("START"); // not ATTR_START because we want all caps
		r_reqexp.normal_clauses.push_back(ATTR_WITHIN_RESOURCE_LIMITS);

		// include the pslot/dslot sub clauses
		if (slot_clauses) {
			for (auto & attr : StringTokenIterator(slot_clauses)) {
				const char * type_expr = SlotType::type_param(r_attr, attr.c_str());
				auto_free_ptr expr(param(attr.c_str()));
				if (expr || type_expr) {
					const char * slot_expr = type_expr ? type_expr : expr.ptr();
					if (slot_expr && ! IsValidClassAdExpression(slot_expr)) {
						dprintf(D_ERROR, "parse error in %s %s = %s\n", slot_param_name, attr.c_str(), slot_expr);
						if (m_parent) {
							dprintf(D_ERROR, "ignoring %s %s\n", slot_param_name, attr.c_str());
							continue; // for d-slots we just omit the unparseable clause rather than failing the match
						}
					}
				} else {
					dprintf(D_ERROR, "ignoring undefined %s %s\n", slot_param_name, attr.c_str());
					continue;
				}
				if (std_clauses.count(attr)) continue;
				r_reqexp.normal_clauses.push_back(attr);
				std_clauses.insert(attr);
			}
		}
	}

	// and so will this one
	r_reqexp.wrl_from_config.set(param(ATTR_WITHIN_RESOURCE_LIMITS));
	if (r_reqexp.wrl_from_config) {
		// WithinResourceLimits set by param, unusual, but if it is, we are done here...
		dprintf(D_FULLDEBUG, "config " ATTR_WITHIN_RESOURCE_LIMITS " = %s\n", r_reqexp.wrl_from_config.ptr());
		return;
	}
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

	r_reqexp.drainingStartExpr.clear();
	if (start_expr) {
		r_reqexp.drainingStartExpr.set(start_expr->Copy());
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
		ca->Delete(ATTR_REQUIREMENTS);
		ca->Delete(ATTR_RUNNING_COD_JOB);
		if (r_reqexp.normal_clauses.empty()) {
			ca->Delete(ATTR_START);
			ca->Delete(ATTR_WITHIN_RESOURCE_LIMITS);
		} else {
			for (auto & attr : r_reqexp.normal_clauses) { ca->Delete(attr); }
		}
		publish_requirements(cb);
	} else {
		publish_requirements(ca);
	}

	if (parent) ca->ChainToAd(parent);
}

void
Resource::publish_requirements( ClassAd* ca )
{
	const char * slot_requirements = ATTR_START " && " ATTR_WITHIN_RESOURCE_LIMITS;
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
	} else if (r_reqexp.rstate == UNAVAIL_REQ && r_reqexp.drainingStartExpr.empty()) {
		// if draining without a draining start expression Requirements is also false
		ca->Assign(ATTR_REQUIREMENTS, false);
	} else {
		// for all others, START controls matching
		if (r_reqexp.rstate == UNAVAIL_REQ) {
			ca->Insert(ATTR_START, r_reqexp.drainingStartExpr.Expr()->Copy());
		} else if (r_reqexp.origstart) {
			if ( ! ca->AssignExpr(ATTR_START, r_reqexp.origstart)) {
				ca->AssignExpr(ATTR_START, "error");
			}
		} else {
			ca->Assign(ATTR_START, true);
		}

		// Set Requirements and active WithinResourceLimits expression
		if (r_reqexp.normal_clauses.empty()) {
			ca->AssignExpr(ATTR_REQUIREMENTS, slot_requirements);
		} else {
			std::string req = join(r_reqexp.normal_clauses, " && ");
			ca->AssignExpr(ATTR_REQUIREMENTS, req.c_str());

			// if publishing to non internal ads, we need to copy expressions out of the config ads
			if (ca != r_config_classad && ca != r_classad && r_config_classad) {
				for (auto & attr : r_reqexp.normal_clauses) {
					if (YourStringNoCase(ATTR_START) != attr && YourStringNoCase(ATTR_WITHIN_RESOURCE_LIMITS) != attr) {
						ExprTree * expr = r_config_classad->Lookup(attr);
						if (expr) { ca->Insert(attr, expr->Copy()); }
					}
				}
			}
		}
		const char * active_wrl;
		if (r_reqexp.wrl_from_config) {
			active_wrl = r_reqexp.wrl_from_config;
		} else if (r_has_cp || (get_parent() && get_parent()->r_has_cp)) {
			active_wrl = resmgr->m_attr->consumptionLimitsExpression();
		} else {
			active_wrl = resmgr->m_attr->withinLimitsExpression();
		}
		ca->AssignExpr(ATTR_WITHIN_RESOURCE_LIMITS, active_wrl);
	}
}

