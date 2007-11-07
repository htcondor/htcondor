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

#ifndef CONDOR_UPDATE_STYLE_H
#define CONDOR_UPDATE_STYLE_H

/* Some events (like a job termination event) can have multiple "modes"
	which dictate how the event is supposed to be handled. */

enum update_style_t {
	US_NORMAL,				/* handle the event via default means */
	US_TERMINATE_PENDING	/* This represents a termination event that happend
								but yet hasn't been recorded by the logfiles,
								etc, but has been recorded by the job queue. */	
};

#endif
