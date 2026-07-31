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

#ifndef STARTD_NAMED_CLASSAD_LIST_H
#define STARTD_NAMED_CLASSAD_LIST_H

#include "condor_common.h"
#include "named_classad.h"
#include "named_classad_list.h"
#include "startd_named_classad.h"

class StartdNamedClassAdList : public NamedClassAdList
{
  public:
	StartdNamedClassAdList( void );
	virtual ~StartdNamedClassAdList( void ) { };

	bool Register( StartdNamedClassAd *ad );
	int	Publish( ClassAd *ad, unsigned r_id, const char * r_id_str = NULL );
	int DeleteJob ( StartdCronJob * job );
	int ClearJob ( StartdCronJob * job );
	virtual NamedClassAd * New( const char *name, ClassAd *ad = NULL );

	const std::list<NamedClassAd*> & Enum() const { return m_ads; }

	void reset_monitors( unsigned r_id, ClassAd * forWhom );
	void unset_monitors( unsigned r_id, ClassAd * forWhom );
};

#endif
