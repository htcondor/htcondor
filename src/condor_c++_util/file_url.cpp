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
#include "condor_open.h"
#include "url_condor.h"

static
int condor_file_open_ckpt_file( const char *name, int flags )
{
	if( flags & O_WRONLY ) {
		return safe_open_wrapper( name, O_CREAT | O_TRUNC | O_WRONLY, 0664 );
	} else {
		return safe_open_wrapper( name, O_RDONLY );
	}
}


#if 0
URLProtocol FILE_URL("file", "CondorFileUrl", condor_file_open_ckpt_file);
#endif

void
init_file()
{
    static URLProtocol	*FILE_URL = 0;

    if (FILE_URL == 0) {
	FILE_URL = new URLProtocol("file", "CondorFileUrl", 
				   condor_file_open_ckpt_file);
    }
}
