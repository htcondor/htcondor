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

#ifndef __RESOURCEGROUP_H__
#define __RESOURCEGROUP_H__

#define WANT_NAMESPACES
#include "classad_distribution.h"
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
