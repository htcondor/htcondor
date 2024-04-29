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

#include <map>
#include <string>

// Note: some of the complexity in this class is to allow a category maxjobs
// line to come before or after the corresponding node(s). wenger 2007-10-10.

class ThrottleByCategory {
public:
	static const int	noThrottleSetting;

	struct ThrottleInfo {
		const std::string *_category;
		bool			isGlobal() const { return _category->find('+') == 0; }
		bool			isSet() const { return _maxJobs != noThrottleSetting; }

		int				_totalJobs; // total # of jobs in this category
		int				_maxJobs; // max jobs in this cat that can run
		int				_currentJobs; // running jobs in this cat

		ThrottleInfo( const std::string *category, int maxJobs ) {
			_category = new std::string( *category );
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
		@return a pointer to this object's category name std::string (to
			avoid duplicate copies of the std::string)
	*/
	ThrottleInfo *AddCategory( const std::string *category,
				int maxJobs = noThrottleSetting );

	/** Get a pointer to the full map of throttles
	*/
	std::map<std::string, ThrottleInfo *>* GetThrottles() { return &_throttles; }

	/** Set the throttle (max # of jobs) for a category.  Adds the category
		if it doesn't already exist.
		@param the category name
		@param the throttle setting
	*/
	void	SetThrottle( const std::string *category, int maxJobs );

	/** Get the throttle info for a category.
		@param the category name
		@return a pointer to the throttle info for this category (NULL
			if the category doesn't exist)
	*/
	ThrottleInfo *	GetThrottleInfo( const std::string *category );

	/** Prefix all category names with the specified label.
		@param a std::string of the prefix for all categories
	*/
	void PrefixAllCategoryNames( const std::string &prefix );

	/** Print the throttle info.
		@param the FILE to print to
	*/
	void		PrintThrottles( FILE *fp ) /* const */;

private:
	std::map<std::string, ThrottleInfo *>	_throttles;
};

#endif	// _THROTTLE_BY_CATEGORY_H
