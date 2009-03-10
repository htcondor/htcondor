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
#include "condor_string.h"  /* for strnewp() */
#include "throttle_by_category.h"
#include "debug.h"
#include "MyString.h"

static const int CATEGORY_HASH_SIZE = 101; // prime

const int ThrottleByCategory::noThrottleSetting = -1;

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

			// Coverity complains about not storing the return value here, but
			// AddCategory also puts it in the hash table.
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
void
ThrottleByCategory::PrefixAllCategoryNames( const MyString &prefix )
{
	_throttles.startIterations();
	ThrottleInfo	*info;
	while ( _throttles.iterate( info ) ) {
		MyString *newCat = new MyString( prefix );
		*newCat += *(info->_category);
		delete info->_category;
		info->_category = newCat;
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
