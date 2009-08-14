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
#include "daemon_types.h"

static const char* daemon_names[] = {
	"none",
	"any",
	"master",
	"schedd",
	"startd",
	"collector",
	"negotiator",
	"kbdd",
	"dagman", 
	"view_collector",
	"cluster_server",
	"shadow",
	"starter",
	"credd",
	"stork",
	"quill",
	"transferd",
	"lease_manager",
	"had",
	"generic"
};

extern "C" {

const char*
daemonString( daemon_t dt )
{	
	if( dt < _dt_threshold_ ) {
		return daemon_names[dt];
	} else {
		return "Unknown";
	}
}

daemon_t
stringToDaemonType( char* name )
{
	int i;
	for( i=0; i<_dt_threshold_; i++ ) {
		if( !stricmp(daemon_names[i], name) ) {
			return (daemon_t)i;
		}
	}
	return DT_NONE;
}

} /* extern "C" */
