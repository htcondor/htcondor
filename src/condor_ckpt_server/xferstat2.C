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
*   File:    server_xferstat.C                                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This module contains routines which keep track of the child processes of  *
*   the Checkpoint Server which are currently transferring files.             *
*                                                                             *
******************************************************************************/


/* Header Files */

#include <sys/types.h>
#include "constants2.h"
#include <time.h>
#include <stdlib.h>
#include <iostream.h>
#include "network2.h"
#include "xferstat2.h"
#include <stdio.h>
#include <signal.h>
#include <string.h>




/* Class TransferState ********************************************************
*                                                                             *
*   Function: TransferState()                                                 *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function is the class constructor and it initializes the private     *
*   data members.  Specifically, it sets the head of the linked list to NULL, *
*   and sets the number of nodes in the list to 0.                            *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        <none>                                                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        <not applicable to class constructors>                               *
*                                                                             *
******************************************************************************/


TransferState::TransferState()
{
  head = NULL;
  num_transfers = 0;
}




/* Class TransferState ********************************************************
*                                                                             *
*   Function: ~TransferState()                                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function is the class destructor, and calls the DestroyList() member *
*   function to destroy the linked list.  By doing so, the memory dynamically *
*   acquired by each node is returned.                                        *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        <none>                                                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        <not applicable to class destructors>                                *
*                                                                             *
******************************************************************************/


TransferState::~TransferState()
{
  DestroyList();
}




