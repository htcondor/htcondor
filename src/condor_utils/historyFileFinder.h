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

#ifndef _HISTORYFILEFINDER_H_
#define _HISTORYFILEFINDER_H_

// Find all of the history files that the schedd created, and put them
// in order by time that they were created, so the current file is always last
// For instance, if there is a current file and 2 rotated older files we would have
//    [0] = "/scratch/condor/spool/history.20151019T161810"
//    [1] = "/scratch/condor/spool/history.20151020T161810"
//    [2] = "/scratch/condor/spool/history"
// the return value is a pointer to an array of const char* filenames.
// if the history configuration is invalid, then NULL is returned.
// if the history configuration is valid, but there are no history files
//    then the return value will be a pointer to a 1 element array where [0] = NULL
// The caller can hold onto the returnd pointer as long as it wants, it should release it
// by passing it to freeHistoryFilesList
extern const char **findHistoryFiles(const char *passedFileName, int *numHistoryFiles);
// Free the array of history files returned by the above function.
extern void freeHistoryFilesList(const char **);

#endif
