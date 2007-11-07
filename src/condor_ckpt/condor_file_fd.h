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

#ifndef CONDOR_FILE_FD_H
#define CONDOR_FILE_FD_H

#include "condor_file_local.h"

/*
This file object opens a file locally on an
existing file descriptor.  Apart from the url
type it understands, this object is entirely
identical to CondorFileLocal.
*/

class CondorFileFD : public CondorFileLocal {
public:
	CondorFileFD();
	virtual ~CondorFileFD();
	virtual int open( const char *url, int flags, int mode );
	virtual int close();
};

#endif
