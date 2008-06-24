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
#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "perm.h"

#include "profile_helpers.WINDOWS.h"
#include "string_conversion.WINDOWS.h"
#include "system_info.WINDOWS.h"

#include <userenv.h>	// for LoadUserProfile, etc.
#include <ntsecapi.h>
#include <lm.h>			// for NetUserGetInfo

/************************************************************************/
/* Constants                                                            */
/************************************************************************/

const char *PARAM_PROFILE_USER_TEMPLATE = "PROFILE_USER_TEMPLATE";
const char *PARAM_PROFILE_CACHE_DIR = "PROFILE_CACHE_DIR";

/************************************************************************/
/* Forward declarations                                                 */
/************************************************************************/

BOOL CondorCopyDirectory ( PCSTR source, PCSTR destination );
BOOL CondorRemoveDirectory ( PCSTR directory );

BOOL CondorGetUserProfileDirectory ( PSTR *directory );
BOOL CondorGetDefaultUserProfileDirectory ( PSTR *directory );

BOOL ForceDeleteA ( PCSTR filename, PCSTR filter = "File" );
BOOL ForceDeleteW ( PCWSTR w_filename, PCWSTR w_filter = L"File" );

/************************************************************************/
/* Globals				                                                */
/************************************************************************/

BOOL	UserProfileInitialized		= FALSE;
PSTR	UserProfileDirectory		= NULL;
PSTR	UserProfileBackupDirectory	= NULL;
HANDLE	UserProfileHandle			= NULL;
HANDLE	UserProfileName				= NULL;

/************************************************************************/
/* message formating                                                    */
/************************************************************************/

extern PSTR 
CondorFormatMessage(DWORD last_error, DWORD flags = 0) {

	DWORD	length	= 0;
	LPVOID 	buffer	= NULL;
	PSTR	message	= NULL;

	/* remove the junk we don't support... this is just for basic
	message handling */
	flags &= ~( FORMAT_MESSAGE_FROM_STRING 
        | FORMAT_MESSAGE_FROM_HMODULE 
		| FORMAT_MESSAGE_ARGUMENT_ARRAY );

    __try {
		
		length = FormatMessage ( 
            flags | FORMAT_MESSAGE_ALLOCATE_BUFFER, 
			NULL, 
            last_error, 
            MAKELANGID ( LANG_NEUTRAL, SUBLANG_DEFAULT ), 
			(PSTR) &buffer, 
            0, 
            NULL );

		if ( 0 == length ) {
			__leave;
		}
		
		message = new CHAR[length + 1];
		ASSERT ( message );
		strcpy ( message, (PCSTR) buffer );
		message[length - 1] = '\0'; /* remove new-line */

	}
	__finally {

		if ( buffer ) {
			LocalFree ( buffer );
		}

	}

    return message;

}

/************************************************************************/
/* Profile Directory function                                           */
/************************************************************************/

/* this is a basically a convenience wrapper around 
GetUserProfileDirectory. All it does it call the API 
function twice in the case we have not allocated 
enough buffer space for the path */
static BOOL 
CondorGetUserProfileDirectory ( PSTR *directory ) {

	HANDLE		user_token				= priv_state_get_handle ();
	PSTR		profile_directory		= new CHAR[MAX_PATH]; 
	DWORD		profile_directory_size	= MAX_PATH;
	BOOL		ok						= TRUE;

	__try {
		
		if ( !GetUserProfileDirectory ( 
            user_token, 
            profile_directory,
			&profile_directory_size ) ) {
		
			/* since we only allocated MAX_PATH CHARs, we may fail 
            with at least ERROR_INSUFFICIENT_BUFFER, so we catch it 
            and allocate the buffer size we were given by 
            GetUserProfileDirectory() */
			if ( ERROR_INSUFFICIENT_BUFFER == GetLastError () ) {
				
				delete [] profile_directory; /* kill the old buffer */
				profile_directory = new CHAR[profile_directory_size];
				ASSERT ( profile_directory );

				if ( !GetUserProfileDirectory ( 
                    user_token, 
                    profile_directory,
					&profile_directory_size ) ) {
					dprintf ( D_FULLDEBUG, 
                        "CondorGetUserProfileDirectory: could not "
                        "get profile directory (last-error = %u)\n",
						GetLastError () );
					ok = FALSE;
					__leave;
				}
				
			} else {
				
				/* print the fact the user has no home directory */
				dprintf ( D_FULLDEBUG, 
                    "CondorGetUserProfileDirectory: "
                    "this user has no profile directory\n" );				
				ok = FALSE;
				__leave;
			
			}

		}
			
		/* if we get this far, then we have a profile directory */
        *directory = profile_directory;
		
	}
	__finally {

		if ( !ok ) {
			delete [] profile_directory;
			profile_directory = NULL;
		}

	}

	return ok;

}

extern PSTR
CondorGetAllUsersProfileDirectory () {

	PSTR	directory	= new CHAR[MAX_PATH]; 
	DWORD	size		= MAX_PATH;
	BOOL	ok			= TRUE;

	_try {
		
		if ( !GetAllUsersProfileDirectory ( 
            directory, 
            &size ) ) {
			
			/* since we only allocated MAX_PATH CHARs, we may fail
            with at least ERROR_INSUFFICIENT_BUFFER, so we catch it 
            and allocate the buffer size we were given by 
            GetDefaultUserProfileDirectory() */
			if ( ERROR_INSUFFICIENT_BUFFER == GetLastError () ) {
				
				delete [] directory; /* kill the old buffer */
				directory = new CHAR[size];
				ASSERT ( directory );

				if ( NULL == directory ) {
					dprintf ( D_FULLDEBUG, 
                        "CondorGetAllUserProfileDirectory: "
						"could not allocate memory for directory "
						"(last-error = %u)\n", GetLastError () );
					ok = FALSE;
					__leave;
				}

				if ( !GetAllUsersProfileDirectory ( 
                    directory, 
                    &size ) ) {

					dprintf ( D_FULLDEBUG, 
                        "CondorGetAllUserProfileDirectory: "
						"could not get default directory "
						"(last-error = %u)\n", GetLastError () );
					ok = FALSE;
					__leave;

				}

			} else {

				/* print the fact the user has no home directory */
				dprintf ( D_FULLDEBUG, 
                    "CondorGetAllUserProfileDirectory: "
					"could not get default directory \n"
					"(last-error = %u)\n", GetLastError () );
				ok = FALSE;
				__leave;

			}
		}
	}
	__finally  {

		if ( !ok && directory ) {
			delete [] directory;
			directory = NULL;
		}

	}

	return directory;

}

