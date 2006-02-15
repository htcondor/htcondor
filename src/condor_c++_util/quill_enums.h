/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef __QUILL_ENUMS_H__
#define __QUILL_ENUMS_H__

//the IS was prepended to the below constants in order 
//to avoid conflict with occurences of the same symbol
//in other parts of condor code, e.g. ../h/proc.h

enum    JobIdType {   IS_CLUSTER_ID,
                     	IS_PROC_ID,
                        IS_UNKNOWN_ID};

enum    XactState {    NOT_IN_XACT,
                        BEGIN_XACT,
                        COMMIT_XACT,
                        ABORT_XACT};

enum QuillErrCode {	FAILURE,
						FAILURE_QUERY_PROCADS_STR,
						FAILURE_QUERY_PROCADS_NUM,
						FAILURE_QUERY_CLUSTERADS_STR,
						FAILURE_QUERY_CLUSTERADS_NUM,
						FAILURE_QUERY_HISTORYADS_HOR,
						FAILURE_QUERY_HISTORYADS_VER,
						JOB_QUEUE_EMPTY,
						HISTORY_EMPTY,
						DONE_JOBS_CURSOR,
						DONE_HISTORY_CURSOR,
						DONE_CLUSTERADS_CURSOR,
						DONE_PROCADS_CURSOR,
						DONE_PROCADS_CUR_CLUSTERAD,
						SUCCESS};


enum ProbeResultType {  PROBE_ERROR, 
						NO_CHANGE, 
						INIT_QUILL, 
						ADDITION, 
						COMPRESSED};

enum FileOpErrCode {    FILE_OPEN_ERROR,
						FILE_READ_ERROR,
						FILE_WRITE_ERROR,
						FILE_READ_EOF,
                        FILE_READ_SUCCESS,
                        FILE_OP_SUCCESS};

#endif







