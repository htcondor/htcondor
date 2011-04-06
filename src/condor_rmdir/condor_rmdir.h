/***************************************************************
 *
 * Copyright (C) 2011, Condor Team, Computer Sciences Department,
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

#pragma once
#define _CONDOR_RMDIR_H_

// TraverseDirectoryTree is a reusable function to walk a tree
// and call a function for each file and/or directory.  traversal
// order is:
//
//     foreach file in dir {
//        callback(file)
//     }
//     foreach subdir in dir {
//        callback(subdir, TDT_DIRFIRST);
//        TraverseDirectoryTree(subdir);
//        callback(subdir, TDT_DIRLAST);
//     }
//
typedef bool (WINAPI *PFNTraverseDirectoryTreeCallback)(
   VOID *  pvUser, 
   LPCTSTR pszPath,    // path and filename, may be absolute or relative.
   int     ochName,    // offset from start of pszPath to first char of the file/dir name.
   DWORD   fdwFlags,
   int     nCurrentDepth,              
   int     ixCurrentItem,
   const WIN32_FIND_DATA & wfd);

int TraverseDirectoryTree (
   LPCTSTR pszPath, 
   LPCTSTR pszPattern, 
   DWORD   fdwFlags, 
   PFNTraverseDirectoryTreeCallback pfnCallback, 
   LPVOID  pvUser,
   int     nCurrentDepth);

// Flags values for TraverseDirectoryTree
//
#define TDT_SUBDIRS    0x0001
#define TDT_NODIRS     0x0002
#define TDT_NOFILES    0x0004
#define TDT_CONTINUE   0x0008
#define TDT_DIAGNOSTIC 0x0010
#define TDT_DIRFIRST   0x0020
#define TDT_DIRLAST    0x0040  // directory callback is after callbacks of all it's children.

// These flags are set and cleared by the directory traversal engine to indicate things
// about the flow of the traversal, not it is not a bug that TDT_DIRLAST == TDT_LAST
//
#define TDT_LAST       0x0040  // this is the directory callback AFTER all child callbacks.
#define TDT_FIRST      0x0080  // this is the first callback in a given directory

#define TDT_INPUTMASK  0xFFFF001F // These flags can be passed in to TraverseDirectoryTree
#define TDT_USERMASK   0xFFFF0000 // These flags are passed on to the callback unmodified. 

// delete directory using TraverseDirectoryTree, so fdwTraverse
// is one or more of TDT_xxx flags. output is written to the
// supplied buffer.
//
int DeletePath(LPCTSTR pszPath,     // delete file/directory
               LPCTSTR pszPattern,  // delete files matching this pattern, "" for all.
               DWORD   fdwTraverse, // control traversal of childern with these flags.
               DWORD   fdwSecurity, // controls removal of DACLS, ownership etc.
               DWORD   fdwVerbose,  // verbosity of output.
               BPRINT_BUFFER & bp); // output to this buffer.

#define DP_V_VERBOSE    0x0001
#define DP_V_DIAGNOSTIC 0x0002
#define DP_V_QUIET      0x0004

#define DP_S_NODELETE   0x0001

// utility functions and buffers, implemented in main.cpp
//
extern BPRINT_BUFFER * g_pbpDiag;
extern BPRINT_BUFFER * g_pbpErr;
int CompareMemory(const char * pb1, const char * pb2, int cb);
int GetLastErrorText (int err, LPTSTR szErr, int cchErr);
void ReportError(int err, LPCSTR szContext, LPCTSTR pszPath=NULL);
