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

 
#include "condor_common.h"
#include "url_condor.h"

static
int condor_file_open_ckpt_file( const char *name, int flags, 
									   size_t n_bytes )
{
	if( flags & O_WRONLY ) {
		return open( name, O_CREAT | O_TRUNC | O_WRONLY, 0664 );
	} else {
		return open( name, O_RDONLY );
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