extern PSTR
CondorGetDefaultUserProfileDirectory () {

    PSTR	directory	= new CHAR[MAX_PATH]; 
    DWORD	size		= MAX_PATH;
    BOOL	ok			= TRUE;

    _try {

        if ( !GetDefaultUserProfileDirectory ( 
            directory, 
            &size ) ) {

                /* since we only allocated MAX_PATH CHARs, we may fail
                with at least ERROR_INSUFFICIENT_BUFFER, so we catch it 
                and allocate the buffer size we were given by 
                GetDefaultUserProfileDirectory() */
                if ( ERROR_INSUFFICIENT_BUFFER == GetLastError () ) {

                    delete [] directory; /* kill the old buffer */
                    directory = new CHAR[size];
                    ASSERT ( directory );

                    if ( NULL == directory ) {
                        dprintf ( D_FULLDEBUG, 
                            "CondorGetDefaultUserProfileDirectory: "
                            "could not allocate memory for directory "
                            "(last-error = %u)\n", GetLastError () );
                        ok = FALSE;
                        __leave;
                    }

                    if ( !GetDefaultUserProfileDirectory ( 
                        directory, 
                        &size ) ) {

                            dprintf ( D_FULLDEBUG, 
                                "CondorGetDefaultUserProfileDirectory: "
                                "could not get default directory "
                                "(last-error = %u)\n", GetLastError () );
                            ok = FALSE;
                            __leave;

                    }

                } else {

                    /* print the fact the user has no home directory */
                    dprintf ( D_FULLDEBUG, 
                        "CondorGetDefaultUserProfileDirectory: "
                        "could not get default directory \n"
                        "(last-error = %u)\n", GetLastError () );
                    ok = FALSE;
                    __leave;

                }
        }
    }
    __finally  {

        if ( !ok && directory ) {
            delete [] directory;
            directory = NULL;
        }

    }

    return directory;

}

/************************************************************************/
/* Profile loading                                                      */
/************************************************************************/

/* returns TRUE if a user profile was created; otherwise, FALSE. */
BOOL 
CondorCreateUserProfile () {

    HANDLE      user_token          = priv_state_get_handle ();
	PCSTR		user_name			= get_user_loginname ();
	PSTR		profile_template	= param ( PARAM_PROFILE_USER_TEMPLATE ),
				profile_directory   = NULL;
	BOOL		profile_loaded		= FALSE,
				profile_exists		= TRUE;
    int         length              = 0;
	bool		ok					= true;
	PROFILEINFO	profile_info;
	priv_state	priv;

	__try {

        /* Do the following as condor (since we can't do it as the 
		user, since we are creating it), plus we need admin rights */
		priv = set_condor_priv ();

        /* initialize profile information */
        ZeroMemory ( &profile_info, sizeof ( PROFILEINFO ) );
        profile_info.dwSize	= sizeof ( PROFILEINFO );

        /* The PI_NOUI flag avoids the "I'm going to make a temporary 
        profile for you... blah, blah, blah" dialog from being 
        displayed in the case when a roaming profile is inaccessible 
        (among other things) */
        profile_info.dwFlags	= PI_NOUI;
        profile_info.lpUserName	= (char*)user_name;

        /* create a directory name based on the user's name 
        and the root of the common user's directory */ 
        length = strlen ( profile_directory ) 
            + strlen ( user_name ) + 1 ; /* +1 for \ */
        profile_directory = new CHAR[length + 1];            
        sprintf ( 
            profile_directory, 
            "%s\\%s", 
            profile_template, 
            user_name );

        /* Some users may want their condor-slot accounts based on 
        different profile than the one supplied by the system */	
        if ( NULL != profile_template ) {		

            /* this must be done as the user, otherwise the directories
            will have the wrong ACLs */
            priv = set_user_priv ();

            /* copy the template directory to the slot-user's 
            new profile directory */
            CondorCopyDirectory (
                profile_template,
                profile_directory );

            /* return to previous privilege level */
            set_priv ( priv );

        }

        /* let the load profile function load the information
        from the new location */
        profile_info.lpDefaultPath = profile_directory;
		
		/* load the user's profile for the first time-- this will 
        create it */
		profile_loaded = LoadUserProfile ( 
            user_token, 
            &profile_info );
		
		dprintf ( D_FULLDEBUG, "CondorCreateProfile: creating "
            "profile %s (last-error = %u)\n", 
            profile_loaded ? "succeeded" : "failed", 
            profile_loaded ? 0 : GetLastError () ); /* have to do this, 
                                                    otherwise we get 
                                                    get garbage from 
                                                    GetLastError */
		
		if ( !profile_loaded ) {
			ok = false;
			__leave;
		}

		/* unload the profile */
		UnloadUserProfile ( user_token, profile_info.hProfile );

		/* now get the user's local profile directory */
		profile_exists = CondorGetUserProfileDirectory ( 
            &UserProfileDirectory );
		
		if ( profile_exists && !UserProfileDirectory ) {
			dprintf ( D_FULLDEBUG, 
                "CondorCreateProfile: failed to load user's "
				"profile directory\n" );
			ok = false;
			__leave;
		}

		/* print the local profile directory... */
		if ( profile_exists ) {
			dprintf ( D_FULLDEBUG, "CondorCreateProfile: new "
				"user's profile directory: '%s'\n", 
                UserProfileDirectory );
		}

	}
	__finally {

		if ( profile_template ) {
			/* was from param(), so we use free() */
			free ( profile_template );
		}
        if ( profile_directory ) {
            delete [] profile_directory;
        }
        if ( profile_loaded && profile_info.hProfile && !ok ) {
			/* unload the profile */
			UnloadUserProfile ( user_token, profile_info.hProfile );
		}
		
		/* return to previous privilege level */
		set_priv ( priv );

	}

	return ok;
	
}

