/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef _CONDOR_PRIVSEP_H
#define _CONDOR_PRIVSEP_H

class ArgList;

// are we running in PrivSep mode?
//
bool privsep_enabled();

#if !defined(WIN32)

// setup an ArgList for running a program as a given user
//
bool privsep_setup_exec_as_user(uid_t, gid_t, ArgList&);

// open a file as the given user
//
int privsep_open(uid_t, gid_t, const char*, int, mode_t = 0);

// make a directory owned by the given user
//
bool privsep_create_dir(uid_t, gid_t, const char*);

// remove a directory tree
//
bool privsep_remove_dir(const char*);

// change the ownership of a directory tree to the given user
//
bool privsep_chown_dir(uid_t, gid_t, const char*);

// do a rename operation as the given user
//
bool privsep_rename(uid_t, gid_t, const char*, const char*);

// do a chmod operation as the given user
//
bool privsep_chmod(uid_t, gid_t, const char*, mode_t);

#endif

#endif
