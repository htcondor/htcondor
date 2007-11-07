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


#ifndef CONDOR_FILE_REMOTE_H
#define CONDOR_FILE_REMOTE_H

#include "condor_file_basic.h"

/**
This class sends all I/O operations to a remotely opened file.
Notice that the class is only a few extensions to CondorFileBasic.
The only operations which are unique to this class are the methods
listed here.  Operations that are common to both local and remote
files should go in CondorFileBasic.
*/

class CondorFileRemote : public CondorFileBasic {
public:
	CondorFileRemote();
	virtual ~CondorFileRemote();

	virtual int read(int offset, char *data, int length);
	virtual int write(int offset, char *data, int length);

	virtual int fcntl( int cmd, int arg );
	virtual int ioctl( int cmd, long arg );
	virtual int ftruncate( size_t length );

	virtual int is_file_local();
};

#endif
