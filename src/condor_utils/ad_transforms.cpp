/***************************************************************
 *
 * Copyright (C) 1990-2020, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_config.h"
#include "ad_transforms.h"
#include "condor_attributes.h"

void 
AdTransforms::config(const char * param_prefix)
{
	int rval;

	// Setup xform_utils hashtable, and record a ckpt
	m_mset.clear();
	m_mset.init();
	m_mset_ckpt = m_mset.save_state();

	m_transforms_list.clear();

	std::string adtransNames;
	if( !param( adtransNames, (std::string(param_prefix) + "_TRANSFORM_NAMES").c_str() ) ) {
		return;
	}
	StringList nameList(adtransNames.c_str());
	nameList.rewind();
	const char *name = nullptr;
	while( (name = nameList.next()) ) {

		if (!strcasecmp(name, "NAMES") ) { continue; }  // prevent recursion!

		std::string attributeName = std::string(param_prefix) + "_TRANSFORM_" + name;

		// fetch unexpanded param, we will only expand it now for the old (classad) style transforms.
		const char * raw_transform_text = param_unexpanded(attributeName.c_str());
		if (!raw_transform_text) {
			dprintf(D_ALWAYS, (std::string(param_prefix) + "_TRANSFORM_%s not defined, ignoring.\n").c_str(), name);
			continue;
		}

		std::unique_ptr<MacroStreamXFormSource> xfm(new MacroStreamXFormSource(name));

		// Always assume that the native xform is used, not the
		// new-ClassAd style used by the JobRouter and JOB_TRANSFORM_*
		std::string errmsg = "";
		int offset = 0;
		if ( (rval=xfm->open(raw_transform_text, offset, errmsg)) < 0 ) {
			dprintf(D_ALWAYS, (std::string(param_prefix) + "_TRANSFORM_%s macro stream malformed, ignoring. (err=%d) %s\n").c_str(),
				name, rval, errmsg.c_str());
			continue;
		}

		// Finally, append the xfm to the end our list (order is important here!)
		m_transforms_list.emplace_back(std::move(xfm));
		std::string xfm_text;
		dprintf(D_ALWAYS, 
			(std::string(param_prefix) + "_TRANSFORM_%s setup as transform rule #%lu :\n%s\n").c_str(),
			name, m_transforms_list.size(), m_transforms_list.back()->getFormattedText(xfm_text, "\t") );
	}
}

int
AdTransforms::transform(ClassAd *ad, CondorError *errorStack)
{
	if (!shouldTransform()) {return 0;}

	int transforms_applied = 0;
	int transforms_considered = 0;
	StringList attrs_changed;
	int rval;
	std::string errmsg;
	std::string applied_names;

	// Revert variables hashtable so it doesn't grow idefinitely
	m_mset.rewind_to_state(m_mset_ckpt, false);

	// Apply our ordered list of transforms to the ad, changing it in-place.
	for (const auto &xfm : m_transforms_list) {
		transforms_considered++;

		if (!xfm->matches(ad)) {continue;}

		rval = TransformClassAd(ad, *xfm, m_mset, errmsg);

		if (rval < 0) {
			dprintf(D_ALWAYS,
				"ad transforms: ERROR applying transform %s (err=-3,rval=%d,msg=%s)\n",
				xfm->getName(), rval, errmsg.c_str());
			if (errorStack)
				errorStack->pushf("TRANSFORM", 3, "ERROR applying transform %s: %s",
					xfm->getName(), errmsg.c_str());
			return -3;
		} 

		if (IsFulldebug(D_FULLDEBUG)) {
			if (transforms_applied) applied_names += ",";
			applied_names += xfm->getName();
		}
		transforms_applied++;
	}
	
	dprintf( D_FULLDEBUG,
		"ad transform: %d considered, %d applied (%s)\n",
		transforms_considered, transforms_applied,
		transforms_applied > 0 ? applied_names.c_str() : "<none>" );

	return 0;
}
