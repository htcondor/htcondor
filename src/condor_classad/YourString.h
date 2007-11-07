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

#ifndef YOUR_STRING_H
#define YOUR_STRING_H

// This is a simple wrapper class to enable char *'s
// that we don't manage to be put into HashTables

// HashTable needs operator==, which we define to be
// case-insensitive for ClassAds

class YourString {
	public:
		YourString() : s(0) {}
		YourString(const char *str) : s(str) {}
		bool operator==(const YourString &rhs) {
			return (strcasecmp(s,rhs.s) == 0);
		}
		const char *s; // Someone else owns this
};
#endif
