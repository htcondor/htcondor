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


#include "condor_common.h"
#include "condor_debug.h"
#include "classad_merge.h"
#include "startd_cron_job.h"
#include "startd_named_classad.h"

// Named classAds
StartdNamedClassAd::StartdNamedClassAd( const char *name,
										StartdCronJob &job,
										ClassAd * ad /*=NULL*/)
		: NamedClassAd( name, ad ), m_job( job )
{
}

// attributes that are defined only for StartdNamedClassAds (i.e STARTD_CRON ads)
//
#define ATTR_SLOT_NAME "SlotName"
#define ATTR_SLOT_MERGE_CONSTRAINT "SlotMergeConstraint"

// initialize the list of attributes that should never be merged from a STARTD_CRON script output
// into the slot ad.  It's more efficient if these are sorted.
static const char * const dont_merge_these[] = {
	ATTR_NAME,
	ATTR_SLOT_ID,
	ATTR_SLOT_MERGE_CONSTRAINT,
	ATTR_SLOT_NAME,
	ATTR_SLOT_TYPE_ID,
};
AttrNameSet StartdNamedClassAd::dont_merge_attrs(dont_merge_these, dont_merge_these+(sizeof(dont_merge_these)/sizeof(const char *)));

bool
StartdNamedClassAd::InSlotList( unsigned slot_id ) const
{
	return m_job.Params().InSlotList( slot_id );
}


bool
StartdNamedClassAd::ShouldMergeInto(ClassAd * merge_into, const char ** pattr_used)
{
	if (pattr_used) *pattr_used = NULL;

	ClassAd * merge_from = this->GetAd();
	if ( ! merge_from || ! merge_into)
		return false;

	// if there is a SlotMergeContraint in the source, then merge into slots
	// for which the constraint evaluates to true, and don't merge otherwise.
	if (merge_from->Lookup(ATTR_SLOT_MERGE_CONSTRAINT)) {
		int matches = false;
		if (pattr_used) *pattr_used = ATTR_SLOT_MERGE_CONSTRAINT;
		merge_from->EvalBool(ATTR_SLOT_MERGE_CONSTRAINT, merge_into, matches);
		return matches;
	}

	// if there is a "Name" or "SlotName" attribute in the source, then merge into slots
	// whose name matches it as a prefix.  (i.e. if merge_from("Name") is "xfer", the merge into all slots
	// whose name starts with "xfer".  this is most useful if you use SLOT_TYPE_n_NAME_PREFIX to name slots.
	std::string from_str, to_str;
	if ( ( merge_from->LookupString(ATTR_SLOT_NAME, from_str) ||
		   merge_from->LookupString(ATTR_NAME, from_str))
		&& merge_into->LookupString(ATTR_NAME, to_str))
	{
		if (from_str.length() > 0) {
			if (pattr_used) *pattr_used = merge_from->Lookup(ATTR_SLOT_NAME) ? ATTR_SLOT_NAME : ATTR_NAME;
			return starts_with(to_str, from_str);
		}
	}

	// if there is a "SlotTypeId" attribute, merge into slots with the same type id.
	int from_id, to_id;
	if (merge_from->LookupInteger(ATTR_SLOT_TYPE_ID, from_id) &&
		merge_into->LookupInteger(ATTR_SLOT_TYPE_ID, to_id)) {
		if (pattr_used) *pattr_used = ATTR_SLOT_TYPE_ID;
		return from_id == to_id;
	}

	// if there is a "SlotId" attribute, merge into slots with the same type id.
	if (merge_from->LookupInteger(ATTR_SLOT_ID, from_id) &&
		merge_into->LookupInteger(ATTR_SLOT_ID, to_id)) {
		if (pattr_used) *pattr_used = ATTR_SLOT_ID;
		return from_id == to_id;
	}

	return true;
}

bool
StartdNamedClassAd::MergeInto(ClassAd *merge_into)
{
	return StartdNamedClassAd::Merge( merge_into, this->GetAd() );
}

bool
StartdNamedClassAd::isResourceMonitor() {
	return m_job.isResourceMonitor();
}

bool
StartdNamedClassAd::Merge( ClassAd * to, ClassAd * from ) {
	if ( ! to || ! from ) {
		return false;
	}

	int cMerged = MergeClassAdsIgnoring(to, from, dont_merge_attrs);
	return cMerged > 0;
}

