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


#ifndef __RESOURCEGROUP_H__
#define __RESOURCEGROUP_H__

#define WANT_CLASSAD_NAMESPACE
#include <iostream>
#include "classad/classad_distribution.h"
#include "list.h"

/** A wrapper for a list of ClassAds.  In the future this may be modified to
	handle ClassAdCollections.
*/
class ResourceGroup
{
 public:

		/** Default Constructor */
	ResourceGroup( );

		/** Destructor */
	~ResourceGroup( );

		/** Initialized the ResourceGroup to contain the given list of
			ClassAds. Only a shallow copy of the list is made.
			@param result the List of ClassAds for the ResourceGroup to
			contain.
			@return true on success, false on failure.
		*/
	bool Init( List<classad::ClassAd>& result );

		/** Populates a list with the classads contained by the ResourceGroup
			Only a shallow copy of the list is made.
			@param result the destination List for the ClassAds.
			@return true on success, false on failure.
		*/
	bool GetClassAds( List<classad::ClassAd>& result );

		/** Gets the number of classads in the ResourceGroup.
			@param result Assigned the number of ClassAds.
			@return true on success, false on failure.
		*/
	bool GetNumberOfClassAds( int& result );

		/** Generates a string representation of the ResourceGroup.  It prints
			the classads using PrettyPrint seperated by newlines.
			@param the buffer to print the ClassAds to.
			@return true on success, false on failure.
		*/
	bool ToString( std::string& buffer );
 private:
	bool initialized;
	List<classad::ClassAd> classads;
};

#endif // __RESOURCEGROUP_H__
