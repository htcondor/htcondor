/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef _LOG_ROTATE_H
#define _LOG_ROTATE_H

class log_rotate_base {
public:
	std::string logBaseName;
	int cleanUpOldLogFiles(int maxNum);
};

/** set the base name for the log file, i.e. the basic daemon log filename w/ path*/
void setBaseName(const char *baseName);

int rotateSingle(void);

/** create a rotation filename depending on maxNum and existing ending */
const char *createRotateFilename(const char *ending, int maxNum, time_t tt);

/** perform rotation, time value may be 0 if maxNum < 2 */
int rotateTimestamp(const char *timeStamp, int maxNum, time_t tt);

/** quantize time tt to the time secs. ie if secs = 3600, quantize to the hour **/
long long quantizeTimestamp(time_t tt, long long secs);

/** Rotate away all old history files exceeding maxNum count */
int cleanUpOldLogFiles(int maxNum);

#endif
