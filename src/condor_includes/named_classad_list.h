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

#ifndef NAMED_CLASSAD_LIST_H
#define NAMED_CLASSAD_LIST_H

#include "condor_common.h"
#include "condor_classad.h"
#include "named_classad.h"
#include <list>

class NamedClassAdList
{
  public:
	NamedClassAdList( void );
	~NamedClassAdList( void );

	NamedClassAd *Find( const char *name );
	NamedClassAd *Find( NamedClassAd &ad ) {
		return Find( ad.GetName() );
	};

	bool Register( const char *name );
	bool Register( NamedClassAd *ad );

	int	Replace( const char *name, ClassAd *ad, bool report_diff = false,
				 StringList* ignore_attrs = NULL );
	int	Delete( const char *name );
	int	Publish( ClassAd *ad );

  protected:
	std::list<NamedClassAd*>		m_ads;

};

#endif
