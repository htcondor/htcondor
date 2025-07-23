/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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
#include "job_transforms.h"
#include "condor_attributes.h"
#include "qmgmt.h"
#include "condor_qmgr.h"

JobTransforms::JobTransforms() : mset_ckpt(NULL)
{
	
}

JobTransforms::~JobTransforms()
{
	clear_transforms_list();
}

void
JobTransforms::clear_transforms_list()
{
	for (MacroStreamXFormSource *xfm : transforms_list) {
		delete xfm;
	}
	transforms_list.clear();
}

void 
JobTransforms::initAndReconfig()
{
	int rval = 0;

	// Setup xform_utils hashtable, and record a ckpt
	mset.clear();
	mset.init();
	mset_ckpt = mset.save_state();

	// Clear out any previously configured transforms.
	clear_transforms_list();

	// Now setup our transforms_list from info in the config file.
	std::string jobtransNames;
	if( !param( jobtransNames, "JOB_TRANSFORM_NAMES" ) ) {
		// No job transforms requested in the config, no more work to do
		return;
	}
	MacroStreamXFormSource *xfm = nullptr;
	for (auto& name: StringTokenIterator(jobtransNames)) {

		if( strcasecmp( name.c_str(), "NAMES" ) == 0 ) { continue; }  // prevent recursion!
		std::string attributeName;
		formatstr( attributeName, "JOB_TRANSFORM_%s", name.c_str() );

		// fetch unexpanded param, we will only expand it now for the old (classad) style transforms.
		// this pointer does not need to be freed.
		const char * raw_transform_text = param_unexpanded( attributeName.c_str() );
		if( !raw_transform_text ) {
			dprintf( D_ALWAYS, "JOB_TRANSFORM_%s not defined, ignoring.\n", name.c_str() );
			continue;
		}
		// skip leading whitespace (I think the param code does this, but...)
		while (isspace(*raw_transform_text)) { ++raw_transform_text; }
		if ( !raw_transform_text[0] ) {
			dprintf( D_ALWAYS, "JOB_TRANSFORM_%s definition is empty, ignoring.\n", name.c_str() );
			continue;
		}

		// Create a fresh new xfm object, since not sure if the state of
		// this object is clean if errors were encountered instantiating a 
		// previous transform rule.
		if (xfm) delete xfm;
		xfm = new MacroStreamXFormSource(name.c_str());
		ASSERT(xfm);

		// Load transform rule from the config param into the xfm object.  If
		// the config param starts with a '[' (after trimming out leading whitespace above)
		// then assume the rule is in the form of a new classad.
		if ( raw_transform_text[0] == '[' ) {
			// Fetch transform with macro expansion
			std::string transform;
			param( transform, attributeName.c_str() );

			// Transform rule is in the form of a job_router style ClassAd, so
			// call the helper XFormLoadFromJobRouterRoute() to convert it.
			std::string empty;
			int offset=0;
			classad::ClassAdParser parser;
			ClassAd transformAd;
			rval = 0;
			if ( (!parser.ParseClassAd(transform, transformAd, offset)) ||
				 ((rval=XFormLoadFromClassadJobRouterRoute(*xfm,empty,offset,transformAd,0)) < 0) )
			{
				dprintf( D_ALWAYS, "JOB_TRANSFORM_%s classad malformed, ignoring. (err=%d)\n",
					name.c_str(), rval );
				continue;
			}
		} else {
			// Transform rule is in the native xform macro stream style, so load it
			// in that way without macro expanding at this time.
			std::string errmsg = "";
			int offset = 0;
			if ( (rval=xfm->open(raw_transform_text, offset, errmsg)) < 0 ) {
				dprintf( D_ALWAYS, "JOB_TRANSFORM_%s macro stream malformed, ignoring. (err=%d) %s\n",
					name.c_str(), rval, errmsg.c_str() );
				continue;
			}
		}

		// Perform a smoke test of the transform rule in an attempt to weed out
		// broken transform rules at reconfig time, so that we don't attempt to 
		// apply a broken rule over and over on each job submit.  
		// TODO

		// Finally, append the xfm to the end our list (order is important here!)
		transforms_list.push_back(xfm);
		std::string xfm_text;
		dprintf(D_ALWAYS, 
			"JOB_TRANSFORM_%s setup as transform rule #%zu :\n%s\n",
			name.c_str(), transforms_list.size(), xfm->getFormattedText(xfm_text, "\t") );
		xfm = nullptr;  // we handed the xfm pointer off to our list

	} // end of while loop thru job transform names	
	if (xfm) delete xfm; 
}

