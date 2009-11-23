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

#if !defined(_CLASSAD_HISTORY_H)
#define _CLASSAD_HISTORY_H

extern bool        DoHistoryRotation;
extern bool        DoDailyHistoryRotation;
extern bool        DoMonthlyHistoryRotation;
extern filesize_t  MaxHistoryFileSize;
extern int         NumberBackupHistoryFiles;
extern char*       PerJobHistoryDir;
extern char* JobHistoryFileName;

class ClassAd;

void WritePerJobHistoryFile(ClassAd*, bool, bool);
void AppendHistory(ClassAd*);
void InitJobHistoryFile(const char *);

#endif
