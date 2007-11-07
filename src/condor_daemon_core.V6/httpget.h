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