/* Class TransferState ********************************************************
*                                                                             *
*   Function: Find(transferinfo*& ptr,                                        *
*                  transferinfo*& trail,                                      *
*                  int            child_pid)                                  *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function finds a node with a given child pid and returns a pointer   *
*   to the node (via a reference parameter).  A pointer to the preceding      *
*   node is also given as the Delete() member function calls upon Find().     *
*                                                                             *
*   In the event that the desired node could not be found, a NULL value is    *
*   returned instead of a pointer to the desired node.                        *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        transferinfo*& ptr       - a reference to a pointer to the desired   *
*                                   node                                      *
*        transferinfo*& trail     - a reference to a pointer to a node which  *
*                                   will point to the node before the one     *
*                                   being sought (or NULL if the desired node *
*                                   is the first in the linked list)          *
*        int            child_pid - the pid of the child process              *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


transferinfo* TransferState::Find(int child_pid)
{ 
  transferinfo* ptr;

  ptr = head;
  while ((ptr != NULL) && (ptr->child_pid != child_pid))
    ptr = ptr->next;
  return ptr;
}




transferinfo* TransferState::Find(struct in_addr SM_addr,
				  const char*    owner,
				  const char*    filename)
{
  transferinfo* ptr;

  ptr = head;
  while ((ptr != NULL) && (memcmp((char*) &SM_addr, (char*) &ptr->shadow_addr, 
				  sizeof(struct in_addr)) || 
			   strncmp(owner, ptr->owner, MAX_NAME_LENGTH) || 
			   strncmp(filename, ptr->filename, 
				   MAX_CONDOR_FILENAME_LENGTH)))
    ptr = ptr->next;
  return ptr;
}




int TransferState::GetKey(int child_pid)
{
  transferinfo* ptr;

  ptr = Find(child_pid);
  if (ptr == NULL)
    return BAD_CHILD_PID;
  else
    return ptr->key;
}




int TransferState::GetKey(struct in_addr SM_addr,
			  const char*    owner,
			  const char*    filename)
{
  transferinfo* ptr;

  ptr = Find(SM_addr, owner, filename);
  if (ptr == NULL)
    return BAD_CHILD_PID;
  else
    return ptr->key;
}




/* Class TransferState ********************************************************
*                                                                             *
*   Function: Insert(int             child_pid,                               *
*                    FileStat&       fs,                                      *
*                    struct in_addr* shadow_addr,                             *
*                    const char*     filename,                                *
*                    const char*     owner,                                   *
*                    int             filesize,                                *
*                    u_lint          priority,                                *
*                    u_lint          key,
*                    u_short         type)                                    *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function inserts the information about a new transferring process    *
*   into the linked list.  As each node is acquired dynamically, it is        *
*   possible for the new operation to fail.  In this event, the program will  *
*   be terminated.                                                            *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int             child_pid   - the pid of the child process           *
*        FileStat&       fs          - a reference parameter of the in-memory *
*                                      data structure                         *
*        struct in_addr* shadow_addr - a pointer to the Internet address of   *
*                                      the shadow process requesting that a   *
*                                      checkpoint file be stored.  This is    *
*                                      the submitting machine IP              *
*        const char*     filename    - the unique filename of the checkpoint  *
*                                      file to be stored                      *
*        const char*     owner       - the name of the owner of the           *
*                                      checkpoint file                        *
*        int             filesize    - the size of the checkpoint file (in    *
*                                      bytes)                                 *
*        u_long          priority    - the priority level of the checkpoint   *
*                                      file.  This can be set so the          *
*                                      reclamation process does not affect a  *
*                                      file transfer                          *
*        u_short         type        - the type of transfer; either RECV or   *
*                                      XMIT (with relation to the server)     *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void TransferState::Insert(int            child_pid, 
			   int            req_id,
			   struct in_addr shadow_addr,
			   const char*    filename, 
			   const char*    owner,
			   int            filesize,
			   u_lint         key,
			   u_lint         priority,
			   xfer_type      type)
{
  transferinfo* t;

  t = new transferinfo;
  if (t == NULL)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot make new fileinfo node" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(DYNAMIC_ALLOCATION);
    }
  memcpy((char*) &t->shadow_addr, (char*) &shadow_addr, 
	 sizeof(struct in_addr));
  strncpy(t->filename, filename, MAX_CONDOR_FILENAME_LENGTH);
  strncpy(t->owner, owner, MAX_NAME_LENGTH);
  t->req_id = req_id;
  t->child_pid = child_pid;
  t->start_time = time(NULL);
  t->file_size = filesize;
  t->status = type;
  t->priority = priority;
  t->key = key;
  t->override = 0;
  t->prev = NULL;
  t->next = head;
  if (head != NULL)
    head->prev = t;
  head = t;
  num_transfers++;
}




/* Class TransferState ********************************************************
*                                                                             *
*   Function: Delete(int       child_pid,                                     *
*                    int       status,                                        *
*                    FileStat& fs,                                            *
*                    int&      fs_code)                                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function removes the node corresponding to a child process           *
*   (specified by the child's pid) from the linked list.  The Find() member   *
*   function is used to find the correct node.                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int       child_pid - the pid of the child process                   *
*        int       status    - the exit status of the process.  This is       *
*                              to determine the proper course of action for   *
*                              keeping track of the in-memory data structure  *
*                              and for deleting files from the server         *
*        FileStat& fs        - a reference parameter of the in-memory data    *
*                              structure                                      *
*        int&      fs_code   - a reference parameter for a return code from   *
*                              accessing the in-memory data structure         *
*                              (FileStat class)                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int : BAD_CHILD_PID      - the specified child pid cannot be found   *
*                                   in any node in the list                   *
*              CANNOT_DELETE_FILE - a partially transferred file could not be *
*                                   removed (only when receiving a file)      *
*              OK                 - deletion was successful                   *
*                                                                             *
******************************************************************************/


int TransferState::Delete(int child_pid)
{
  transferinfo* ptr;

  ptr = Find(child_pid);
  if (ptr == NULL)
    {
      cout << endl << "WARNING:" << endl;
      cout << "WARNING:" << endl;
      cout << "WARNING: cannot find child with pid=" << child_pid << endl;
      cout << "WARNING:" << endl;
      cout << "WARNING:" << endl << endl;
      return BAD_CHILD_PID;
    }
  else 
    {
      if ((ptr->prev == NULL) && (ptr->next == NULL))
	head = NULL;
      else if (ptr->prev == NULL)
	{
	  head = ptr->next;
	  head->prev = NULL;
	}
      else if (ptr->next == NULL)
	ptr->prev->next = NULL;
      else
	{
	  ptr->prev->next = ptr->next;
	  ptr->next->prev = ptr->prev;
	}
      delete ptr;
      num_transfers--;
      return OK;
    }
}




