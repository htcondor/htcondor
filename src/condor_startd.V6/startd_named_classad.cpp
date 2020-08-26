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
		bool matches = false;
		if (pattr_used) *pattr_used = ATTR_SLOT_MERGE_CONSTRAINT;
		(void) EvalBool(ATTR_SLOT_MERGE_CONSTRAINT, merge_from, merge_into, matches);
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
			if(! v.IsNumber( newValue )) {
				dprintf( D_ALWAYS, "Metric %s in job %s is not a number.  Ignoring, but you probably shouldn't.\n", name.c_str(), params.GetName() );
				continue;
			}

			double oldValue;
			if( to->EvaluateAttrNumber( name, oldValue ) ) {
				// dprintf( D_ALWAYS, "Aggregate( %p, %p ): %s = %f %s %f\n", to, from, name.c_str(), oldValue, metric.c_str(), newValue );
				to->InsertAttr( name, metric( oldValue, newValue ) );
			} else {
				// dprintf( D_ALWAYS, "Aggregate( %p, %p ): %s = %f\n", to, from, name.c_str(), newValue );
				to->InsertAttr( name, newValue );
			}

			//
			// In addition to aggregating the (global) Uptime* statistics,
			// we need to aggregate the per-job statistics for SUM and PEAK
			// metrics, which call their per-job attributes different things
			// because they're computed differently.
			//
			// Per-job SUM metrics are computed when we write the slot's
			// update ad (in Resource::Publish()), using the aggregate of the
			// StartOfJob* attributes (which record the metric as of the job's
			// first sample).  Because the slot ads are regenerated from
			// scrach every time, we need to recompute the StartOfJob*
			// aggregates every time.
			//
			// Per-job PEAK metrics are computed each time we receive a new
			// sample (in AggregateFrom()), but for the same reason as for
			// SUM metrics, we have to recompute the aggregate PEAK of each
			// resource instance again every time.
			//

			std::string perJobAttributeName;
			std::string perJobAvgAttr;
			if( StartdCronJobParams::attributeIsSumMetric( name ) ) {
				perJobAttributeName = "StartOfJob" + name;
				perJobAvgAttr = name + "AverageUsage";
			} else if( StartdCronJobParams::attributeIsPeakMetric( name ) ) {
				if(! StartdCronJobParams::getResourceNameFromAttributeName( name, perJobAttributeName )) { continue; }
				perJobAttributeName += "Usage";
			} else {
				dprintf( D_ALWAYS, "Found metric '%s' of unknown type.  Ignoring, but you probably shouldn't.\n", name.c_str() );
				continue;
			}

			expr = to->Lookup( perJobAttributeName );
			if( expr == NULL ) {
				// dprintf( D_ALWAYS, "Aggregate( %p, %p ): %s = %f\n", to, from, perJobAttributeName.c_str(), oldValue );
				CopyAttribute( perJobAttributeName, *to, *from );
			} else {
				classad::Value v;
				expr->Evaluate( v );
				if( v.IsNumber( oldValue ) &&
				  from->EvaluateAttrNumber( perJobAttributeName, newValue ) ) {
					// dprintf( D_ALWAYS, "Aggregate( %p, %p ): %s = %f %s %f\n", to, from, perJobAttributeName.c_str(), oldValue, metric.c_str(), newValue );
					to->InsertAttr( perJobAttributeName, metric( oldValue, newValue ) );
				}
			}

			// Record for each resource when we last updated it.
			std::string lastUpdateName = "LastUpdate" + name;
			CopyAttribute( lastUpdateName, *to, "LastUpdate", *from );

			// for SUM metrics, inject an expression for the average over a job lifetime
			// we do this so that policy expressions can access it. 
			// The ad that gets sent to the Collector will overwrite this with a calculated value
			// but that happens much less frequently than 
			if (! perJobAvgAttr.empty()) {

				// Note that we calculate the usage rate only for full
				// sample intervals.  This eliminates the imprecision of
				// the sample interval in which the job started; since we
				// can't include the usage from the sample interval in
				// which the job ended, we also don't include the time
				// the job was running in that interval.  The computation
				// is thus exact for its time period.
				std::string avgExpr;
				formatstr(avgExpr, "(%s - StartOfJob%s)/(LastUpdate%s - FirstUpdate%s)", name.c_str(), name.c_str(), name.c_str(), name.c_str());
				to->AssignExpr(perJobAvgAttr, avgExpr.c_str());
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
				if( StartdCronJobParams::attributeIsSumMetric( uptimeName ) ) {
					CopyAttribute( name, *to, uptimeName );

					std::string firstUpdateName = "FirstUpdate" + uptimeName;
					CopyAttribute( firstUpdateName, *to, "LastUpdate" );
				} else if( StartdCronJobParams::attributeIsPeakMetric( uptimeName ) ) {
					// PEAK metrics don't use the StartOfJob* attributes.  If
					// the current job peak isn't set when a new sample comes
					// in, it becomes the new peak.
					continue;
				} else {
					dprintf( D_ALWAYS, "Found StartOfJob* attribute '%s' of uknown metric type.  Ignoring, but you probably shouldn't.\n", name.c_str() );
					continue;
				}
			}

			to->Delete( "ResetStartOfJob" );
		}

		// The per-job value for PEAK metrics can only be calculated here,
		// where we know we're looking at a new sample value, and not
		// aggregating old ones.
		const StartdCronJobParams & params = m_job.Params();
		for( auto i = from->begin(); i != from->end(); ++i ) {
			const std::string & name = i->first;
			ExprTree * expr = i->second;

			StartdCronJobParams::Metric metric;
			if( params.getMetric( name, metric ) ) {
				double sampleValue;
				classad::Value v;
				expr->Evaluate( v );
				if(! v.IsNumber( sampleValue )) {
					// Don't duplicate the warning in Aggregate() here.
					continue;
				}

				if(! StartdCronJobParams::attributeIsPeakMetric( name )) {
					continue;
				}

				std::string usageName;
				if(! StartdCronJobParams::getResourceNameFromAttributeName( name, usageName )) { continue; }
				usageName += "Usage";

				if(! to->Lookup( usageName )) {
					to->InsertAttr( usageName, sampleValue );
					// dprintf( D_ALWAYS, "First %s sample for current job set to %.2f\n", GetName(), sampleValue );
				} else {
					double currentUsage;
					to->EvaluateAttrNumber( usageName, currentUsage );
					to->InsertAttr( usageName, metric( sampleValue, currentUsage ) );
					// dprintf( D_ALWAYS, "%s sample was %.2f, current job peak now %.2f\n", GetName(), sampleValue, metric( sampleValue, currentUsage ) );
				}

				// Record for each resource when we last updated it.
				std::string lastUpdateName = "LastUpdate" + usageName;
				CopyAttribute( lastUpdateName, *to, "LastUpdate", *from );
			}
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
		//
		// AggregateInto() is only called by StartdNamedClassAdList::Publish(),
		// which only called from ResMgr::adlist_publish(), which is only
		// called from Resource::publish(), which is called to construct the
		// slot ads.
		//
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

	// dprintf( D_FULLDEBUG, "StartdNameClassAd::reset_monitor() for %s\n", GetName() );
	const StartdCronJobParams & params = m_job.Params();
	for( auto i = from->begin(); i != from->end(); ++i ) {
		const std::string & name = i->first;
		ExprTree * expr = i->second;

		if( params.isMetric( name ) ) {
			double initialValue;
			classad::Value v;
			expr->Evaluate( v );
			if(! v.IsNumber( initialValue )) {
				dprintf( D_ALWAYS, "Metric %s in job %s is not a number.  Ignoring, but you probably shouldn't.\n", name.c_str(), params.GetName() );
				continue;
			}

			if( StartdCronJobParams::attributeIsSumMetric( name ) ) {
				std::string jobAttributeName;
				formatstr( jobAttributeName, "StartOfJob%s", name.c_str() );
				from->InsertAttr( jobAttributeName.c_str(), initialValue );
				from->InsertAttr( "ResetStartOfJob", true );
			} else if( StartdCronJobParams::attributeIsPeakMetric( name ) ) {
				std::string usageName;
				if(! StartdCronJobParams::getResourceNameFromAttributeName( name, usageName )) { continue; }
				usageName += "Usage";
				// If we just delete usageName, when Resource::refresh_ad()
				// (probably wrongly) reuses its ClassAd member variable,
				// the value from the last time we updated the slot ad will
				// persist.  Instead, make sure that the persistent resource
				// instance ad we're modifying here will stomp on the value
				// by instead setting usageName to undefined explicitly.
				from->AssignExpr( usageName, "undefined" );
			} else {
				dprintf( D_ALWAYS, "Found metric '%s' of unknown type.  Ignoring, but you probably shouldn't.\n", name.c_str() );
			}
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

	// dprintf( D_FULLDEBUG, "StartdNameClassAd::unset_monitor() for %s\n", GetName() );
	const StartdCronJobParams & params = m_job.Params();
	for( auto i = from->begin(); i != from->end(); ++i ) {
		std::string name = i->first;
		ExprTree * expr = i->second;

		if( params.isMetric( name ) ) {
			double initialValue;
			classad::Value v;
			expr->Evaluate( v );
			if(! v.IsNumber( initialValue )) {
				dprintf( D_ALWAYS, "Metric %s in job %s is not a number.  Ignoring, but you probably shouldn't.\n", name.c_str(), params.GetName() );
				continue;
			}

			std::string jobAttributeName;
			formatstr( jobAttributeName, "StartOfJob%s", name.c_str() );
			from->Delete( jobAttributeName );

			std::string firstUpdateName;
			formatstr( firstUpdateName, "FirstUpdate%s", name.c_str() );
			from->Delete( firstUpdateName );

			std::string lastUpdateName;
			formatstr( lastUpdateName, "LastUpdate%s", name.c_str() );
			from->Delete( lastUpdateName );

			std::string usageName;
			if(! StartdCronJobParams::getResourceNameFromAttributeName( name, usageName )) { continue; }
			usageName += "Usage";
			from->Delete( usageName );

			std::string lastUsageUpdateName;
			formatstr( lastUsageUpdateName, "LastUpdate%s", usageName.c_str() );
			from->Delete( lastUsageUpdateName );
		}
	}
}
