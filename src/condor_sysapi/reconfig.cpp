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
time_t _sysapi_last_x_event = 0;

/* needed by free_fs_blocks.c */
#ifndef WIN32
long long _sysapi_reserve_afs_cache = FALSE;
#endif
long long _sysapi_reserve_disk = 0;

/* needed by idle_time.C */
#ifndef WIN32
int _sysapi_startd_has_bad_utmp = FALSE;
#endif

#ifdef LINUX
bool _sysapi_count_hyperthread_cpus = true;
#elif defined WIN32
bool _sysapi_count_hyperthread_cpus = true; // we can only detect hyperthreads on WinXP SP3 or later
#endif

/* needed by everyone, if this is false, then call sysapi_reconfig() */
int _sysapi_config = 0;

/* needed by ncpus.c */
#if 1
int _sysapi_detected_phys_cpus = 1; // we know we will have at least 1
int _sysapi_detected_hyper_cpus = 1;
#else
int _sysapi_ncpus = 0;
int _sysapi_max_ncpus = 0;
#endif

/* needed by phys_mem.c */
int _sysapi_memory = 0;
int _sysapi_reserve_memory = 0;

/* needed by load_avg.c */
int _sysapi_getload = 0;

bool _sysapi_net_devices_cached = false;
bool _sysapi_opsys_is_versioned = false;

/* needed by processor_flags.c */
const char * _sysapi_processor_flags_raw = NULL;
const char * _sysapi_processor_flags = NULL;

extern "C"
{

/*
   The function that configures the above variables each time it is called.
   This function is meant to be called outside of the library to configure it
*/
void
sysapi_reconfig(void)
{
	char *tmp = NULL;

#ifdef WIN32
    /* configuration set up to enable legacy OpSys values (WINNT51 etc) */
    _sysapi_opsys_is_versioned = param_boolean( "ENABLE_VERSIONED_OPSYS", false );
#else
    _sysapi_opsys_is_versioned = param_boolean( "ENABLE_VERSIONED_OPSYS", true );

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
		    char *tmpname = strdup( devname );
		    _sysapi_console_devices->deleteCurrent();
		    _sysapi_console_devices->insert( tmpname + striplen );
		    free( tmpname );
		  }
		}
	}

        free( tmp );
    }

	_sysapi_startd_has_bad_utmp = param_boolean_int( "STARTD_HAS_BAD_UTMP", FALSE );

	/* configuration setup for free_fs_blocks.c */
	_sysapi_reserve_afs_cache = param_boolean_int( "RESERVE_AFS_CACHE", FALSE );
#endif /* ! WIN32 */

	_sysapi_reserve_disk = param_integer_c( "RESERVED_DISK", 0, INT_MIN, INT_MAX );
	_sysapi_reserve_disk *= 1024;    /* Parameter is in meg */

#if 1
	// _sysapi_detected_phys_cpus;
	// _sysapi_detected_hyper_cpus;
#else
	_sysapi_ncpus = param_integer_c( "NUM_CPUS", 0, 0, INT_MAX );

	_sysapi_max_ncpus = param_integer_c( "MAX_NUM_CPUS", 0, 0, INT_MAX );
	if(_sysapi_max_ncpus < 0) {
		_sysapi_max_ncpus = 0;
	}
#endif


	_sysapi_memory = param_integer_c( "MEMORY", 0, 0, INT_MAX );

	_sysapi_reserve_memory = param_integer_c( "RESERVED_MEMORY", 0, INT_MIN, INT_MAX );

	_sysapi_getload = param_boolean_int("SYSAPI_GET_LOADAVG",1);

#ifdef LINUX
	/* Should we count hyper threads? */
	_sysapi_count_hyperthread_cpus = 
		param_boolean("COUNT_HYPERTHREAD_CPUS", true);
#elif defined WIN32
	_sysapi_count_hyperthread_cpus = param_boolean("COUNT_HYPERTHREAD_CPUS", _sysapi_count_hyperthread_cpus);
#endif

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

}
