/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 2005, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"


/// Support object for Create_Thread_With_Data
struct Create_Thread_With_Data_Data {
	int data_n1;
	int data_n2;
	void * data_vp;
	DataThreadWorkerFunc worker;
	DataThreadReaperFunc reaper;
};

/// Support function for Create_Thread_With_Data
static Create_Thread_With_Data_Data * 
malloc_Create_Thread_With_Data_Data(int data_n1, int data_n2, void * data_vp,
	DataThreadWorkerFunc worker, DataThreadReaperFunc reaper)
{
	Create_Thread_With_Data_Data * tmp = (Create_Thread_With_Data_Data *)
		malloc(sizeof(Create_Thread_With_Data_Data));
	tmp->data_n1 = data_n1;
	tmp->data_n2 = data_n2;
	tmp->data_vp = data_vp;
	tmp->worker = worker;
	tmp->reaper = reaper;
	return tmp;
}

/// Support function for Create_Thread_With_Data
static int Create_Thread_With_Data_Start(void * data, Stream *)
{
	Create_Thread_With_Data_Data * tmp = (Create_Thread_With_Data_Data *)data;
	ASSERT(tmp);
	ASSERT(tmp->worker);
	return tmp->worker(tmp->data_n1, tmp->data_n2, tmp->data_vp);
}

template class HashTable<int, Create_Thread_With_Data_Data *>;
template class HashBucket<int, Create_Thread_With_Data_Data *>;
static HashTable<int, Create_Thread_With_Data_Data *> tid_to_data(10, hashFuncInt, rejectDuplicateKeys);

/// Support function for Create_Thread_With_Data
static int Create_Thread_With_Data_Reaper(Service *, int tid, int exit_status)
{
	Create_Thread_With_Data_Data * tmp = 0;
	if( tid_to_data.lookup(tid, tmp) != 0 )
	{
		// This tid didn't get inserted.
		ASSERT(0);  
	}
	ASSERT(tmp);
	int ret = 0;
	if( tmp->reaper) {
		ret = tmp->reaper(tmp->data_n1, tmp->data_n2, tmp->data_vp, exit_status);
	}
	if( tid_to_data.remove(tid) != 0 )
	{
		// Index/key wasn't found?  Can't happen.
		ASSERT(0);
	}
	free(tmp);
	return ret;
}

int Create_Thread_With_Data(DataThreadWorkerFunc Worker, DataThreadReaperFunc Reaper, 
	int data_n1 /*= 0*/, int data_n2 /*= 0*/, void * data_vp /*= 0*/)
{
	static bool initialized = false;
	static int reaper = 0;
	if( ! initialized )
	{
		reaper =
		daemonCore->Register_Reaper("Create_Thread_With_Data_Reaper",
		Create_Thread_With_Data_Reaper,
			"Create_Thread_With_Data_Reaper");
		dprintf(D_FULLDEBUG, "Registered reaper for job threads, id %d\n", reaper);
		initialized = true;
	}

	ASSERT(Worker);

	// Passing data to the worker thread is easy, 
	// just pass a malloc()ed struct in.
	// (Must be malloc()ed; the thread code free()s it.)
	Create_Thread_With_Data_Data * tmp = malloc_Create_Thread_With_Data_Data(
		data_n1, data_n2, data_vp, Worker, 0);

	int tid = daemonCore->Create_Thread(Create_Thread_With_Data_Start, 
		tmp, NULL, reaper);

	// Passing per-thread data to the reader is not quite so easy.  
	// Keep a tid->data map.
	tmp = malloc_Create_Thread_With_Data_Data(data_n1, data_n2, data_vp, 
		0, Reaper);
	if( tid_to_data.insert(tid, tmp) != 0 )
	{
		// This tid already exists!
		ASSERT(0);
	}
	return tid;
}
