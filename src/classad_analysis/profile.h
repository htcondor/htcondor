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

#ifndef __PROFILE_H__
#define __PROFILE_H__

#include "boolExpr.h"
#include "list.h"

/** A BoolExpr in Profile Form, which is a conjunction of Conditions. Currently
	the only way to initialize a Profile is with the ExprToProfile method in
	the BoolExpr class.
	@see BoolExpr
*/
class Profile : public BoolExpr
{
	friend class BoolExpr;
 public:

		/** Default Constructor */
	Profile( );

		/** Destructor */
	~Profile( );

		/** Gets the number of conditions contained in the Profile
			@param result The number of profiles.
			@return true on success, false on failure.
		*/
   	bool GetNumberOfConditions( int &result );

		/** Rewinds the list of Conditions in the Profile
			@return true on success, false on failure
		*/
	bool Rewind( );

		/** Gets the next Condition in the Profile.
			@param result the next profile in the Profile
			@return true on success, false on failure
		*/
	bool NextCondition( Condition *&result );

		// unsupported methods
	bool AddCondition( Condition & );
	bool RemoveCondition( Condition & );
	bool RemoveAllConditions( );

		/** A repository for annotations for the Profile.
			@see ProfileExplain
		*/
    Profile& operator=(const Profile& copy) { return *this; } 
	ProfileExplain explain;
 private:
	bool AppendCondition( Condition * );
	List<Condition> conditions;
};

#endif // __PROFILE_H__