/* returns TRUE if a user profile was loaded; otherwise, FALSE.*/
BOOL
CondorLoadUserProfile () {

	/* short-cut if we've already loaded the profile */
	if ( UserProfileInitialized ) {
		return TRUE;
	}

	HANDLE			user_token		      = priv_state_get_handle ();
    PCSTR			domain_name		      = ".",
					user_name		      = get_user_loginname (),
                    profile_user_template = param ( PARAM_PROFILE_USER_TEMPLATE ),
                    profile_cache_dir     = param ( PARAM_PROFILE_CACHE_DIR );                  
	HANDLE			have_access		      = NULL;
	DWORD			last_error		      = 0,
					length			      = 0,
					i				      = 0;
	PROFILEINFO		profile_info;
	PUSER_INFO_4	puser_info		      = NULL;
	BOOL			backup_created	      = FALSE,
					profile_exists	      = FALSE,
					profile_loaded	      = FALSE,					
					ok				      = TRUE;	
	priv_state		priv;

		
	__try {

        /* we must do the following as condor */
		priv = set_condor_priv ();

		/* basic init */
		ZeroMemory ( &profile_info, sizeof ( PROFILEINFO ) );
		profile_info.dwSize	= sizeof ( PROFILEINFO );
		
		/* The PI_NOUI flag avoids the "I'm going to make a 
        temporary profile for you... blah, blah, blah" dialog 
        from being displayed in the	case when a roaming profile 
        is inaccessible (among other things) */
		profile_info.dwFlags	= PI_NOUI;
		profile_info.lpUserName	= (char*) user_name;

		/* now get the user's local profile directory (if this user 
        has a roaming profile, this is where it's cached locally */
		profile_exists = CondorGetUserProfileDirectory ( 
            &UserProfileDirectory );
		
		if ( profile_exists && !UserProfileDirectory ) {
			dprintf ( D_FULLDEBUG, 
                "CondorLoadUserProfile: failed to load "
				"user's profile directory (last-error = %u)\n", 
                GetLastError() );
			ok = FALSE;
			__leave;
		}

		/* print the local profile directory... */
		if ( profile_exists ) {
			dprintf ( D_FULLDEBUG, 
                "CondorLoadUserProfile: actual profile "
				"directory: '%s'\n", 
                UserProfileDirectory );
		}
	
		/* if we do have a profile directory, let's make sure we have 
        access to it.  Sometimes, like if the startd were to crash, 
        we may have a profile directory, but it may	still be locked by 
        the previous login that was not cleaned up properly (because 
        of the crash: the only resource I know of that the system does 
        not clean up on process termination are user login handles). */

		if ( profile_exists ) {

			have_access = CreateFile ( UserProfileDirectory, 
				GENERIC_WRITE, 0 /*  magic for NOT shared */, NULL, 
				OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS ,NULL );
		
			if ( INVALID_HANDLE_VALUE == have_access ) {

				last_error = GetLastError ();
				
				if ( ERROR_ACCESS_DENIED == last_error ) {

					/* so we don't have access, lets just blow it away
					and create a new one (see bellow) */
					ok = CondorRemoveDirectory ( 
                        UserProfileDirectory );
					
					dprintf ( D_FULLDEBUG, 
                        "CondorLoadUserProfile: removing "
						"locked profile %s (last-error = %u)\n", 
						ok ? "succeeded" : "failed", 
						ok ? 0 : GetLastError () );
					
                    /* we no longer have a profile directory, so flag 
                    it as such (this will induce the creation of one;
                    see bellow for the fun and exciting details)*/
                    profile_exists = FALSE;
                    
                    if ( !ok ) {
						__leave;
					}

				}
				
			} else {	
				
				/* we're all done, release the lock on the 
				directory so we can actually load it  */
				CloseHandle ( have_access );

			}

		} 

		/* if this user does not have profile this a problem because 
		the LoadUserProfile bellow will create a fresh one, and then allow
		the user to modify it without us being able to unroll the 
		changes... which is what we are trying to avoid. To fix this, 
		we explicitly create the profile ourselves */
		if ( !profile_exists ) {

			dprintf ( D_ALWAYS, 
                "CondorLoadUserProfile: profile "
				"directory does not exist, so we're "
                "going to create one\n" );

			/* so, go create the profile */
			ok = CondorCreateUserProfile ();
			
			dprintf ( D_FULLDEBUG, 
                "CondorLoadUserProfile: creating "
				"profile %s (last-error = %u)\n", 
				ok ? "succeeded" : "failed", 
				ok ? 0 : GetLastError () );
			
			if ( !ok ) {
				__leave;
			}

			/* now we can backup the existing profile :) */
		} 
		
		/*************************************************************
			Now we transfer the user's profile directory to the cache 
		so that we can revert to it once the user is done doing 
		his/her thang.                                                                    
		/************************************************************/ 
		
        /* create a backup directory name based on the profile 
		directory (i.e. profile_cache_dir) and the user's login 
        name */ 
		length = strlen ( profile_cache_dir ) 
			+ strlen ( user_name ) + 1
            + 10; /* +1 for \ +10 for pid */
		UserProfileBackupDirectory = new CHAR[length + 1];
		ASSERT ( UserProfileBackupDirectory );
		sprintf ( 
            UserProfileBackupDirectory, 
            "%s\\%s-%d", 
            profile_cache_dir, 
			user_name,
            GetCurrentProcessId () );

		/* finally, copy the user's profile to the back-up directory */
		backup_created = CondorCopyDirectory ( 
            UserProfileDirectory, 
			UserProfileBackupDirectory );
			
		dprintf ( D_FULLDEBUG, 
            "CondorLoadUserProfile: creating "
			"profile backup %s (last-error = %u)\n", 
			backup_created ? "succeeded" : "failed", 
			backup_created ? 0 : GetLastError () );

		if ( !backup_created ) {
			ok = FALSE;
			__leave;
		}

		/* load the user's profile */
		profile_loaded = LoadUserProfile ( 
            user_token, 
            &profile_info );

		dprintf ( D_FULLDEBUG, 
            "CondorLoadUserProfile: profile loading %s "
			"(last-error = %u)\n", 
            profile_loaded ? "succeeded" : "failed", 
			profile_loaded ? 0 : GetLastError () ); /* have to do this, 
													   otherwise we get 
													   get garbage from 
													   GetLastError */

		if ( !profile_loaded ) {
			ok = FALSE;
			__leave;
		}	

		/* save the handle to the profile, so we can unload it later */
		UserProfileHandle = profile_info.hProfile;
	
	}
	__finally {

		/* something went wrong, so inform the user */
		if ( !ok ) {			
			dprintf ( D_FULLDEBUG, "CondorLoadUserProfile: "
				"not loading user profile\n" );
		}			
		
        /* free the globals, if required */
        if ( !ok && UserProfileDirectory ) {
            delete [] UserProfileDirectory;
            UserProfileDirectory = NULL;
        }
        if ( !ok && backup_created ) {
            CondorRemoveDirectory ( UserProfileBackupDirectory );
            delete [] UserProfileBackupDirectory;	
            UserProfileBackupDirectory = NULL;
        }
        
		/* if we loaded the profile, but failed for some other reason,
		   then we should make sure to unload the profile */			
		if ( profile_loaded && !ok ) {
			UnloadUserProfile ( user_token, profile_info.hProfile );
		}

		
		if ( puser_info ) {
			NetApiBufferFree ( puser_info );
		}
		if ( have_access ) {
			CloseHandle ( have_access );
		}
		
		/* return to previous privilege level */
		set_priv ( priv );

	}


	return ( UserProfileInitialized = ok );

}

