/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


/* this file holds a little stash of stuff that you get from param() 
   that various functions need in the sysapi. If for some reason you
   want to change your config file stuff, then call sysapi_reconfig()
   and the sysapi library will use the correct answers. If any function
   inside of sysapi is called before a reconfig, it preforms one, but only
   once. This is done to increase performance of the functions in sysapi
   because they might be called very often, to where a hash table might even
   be too slow. So if you are writing code and know that some config parameters
   have changed, then call this function to inform the sysapi of what has
   happened. -pete 05/07/99
*/

#include "condor_common.h"
#include "condor_config.h"
#include "sysapi.h"
#include "sysapi_externs.h"

/* needed by idle_time.C and last_x_event.c */
#ifndef WIN32
StringList *_sysapi_console_devices = NULL;
#endif
/* this is not configured here, but is global, look in last_x_event.c */
int _sysapi_last_x_event = 0;

/* needed by free_fs_blocks.c */
#ifndef WIN32
int _sysapi_reserve_afs_cache = FALSE;
#endif
int _sysapi_reserve_disk = 0;

/* needed by idle_time.C */
#ifndef WIN32
int _sysapi_startd_has_bad_utmp = FALSE;
#endif

/* needed by everyone, if this is false, then call sysapi_reconfig() */
int _sysapi_config = 0;

/* needed by ncpus.c */
int _sysapi_ncpus = 0;

/* needed by phys_mem.c */
int _sysapi_memory = 0;
int _sysapi_reserve_memory = 0;


BEGIN_C_DECLS

/*
   The function that configures the above variables each time it is called.
   This function is meant to be called outside of the library to configure it
*/
void
sysapi_reconfig(void)
{
	char *tmp = NULL;

#ifndef WIN32
	/* configuration set up for idle_time.C */
    if( _sysapi_console_devices ) {
        delete( _sysapi_console_devices );
        _sysapi_console_devices = NULL;
    }
    tmp = param( "CONSOLE_DEVICES" );
    if( tmp ) {
        _sysapi_console_devices = new StringList();
		if (_sysapi_console_devices == NULL)
		{
			EXCEPT( "Out of memory in sysapi_reconfig()!" );
		}
        _sysapi_console_devices->initializeFromString( tmp );
        free( tmp );
    }

	tmp = param( "STARTD_HAS_BAD_UTMP" );
	if( tmp && (*tmp == 'T' || *tmp =='t') ) {
		_sysapi_startd_has_bad_utmp = TRUE;
	} else {
		_sysapi_startd_has_bad_utmp = FALSE;
	}
	if (tmp) free(tmp);

	/* configuration setup for free_fs_blocks.c */
	tmp = param( "RESERVE_AFS_CACHE" );
	if( tmp && (*tmp == 'T' || *tmp == 't')) {
		_sysapi_reserve_afs_cache = TRUE;
	} else {
		_sysapi_reserve_afs_cache = FALSE;
	}
	if (tmp) free(tmp);
#endif /* ! WIN32 */

	_sysapi_reserve_disk = 0;
	tmp = param( "RESERVED_DISK" );
	if( tmp ) {
		_sysapi_reserve_disk = atoi( tmp ) * 1024;    /* Parameter is in meg */
		free( tmp );
	}

	_sysapi_ncpus = 0;
	tmp = param( "NUM_CPUS" );
	if( tmp ) {
		_sysapi_ncpus = atoi( tmp );
		free( tmp );
	}

	_sysapi_memory = 0;
	tmp = param( "MEMORY" );
	if( tmp ) {
		_sysapi_memory = atoi( tmp );
		free( tmp );
	}

	_sysapi_reserve_memory = 0;
	tmp = param( "RESERVED_MEMORY" );
	if( tmp ) {
		_sysapi_reserve_memory = atoi( tmp );
		free( tmp );
	}

	/* tell the library I have configured myself */
	_sysapi_config = TRUE;
}

/* this function is to be called by any and all sysapi functions in sysapi.h */
/* except of course, for sysapi_reconfig() */
void
sysapi_internal_reconfig(void)
{
	if (_sysapi_config == FALSE) {
		sysapi_reconfig();
	}
}

END_C_DECLS
