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

#ifndef NAMED_CLASSAD_H
#define NAMED_CLASSAD_H

#include "condor_common.h"
#include "condor_classad.h"

// A name / ClassAd pair to manage together
class NamedClassAd
{
  public:
	NamedClassAd( const char *name, ClassAd *ad = NULL );
	virtual ~NamedClassAd( void );
	const char *GetName( void ) const { return m_name; };
	ClassAd *GetAd( void ) { return m_classad; };
	void ReplaceAd( ClassAd *newAd );

	bool operator == ( const NamedClassAd &other ) const;
	bool operator == ( const char *other ) const;

  private:
	const char	*m_name;
	ClassAd		*m_classad;
};

#endif