/* returns TRUE if a user profile was un-loaded; otherwise, FALSE.*/
BOOL 
CondorUnloadUserProfile () {

	HANDLE		user_token	= priv_state_get_handle ();
	BOOL		ok			= FALSE;
	priv_state	priv;
	
	__try {
		
		/* we must be running as the Condor to do this */
		priv = set_condor_priv ();
		
		/* unload the user's profile */
		ok = UnloadUserProfile ( user_token, UserProfileHandle );

		/* not much we can do if unloading fails... so we just tell someone */
		dprintf ( D_ALWAYS, "Unloading the user's profile "
			"(last-error = %u)\n", 
			ok ? "was successful" : "failed", 
            GetLastError() );	

		/**************************************************************/
        /* *NEVER* log the user off here, let our user-cache do that  */
		/**************************************************************/

		/* throw away the used copy of the profile */		
		if ( UserProfileBackupDirectory ) {
			
			/* remove the existing copy */
			ok = CondorRemoveDirectory ( UserProfileDirectory );
			
			/* again, there is not much we can do if directory removal
			fails... so we just tell someone */
			dprintf ( D_FULLDEBUG, "Removing the user's modified "
								   "profile %s (last-error = %u)\n",
					ok ? "was successful" : "failed", GetLastError() );

			if ( !ok ) {			
				__leave;
			}

			/* replace it with the back-up: we do NOT user MoveFile because
			it cannot move directories across volumes, nor does it preserve
			security descriptors, both of which are required for user 
			profiles */
			ok = CondorCopyDirectory ( 
                UserProfileBackupDirectory, 
				UserProfileDirectory );

			/* to be far too redundant-- again, there is not much we can do 
			if restoring the back-up fails... so we just tell someone */
			dprintf ( D_FULLDEBUG, "Restoring the user's profile "
				                   "%s (last-error = %u)\n",
					ok ? "was successful" : "failed", GetLastError() );
			
			if ( !ok ) {					
				__leave;
			}

			/* finally, remove the back-up copy */
			ok = CondorRemoveDirectory ( UserProfileBackupDirectory );

			/* to be far too redundant-- again, there is not much we can do 
			if restoring the back-up fails... so we just tell someone */
			dprintf ( D_FULLDEBUG, "Removing the back-up profile "
				                   "%s (last-error = %u)\n",
					ok ? "was successful" : "failed", GetLastError() );
			
			if ( !ok ) {
				/* if we fail to remove the back-up, we still consider this
				operation a success, because not only will Condor clean it 
				up for us later, but not only is the user unaffected by this 
				event, they really shouldn't care. */
				ok = true;
				__leave;
			}

		}	

	}
	__finally {

		/* reset init flag */
		UserProfileInitialized = FALSE;
		UserProfileHandle = NULL;

		/* free up the globals */
		if ( UserProfileDirectory ) {
			delete [] UserProfileDirectory;
			UserProfileDirectory = NULL;
		}
		if ( UserProfileBackupDirectory ) {
			delete [] UserProfileBackupDirectory;	
			UserProfileBackupDirectory = NULL;
		}

		/* return to previous privilege level */
		set_priv ( priv );

	}

	return ok;

}

