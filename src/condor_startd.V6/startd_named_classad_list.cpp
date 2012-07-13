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
		if ( sad->InSlotList(r_id) ) {
			ClassAd	*ad = nad->GetAd( );
			if ( NULL != ad ) {
				dprintf( D_FULLDEBUG,
						 "Publishing ClassAd for '%s' to slot %d\n",
						 nad->GetName(), r_id );
				MergeClassAds( merged_ad, ad, true );
			}
		}
	}

	// Done
	return 0;
}
