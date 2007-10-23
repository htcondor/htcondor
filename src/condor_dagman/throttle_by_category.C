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
#include "condor_string.h"  /* for strnewp() */
#include "throttle_by_category.h"
#include "debug.h"
#include "MyString.h"

static const int CATEGORY_HASH_SIZE = 101; // prime

//---------------------------------------------------------------------------
ThrottleByCategory::ThrottleByCategory() :
			_throttles( CATEGORY_HASH_SIZE, MyStringHash, rejectDuplicateKeys )
{
}

//---------------------------------------------------------------------------
ThrottleByCategory::~ThrottleByCategory()
{
	_throttles.startIterations();
	ThrottleInfo	*info;
	while ( _throttles.iterate( info ) ) {
		delete info->_category;
		delete info;
	}
}

//---------------------------------------------------------------------------
ThrottleByCategory::ThrottleInfo *
ThrottleByCategory::AddCategory( const MyString *category, int maxJobs )
{
	ASSERT( category != NULL );

	ThrottleInfo *	info = new ThrottleInfo();
	info->_category = new MyString( *category );
	info->_maxJobs = maxJobs;
	info->_currentJobs = 0;
	if ( _throttles.insert( *(info->_category), info ) != 0 ) {
		EXCEPT( "HashTable error" );
	}

	return info;
}

//---------------------------------------------------------------------------
void
ThrottleByCategory::SetThrottle( const MyString *category, int maxJobs )
{
	ASSERT( category != NULL );

	ThrottleInfo	*info;
	if ( _throttles.lookup( *category, info ) != 0 ) {
		// category not in table
		AddCategory( category, maxJobs );
	} else {
		// category is in table
		if ( info->_maxJobs != noThrottleSetting &&
					info->_maxJobs != maxJobs ) {
			debug_printf( DEBUG_NORMAL, "Warning: new maxjobs value %d "
						"for category %s overrides old value %d\n",
						maxJobs, category->Value(), info->_maxJobs );
		}
		info->_maxJobs = maxJobs;
	}
}

//---------------------------------------------------------------------------
ThrottleByCategory::ThrottleInfo *
ThrottleByCategory::GetThrottleInfo( const MyString *category )
{
	ASSERT( category != NULL );

	ThrottleInfo	*info;
	if ( _throttles.lookup( *category, info ) != 0 ) {
		return NULL;
	} else {
		return info;
	}
}

//---------------------------------------------------------------------------
// Note: don't change the format here -- this is used for rescue DAG files,
// so what we print has to be parseable by parse().
void
ThrottleByCategory::PrintThrottles( FILE *fp ) /* const */
{
	ASSERT( fp != NULL );

	_throttles.startIterations();
	ThrottleInfo	*info;
	while ( _throttles.iterate( info ) ) {
		if ( info->_maxJobs != noThrottleSetting ) {
			fprintf( fp, "MAXJOBS %s %d\n", info->_category->Value(),
						info->_maxJobs );
		}
	}
}