BOOL 
CondorCreateEnvironmentBlock ( 
    Env &env ) {

	HANDLE		    user_token          = priv_state_get_handle ();
    PSTR            tmp                 = NULL;
    PWSTR			w_penv				= NULL,
					w_tmp				= NULL;
	PVOID			penv				= NULL;
	BOOL			env_block_created	= FALSE,
					ok					= TRUE;
	priv_state		priv;
	
	
	__try {
		
		/* we must do the following as condor */
		priv = set_condor_priv ();
		
		/* if we are loading the user's profile, then overwrite 
        any existing environment variables with the values in the 
        user's profile */
		env_block_created = CreateEnvironmentBlock ( 
            &penv, 
            user_token, 
			FALSE /* we already have the current process env */ );
		ASSERT ( penv );

		dprintf ( D_ALWAYS, "CondorCreateEnvironmentBlock: Loading %s "
			"while getting the user's environment (last-error = %u)\n",
			env_block_created ? "succeeded" : "failed", 
            GetLastError () );

		if ( !env_block_created ) {
			ok = FALSE;
			__leave;
		}
		
        /* fill the environment with values */
        w_tmp = w_penv;
        while ( '\0' != *w_tmp ) { /* read: while not "\0\0\" */
            tmp = ProduceAFromW ( w_tmp );
            if ( tmp && strlen ( tmp ) > 0 ) {
                env.SetEnv ( tmp );
                delete [] tmp;
            }
            w_tmp += wcslen ( w_tmp ) + 1;
        }

	}
	__finally {
	
		if ( penv ) {
			if ( !DestroyEnvironmentBlock ( penv ) ) {
				dprintf ( D_ALWAYS, "CondorCreateEnvironmentBlock: "
					"DestroyEnvironmentBlock failed "
					"(last-error = %u)\n", GetLastError () );
			}
		}

		/* return to previous privilege level */
		set_priv ( priv );

	}
	
	return ok;
}

/************************************************************************/
/* Directory function helpers                                           */
/************************************************************************/

extern PWSTR 
CondorAppendDirectoryToPath (
    PCWSTR w_path, 
    PCWSTR w_directory ) {
	
	DWORD	length	= 0;
	PWSTR	w_new	= NULL;
	
	length = wcslen ( w_path ) 
        + wcslen ( w_directory ) + 1; /* +1 for \ */
	w_new = new WCHAR[length + 1];    /* +1 for NULL */	
	ASSERT ( w_new );
	wsprintfW ( w_new, L"%s\\%s", w_path, w_directory );

	return w_new;
}

/************************************************************************/
/* Textbook recursive directory copy                                    */
/************************************************************************/

extern BOOL 
CondorCopyDirectoryImpl (
    PCWSTR w_source, 
    PCWSTR w_destination ) {
	
	PWSTR				w_source_searcher	= NULL,
						w_new_source		= NULL,
						w_new_destination	= NULL,
						all_pattern			= L"\\*.*";
	DWORD				length				= 0;
	PSTR				file_name			= NULL;
	WIN32_FIND_DATAW	find_data;
	HANDLE				current				= INVALID_HANDLE_VALUE;
	SECURITY_ATTRIBUTES security_attributes;
	BOOL				directory_created	= TRUE,
						file_created		= TRUE,
						ok					= TRUE;
	priv_state			priv;
	
	__try {

		/* we must do the following as condor */
		// priv = set_condor_priv ();
		priv = set_user_priv ();

		/* basic init */
		ZeroMemory ( &security_attributes, sizeof ( SECURITY_ATTRIBUTES ) );
		security_attributes.nLength = sizeof ( SECURITY_ATTRIBUTES );

		/* create the destination directory based on the settings of the
		source directory */
		ok = CreateDirectoryExW ( 
            w_source, 
			w_destination, 
            &security_attributes );

		if ( !ok ) {
			DWORD last_error = GetLastError ();
			dprintf ( D_FULLDEBUG, "CondorCopyDirectoryImpl: unable "
                "to write '%S' (last-error = %u)\n", 
                w_destination, last_error );
			PSTR error_message = CondorFormatMessage ( last_error );
			dprintf ( D_FULLDEBUG, "Reason: %s\n", error_message );
			delete [] error_message;
			ok = FALSE;
			__leave;
		}

		/* we want find things *in* w_source no just find w_source, so we append
		the *.* regex */
		length = wcslen ( w_source ) 
			+ wcslen ( all_pattern ) + 1; /* +1 for \ */
		w_source_searcher = new WCHAR[length + 1];
		ASSERT ( w_source_searcher );
		wsprintfW ( w_source_searcher, L"%s%s", w_source, all_pattern );
			
		current = FindFirstFileW ( w_source_searcher, &find_data );		
		
		if ( INVALID_HANDLE_VALUE == current ) {
			
			if ( ERROR_NO_MORE_FILES == GetLastError () ) {
			
				/* we're done in this directory */
				ok = TRUE;
				__leave;
			
			} 
			
			dprintf ( D_FULLDEBUG, "CondorCopyDirectoryImpl: unable to search '%S' "
				"(last-error = %u)\n", w_source, GetLastError () );
			
			ok = FALSE;
			__leave;

		}	
		
		do {
			
			if ( !( 0 == wcscmp ( find_data.cFileName, L"." ) 
				|| 0 == wcscmp ( find_data.cFileName, L".." ) ) ) {

				w_new_source = CondorAppendDirectoryToPath ( 
                    w_source, 
					find_data.cFileName );
				ASSERT ( w_new_source );
				
				w_new_destination = CondorAppendDirectoryToPath ( 
                    w_destination, 
                    find_data.cFileName );
				ASSERT ( w_new_destination );

				if ( FILE_ATTRIBUTE_DIRECTORY & find_data.dwFileAttributes ) {					

					/* recurse to the next level */
					ok = CondorCopyDirectoryImpl ( 
                        w_new_source, 
						w_new_destination );
					
				} else {

					/* it's just a file, so copy it */
					ok = CopyFileW ( 
                        w_new_source, 
                        w_new_destination, 
                        TRUE );

				}
				
				if ( !ok ) {
					dprintf ( D_FULLDEBUG, "CondorCopyDirectoryImpl: "
                        "unable to copy '%S' (last-error = %u)\n", 
                        find_data.cFileName, GetLastError () );
					__leave;
				}
				
				delete [] w_new_source;	
				w_new_source = NULL;
				delete [] w_new_destination;
				w_new_destination = NULL;

			} 

		} while ( ok && FindNextFileW ( current, &find_data ) );

	}
	__finally {

		if ( INVALID_HANDLE_VALUE != current ) {
			FindClose ( current );
		}

		if ( w_source_searcher ) {
			delete [] w_source_searcher;
		}
		if ( w_new_source ) {
			delete [] w_new_source;
		}
		if ( w_new_destination ) {
			delete [] w_new_destination;
		}

		/* return to previous privilege level */
		set_priv ( priv );

	}

	return ok;

}


