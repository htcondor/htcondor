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
StringList *_sysapi_console_devices = NULL;
/* this is not configured here, but is global, look in last_x_event.c */
int _sysapi_last_x_event = 0;

/* needed by free_fs_blocks.c */
int _sysapi_reserve_afs_cache = FALSE;
int _sysapi_reserve_disk = 0;

/* needed by idle_time.C */
int _sysapi_startd_has_bad_utmp = FALSE;

/* needed by everyone, if this is false, then call sysapi_reconfig() */
int _sysapi_config = FALSE;


/* the function that configures the above variables each time it is called.
   This function is meant to be called outside of the library to configure it
*/

extern "C" void
sysapi_reconfig(void)
{
	char *tmp = NULL;

	/* configuration set up for idle_time.C */
    if( _sysapi_console_devices ) {
        delete( _sysapi_console_devices );
        _sysapi_console_devices = NULL;
    }
    tmp = param( "CONSOLE_DEVICES" );
    if( tmp ) {
        _sysapi_console_devices = new StringList();
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

	_sysapi_reserve_disk = 0;
	tmp = param( "RESERVED_DISK" );
	if( tmp ) {
		_sysapi_reserve_disk = atoi( tmp ) * 1024;    /* Parameter is in meg */
		free( tmp );
	}

	/* tell the library I have configured myself */
	_sysapi_config = TRUE;
}

/* this function is to be called by any and all sysapi functions in sysapi.h */
/* except of course, for sysapi_reconfig() */
extern "C" void
sysapi_internal_reconfig(void)
{
	if (_sysapi_config == FALSE) {
		sysapi_reconfig();
	}
}


