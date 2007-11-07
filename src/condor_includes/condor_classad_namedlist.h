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

#ifndef CONDOR_CLASSAD_NAMEDLIST_H
#define CONDOR_CLASSAD_NAMEDLIST_H

#include "condor_common.h"
#include "condor_classad.h"
#include "simplelist.h"
#include "string_list.h"

// A name / ClassAd pair to manage together
class NamedClassAd
{
  public:
	NamedClassAd( const char *name, ClassAd *ad = NULL );
	~NamedClassAd( void );
	char *GetName( void ) { return myName; };
	ClassAd *GetAd( void ) { return myClassAd; };
	void ReplaceAd( ClassAd *newAd );

  private:
	char	*myName;
	ClassAd	*myClassAd;
};

class NamedClassAdList
{
  public:
	NamedClassAdList( void );
	~NamedClassAdList( void );

	int Register( const char *name );
	int	Replace( const char *name, ClassAd *ad, bool report_diff = false,
				 StringList* ignore_attrs = NULL );
	int	Delete( const char *name );
	int	Publish( ClassAd *ad );

  private:
	SimpleList<NamedClassAd*>		ads;

};

#endif
