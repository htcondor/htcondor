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
	NamedClassAd *nad = dynamic_cast<NamedClassAd *>(ad);
	return NamedClassAdList::Register( nad );
}

int
StartdNamedClassAdList::Publish( ClassAd *merged_ad, unsigned r_id )
{
	std::list<NamedClassAd *>::iterator iter;
	for( iter = m_ads.begin(); iter != m_ads.end(); iter++ ) {
		NamedClassAd		*nad = *iter;
		StartdNamedClassAd	*sad = dynamic_cast<StartdNamedClassAd*>(nad);
		ASSERT( sad );
		const char * match_attr = NULL;
		if ( sad->InSlotList(r_id) && sad->ShouldMergeInto(merged_ad, &match_attr) ) {
			ClassAd	*ad = nad->GetAd( );
			if ( NULL != ad ) {
				dprintf( D_FULLDEBUG,
						 "Publishing ClassAd '%s' to slot %d [%s matches]\n",
						 nad->GetName(), r_id, match_attr ? match_attr : "InSlotList" );
				sad->MergeInto(merged_ad);
			}
		}
		else if (match_attr) {
			dprintf( D_FULLDEBUG,
						"Rejecting ClassAd '%s' for slot %d [%s does not match]\n",
						nad->GetName(), r_id, match_attr );
		}
	}

	// Done
	return 0;
}
