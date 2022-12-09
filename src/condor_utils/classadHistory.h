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

#include "condor_classad.h"

extern bool        DoHistoryRotation;
extern char*       PerJobHistoryDir;
extern char* JobHistoryFileName;

//Struct to hold information needed for classad record file rotation/deletion
struct HistoryFileRotationInfo {
	filesize_t MaxHistoryFileSize = 1024 * 1024 * 20; //20MB
	int NumberBackupHistoryFiles  = 2;
	bool IsStandardHistory        = true; //Bool for if this is standard schedd history or startd history file
	bool DoDailyHistoryRotation   = false;
	bool DoMonthlyHistoryRotation = false;
};

void WritePerJobHistoryFile(ClassAd*, bool);
void AppendHistory(ClassAd*);
void InitJobHistoryFile(const char *, const char *);
void MaybeRotateHistory(const HistoryFileRotationInfo&, int, const char*, const char* new_filepath = NULL);

#endif
