/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#ifndef _CONDOR_DAEMON_TYPES_H
#define _CONDOR_DAEMON_TYPES_H


// if you add another type to this list, make sure to edit
// daemon_types.C and add the string equivilant.

enum daemon_t { DT_NONE, DT_ANY,  DT_MASTER, DT_SCHEDD, DT_STARTD,
				DT_COLLECTOR, DT_NEGOTIATOR, DT_KBDD, 
				DT_DAGMAN, DT_VIEW_COLLECTOR, DT_CLUSTER,  
				DT_SHADOW, DT_STARTER, DT_CREDD, DT_STORK, DT_QUILL,
				_dt_threshold_ };

#ifdef __cplusplus
extern "C" {
#endif

const char* daemonString( daemon_t dt );
daemon_t stringToDaemonType( char* name );

#ifdef __cplusplus
}
#endif


#endif /* _CONDOR_DAEMON_TYPES_H */
