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

#include "stdsoap2.h"

#define WEBSRV_ID "WEBSRV-GET-1.0"

extern char* websrv_id;

struct websrv_data {
  int (*fpost)(struct soap*, const char*, const char*, int, const char*, const char*, size_t);
  int (*fparse)(struct soap*);
  int (*fget)(struct soap*);
};

int websrv(struct soap* soap, struct soap_plugin* plug, void *get_handler);