extern BOOL 
CondorCopyDirectory ( 
    PCSTR source, 
    PCSTR destination ) {
	
	PWSTR	w_source		= NULL,
			w_destination	= NULL;
    HANDLE	have_access		= NULL;
	BOOL	ok				= TRUE;
	
	__try {
		
		/* create wide versions of the strings */
		w_source = ProduceWFromA ( source );
		ASSERT ( w_source );
		w_destination = ProduceWFromA ( destination );
		ASSERT ( w_destination );

		dprintf ( D_FULLDEBUG, "CondorCopyDirectory: Source: %s\n", source );
		dprintf ( D_FULLDEBUG, "CondorCopyDirectory: Destination: %s\n", destination );

        /* check if the destination already exists first */
        have_access = CreateFile ( destination, 
            GENERIC_WRITE, 0 /*  magic for NOT shared */, NULL, 
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS ,NULL );

        if ( INVALID_HANDLE_VALUE == have_access ) {

            if ( ERROR_ACCESS_DENIED == GetLastError () ) {

                /* We don't have access, lets just blow it away
                and copy over it */
                ok = CondorRemoveDirectory ( 
                    destination );

                dprintf ( D_FULLDEBUG, 
                    "CondorCopyDirectory: removing "
                    "locked directory %s (last-error = %u)\n", 
                    ok ? "succeeded" : "failed", 
                    ok ? 0 : GetLastError () );

                if ( !ok ) {
                    __leave;
                }

            }
        
        }

        /* do the actual work */
		ok = CondorCopyDirectoryImpl ( w_source, w_destination );

		if ( !ok ) {
			dprintf ( D_FULLDEBUG, "CondorCopyDirectory: unable "
				"to copy from '%S' to '%S' (last-error = %u)\n", 
				w_source, w_destination, GetLastError () );
			__leave;
		}

	}
	__finally {

		if ( ok ) {
			SetLastError ( 0 );
		}

		delete [] w_source;
		delete [] w_destination;

	}

	return ok;

}

/************************************************************************/
/* Textbook recursive directory deletion                                */
/************************************************************************/

/* recursively delete a directory */
extern BOOL 
CondorRemoveDirectoryImpl ( 
    PCWSTR w_directory ) {

	PWSTR				w_directory_searcher = NULL,
						w_tmp				 = NULL,
						all_pattern			 = L"\\*.*";
	DWORD				length				 = 0,
						attributes			 = 0;
	PSTR				file_name			 = NULL;
	WIN32_FIND_DATAW	find_data;
	HANDLE				current		 		 = INVALID_HANDLE_VALUE;
	BOOL				ok					 = TRUE;

	_try {

		/* we want find things *in* w_source no just find 
        w_source, so we append the *.* regex */
		length = wcslen ( w_directory ) 
			+ wcslen ( all_pattern ) + 1;				/* +1 for \ */
		w_directory_searcher = new WCHAR[length + 1];	/* +1 for NULL */
		ASSERT ( w_directory_searcher );
		wsprintfW ( 
            w_directory_searcher, 
            L"%s%s", 
            w_directory, 
            all_pattern );
			
		current = FindFirstFileW ( 
            w_directory_searcher, 
            &find_data );

		if ( INVALID_HANDLE_VALUE == current ) {
			
			if ( ERROR_NO_MORE_FILES == GetLastError () ) {
			
				/* we're done in this directory */
				ok = TRUE;
				__leave;
			
			} 
			
			dprintf ( D_FULLDEBUG, "CondorRemoveDirectoryImpl: "
                "unable to search '%S' (last-error = %u)\n", 
                w_directory, GetLastError () );
			
			ok = FALSE;
			__leave;

		}	
		
		do {
			
			if ( !( 0 == wcscmp ( find_data.cFileName, L"." ) 
				||  0 == wcscmp ( find_data.cFileName, L".." ) ) ) {

				w_tmp = CondorAppendDirectoryToPath ( 
                    w_directory, 
					find_data.cFileName );
				ASSERT ( w_tmp );

				/* remove any attributes that may inhibit a deletion */
				attributes = GetFileAttributesW ( w_tmp );
				attributes &= ~( FILE_ATTRIBUTE_HIDDEN | 
                    FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM );
				SetFileAttributesW ( 
                    w_tmp, 
                    attributes );

				if ( FILE_ATTRIBUTE_DIRECTORY & find_data.dwFileAttributes ) {					

					/* recurse to the next level */
					ok = CondorRemoveDirectoryImpl ( w_tmp );
					
					if ( !ok ) {
						dprintf ( D_FULLDEBUG, "CondorRemoveDirectoryImpl: "
                            "unable to delete the contents of "
                            "directory '%S' (last-error = %u)\n", 
							find_data.cFileName, GetLastError () );
						__leave;
					}

					/* finally, now that we are done with deleting 
                    the contents of the given directory, delete it 
                    too */
					ok = RemoveDirectoryW ( w_tmp );

					/* if we can't delete the directory regularly, 
                    then we pull some kernel magic out, and try and 
                    release any locks on the directory, and then 
					delete it */
					if ( !ok ) {
						ok = ForceDeleteW ( w_tmp, L"Directory" );
					}

					if ( !ok ) {
						dprintf ( D_FULLDEBUG, 
                            "CondorRemoveDirectoryImpl: unable "
							"to delete directory '%S' "
                            "(last-error = %u)\n", 
							find_data.cFileName, GetLastError () );
						__leave;
					}
					
				} else {

					/* it's just a file, so delete it */
					ok = DeleteFileW ( w_tmp );

					/* if we can't delete the file regularly, then we 
                    pull some kernel magic out, and try and release 
                    any locks on the file, and then delete it */
					if ( !ok ) {
						ok = ForceDeleteW ( w_tmp );
					} 

					if ( !ok ) {
						dprintf ( D_FULLDEBUG, 
                            "CondorRemoveDirectoryImpl: unable "
							"to delete file '%S' (last-error = %u)\n", 
							find_data.cFileName, GetLastError () );
						__leave;
					}

				}			
				
				delete [] w_tmp;	
				w_tmp = NULL;
			
			} 

		} while ( ok && FindNextFileW ( current, &find_data ) );

	}
	__finally {

		if ( INVALID_HANDLE_VALUE != current ) {
			FindClose ( current );
		}
		
		if ( w_directory_searcher ) {
			delete [] w_directory_searcher;
		}
		if ( w_tmp ) {
			delete [] w_tmp;
		}

	}

	return ok;

}

