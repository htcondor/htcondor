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
/*	Portions of this file may also be 
	Copyright (C) 2000-2003 Robert A. van Engelen, Genivia inc.
	All Rights Reserved.
*/

#include "stdsoap2.h"

#define HTTP_GET_ID "HTTP-GET-1.0" /* plugin identification */

extern char* http_get_id;

struct http_get_data
{ int (*fpost)(struct soap*, const char*, const char*, int, const char*, const char*, size_t); /* client-side HTTP GET override */
  int (*fparse)(struct soap*); /* to save and call the internal HTTP header parser */
  int (*fget)(struct soap*); /* user-defined server-side HTTP GET handler */
  size_t stat_get;  /* HTTP GET usage statistics */
  size_t stat_post; /* HTTP POST usage statistics */
  size_t stat_fail; /* HTTP failure statistics */
};

int http_get(struct soap*, struct soap_plugin*, void*);
int soap_get_connect(struct soap*, const char*, const char*);

char *query(struct soap*);
char *query_key(struct soap*, char**);
char *query_val(struct soap*, char**);
int soap_encode_string(const char*, char*, int);
const char* soap_decode_string(char*, int, const char*);
int http_get_handler(struct soap*);	/* HTTP get handler */