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
										StartdCronJob &job)
		: NamedClassAd( name, NULL ), m_job( job )
{
}

bool
StartdNamedClassAd::InSlotList( unsigned slot_id ) const
{
	return m_job.Params().InSlotList( slot_id );
}
