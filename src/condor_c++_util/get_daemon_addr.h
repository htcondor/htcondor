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
#ifndef CONDOR_GET_DAEMON_ADDR_H
#define CONDOR_GET_DAEMON_ADDR_H

#include "daemon_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char* get_daemon_addr( daemon_t dt = DT_MASTER, 
							  const char* name = NULL, const char* pool = NULL);
extern char* get_schedd_addr(const char* name = NULL, const char* pool = NULL);
extern char* get_startd_addr(const char* name = NULL, const char* pool = NULL);
extern char* get_master_addr(const char* name = NULL, const char* pool = NULL);
extern char* get_negotiator_addr(const char* name = NULL);
extern char* get_collector_addr(const char* name = NULL);
extern char* get_daemon_name(const char* name);
extern const char* get_host_part(const char* name);
extern char* build_valid_daemon_name(char* name);
extern char* default_daemon_name( void );

#ifdef __cplusplus
}
#endif

#endif /* CONDOR_GET_DAEMON_ADDR_H */
