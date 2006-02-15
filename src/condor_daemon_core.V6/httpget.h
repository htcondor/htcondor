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