int
JobTransforms::transformJob(
	ClassAd *ad,
	const PROC_ID & jid, // the ad transforms will be written to. (may differ from the ad being transformed)
	classad::References * xform_attrs,
	CondorError * /* errorStack */,
	bool is_late_mat /*= false*/)
{
	int transforms_applied = 0;
	int transforms_considered = 0;
	int rval = 0;
	std::string errmsg;
	std::string applied_names;

	// Bail out early if there is no work we need to do
	if (!shouldTransform()) {
		return 0;
	}

	// Revert variables hashtable so it doesn't grow idefinitely
	mset.rewind_to_state(mset_ckpt, false);
	mset.set_factory_vars(jid.proc < 0, is_late_mat); // make ids visible to transform as temp variables

	// Enable dirty tracking of ad attributes and mark them as clean,
	// since after the transform we need to discover which attributes changed.
	ad->EnableDirtyTracking();
	ad->ClearAllDirtyFlags();

	// Apply our ordered list of transforms to the ad, changing it in-place.
	for (MacroStreamXFormSource *xfm : transforms_list) {
	
		transforms_considered++;

		// Before applying this transform, check its requirements
		if ( ! xfm->matches(ad) ) {
			// The requirements for this transform does not match the job ad
			// TODO DPRINTF?
			continue;
		}

		rval = TransformClassAd(ad,*xfm,mset,errmsg);

		if (rval < 0) {
			// Transformation failed; errmsg should say why.
			// TODO errorStack ?
			dprintf(D_ALWAYS,
				"(%d.%d) job_transforms: ERROR applying transform %s (err=-3,rval=%d,msg=%s)\n",
				jid.cluster, jid.proc, xfm->getName(), rval, errmsg.c_str() ? errmsg.c_str() : "<none>");
			return -3;
		} 

		// Keep a count of how many transform rules we applied, and the names
		// of the transforms applied.
		transforms_applied++;
		if (transforms_applied > 1 ) {
			applied_names += ",";
		}
		applied_names += xfm->getName();

	}	// end of while-loop through transforms
	
	// So now the ad has been transformed in-place.  Now we need to push the
	// changes to the ad into the transaction by calling SetAttribute() on each
	// dirty attribute.
	if (transforms_applied > 0) {
		rval = set_dirty_attributes(ad, jid.cluster, jid.proc, xform_attrs);
		if ( rval < 0 ) {
			// TODO errorStack?
			dprintf(D_ALWAYS,
				"(%d.%d) job_transforms: ERROR applying transforms (err=-4,rval=%d)\n",
				jid.cluster, jid.proc, rval);
			return -4;
		}
	}

	dprintf( D_ALWAYS, 
		"job_transforms for %d.%d: %d considered, %d applied (%s)\n",
		jid.cluster,jid.proc,transforms_considered,transforms_applied,
		transforms_applied > 0 ? applied_names.c_str() : "<none>" );

	return 0;
}

int
JobTransforms::set_dirty_attributes(ClassAd *ad, int cluster, int proc, classad::References * attrs /*=nullptr*/)
{
	int num_attrs_set = 0;
	const char *rhstr = nullptr;
	ExprTree * tree = nullptr;

	// make sure we don't write the ProdId attribute here
	// If we are setting dirty attributes into a cluster ad it would be a fatal
	// and for a proc ad, it is unnecessary
	ad->MarkAttributeClean(ATTR_PROC_ID);

	for ( auto it = ad->dirtyBegin();
		  it != ad->dirtyEnd(); ++it ) 
	{
		rhstr = nullptr;
		tree = ad->Lookup( *it );
		if ( tree ) {
			rhstr = ExprTreeToString( tree );
		} else {
			// If an attribute is marked as dirty but Lookup() on this
			// attribute fails, it means that attribute was deleted.
			// We handle this by inserting into the transaction log a
			// SetAttribute to UNDEFINED, which will work properly even if we
			// are dealing with a chained job ad that has the attribute set differently
			// in the cluster ad.
			rhstr = "UNDEFINED";
		}
		if( !rhstr) { 
			dprintf(D_ALWAYS,"(%d.%d) job_transforms: Problem processing classad\n",
				cluster, proc);
			return -1;
		}
		dprintf(D_FULLDEBUG, "(%d.%d) job_transforms: Setting %s = %s\n",
				cluster, proc, it->c_str(), rhstr);
		if( SetAttribute(cluster, proc, it->c_str(), rhstr, SetAttribute_SubmitTransform) == -1 ) {
			dprintf(D_ALWAYS,"(%d.%d) job_transforms: Failed to set %s = %s\n",
				cluster, proc, it->c_str(), rhstr);
			return -2;
		}
		if (attrs) { attrs->insert(*it); }
		num_attrs_set++;
	}

	return num_attrs_set;
}

