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

#ifndef SPOOL_VERSION_H
#define SPOOL_VERSION_H

#define SPOOL_MIN_VERSION_SCHEDD_SUPPORTS 0 /* before 7.5.5 */
#define SPOOL_CUR_VERSION_SCHEDD_SUPPORTS 1 /* spool version circa 7.5.5 */
#define SPOOL_MIN_VERSION_SCHEDD_WRITES   1 /* spool version circa 7.5.5 */

// The shadow only supports the current spool version.
// It expects the schedd to upgrade the spool on startup.
#define SPOOL_MIN_VERSION_SHADOW_SUPPORTS SPOOL_CUR_VERSION_SCHEDD_SUPPORTS
#define SPOOL_CUR_VERSION_SHADOW_SUPPORTS SPOOL_CUR_VERSION_SCHEDD_SUPPORTS

// Read the spool version and check if it is compatible
// with this daemon.  This function will EXCEPT if the spool version
// is not compatible.
void CheckSpoolVersion(
	char const *spool,
	int spool_min_version_i_support,
	int spool_cur_version_i_support,
	int &spool_min_version,
	int &spool_cur_version);

// Convenience form of above function that just takes two arguments.
void CheckSpoolVersion(
	int spool_min_version_i_support,
	int spool_cur_version_i_support);

void WriteSpoolVersion(
	char const *spool,
	int spool_min_version_i_write,
	int spool_cur_version_i_support);

#endif