extern BOOL 
CondorRemoveDirectory( PCSTR directory ) {

	PWSTR	w_directory	= NULL;
	BOOL	ok			= TRUE;
	
	__try { 
		
		/* create wide versions of the strings */
		w_directory = ProduceWFromA ( directory );
		ASSERT ( w_directory );
		
		dprintf ( D_FULLDEBUG, "CondorRemoveDirectory: Removing: %s\n", directory );					
		
		/* do the actual work */
		ok = CondorRemoveDirectoryImpl ( w_directory );
		
		if ( !ok ) {
			dprintf ( D_FULLDEBUG, "CondorRemoveDirectory: unable "
				"to delete the contents of directory '%S' "
                "(last-error = %u)\n", 
				w_directory, GetLastError () );
			__leave;
		}

		/* finally, now that we are done with deleting the contents of 
		the given directory, delete it too */
		ok = RemoveDirectoryW ( w_directory );

		/* if we can't delete the directory regularly, then we pull some kernel
		magic out, and try and release any locks on the directory, and then 
		delete it */
		if ( !ok ) {
			ok = ForceDeleteW ( w_directory );
		} 
		
		if ( !ok ) {
			dprintf ( D_FULLDEBUG, "CondorRemoveDirectory: unable "
				"to delete directory '%S' (last-error = %u)\n", 
				w_directory, GetLastError () );
			__leave;
		}

	}
	__finally {

		if ( ok ) {
			SetLastError ( 0 );
		}

		if ( w_directory ) {
			delete [] w_directory;
		}	

	}
	
	return ok;

}

/************************************************************************/
/* Network stuff                                                        */
/************************************************************************/

extern BOOL 
CondorGetNetUserHomeDirectory (
    PCSTR domain_name, 
    PSTR  *home_directory ) {

    PCSTR           user_name		= get_user_loginname ();
    PWSTR			w_user_name		= NULL,
					w_domain_name	= NULL;
	PUSER_INFO_4	puser_info		= NULL;
	NET_API_STATUS	got_user_info	= NERR_Success;
	BOOL			ok				= TRUE;
	priv_state		priv;

	__try {

		/* the following should be run as Condor */
		priv = set_condor_priv ();

		/* we need Unicode versions of these strings for NetUserGetInfo */
		w_user_name	  = ProduceWFromA ( user_name ),
		w_domain_name = ProduceWFromA ( domain_name );
		ASSERT ( w_user_name );
		ASSERT ( w_domain_name );

		if ( NULL == w_user_name || NULL == w_domain_name ) {
			dprintf ( D_MALLOC, "CondorGetNetUserHomeDirectory: failed "
                "to allocate memory for user/domain strings?!" );
			ok = FALSE;
			__leave;
		}

		dprintf ( D_FULLDEBUG, "CondorGetNetUserHomeDirectory: getting "
							   "network user information\n");

		/* if the user has a roaming profile, then we need 
		to get the remote path */
		got_user_info = NetUserGetInfo ( w_domain_name, w_user_name, 
			4 /* magically fill PUSER_INFO_4 */, (PBYTE*) &puser_info );
		
		if ( NERR_Success != got_user_info ) {
			dprintf ( D_FULLDEBUG, "CondorGetNetUserHomeDirectory: "
                "failed to get network user information "
				"(last-error = %u)\n", 
				GetLastError () );
			puser_info = NULL;
			ok = FALSE;
			__leave;
		}		

		/* make sure the account is actually enabled */
		if ( puser_info->usri4_flags & UF_ACCOUNTDISABLE ) {
			dprintf ( D_ALWAYS, "CondorGetNetUserHomeDirectory: "
                "user's account it is disabled!!\n" );
			ok = FALSE;
			__leave;
		}

		/* There will aways a non-NULL home directory entry, but it may be 
		of zero length. If it is of zero length, it means this is not a 
		roaming profile */			
		if ( wcslen ( puser_info->usri4_home_dir ) > 0 ) {			
			
			dprintf ( D_FULLDEBUG, "CondorGetNetUserHomeDirectory: "
                "this user has a roaming profile. You may experience "
                "a delay in execution, if the user has a large "
                "profile\n" );
			
			*home_directory = ProduceAFromW ( puser_info->usri4_home_dir );
			ASSERT ( *home_directory );

			if ( NULL == *home_directory ) {
				dprintf ( D_MALLOC, "CondorGetNetUserHomeDirectory: failed to allocate "
									"memory for home directory string?!" );
				ok = FALSE;
				__leave;
			}
			
			/* print the listed home directory... */			
			dprintf ( D_FULLDEBUG, "CondorGetNetUserHomeDirectory: network home directory: "
				"'%s'\n", *home_directory );

		}
		
	}		
	__finally {

		if ( puser_info ) {
			NetApiBufferFree ( puser_info );
		}
		if ( w_user_name ) {
			delete [] w_user_name;
		}
		if ( w_domain_name ) {
			delete [] w_domain_name;
		}

		/* return to previous privilege level */
		set_priv ( priv );

	}

	return ok;

}

/************************************************************************/
/* Scary stuff                                                          */
/************************************************************************/

/* All the following is based on CodeGuru Articles:
	+ www.codeguru.com/Cpp/W-P/files/fileio/article.php/c1287/
	+ www.codeguru.com/Cpp/W-P/system/processesmodules/article.php/c2827 
*/

