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

/******************************************************************************
*                                                                             *
*   Author:  Hsu-lin Tsao                                                     *
*   Project: Condor Checkpoint Server                                         *
*   Date:    May 1, 1995                                                      *
*   Version: 0.5 Beta                                                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Module:  server_xferstat                                                  *
*                                                                             *
*******************************************************************************
*                                                                             *
*   File:    server_xferstat.h                                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This module contains prototypes and type definitions for the              *
*   server_xferstat module.  This module is used to keep track of process     *
*   information for child processes which are transferring data to/from the   *
*   Checkpoint Server.                                                        *
*                                                                             *
******************************************************************************/


#ifndef SERVER_XFERSTAT_H
#define SERVER_XFERSTAT_H


/* Header Files */

#include "constants2.h"
#include "network2.h"
#include <sys/types.h>
#include <time.h>




/* Type Definitions */

typedef enum xfer_type
{
  RECV=300,
  XMIT=301,
  FILE_STATUS=302,
  REPLICATE=303
} xfer_type;


typedef struct transferinfo
{
	int            req_id;
	int            child_pid;
	time_t         start_time;
	u_lint         file_size;
	xfer_type      status;
	int            priority;
	int            key;
	u_short        override;
	transferinfo*  prev;
	transferinfo*  next;
	struct in_addr shadow_addr;
	char           filename[MAX_CONDOR_FILENAME_LENGTH];
	char           owner[MAX_NAME_LENGTH];
} transferinfo;




/******************************************************************************
*                                                                             *
*   Class: TransferState                                                      *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This class is used to keep track of process information of server child   *
*   processes which are used to transfer files to/from the Checkpoint Server. *
*   Although this class uses the "transferinfo" struct (defined above), the   *
*   information returned from member functions is strictly a simple type      *
*   (i.e., no pointers to structs are used).  Therefore, the functionality    *
*   of this class is assured.                                                 *
*                                                                             *
*   As new child processes are created and as they finish, this class must    *
*   track these changes.  The data is handled as a simple linked list, and    *
*   the parent Checkpoint Server process (i.e., the Master Server) is the     *
*   only process which inserts new data into this list.  Whenever a child     *
*   process finishes the data transfer (or when it exits due to errors), a    *
*   signal handler is invoked from the Master Server.  This signal handler    *
*   calls a member function of this class to delete its entry from the list.  *
*                                                                             *
*   Because this class contains all of the information pertaining to a file   *
*   transfer, it is responsible for updating the in-memory data structure of  *
*   all files on the checkpoint server.  (See the server_filestat module.)    *
*   Thus, this class is given the reference of the in-memory data structure,  *
*   and calls its member functions to perform the updates.                    *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Features:                                                                 *
*       1) Code has been included so the contents of the transfer list can be *
*          printed.                                                           *
*       2) Insertions into the list are fast.  Insertions occur in the front  *
*          of the list.  This is done because there is very little            *
*          correlation between the time a file transfer starts and when it    *
*          completes.                                                         *
*       3) Because it is possible for a file transfer to "hang" (if the other *
*          end hangs), there is code for a "reclamation" function which will  *
*          kill any child whose data transfer has taken an inordinate amount  *
*          of time.  (The actual time is in server_constants.h.)              *
*       4) Because the reclamation process may be unfair for large checkpoint *
*          files, a priority may be associated with a process, thus making a  *
*          file transfer immune to the reclamation procedure.                 *
*       5) A linked list is used rather than a table.  Although the           *
*          Checkpoint Server has severe limitations on Ethernet, an FDDI or   *
*          ATM network would allow many simultaneous transfers.  Thus, no     *
*          hard limit is placed upon this class as to the maximum number of   *
*          concurrent file transfers.                                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Known Bugs:                                                               *
*       1) If a large file has its priority set so the reclamation procedure  *
*          does not affect it, it is possible for the file transfer to hang   *
*          indefinitely, thereby reducing the number of file transfers        *
*          allowed by the Checkpoint Server.                                  *
*                                                                             *
*          Bug fix: Do not let the priority "disable" the reclamation         *
*                   process, but let it affect the overall time allowed for   *
*                   the file transfer.  However, as the existing values for   *
*                   the maximum transfer time have not been fine-tuned, it    *
*                   is difficult to implement this correctly.                 *
*                                                                             *
*       2) There is a small window of opportunity for a file transfer to      *
*          complete/abort before its information is placed into the list.     *
*          A fork() is performed, and the child's pid is the unique key for   *
*          the process' information node.  The parent process (Master Server) *
*          would then insert the information into the list.  However, if the  *
*          child quickly finishes/aborts before the time the parent inserts   *
*          the information, the data structure will report an error.          *
*                                                                             *
*          Bug fix: (FIXED) Before the child is forked off, the signal        *
*                   handler is disabled.  Therefore, the signal generated by  *
*                   the child process will not be received until after the    *
*                   the parent inserts the data into the transfer list.       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Data Members:                                                             *
*       1) head          - a pointer to the first transferinfo node in the    *
*                          list                                               *
*       2) num_transfers - a running total of the number of transfers in the  *
*                          list.  This value is examined when the list is     *
*                          destroyed as an extra check for correctness        *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Member Functions:                                                         *
*       1) Find()        - this function, given a child's process ID (pid),   *
*                          will find the child process' information           *
*       2) Insert()      - this function inserts information about a new file *
*                          transfer into the list                             *
*       3) Delete()      - this function will remove a specific node from the *
*                          linked list                                        *
*       4) GetXferType() - returns the transfer type (either RECV or XMIT) of *
*                          a specific node                                    *
*       5) Reclaim()     - this function will send signals to all jobs in the *
*                          linked list to terminate if their file transfer    *
*                          has taken too much time                            *
*       6) DestroyList() - this function removes all of the nodes from the    *
*                          linked list                                        *
*       7) Dump()        - this function dumps the contents of the list to    *
*                          the screen                                         *
*                                                                             *
******************************************************************************/


class TransferState
{
  private:
    transferinfo* head;
    int           num_transfers;
    void DestroyList();

  public:
    TransferState();
    ~TransferState();
    void Insert(int            child_pid, 
		int            req_id,
		struct in_addr shadow_addr,
		const char*    filename, 
		const char*    owner,
		int            filesize,
		u_lint         key,
		u_lint         priority,
		xfer_type      type);
    void Dump();
    transferinfo* Find(int child_pid);
    transferinfo* Find(struct in_addr SM_addr,
		       const char*    owner,
		       const char*    filename);
    int Delete(int child_pid, bool success_flag, struct in_addr peer,
			   int xfer_size);
    int GetXferType(int child_pid);
    int GetKey(int child_pid);
    int GetKey(struct in_addr SM_addr,
	       const char*    owner,
	       const char*    filename);
    void SetOverride(int child_pid);
    void SetOverride(struct in_addr SM_addr,
		     const char*    owner,
		     const char*    filename);
    void Reclaim(time_t current);
};


#endif