void
StartdNamedClassAd::Aggregate( ClassAd * to, ClassAd * from ) {
	if ( ! to || ! from ) { return; }

	const StartdCronJobParams & params = m_job.Params();
	for( auto i = from->begin(); i != from->end(); ++i ) {
		const std::string & name = i->first;
		ExprTree * expr = i->second;

		StartdCronJobParams::Metric metric;
		if( params.getMetric( name, metric ) ) {
			double newValue;
			classad::Value v;
			expr->Evaluate( v );
			if(! v.IsRealValue( newValue )) {
				dprintf( D_ALWAYS, "Metric %s in job %s is not a real value.  Ignoring, but you probably shouldn't.\n", name.c_str(), params.GetName() );
				continue;
			}

			double oldValue;
			if( to->EvaluateAttrReal( name, oldValue ) ) {
				// dprintf( D_FULLDEBUG, "Aggregate(): %s is %.6f = %.6f %s %.6f\n", name.c_str(), metric(oldValue, newValue), oldValue, metric.c_str(), newValue );
				to->InsertAttr( name, metric( oldValue, newValue ) );
			} else {
				// dprintf( D_FULLDEBUG, "Aggregate(): %s = %.6f\n", name.c_str(), newValue );
				to->InsertAttr( name, newValue );
			}

			// Now that we've aggregated the value, set a resource-specific
			// LastUpdate* attribute for when we aggregate into a slot ad
			// with more than one resource type.
			std::string lastUpdateName = "LastUpdate" + name;
			to->CopyAttribute( lastUpdateName.c_str(), "LastUpdate", from );
			// dprintf( D_FULLDEBUG, "Aggregate(): set %s\n", lastUpdateName.c_str() );

			// We need to aggregate the StartOfJob* values as well, since
			// the slot ads are regenerate from scratch every time.
			std::string sojName = "StartOfJob" + name;
			ExprTree * e = to->Lookup( sojName );
			if( e == NULL ) {
				// dprintf( D_FULLDEBUG, "Aggregate(): %s = %.6f\n", sojName.c_str(), newValue );
				to->CopyAttribute( sojName.c_str(), sojName.c_str(), from );
			} else {
				e->Evaluate( v );
				if( v.IsRealValue( oldValue ) &&
				  from->EvaluateAttrReal( sojName, newValue ) ) {
					// dprintf( D_FULLDEBUG, "Aggregate(): %s is %.6f = %.6f %s %.6f\n", sojName.c_str(), metric( oldValue, newValue ), oldValue, metric.c_str(), newValue );
					to->InsertAttr( sojName, metric( oldValue, newValue ) );
				}
			}
		} else if( name.find( "StartOfJob" ) == 0 ) {
			// dprintf( D_FULLDEBUG, "Aggregate(): skipping StartOfJob* attribute '%s'\n", name.c_str() );
		} else if( name == "ResetStartOfJob" ) {
			// dprintf( D_FULLDEBUG, "Aggregate(): skipping ResetStartOfJob\n", name.c_str() );
		} else if( name == "SlotMergeConstraint" ) {
			// dprintf( D_FULLDEBUG, "Aggregate(): skipping SlotMergeConstraintn", name.c_str() );
		} else {
			// dprintf( D_FULLDEBUG, "Aggregate(): copying '%s'.\n", name.c_str() );
			ExprTree * copy = expr->Copy();
			if(! to->Insert( name, copy )) {
				dprintf( D_ALWAYS, "Failed to copy attribute while aggregating ad.  Ignoring, but you probably shouldn't.\n" );
			}
		}
	}
}

