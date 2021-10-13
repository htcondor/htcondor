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
#include "throttle_by_category.h"
#include "dagman_main.h"
#include "debug.h"
#include "MyString.h"

const int ThrottleByCategory::noThrottleSetting = -1;

//---------------------------------------------------------------------------
ThrottleByCategory::ThrottleByCategory() :
			_throttles( {} )
{
}

//---------------------------------------------------------------------------
ThrottleByCategory::~ThrottleByCategory()
{
	for ( auto throttle: _throttles ) {
		delete throttle.second;
	}
}

//---------------------------------------------------------------------------
ThrottleByCategory::ThrottleInfo *
ThrottleByCategory::AddCategory( const MyString *category, int maxJobs )
{
	ASSERT( category != NULL );

	ThrottleInfo *	info = new ThrottleInfo( category, maxJobs );
	auto insertResult = _throttles.insert( std::make_pair( *(info->_category), info ) );
	if ( insertResult.second == false ) {
		EXCEPT( "Error adding new throttle category" );
	}

	return info;
}

//---------------------------------------------------------------------------
void
ThrottleByCategory::SetThrottle( const MyString *category, int maxJobs )
{
	ASSERT( category != NULL );

	auto findResult = _throttles.find( *category );
	if ( findResult == _throttles.end() ) {
			// category not in table

			// Coverity complains about not storing the return value here, but
			// AddCategory also puts it in the hash table.
		AddCategory( category, maxJobs );
	} else {
			// category is in table
		ThrottleInfo *info = ( *findResult ).second;
		if ( info->isSet() && info->_maxJobs != maxJobs ) {
			debug_printf( DEBUG_NORMAL, "Warning: new maxjobs value %d "
						"for category %s overrides old value %d\n",
						maxJobs, category->c_str(), info->_maxJobs );
			check_warning_strictness( DAG_STRICT_3 );
		}
		info->_maxJobs = maxJobs;
	}
}

//---------------------------------------------------------------------------
ThrottleByCategory::ThrottleInfo *
ThrottleByCategory::GetThrottleInfo( const MyString *category )
{
	ASSERT( category != NULL );

	auto findResult = _throttles.find( *category );
	if ( findResult == _throttles.end() ) {
		return NULL;
	} else {
		return ( *findResult ).second;
	}
}

//---------------------------------------------------------------------------
void
ThrottleByCategory::PrefixAllCategoryNames( const MyString &prefix )
{
		// We copy the ThrottleInfo objects into this hash table,
		// and then copy this hash table over the one in this object
		// so that the ThrottleInfo objects are indexed by their
		// new names.  Note that we don't need to delete any
		// ThrottleInfo objects because we're re-using the ones
		// we already have.
	std::map<MyString, ThrottleInfo *> tmpThrottles( {} );

	for ( auto throttle: _throttles ) {
		ThrottleInfo *info = throttle.second;
			// Don't change category names for global categories (names
			// starting with '+') (allows nodes in different splices to
			// be in the same category).
		if ( ! info->isGlobal() ) {
			MyString *newCat = new MyString( prefix );
			*newCat += *(info->_category);
			delete info->_category;
			info->_category = newCat;
		}
		auto insertResult = tmpThrottles.insert( std::make_pair( *(info->_category), info ) );
		if ( !insertResult.second ) {
			EXCEPT( "Error inserting temporary throttle" );
		}
	}

		// Get rid of old hash buckets.
	_throttles.clear();	// get rid of hash buckets

		// Shallow copy here, as far as ThrottleInfo objects are
		// concerned (we get new hash buckets).
	_throttles = tmpThrottles;
}

//---------------------------------------------------------------------------
// Note: don't change the format here -- this is used for rescue DAG files,
// so what we print has to be parseable by parse().
void
ThrottleByCategory::PrintThrottles( FILE *fp ) /* const */
{
	ASSERT( fp != NULL );

	for ( auto throttle: _throttles ) {
		ThrottleInfo *info = throttle.second;
		if ( info->isSet() ) {
			fprintf( fp, "MAXJOBS %s %d\n", info->_category->c_str(),
						info->_maxJobs );
		}
	}
}