extern DWORD 
CloseRemoteHandle ( PCSTR process_name, DWORD pid, HANDLE handle ) {
	
	HANDLE					process_handle	= NULL,
							thread_handle	= NULL;
	HMODULE					kernel32_module	= NULL;							
	LPTHREAD_START_ROUTINE	close_handle_fn	= NULL;
	DWORD					last_error		= 0;
	
	__try {
		
		dprintf ( D_FULLDEBUG, "CloseRemoteHandle: Closing handle in "
			"process '%s' (%u)", process_name, pid );

		/* open the remote process */
		process_handle = OpenProcess ( 
            PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION 
			| PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid );
		
		if ( NULL == process_handle ) {
			last_error = GetLastError ();
			dprintf ( D_FULLDEBUG, "CloseRemoteHandle: OpenProcess "
                "failed (last-error = %u)", last_error );			
			__leave;
		}

		/* load kernel32.dll */
		kernel32_module = LoadLibrary ( "kernel32.dll" );
		ASSERT ( kernel32_module ); /* should never assert :) */

		/* get a pointer to the close handle function */
		close_handle_fn = (LPTHREAD_START_ROUTINE) GetProcAddress ( 
			kernel32_module, "CloseHandle" ); 
		ASSERT ( close_handle_fn ); /* again, this will not fail */

		/* create a thread in the remote process */
		thread_handle = CreateRemoteThread ( process_handle, NULL, 0, 
			close_handle_fn, kernel32_module, 0, &last_error );
		
		if ( NULL == thread_handle ) { 
			/* something is wrong with the privileges, or the process 
			doesn't like us */
			last_error = GetLastError ();
			dprintf ( D_FULLDEBUG, "CloseRemoteHandle: "
                "CreateRemoteThread() failed (last-error = %u)", 
                last_error );			
			__leave;
		}

		/* wait while the handle is closed... as for the time out, well:
		careful years research has show that 20 units of anything seems 
		to be the right magic number (so why not a multiple of it, too) */
		switch ( WaitForSingleObject ( thread_handle, 2000 ) ) {
		case WAIT_OBJECT_0:
			/* killed the little bugger */
			last_error = 0;
			break;		
		default:
			/* something went wrong.  maybe further research into the 20
			unit heuristic might shed some light on this problem */
			last_error = GetLastError ();
			dprintf ( D_FULLDEBUG, "CloseRemoteHandle: our remote "
				"CloseHandle() call failed (last-error = %u)", 
                last_error );			
			__leave;
			break;
		}

	}
	__finally {

		/* close the remote thread handle */
		if ( thread_handle ) {
			CloseHandle( thread_handle );
		}
		
		/* free up kernel32.dll */
		if ( kernel32_module ) {
			FreeLibrary ( kernel32_module );
		}
		
		/* close the process handle */
		if ( process_handle ) {
			CloseHandle ( process_handle );
		}

	}

	return last_error;

}

/* take straight from the web-site code listed above, so I take 
no responsibility for it... I only made it use our data structures
rather than MFC ones 

-Ben 2007-09-20
*/

/*
Filter can be any of the following:
 
"Directory", "SymbolicLink", "Token",
"Process", "Thread", "Unknown7", "Event", "EventPair", "Mutant", 
"Unknown11", "Semaphore", "Timer", "Profile", "WindowStation",
"Desktop", "Section", "Key", "Port", "WaitablePort", 
"Unknown21", "Unknown22", "Unknown23", "Unknown24",
		"IoCompletion", "File" 
*/
extern BOOL 
ForceDeleteA ( PCSTR filename, PCSTR filter ) {

	dprintf ( D_FULLDEBUG, "ForceDeleteA: name = %s, filter = %s\n", 
		filename, filter );

	MyString deviceFileName;
	MyString fsFilePath;
	MyString name;
	MyString processName;
	SystemHandleInformation hi;
	SystemProcessInformation pi;
	SystemProcessInformation::SYSTEM_PROCESS_INFORMATION* pPi;
	
	//Convert it to device file name
	if ( !SystemInfoUtils::GetDeviceFileName( filename, deviceFileName ) )
	{
		dprintf( D_FULLDEBUG, "ForceDeleteA: GetDeviceFileName() failed.\n" );
		return FALSE;
	}

	//Query every file handle (system wide)
	if ( !hi.SetFilter( filter, TRUE ) )
	{
		dprintf( D_FULLDEBUG, "ForceDeleteA: SystemHandleInformation::SetFilter() failed.\n" );
		return FALSE;
	}

	if ( !pi.Refresh() )
	{
		dprintf( D_FULLDEBUG, "ForceDeleteA: SystemProcessInformation::Refresh() failed.\n" );
		return FALSE;
	}
	
	//Iterate through the found file handles
	/* for ( POSITION pos = hi.m_HandleInfos.GetHeadPosition(); pos != NULL; ) */
	SystemHandleInformation::SYSTEM_HANDLE h;
	ListIterator<SystemHandleInformation::SYSTEM_HANDLE> it ( hi.m_HandleInfos );
	it.ToBeforeFirst ();

	while ( it.Next ( h ) ) {

		if ( 0 == pi.m_ProcessInfos.lookup ( h.ProcessID, pPi ) )
			continue;
		
		if ( pPi == NULL )
			continue;
		
		//Get the process name
		SystemInfoUtils::Unicode2MyString ( &pPi->usName, processName );

		//what's the file name for this given handle?
		hi.GetName( (HANDLE)h.HandleNumber, name, h.ProcessID );
		
		//This is what we want to delete, so close the handle
		if ( deviceFileName == name ) {

			CloseRemoteHandle ( processName.GetCStr (), h.ProcessID, 
				(HANDLE) h.HandleNumber );

		}
	}

	/********************** REMOVE ME ************************/
	dprintf( D_FULLDEBUG, "loop end\n" );
	/********************** REMOVE ME ************************/

	return TRUE;
	
}
	
extern BOOL 
ForceDeleteW ( PCWSTR w_filename, PCWSTR w_filter ) {
	PSTR filename	= ProduceAFromW ( w_filename );
	PSTR filter		= ProduceAFromW ( w_filter );
	BOOL ok			= ForceDeleteA ( filename, filter );
	delete [] filename;
	delete [] filter;
	return ok;
}

