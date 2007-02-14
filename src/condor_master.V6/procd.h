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

#ifndef _PROCD_H
#define _PROCD_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"

#define MAX_PID_STR_LEN 20
#define MAX_BIRTHDAY_STR_LEN 50

class ProcD : public Service {

public:

	// create a ProcD object
	//
	ProcD();

	// destructor
	//
	~ProcD();

	// start the procd, blocking until it has set up its "server port"
	//
	void start();

	// stop the procd
	//
	void stop();

	// notified by daemon core when the procd exits
	//
	int reaper(int, int);

private:

	// path to the procd binary
	//
	char* m_binary_path;

	// "address" that the procd server will listen on
	//
	char* m_address;

	// name of the file for the procd to log to
	//
	char* m_log_file;

	// the procd's maximum snapshot interval
	//
	char* m_max_snapshot_interval;

	// debug option
	//
	bool m_debug;

	// string form of the procd's root process's pid
	//
	char m_root_pid[MAX_PID_STR_LEN + 1];

	// string form of the procd's root process's birthday
	//
	char m_root_birthday[MAX_BIRTHDAY_STR_LEN + 1];

	// if a procd is running, its PID; otherwise, -1
	//
	int m_pid;

#if defined(WIN32)
	// program to use for sending softkills on Winblows
	//
	char* m_softkill_binary;
#endif
};

#endif
