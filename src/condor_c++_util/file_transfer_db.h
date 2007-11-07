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

#ifndef FILE_TRANSFER_DB_H
#define FILE_TRANSFER_DB_H

#include "condor_classad.h"
#include "condor_attrlist.h"
#include "reli_sock.h"

typedef struct 
{
	char *fullname; /* file name in the destination */
	filesize_t   bytes; /* size in bytes */
	time_t elapsed; /* elapsed seconds */
	char *daemon; /* deamon doing the download */
	ReliSock *sockp; /* associated socket for getting source info */
	time_t transfer_time;  /* epoch time when transfer is initiated */	
	int delegation_method_id; /* 0 if it's not a delegation, 1 if it is */
} file_transfer_record;

void file_transfer_db(file_transfer_record *rp, ClassAd *ad);

#endif
