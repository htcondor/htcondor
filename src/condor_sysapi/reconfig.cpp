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
std::vector<std::string> *_sysapi_console_devices = NULL;
#endif
/* this is not configured here, but is global, look in last_x_event.c */
time_t _sysapi_last_x_event = 0;

/* needed by free_fs_blocks.c */
long long _sysapi_reserve_disk = 0;

/* needed by idle_time.C */
#ifndef WIN32
bool _sysapi_startd_has_bad_utmp = false;
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
bool _sysapi_getload = false;

bool _sysapi_net_devices_cached = false;

/*
   The function that configures the above variables each time it is called.
   This function is meant to be called outside of the library to configure it
*/
void
sysapi_reconfig(void)
{
	char *tmp = NULL;

#if !defined(WIN32)
	/* configuration set up for idle_time.C */
    if( _sysapi_console_devices ) {
        delete( _sysapi_console_devices );
        _sysapi_console_devices = NULL;
    }
    tmp = param( "CONSOLE_DEVICES" );
    if( tmp ) {
        _sysapi_console_devices = new std::vector<std::string>();
		if (_sysapi_console_devices == NULL)
		{
			EXCEPT( "Out of memory in sysapi_reconfig()!" );
		}
        *_sysapi_console_devices = split(tmp);

		// if "/dev/" is prepended to any of the device names, strip it
		// here, since later on we're expecting the bare device name
		const char* striptxt = "/dev/";
		const unsigned int striplen = strlen( striptxt );
		for (auto& devname : *_sysapi_console_devices) {
			if( strncmp( devname.c_str(), striptxt, striplen ) == 0 &&
				strlen( devname.c_str() ) > striplen )
			{
				devname.erase(0, striplen);
			}
		}

        free( tmp );
    }

	_sysapi_startd_has_bad_utmp = param_boolean( "STARTD_HAS_BAD_UTMP", false );
#endif /* ! WIN32 */

	_sysapi_reserve_disk = param_integer( "RESERVED_DISK", 0, INT_MIN, INT_MAX );
	_sysapi_reserve_disk *= 1024;    /* Parameter is in meg */

	_sysapi_memory = param_integer( "MEMORY", 0, 0, INT_MAX );

	_sysapi_reserve_memory = param_integer( "RESERVED_MEMORY", 0, INT_MIN, INT_MAX );

	_sysapi_getload = param_boolean("SYSAPI_GET_LOADAVG",true);

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
