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

/* 
	Implementation of an easy name for this functionality since the 
	remote i/o and checkpointing libraries would like this functionality
	as well, but there are naming restrictions for functions that go into
	those libraries. 
*/

ssize_t full_read(int filedes, void *ptr, size_t nbyte)
{
	return _condor_full_read(filedes, ptr, nbyte);
}

ssize_t full_write(int filedes, const void *ptr, size_t nbyte)
{
	return _condor_full_write(filedes, ptr, nbyte);
}
