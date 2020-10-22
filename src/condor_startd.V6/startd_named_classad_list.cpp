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
#include "named_classad.h"
#include "named_classad_list.h"
#include "startd_named_classad.h"
#include "startd_named_classad_list.h"

StartdNamedClassAdList::StartdNamedClassAdList( void )
		: NamedClassAdList( )
{
}

// virtual method so that NamedClassAdList can create a new StartdNamedClassAd if needed.
//
NamedClassAd * StartdNamedClassAdList::New( const char * name, ClassAd * ad)
{
	StartdNamedClassAd * sad = NULL;
	const char * pdot = strchr(name, '.');
	if (pdot) {
		// tj: its a bit of a hack, but for now, we look for an existing named ad
		// that matches the new ad's basename (i.e. everthing before the first '.')
		// then we ask the existing ad to create a new ad that shares it's job object.
		// this works because the CronJob will always register the basename in this list.
		//
		std::string pre(name);
		pre[pdot-name] = 0;
		NamedClassAd * nad = Find(pre.c_str());
		sad = dynamic_cast<StartdNamedClassAd*>(nad);
		if (sad) {
			sad = sad->NewPeer(name, ad);
		}
	}
	ASSERT(sad);
	return sad;
}

int
StartdNamedClassAdList::DeleteJob( StartdCronJob * job )
{
	bool no_match = 1;

	std::list<NamedClassAd *>::iterator iter = m_ads.begin();
	while (iter != m_ads.end())
	{
		NamedClassAd * nad = *iter;
		StartdNamedClassAd *sad = dynamic_cast<StartdNamedClassAd *>(nad);
		if ( sad && sad->IsJob (job) )
		{
			no_match = 0;
			iter = m_ads.erase(iter);
			delete nad;
		} else {
			++iter;
		}
	}

	// No match found; done
	return no_match;
}

int
StartdNamedClassAdList::ClearJob ( StartdCronJob * job )
{
	bool no_match = 1;

	std::list<NamedClassAd *>::iterator iter = m_ads.begin();
	while (iter != m_ads.end())
	{
		NamedClassAd * nad = *iter;
		StartdNamedClassAd *sad = dynamic_cast<StartdNamedClassAd *>(nad);
		if ( sad && sad->IsJob (job) )
		{
			no_match = 0;
			if (  ! strcmp(sad->GetName(), job->GetName()) ) {
				sad->ReplaceAd(NULL);
				++iter;
			} else {
				iter = m_ads.erase(iter);
				delete nad;
			}
		} else {
			++iter;
		}
	}

	// No match found; done
	return no_match;
}


bool
StartdNamedClassAdList::Register( StartdNamedClassAd *ad )
{
	NamedClassAd *nad = static_cast<NamedClassAd *>(ad);
	return NamedClassAdList::Register( nad );
}

int
StartdNamedClassAdList::Publish( ClassAd *merged_ad, unsigned r_id, const char * r_id_str )
{
	std::list<NamedClassAd *>::iterator iter;
	for( iter = m_ads.begin(); iter != m_ads.end(); iter++ ) {
		NamedClassAd		*nad = *iter;
		StartdNamedClassAd	*sad = dynamic_cast<StartdNamedClassAd*>(nad);
		ASSERT( sad );

		if( sad->isResourceMonitor() ) { continue; }

		const char * match_attr = NULL;
		if ( sad->InSlotList(r_id) && sad->ShouldMergeInto(merged_ad, &match_attr) ) {
			ClassAd	*ad = nad->GetAd( );
			if ( NULL != ad ) {
				dprintf( D_FULLDEBUG,
						 "Publishing ClassAd '%s' to %s [%s matches]\n",
						 nad->GetName(), r_id_str, match_attr ? match_attr : "InSlotList" );
				sad->MergeInto(merged_ad);
			}
		}
		else if (match_attr) {
			dprintf( D_FULLDEBUG,
						"Rejecting ClassAd '%s' for %s [%s does not match]\n",
						nad->GetName(), r_id_str, match_attr );
		}
	}

	//
	// Because each (this) slot may have more than one custom resource of a
	// given type (e.g., two GPUs), we need to aggregate, rather than merge,
	// the resource-specific ads produce by the resource monitor.  We assume
	// -- and once we fix #6489, will be able to enforce -- that monitors
	// for different resources will not have the same attribute names, so
	// we only need single accumulator ad (and don't have to split the
	// monitor ads by resource type).
	//
	ClassAd accumulator;
	// dprintf( D_FULLDEBUG, "Generating usage for %s...\n", r_id_str );
	for( iter = m_ads.begin(); iter != m_ads.end(); iter++ ) {
		NamedClassAd		*nad = *iter;
		StartdNamedClassAd	*sad = dynamic_cast<StartdNamedClassAd*>(nad);
		ASSERT( sad );

		if(! sad->isResourceMonitor()) { continue; }

		const char * match_attr = NULL;
		// dprintf( D_FULLDEBUG, "... adding %s...\n", sad->GetName() );
		if( sad->InSlotList( r_id ) && sad->ShouldMergeInto( merged_ad, & match_attr ) ) {
			sad->AggregateInto( & accumulator );
		}
	}
	// dprintf( D_FULLDEBUG, "... done generating usage report for %s.\n", r_id_str );

#if 0
	classad::Value v, w;
	double usmpu = -1, smu = -1;
	accumulator.EvaluateAttr( "UptimeSQUIDsMemoryPeakUsage", v ) && v.IsNumber( usmpu );
	accumulator.EvaluateAttr( "SQUIDsMemoryUsage", w ) && w.IsNumber( smu );
	dprintf( D_FULLDEBUG, "%s UptimeSQUIDsMemoryPeakUsage = %.2f SQUIDsMemoryUsage = %.2f\n",
		r_id_str, usmpu, smu );
#endif

	// We don't filter out the (raw) Uptime* metrics here, because the
	// starter needs them to compute the (per-job) *Usage metrics.  Instead,
	// we filter them out in Resource::process_update_ad().
	StartdNamedClassAd::Merge( merged_ad, & accumulator );
	return 0;
}

void
StartdNamedClassAdList::reset_monitors( unsigned r_id, ClassAd * forWhom ) {
	std::list<NamedClassAd *>::iterator iter;
	for( iter = m_ads.begin(); iter != m_ads.end(); iter++ ) {
		NamedClassAd		*nad = *iter;
		StartdNamedClassAd	*sad = dynamic_cast<StartdNamedClassAd*>(nad);
		ASSERT( sad );

		if(! sad->isResourceMonitor()) { continue; }
		const char * match_attr = NULL;
		if( sad->InSlotList( r_id ) && sad->ShouldMergeInto( forWhom, & match_attr ) ) {
			sad->reset_monitor();
		}
	}

}

void
StartdNamedClassAdList::unset_monitors( unsigned r_id, ClassAd * forWhom ) {
	std::list<NamedClassAd *>::iterator iter;
	for( iter = m_ads.begin(); iter != m_ads.end(); iter++ ) {
		NamedClassAd		*nad = *iter;
		StartdNamedClassAd	*sad = dynamic_cast<StartdNamedClassAd*>(nad);
		ASSERT( sad );

		if(! sad->isResourceMonitor()) { continue; }
		const char * match_attr = NULL;
		if( sad->InSlotList( r_id ) && sad->ShouldMergeInto( forWhom, & match_attr ) ) {
			sad->unset_monitor();
		}
	}

}
