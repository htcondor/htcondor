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

#ifndef _USER_PROFILE_WINDOWS_H_
#define _USER_PROFILE_WINDOWS_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include <profinfo.h>
#include "env.h"

/***************************************************************
 * Forward declarations
 ***************************************************************/

class OsProc;

/***************************************************************
 * OwnerProfile class
 ***************************************************************/

class OwnerProfile {

    /***************************************************************
     * Rocky relationships
     ***************************************************************/

    /* Allow OsProc to see our private bounty */
    friend OsProc;

public:

    /***************************************************************
     * {c,d}tors
     ***************************************************************/

    OwnerProfile ();
    virtual ~OwnerProfile ();
	
    /***************************************************************
     * Methods
     ***************************************************************/

    /* returns TRUE if the internals were correctly initialized;
       otherwise, FALSE */
    virtual BOOL update ();
    
    /* returns TRUE if a user profile was created; otherwise, FALSE. */
    virtual BOOL create ();
    
    /* returns TRUE if a user profile was loaded; otherwise, FALSE.*/
    virtual BOOL load ();
    
    /* returns TRUE if a user profile was unloaded; otherwise, FALSE.*/
    virtual BOOL unload ();
    
    /* returns TRUE if a user's environment was loaded; otherwise, FALSE.*/
	virtual BOOL environment(Env &env) { return environment(env, user_token_, user_name_); }
    
    /***************************************************************
     * Attributes
     ***************************************************************/
    
    /* returns TRUE if a user profile is loaded; otherwise, FALSE.*/
    virtual BOOL loaded () const;
    
    /* returns TRUE if a user's profile exists; otherwise, FALSE.*/
    virtual BOOL exists () const;
    
    /* returns the type of profile the current user has; 
       otherwise 0. */
    virtual DWORD type () const;

    /* if it exits, this function returns the path to the root 
       directory of the user's profile; otherwise, NULL. Remember 
       to delete the return value (using the delete [] form).*/
    virtual PSTR directory ();	

     PCSTR username() const { return user_name_; }
	 static BOOL environment(Env & env, HANDLE user_token, PCSTR username);

    /* returns TRUE if a user profile was destroyed; otherwise, FALSE. 
       It's protected because it does not change the state of the 
       profile object, so even though the profile object may think it 
       is loaded, in reality, it's completely gone. This is directly 
       remedied by the code surrounding its invocation. */
    virtual BOOL destroy () const;

protected:

	/* returns TRUE if the user profile template was backup-ed up;
    otherwise, FALSE.*/
    virtual BOOL backup ();

    /* returns TRUE if the user profile directory was restored; 
    otherwise, FALSE.*/
    virtual BOOL restore ();

private:

    /***************************************************************
     * Private {c,d}tors and copy operator.  Here to stop silly users
     * from copying the profile object.
     ***************************************************************/
    
    /* hide the copy constructor and assignment operator from any
       naughty users-- you want to copy what?!? */
    OwnerProfile ( OwnerProfile const& );
    OwnerProfile& operator = ( OwnerProfile const& );
    
    /***************************************************************
     * Private Methods
     ***************************************************************/
    
    /* returns TRUE if the user profile is loaded; otherwise, 
       FALSE.  This is a simple helper that does all the initialization
       required to do the loading of a profile. */
    BOOL loadProfile ();
    
    /* returns TRUE if the user profile was unloaded; otherwise, 
       FALSE.  This is a simple helper that not only unloads the 
       profile, but ensures that it is made to look like it is 
       unloaded-- which is a state relied upon heavily in this class. */
    BOOL unloadProfile ();
    
    /***************************************************************
     * Data
     ***************************************************************/
    
    /* a boolean representing the state of the profile */
    BOOL        profile_loaded_;
    
    /* a handle to user's profile */
    PROFILEINFO	user_profile_;
    
    /* the actual user's login token. */
    HANDLE      user_token_;
    
    /* the user's name. */
    PCSTR       user_name_;
    
    /* the domain the user belongs to, if any */
    PCSTR       domain_name_;
    
    /* the user's profile directory */
    PSTR        profile_directory_;
    
    /* the directory we use as a template to create new user 
       profile directories */
    PSTR        profile_template_;

    /* the directory we use as the root of our cache for 
       user profile directories */
    PSTR        profile_cache_;

    /* a per-session directory we use as a cache for the current
       user profile directory */
    PSTR        profile_backup_;
    
};

#endif // _USER_PROFILE_WINDOWS_H_
