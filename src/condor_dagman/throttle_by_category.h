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

#ifndef _THROTTLE_BY_CATEGORY_H
#define _THROTTLE_BY_CATEGORY_H

#include "HashTable.h"

// Note: some of the complexity in this class is to allow a category maxjobs
// line to come before or after the corresponding node(s).  Also, the fact
// that HashTable doesn't provide a way to get the index, and we want to
// avoid duplicate MyStrings causes more complexity.  wenger 2007-10-10.

class ThrottleByCategory {
public:
	static const int	noThrottleSetting = -1;

	struct ThrottleInfo {
		const MyString *_category;
		int				_maxJobs;
		int				_currentJobs;
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

	/** Print the throttle info.
		@param the FILE to print to
	*/
	void		PrintThrottles( FILE *fp ) /* const */;

private:
	HashTable<MyString, ThrottleInfo *>	_throttles;
};

#endif	// _THROTTLE_BY_CATEGORY_H