void
StartdNamedClassAd::AggregateFrom(ClassAd *from)
{
	if( isResourceMonitor() ) {
		ClassAd * to = this->GetAd();

		//
		// AggregateFrom() is called only from StartdCronJob::Publish(),
		// which is only called when a new ad arrives from the cron job,
		// which is the only time we want to reset the start of job
		// attribute.
		//

		bool resetStartOfJob = false;
		if( to->LookupBool( "ResetStartOfJob", resetStartOfJob ) && resetStartOfJob ) {
			// dprintf( D_FULLDEBUG, "AggregateFrom(): resetting StartOfJob* attributes...\n" );

			for( auto i = to->begin(); i != to->end(); ++i ) {
				const std::string & name = i->first;
				if( name.find( "StartOfJob" ) != 0 ) { continue; }

				std::string uptimeName = name.substr( 10 );
				to->CopyAttribute( name.c_str(), uptimeName.c_str() );
				// dprintf( D_FULLDEBUG, "AggregateFrom(): copied %s to %s\n", uptimeName.c_str(), name.c_str() );

				std::string firstUpdateName = "FirstUpdate" + uptimeName;
				to->CopyAttribute( firstUpdateName.c_str(), "LastUpdate" );
				// dprintf( D_FULLDEBUG, "AggregateFrom(): copied %s to %s\n", "LastUpdate", firstUpdateName.c_str() );
			}

			to->Delete( "ResetStartOfJob" );
		}

		Aggregate( to, from );
	} else {
		ReplaceAd( from );
	}
}

bool
StartdNamedClassAd::AggregateInto(ClassAd *into)
{
	if( isResourceMonitor() ) {
		Aggregate( into, this->GetAd() );
		return true;
	} else {
		return this->GetAd() ? into->CopyFrom( * this->GetAd() ) : true;
	}
}

void
StartdNamedClassAd::reset_monitor() {
	if(! isResourceMonitor()) {
		dprintf( D_ALWAYS, "StartdNamedClassAd::reset_monitor(): ignoring request to reset monitor of non-monitor.\n" );
		return;
	}

	ClassAd * from = this->GetAd();
	if( from == NULL ) { return; }

	const StartdCronJobParams & params = m_job.Params();
	for( auto i = from->begin(); i != from->end(); ++i ) {
		const std::string & name = i->first;
		ExprTree * expr = i->second;

		if( params.isMetric( name ) ) {
			double initialValue;
			classad::Value v;
			expr->Evaluate( v );
			if(! v.IsRealValue( initialValue )) {
				dprintf( D_ALWAYS, "Metric %s in job %s is not a floating-point value.  Ignoring, but you probably shouldn't.\n", name.c_str(), params.GetName() );
				continue;
			}

			std::string jobAttributeName;
			formatstr( jobAttributeName, "StartOfJob%s", name.c_str() );
			dprintf( D_ALWAYS, "reset_monitor(): recording %s = %.6f\n", jobAttributeName.c_str(), initialValue );
			from->InsertAttr( jobAttributeName.c_str(), initialValue );
			from->InsertAttr( "ResetStartOfJob", true );
		}
	}
}

void
StartdNamedClassAd::unset_monitor() {
	if(! isResourceMonitor()) {
		dprintf( D_ALWAYS, "StartdNamedClassAd::unset_monitor(): ignoring request to reset monitor of non-monitor.\n" );
		return;
	}

	ClassAd * from = this->GetAd();
	if( from == NULL ) { return; }

	const StartdCronJobParams & params = m_job.Params();
	for( auto i = from->begin(); i != from->end(); ++i ) {
		std::string name = i->first;
		ExprTree * expr = i->second;

		if( params.isMetric( name ) ) {
			double initialValue;
			classad::Value v;
			expr->Evaluate( v );
			if(! v.IsRealValue( initialValue )) {
				dprintf( D_ALWAYS, "Metric %s in job %s is not a real value.  Ignoring, but you probably shouldn't.\n", name.c_str(), params.GetName() );
				continue;
			}

			std::string jobAttributeName;
			formatstr( jobAttributeName, "StartOfJob%s", name.c_str() );
			// dprintf( D_FULLDEBUG, "unset_monitor(): removing %s\n", jobAttributeName.c_str() );
			from->Delete( jobAttributeName );

			std::string firstUpdateName;
			formatstr( firstUpdateName, "FirstUpdate%s", name.c_str() );
			// dprintf( D_FULLDEBUG, "unset_monitor(): removing %s\n", firstUpdateName.c_str() );
			from->Delete( firstUpdateName );

			std::string lastUpdateName;
			formatstr( lastUpdateName, "LastUpdate%s", name.c_str() );
			// dprintf( D_FULLDEBUG, "unset_monitor(): removing %s\n", lastUpdateName.c_str() );
			from->Delete( lastUpdateName );
		}
	}
}
