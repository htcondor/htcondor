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
#include "named_classad_list.h"


// Methods to manipulate the supplemental ClassAd list
NamedClassAdList::NamedClassAdList( void )
{
}

NamedClassAdList::~NamedClassAdList( void )
{
	std::list<NamedClassAd *>::iterator iter;
	for( iter = m_ads.begin(); iter != m_ads.end(); iter++ ) {
		delete *iter;
	}
	m_ads.clear();
}

NamedClassAd *
NamedClassAdList::Find( const char *name )
{
	std::list<NamedClassAd *>::iterator iter;
	for( iter = m_ads.begin(); iter != m_ads.end(); iter++ ) {
		NamedClassAd	*nad = *iter;
		if ( ! strcmp( nad->GetName( ), name ) ) {
			return nad;
		}
	}
	return NULL;
}

bool
NamedClassAdList::Register( const char *name )
{
	// Already in the list?
	if ( Find(name) != NULL ) {
		return false;
	}

	// No match found; insert it into the list
	dprintf( D_JOB, "Adding '%s' to the Supplemental ClassAd list\n", name );
	m_ads.push_back( new NamedClassAd(name, NULL) );
	return true;
}

bool
NamedClassAdList::Register( NamedClassAd *ad )
{
	if ( Find(*ad) != NULL ) {
		return false;
	}

	// No match found; insert it into the list
	dprintf( D_JOB, "Adding '%s' to the Supplemental ClassAd list\n",
			 ad->GetName() );
	m_ads.push_back( ad );
	return true;
}

int
NamedClassAdList::Replace( const char *name, ClassAd *newAd, 
						   bool report_diff, classad::References* ignore_attrs )
{
	NamedClassAd	*named_ad = Find( name );
	if ( NULL == named_ad ) {
		named_ad = New( name, newAd );
		if ( NULL == named_ad ) {
			return -1;
		}
		dprintf( D_FULLDEBUG,
				 "Adding '%s' to the 'extra' ClassAd list\n", name );
		m_ads.push_back( named_ad );
		return report_diff;
	}

	// Did we find a match?
	dprintf( D_FULLDEBUG, "Replacing ClassAd for '%s'\n", name );
	bool found_diff = false;
	if( report_diff ) {
		/*
		  caller wants to know if this new ad actually has
		  any different data from the previous, (except
		  attributes they want to ignore, for example,
		  "LastUpdate" for startd-cron ads).  this is a
		  little expensive, but in general, these ads are
		  small, and this isn't the default behavior...
		*/
		ClassAd* oldAd = named_ad->GetAd();
		if( ! oldAd ) {
			// there's no oldAd, everything is different. ;)
			found_diff = true;
		} else { 
			// we actually have to see if there's any diff...
			found_diff = ! ClassAdsAreSame(newAd, oldAd, ignore_attrs);
		}
	}
	named_ad->ReplaceAd( newAd );
	return ( report_diff && found_diff );
}

int
NamedClassAdList::Delete( const char *name )
{
	std::list<NamedClassAd *>::iterator iter;
	for( iter = m_ads.begin(); iter != m_ads.end(); iter++ ) {
		NamedClassAd	*ad = *iter;
		if ( ! strcmp( ad->GetName( ), name ) ) {
			m_ads.erase( iter );
			delete ad;
			return 0;
		}
	}

	// No match found; done
	return 1;
}

int
NamedClassAdList::Publish( ClassAd *merged_ad )
{
	std::list<NamedClassAd *>::iterator iter;
	for( iter = m_ads.begin(); iter != m_ads.end(); iter++ ) {
		NamedClassAd	*nad = *iter;
		ClassAd			*ad = nad->GetAd( );
		if ( NULL != ad ) {
			dprintf( D_FULLDEBUG,
					 "Publishing ClassAd for '%s'\n", nad->GetName() );
			MergeClassAds( merged_ad, ad, true );
		}
	}

	// Done
	return 0;
}
