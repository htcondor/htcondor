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


#ifndef _THROTTLE_BY_CATEGORY_H
#define _THROTTLE_BY_CATEGORY_H

#include "MyString.h"
#include <map>

// Note: some of the complexity in this class is to allow a category maxjobs
// line to come before or after the corresponding node(s). wenger 2007-10-10.

class ThrottleByCategory {
public:
	static const int	noThrottleSetting;

	struct ThrottleInfo {
		const MyString *_category;
		// Note: MyString::FindChar() returns the index of the
		// first instance of the character, if any (-1 if none).
		bool			isGlobal() const { return _category->FindChar('+') == 0; }
		bool			isSet() const { return _maxJobs != noThrottleSetting; }

		int				_totalJobs; // total # of jobs in this category
		int				_maxJobs; // max jobs in this cat that can run
		int				_currentJobs; // running jobs in this cat

		ThrottleInfo( const MyString *category, int maxJobs ) {
			_category = new MyString( *category );
			_totalJobs = 0;
			_maxJobs = maxJobs;
			_currentJobs = 0;
		}

		virtual ~ThrottleInfo() {
			delete _category;
		}
	};

	/** Constructor.
	*/
	ThrottleByCategory();

	/** Destructor.
	*/
	~ThrottleByCategory();

	/** Add a category to the list of categories.  This will fail if the
		category already exists.
		@param the name of the category
		@param (optional) the throttle setting (max # of jobs for this
			category)
		@return a pointer to this object's category name MyString (to
			avoid duplicate copies of the MyString)
	*/
	ThrottleInfo *AddCategory( const MyString *category,
				int maxJobs = noThrottleSetting );

	/** Get a pointer to the full map of throttles
	*/
	std::map<MyString, ThrottleInfo *>* GetThrottles() { return &_throttles; }

	/** Set the throttle (max # of jobs) for a category.  Adds the category
		if it doesn't already exist.
		@param the category name
		@param the throttle setting
	*/
	void	SetThrottle( const MyString *category, int maxJobs );

	/** Get the throttle info for a category.
		@param the category name
		@return a pointer to the throttle info for this category (NULL
			if the category doesn't exist)
	*/
	ThrottleInfo *	GetThrottleInfo( const MyString *category );

	/** Prefix all category names with the specified label.
		@param a MyString of the prefix for all categories
	*/
	void PrefixAllCategoryNames( const MyString &prefix );

	/** Print the throttle info.
		@param the FILE to print to
	*/
	void		PrintThrottles( FILE *fp ) /* const */;

private:
	std::map<MyString, ThrottleInfo *>	_throttles;
};

#endif	// _THROTTLE_BY_CATEGORY_H
