/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_string.h"
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

/* needed by ckptpltfrm.c */
char *_sysapi_ckptpltfrm = NULL;


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

	// if "/dev/" is prepended to any of the device names, strip it
	// here, since later on we're expecting the bare device name
	if( _sysapi_console_devices ) {
 	        char *devname = NULL;
		const char* striptxt = "/dev/";
		const unsigned int striplen = strlen( striptxt );
		_sysapi_console_devices->rewind();
		while( (devname = _sysapi_console_devices->next()) ) {
		  if( strncmp( devname, striptxt, striplen ) == 0 &&
		      strlen( devname ) > striplen ) {
		    char *tmpname = strnewp( devname );
		    _sysapi_console_devices->deleteCurrent();
		    _sysapi_console_devices->insert( tmpname + striplen );
		    delete[] tmpname;
		  }
		}
	}

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

	/* _sysapi_ckptpltfrm is either set to NULL, or whatever 
		CHECKPOINT_PLATFORM says in the config file. If set to NULL,
		then _sysapi_ckptpltfrm will be properly initialized after the first
		call to sysapi_ckptpltfrm() */
	if (_sysapi_ckptpltfrm != NULL) {
		free(_sysapi_ckptpltfrm);
		_sysapi_ckptpltfrm = NULL;
	}
	tmp = param( "CHECKPOINT_PLATFORM" );
	if (tmp != NULL) {
		_sysapi_ckptpltfrm = strdup(tmp);
		free(tmp);
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
