/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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


#include "condor_common.h"
#include "condor_attributes.h"

#include "consumption_policy.h"


void assign_preserve_integers(ClassAd& ad, const char* attr, double v) {
    if ((v - floor(v)) > 0.0) {
        ad.Assign(attr, v);
    } else {
        ad.Assign(attr, (long long)(v));
    }
}


bool cp_supports_policy(ClassAd& resource, bool strict) {
    // currently, only p-slots can support a functional consumption policy
    if (strict) {
        bool part = false;
        if (!resource.LookupBool(ATTR_SLOT_PARTITIONABLE, part)) part = false;
        if (!part) return false;
    }

    // must support MachineResources attribute
	std::string mrv;
    if (!resource.LookupString(ATTR_MACHINE_RESOURCES, mrv)) return false;

    // must define ConsumptionXxx for all resources Xxx (including extensible resources)
    for (auto& asset: StringTokenIterator(mrv)) {
        if (MATCH == strcasecmp(asset.c_str(), "swap")) continue;
		std::string ca;
        formatstr(ca, "%s%s", ATTR_CONSUMPTION_PREFIX, asset.c_str());
        if (! resource.Lookup(ca)) return false;
    }

    return true;
}


void cp_compute_consumption(ClassAd& job, ClassAd& resource, consumption_map_t& consumption) {
    consumption.clear();

	std::string mrv;
    if (!resource.LookupString(ATTR_MACHINE_RESOURCES, mrv)) {
        EXCEPT("Resource ad missing %s attribute", ATTR_MACHINE_RESOURCES);
    }

    for (auto& asset: StringTokenIterator(mrv)) {
        if (MATCH == strcasecmp(asset.c_str(), "swap")) continue;

		std::string ra;
		std::string coa;
        formatstr(ra, "%s%s", ATTR_REQUEST_PREFIX, asset.c_str());
        formatstr(coa, "_condor_%s", ra.c_str());
        bool override = false;
        double ov=0;
        if (job.LookupFloat(coa, ov)) {
            // Allow _condor_RequestedXXX to override RequestedXXX
            // this case is intended to be operative when a scheduler has set 
            // such values and sent them on to the startd that owns this resource
            // (e.g. I'd not expect this case to arise elsewhere, like the negotiator)
			std::string ta;
            formatstr(ta, "_cp_temp_%s", ra.c_str());
            CopyAttribute(ta, job, ra);
            job.Assign(ra, ov);
            override = true;
        }

        // get the requested asset value
        bool missing = false;
        if (!job.Lookup(ra)) {
            // a RequestXxx attribute not present - default to zero
            job.Assign(ra, 0);
            missing = true;
        }

        // compute the consumed value for the asset
		std::string ca;
        formatstr(ca, "%s%s", ATTR_CONSUMPTION_PREFIX, asset.c_str());
        double cv = 0;
        if (!EvalFloat(ca.c_str(), &resource, &job, cv) || (cv < 0)) {
			std::string name;
            resource.LookupString(ATTR_NAME, name);
            dprintf(D_ALWAYS, "WARNING: consumption policy for %s on resource %s failed to evaluate to a non-negative numeric value\n", ca.c_str(), name.c_str());
            // flag this failure with a negative value, for the benefit of cp_sufficient_assets()
            // if it evaluated to a negative value, preserve that value for informational purposes
            if (cv >= 0) cv = -999;
        }
        consumption[asset] = cv;

        if (override) {
            // restore saved value for RequestedXXX if it was overridden by _condor_RequestedXXX
			std::string ta;
            formatstr(ta, "_cp_temp_%s", ra.c_str());
            CopyAttribute(ra, job, ta);
            job.Delete(ta.c_str());
        }
        if (missing) {
            // remove temporary attribute to restore original state
            job.Delete(ra.c_str());
        }
    }
}


bool cp_sufficient_assets(ClassAd& job, ClassAd& resource) {
    consumption_map_t consumption;
    cp_compute_consumption(job, resource, consumption);
    return cp_sufficient_assets(resource, consumption);
}


