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


#ifndef CONDOR_FILE_SPECIAL_H
#define CONDOR_FILE_SPECIAL_H

#include "condor_file_local.h"

/**
There are several items which look like files, but which Condor
does not know how to checkpoint.  We will allow the use of things
like pipes and sockets, but we must inhibit checkpointing while
these endpoints exist.

This object blocks the appropriate signals in its constructor
and then resumes those signals in the destructor.
When none of these objects are in the file table, checkpointing
is allowed.
*/

class CondorFileSpecial : public CondorFileLocal {
public:
	CondorFileSpecial();
	virtual ~CondorFileSpecial();

	virtual int attach( int fd );
};

#endif