void TransferState::SetOverride(int child_pid)
{
  transferinfo* ptr;

  ptr = Find(child_pid);
  if (ptr != NULL)
    ptr->override = OVERRIDE;
}



void TransferState::SetOverride(struct in_addr SM_addr,
				const char*    owner,
				const char*    filename)
{
  transferinfo* ptr;
  
  ptr = Find(SM_addr, owner, filename);
  if (ptr != NULL)
    ptr->override = OVERRIDE;
}




/* Class TransferState ********************************************************
*                                                                             *
*   Function: GetXferType(int child_pid)                                      *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function finds a node with a given child pid and returns the type of *
*   transfer the child process is performing.                                 *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int            child_pid - the pid of the child process              *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int - the transfer type (either RECV or XMIT) of the process         *
*                                                                             *
******************************************************************************/


int TransferState::GetXferType(int child_pid)
{
  transferinfo* ptr;

  ptr = Find(child_pid);
  if (ptr == NULL)
    return BAD_CHILD_PID;
  else
    return ptr->status;
}




/* Class TransferState ********************************************************
*                                                                             *
*   Function: Reclaim(time_t current)                                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function goes through the list of transferring processes and sends a *
*   terminate signal (SIGTERM) to those processes which have taken an         *
*   inordinate amount of time for the transfer.  This is necessary as the     *
*   child process doing the file transfer may block indefinitely if its peer  *
*   entity "hangs."                                                           *
*                                                                             *
*   As this may be unfair for large files on a busy network, a special        *
*   priority may be used which makes a child process immune to the            *
*   reclamation process.  (This in itself has problems; see server_xferstat.h *
*   for more information.)
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int child_pid - the pid of the child process                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void TransferState::Reclaim(time_t current)
{
  transferinfo* ptr;

  ptr = head;
  while (ptr != NULL)
    {
      if (ptr->priority != RECLAIM_BYPASS)
	if (current-ptr->start_time > MAX_ALLOWED_XFER_TIME)
	  kill(ptr->child_pid, SIGTERM);
      ptr = ptr->next;
    }
}




/* Class TransferState ********************************************************
*                                                                             *
*   Function: DestroyList()                                                   *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function goes through the list of transferring processes and removes *
*   every node, thereby destroying the list.                                  *
*                                                                             *
*   This function checks to see if it has maintained a correct count of the   *
*   number of nodes in the list.  If the count is incorrect, a warning        *
*   message is printed.                                                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        <none>                                                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void TransferState::DestroyList()
{
  transferinfo* p;
  transferinfo* trail;

  p = head;
  head = NULL;
  while (p != NULL)
    {
      trail = p;
      p = p->next;
      delete trail;
      num_transfers--;
    }
  if (num_transfers != 0)
    {
      cerr << endl << "WARNING:" << endl;
      cerr << "WARNING:" << endl;
      cerr << "WARNING: incorrect count of nodes in transfer list" << endl;
      cerr << "WARNING:" << endl;
      cerr << "WARNING:" << endl << endl;
    }
}




/* Class TransferState ********************************************************
*                                                                             *
*   Function: Dump()                                                          *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function goes through the list of transferring processes and prints  *
*   out the data held in each node.                                           *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        <none>                                                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void TransferState::Dump()
{
  transferinfo* p;
  int           count=0;
  
  cout << "Transfer List Dump" << endl;
  cout << "=-=-=-=-=-=-=-=-=-" << endl;
  p = head;
  while (p != NULL)
    {
      cout << "Node #" << (++count) << ':' << endl;
      cout << "\tProcess ID: " << p->child_pid << endl;
      cout << "\tMachine:    " << inet_ntoa(p->shadow_addr) << endl;
      if (p->status != FILE_STATUS)
	{
	  cout << "\tOwner:      " << p->owner << endl;
	  cout << "\tFilename:   " << p->filename << endl;
	  cout << "\tPriority:   " << p->priority << endl;
	  cout << "\tStart time: " << p->start_time << endl << endl;
	}
      else
	cout << "\tTransferring Checkpoint Server Status" << endl << endl;
      p = p->next;
    }
  cout << "End of Transfer List Dump" << endl;
  cout << "=-=-=-=-=-=-=-=-=-=-=-=-=" << endl << endl << endl;
}