bool cp_sufficient_assets(ClassAd& resource, const consumption_map_t& consumption) {
    int npos = 0;
    for (consumption_map_t::const_iterator j(consumption.begin());  j != consumption.end();  ++j) {
        const char* asset = j->first.c_str();
        double av=0;
        if (!resource.LookupFloat(asset, av)) {
            EXCEPT("Missing %s resource asset", asset);
        }
        if (av < j->second) {
            return false;
        }
        if (j->second < 0) {
			std::string name;
            resource.LookupString(ATTR_NAME, name);
            dprintf(D_ALWAYS, "WARNING: Consumption for asset %s on resource %s was negative: %g\n", asset, name.c_str(), j->second);
            return false;
        }
        if (j->second > 0) npos += 1;
    }

    if (npos <= 0) {
		std::string name;
        resource.LookupString(ATTR_NAME, name);
        dprintf(D_ALWAYS, "WARNING: Consumption for all assets on resource %s was zero\n", name.c_str());
        return false;
    }

    return true;
}


double cp_deduct_assets(ClassAd& job, ClassAd& resource, bool test) {
    consumption_map_t consumption;
    cp_compute_consumption(job, resource, consumption);

    // slot weight before asset deductions
    double w0 = 0;
    if (!resource.LookupFloat(ATTR_SLOT_WEIGHT, w0)) {
        EXCEPT("Failed to evaluate %s", ATTR_SLOT_WEIGHT);
    }

    // deduct consumption from the resource assets
    for (consumption_map_t::iterator j(consumption.begin());  j != consumption.end();  ++j) {
        const char* asset = j->first.c_str();
        double av=0;
        if (!resource.LookupFloat(asset, av)) {
            EXCEPT("Missing %s resource asset", asset);
        }
        assign_preserve_integers(resource, asset, av - j->second);
    }

    // slot weight after deductions
    double w1 = 0;
    if (!resource.LookupFloat(ATTR_SLOT_WEIGHT, w1)) {
        EXCEPT("Failed to evaluate %s", ATTR_SLOT_WEIGHT);
    }

    // define cost as difference in slot weight before and after asset deduction
    double cost = w0 - w1;

    // if we are not in testing mode, then keep the asset deductions
    if (!test) return cost;

    // The Dude just wants his assets back
    for (consumption_map_t::iterator j(consumption.begin());  j != consumption.end();  ++j) {
        const char* asset = j->first.c_str();
        double av=0;
        resource.LookupFloat(asset, av);
        assign_preserve_integers(resource, asset, av + j->second);
    }

    return cost;
}


void cp_override_requested(ClassAd& job, ClassAd& resource, consumption_map_t& consumption) {
    cp_compute_consumption(job, resource, consumption);

    for (consumption_map_t::iterator j(consumption.begin());  j != consumption.end();  ++j) {
        const char* asset = j->first.c_str();

		std::string ra;
        formatstr(ra, "%s%s", ATTR_REQUEST_PREFIX, asset);

        // If there was no RequestXXX to begin with, don't screw things up by creating one
        if (!job.Lookup(ra)) continue;

		std::string oa;
        formatstr(oa, "_cp_orig_%s%s", ATTR_REQUEST_PREFIX, asset);
        CopyAttribute(oa, job, ra);
        assign_preserve_integers(job, ra.c_str(), j->second);
    }
}


void cp_restore_requested(ClassAd& job, const consumption_map_t& consumption) {
    for (consumption_map_t::const_iterator j(consumption.begin());  j != consumption.end();  ++j) {
        const char* asset = j->first.c_str();
		std::string ra;
		std::string oa;
        formatstr(ra, "%s%s", ATTR_REQUEST_PREFIX, asset);
        formatstr(oa, "_cp_orig_%s%s", ATTR_REQUEST_PREFIX, asset);
        CopyAttribute(ra, job, oa);
        job.Delete(oa);
    }    
}
