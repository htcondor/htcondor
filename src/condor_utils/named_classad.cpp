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
#include "condor_classad.h"
#include "named_classad.h"

// Instantiate the Named Class Ad list


// Named classAds
NamedClassAd::NamedClassAd( const char *name, ClassAd *ad )
		: m_name( strdup(name) ),
		  m_classad( ad )
{
}

// Destructor
NamedClassAd::~NamedClassAd( void )
{
	free( const_cast<char *>(m_name) );
	delete m_classad;
}

// Comparison operators
bool
NamedClassAd::operator == ( const NamedClassAd &other ) const
{
	return strcmp(m_name, other.GetName()) == 0;
}

bool
NamedClassAd::operator == ( const char *other ) const
{
	return strcmp(m_name, other) == 0;
}

// Replace existing ad
void
NamedClassAd::ReplaceAd( ClassAd *newAd )
{
	if ( NULL != m_classad ) {
		delete m_classad;
		m_classad = NULL;
	}
	m_classad = newAd;
}
