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

// This #define doesn't actually do anything. This value needs to be
// defined before any system header files are included in the source file
// to have any effect.

#include <string>
#include <map>
struct DebugFileInfo
{
	FILE *debugFP;
	int debugFlags;
	std::string logPath;
	int64_t maxLog;
	int maxLogNum;

	DebugFileInfo() : debugFP(0), debugFlags(0), maxLog(0), maxLogNum(0) {}
	DebugFileInfo(const DebugFileInfo &debugFileInfo);
	~DebugFileInfo();
    bool MatchesFlags(int flags) const;
};
