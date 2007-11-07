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


