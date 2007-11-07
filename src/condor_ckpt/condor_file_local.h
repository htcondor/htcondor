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


#ifndef CONDOR_FILE_LOCAL_H
#define CONDOR_FILE_LOCAL_H

#include "condor_file_basic.h"

/**
This class sends all operations to a locally opened file.
Notice that this class is identical to a CondorFileBasic,
except for the few operations listed below.  Operations
which are common to both local and remote files should
go in CondorFileBasic.
*/

class CondorFileLocal : public CondorFileBasic {
public:
	CondorFileLocal();
	virtual ~CondorFileLocal();

	virtual int read(int offset, char *data, int length);
	virtual int write(int offset, char *data, int length);

	virtual int is_file_local();
};

#endif
