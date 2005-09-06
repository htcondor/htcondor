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
	"quill"
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
