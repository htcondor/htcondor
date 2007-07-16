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

#ifndef _STARTER_PRIVSEP_HELPER_H
#define _STARTER_PRIVSEP_HELPER_H

#include "../condor_privsep/condor_privsep.h"

// helper class for allowing the Starter to run in PrivSep mode. mainly,
// this takes care of two things:
//   1) storing the user's UID for passing to the PrivSep Switchboard
//   2) keeping track of who owns the sandbox. this will normally be the user,
//      but the FileTransfer object will chown the sandbox to condor so it can
//      do its thing
//

class ArgList;

class StarterPrivSepHelper {

public:

	StarterPrivSepHelper();
	~StarterPrivSepHelper();

	// initialize the UID
	//
#if !defined(WIN32)
	void initialize_user(uid_t, gid_t);
#endif
	void initialize_user(const char* name);

	// initialize the sandbox
	//
	void initialize_sandbox(const char* path);

#if !defined(WIN32)
	// get the user's UID and GID
	//
	void get_user_ids(uid_t&, gid_t&);
#endif

	// open a file as the user
	//
	int open_user_file(const char*, int, mode_t = 0);

	// open a file in the sandbox (which could be owned
	// by the user or condor)
	//
	int open_sandbox_file(const char*, int, mode_t = 0);

	// convenience function to get a FILE*
	//
	FILE* fopen_sandbox_file(const char*, const char*);

	// change ownership of the sandbox to the user
	//
	void chown_sandbox_to_user();

	// change ownership of the sandbox to condor
	//
	void chown_sandbox_to_condor();

	// setup the input ArgList to start a process as the user
	//
	void setup_exec_as_user(MyString&, ArgList&);

	// do a rename operation as the user
	//
	bool rename(const char*, const char*);

	// do a chmod operation as the user
	//
	bool chmod(const char*, mode_t);

private:

	// we only want one of these instantiated, so keep a static flag
	//
	static bool s_instantiated;

	// set true once we've been initialized the user info
	//
	bool m_user_initialized;

	// set true once we've initialized the sandbox info
	//
	bool m_sandbox_initialized;

#if !defined(WIN32)
	// the "user priv" UID and GID
	//
	uid_t m_uid;
	gid_t m_gid;
#endif

	// the sandbox directory name
	//
	char* m_sandbox_path;

	// true when the sandbox is owned by the user, not condor
	//
	bool m_sandbox_owned_by_user;
};

// declare the Starter's (one and only) instantiation of StarterPrivSepHelper
//
extern StarterPrivSepHelper privsep_helper;

#endif
