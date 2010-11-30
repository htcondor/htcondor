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
#include "condor_classad.h"
#include "condor_classad_util.h"
#include "condor_classad_namedlist.h"
#include "classad_merge.h"

// Instantiate the Named Class Ad list


// Named classAds
NamedClassAd::NamedClassAd( const char *name, ClassAd *ad )
{
	myName = strdup( name );
	myClassAd = ad;
}

// Destructor
NamedClassAd::~NamedClassAd( )
{
	free( myName );
	delete myClassAd;
}

// Replace existing ad
void
NamedClassAd::ReplaceAd( ClassAd *newAd )
{
	if ( NULL != myClassAd ) {
		delete myClassAd;
		myClassAd = NULL;
	}
	myClassAd = newAd;
}


// Methods to manipulate the supplemental ClassAd list
NamedClassAdList::NamedClassAdList( void )
{
}

NamedClassAdList::~NamedClassAdList( void )
{
}

int
NamedClassAdList::Register( const char *name )
{
	NamedClassAd	*cur;

	// Walk through the list
	ads.Rewind( );
	while ( ads.Current( cur ) ) {
		if ( ! strcmp( cur->GetName( ), name ) ) {
			// Do nothing
			return 0;
		}
		ads.Next( cur );
	}

	// No match found; insert it into the list
	dprintf( D_JOB, "Adding '%s' to the Supplimental ClassAd list\n", name );
	if (  ( cur = new NamedClassAd( name, NULL ) ) != NULL ) {
		ads.Append( cur );
		return 0;
	}

	// new failed; bad
	return -1;
}

int
NamedClassAdList::Replace( const char *name, ClassAd *newAd, 
						   bool report_diff, StringList* ignore_attrs )
{
	NamedClassAd	*cur;

	// Walk through the list
	ads.Rewind( );
	while ( ads.Next( cur ) ) {
		if ( ! strcmp( cur->GetName( ), name ) ) {	
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
				ClassAd* oldAd = cur->GetAd();
				if( ! oldAd ) {
						// there's no oldAd, everything is different. ;)
					found_diff = true;
				} else { 
						// we actually have to see if there's any diff...
					found_diff = ! ClassAdsAreSame(newAd, oldAd, ignore_attrs);
				}
			}
			cur->ReplaceAd( newAd );
			if( report_diff && found_diff ) {
				return 1;
			}
			return 0;
		}
	}

	// No match found; insert it into the list
	if (  ( cur = new NamedClassAd( name, newAd ) ) != NULL ) {
		dprintf( D_FULLDEBUG,
				 "Adding '%s' to the 'extra' ClassAd list\n", name );
		ads.Append( cur );
		return 0;
	}

	// new failed; bad
	delete newAd;		// The caller expects us to always free the memory
	return -1;
}

int
NamedClassAdList::Delete( const char *name )
{
	NamedClassAd	*cur;

	// Walk through the list
	ads.Rewind( );
	while ( ads.Next( cur ) ) {
		if ( ! strcmp( cur->GetName( ), name ) ) {
			dprintf( D_FULLDEBUG, "Deleting ClassAd for '%s'\n", name );
			ads.DeleteCurrent( );
			delete cur;
			return 0;
		}
	}

	// No match found; done
	return 0;
}

int
NamedClassAdList::Publish( ClassAd *merged_ad )
{
	NamedClassAd	*cur;

	// Walk through the list
	ads.Rewind( );
	while ( ads.Next( cur ) ) {
		ClassAd	*ad = cur->GetAd( );
		if ( NULL != ad ) {
			dprintf( D_FULLDEBUG,
					 "Publishing ClassAd for '%s'\n", cur->GetName() );
			MergeClassAds( merged_ad, ad, true );
		}
	}

	// Done
	return 0;
}
