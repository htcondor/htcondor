/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_classad_util.h"
#include "condor_classad_namedlist.h"
#include "classad_merge.h"

// Instantiate the Named Class Ad list
template class SimpleList<NamedClassAd*>;


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
