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

#define _FILE_OFFSET_BITS 64

#include <string>
#include <map>
struct DebugFileInfo
{
	FILE *debugFP;
	int debugFlags;
	std::string logPath;
	off_t maxLog;
	int maxLogNum;

	DebugFileInfo() : debugFP(0), debugFlags(0), maxLog(0), maxLogNum(0) {}
	DebugFileInfo(const DebugFileInfo &debugFileInfo);
	~DebugFileInfo();
    bool MatchesFlags(int flags) const;
};
