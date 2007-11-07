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
*   Module:  server_filestat                                                  *
*                                                                             *
*******************************************************************************
*                                                                             *
*   File:    server_filestat.C                                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This module contains routine which keep an in-memory database of all      *
*   files kept on the Checkpoint Server.  Information about a backup          *
*   checkpoint file is also kept in this simple database.                     *
*                                                                             *
******************************************************************************/


/* Header Files */

#include "server_constants.h"
#include "server_network.h"
#include <stdlib.h>
#include <string.h>
#include <iostream.h>
#include <iomanip.h>
#include "server_filestat.h"
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>




extern "C" { struct hostent* gethostbyname(char*); }




/* Class FileStat *************************************************************
*                                                                             *
*   Function: FileStat()                                                      *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function is the class constructor, and its main function is to       *
*   initialize the private data members.  Specifically, it sets the total,    *
*   free, and used capacities to their inital values, and sets each bucket in *
*   the hash table to point at nothing (i.e., NULL).                          *
*                                                                             *
*   Currently, this module cannot write the in-memory data structure to disk. *
*   This is scheduled to be changed in the next version, and, thus, this      *
*   function would also read in the in-memory data structure from disk.       *
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


FileStat::FileStat()
{
  int count;

  capacity = 1000000000.0;
  capacity_free = 1000000000.0;
  capacity_used = 0.0;
  for (count=0; count<MAX_HASH_SIZE; count++)
    hash_table[count] = NULL;
  // Load stats from file, if it exists
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: ~FileStat()                                                     *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function is the class destructor, and its main function is to return *
*   dynamically acquired memory.  It destroys all of the trees, returning all *
*   dynamic memory.                                                           *
*                                                                             *
*   Currently, this module cannot write the in-memory data structure to disk. *
*   This is scheduled to be changed in the next version.  Therefore, this     *
*   function will also be responsible for writing the in-memory data          *
*   structure to disk before destroying it.                                   *
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


FileStat::~FileStat()
{
  int count;

  // Store stats to new file
  for (count=0; count<MAX_HASH_SIZE; count++)
    KillMachineTree(hash_table[count]);
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: FindOrAddMachine(const char* machine_name)                      *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function, given a machine name, uses the Hash() function to          *
*   determine in which bucket it belongs.  If it is already in the machine    *
*   name binary tree, then a pointer to the node is returned.  Otherwise, a   *
*   new machine name node is created, and a pointer to this new node is       *
*   returned.                                                                 *
*                                                                             *
*   If a dynamic memory allocation request fails, then the program will be    *
*   terminated.  Also, a subdirectory is created for a new submitting         *
*   machine.  If the subdirectory creation fails, then the program will       *
*   terminate.                                                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        const char* machine_name - a pointer to the name of the submitting   *
*                                   machine                                   *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        machinenode* - a pointer to the desired machine name node (which may *
*                       have been created by the routine)                     *
*                                                                             *
******************************************************************************/


machinenode* FileStat::FindOrAddMachine(const char* machine_name)
{
  int          h_value;
  machinenode* m;
  machinenode* trail = NULL;
  int          sc_val;

  h_value = Hash(machine_name);
  m = hash_table[h_value];
  while ((m != NULL) && ((sc_val=strncmp(machine_name, m->machine_name, 
				 MAX_MACHINE_NAME_LENGTH)) != 0))
    {
      trail = m;
      if (sc_val < 0)
	m = m->left;
      else
	m = m->right;
    }
  sprintf(pathname, "/tmp/%s", machine_name);
  if (m == NULL)
    {
      m = new machinenode;
      if (m == NULL)
	{
	  cerr << endl << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR: cannot make new machine node" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR:" << endl << endl;
	  exit(DYNAMIC_ALLOCATION);
	}
      if (mkdir(pathname, (mode_t) INT_MAX) != 0)
	if (errno != EEXIST)
	  {
	    cerr << endl << "ERROR:" << endl;
	    cerr << "ERROR:" << endl;
	    cerr << "ERROR: cannot make subdirectory \"" << pathname << "\""
	         << endl;
	    cerr << "ERROR:" << endl;
	    cerr << "ERROR:" << endl << endl;
	    perror("errno explanation:");
	    exit(MKDIR_ERROR);
	  }
      strncpy(m->machine_name, machine_name, MAX_MACHINE_NAME_LENGTH); 
      m->owner_root = NULL;
      m->left = NULL;
      m->right = NULL;
      if (trail == NULL)
	hash_table[h_value] = m;
      else if (sc_val < 0)
	trail->left = m;
      else
	trail->right = m;
    }
  return m;
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: FindOrAddOwner(const char* owner_name,                          *
*                            ownerinfo*& root)                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function, given a pointer to the root of an owner name binary tree,  *
*   attempts to find a specified owner.  If the owner is found, then a        *
*   pointer to the node is returned.  If the owner could not be found, then a *
*   new node is created for that owner, and a pointer to this new node is     *
*   returned.                                                                 *
*                                                                             *
*   This is done "optimistically" under the assumption that Condor will not   *
*   ask for checkpoint files belonging to a non-existent owner.  This code    *
*   is beneficial when a new Condor user submits a job (or an old Condor user *
*   gets an account on a different machine).                                  *
*                                                                             *
*   If a dynamic memory allocation request fails, then the program will be    *
*   terminated.  Also, a subdirectory is created for a new owner.  If the     *
*   subdirectory creation fails, then the program will terminate.             *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        const char* owner_name - a pointer to the owner's name               *
*        ownerinfo*& root     - a reference to a pointer to the root of an    *
*                               owner name binary tree                        *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        ownerinfo* - a pointer to the desired owner name node (which may     *
*                     have been created by the routine)                       *
*                                                                             *
******************************************************************************/


ownerinfo* FileStat::FindOrAddOwner(const char* owner_name,
				    ownerinfo*& root)
{
  ownerinfo* o;
  ownerinfo* trail=NULL;
  int        sc_val;

  o = root;
  while ((o != NULL) && ((sc_val=strncmp(owner_name, o->owner, 
					 MAX_NAME_LENGTH)) != 0))
    {
      trail = o;
      if (sc_val < 0)
	o = o->left;
      else
	o = o->right;
    }
  sprintf(pathname, "%s/%s", pathname, owner_name);
  if (o == NULL)
    {
      o = new ownerinfo;
      if (o == NULL)
	{
	  cerr << endl << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR: cannot make new ownerinfo node" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR:" << endl << endl;
	  exit(DYNAMIC_ALLOCATION);
	}
      if (mkdir(pathname, (mode_t) INT_MAX) != 0)
	if (errno != EEXIST)
	  {
	    cerr << endl << "ERROR:" << endl;
	    cerr << "ERROR:" << endl;
	    cerr << "ERROR: cannot make subdirectory \"" << pathname << "\""
	         << endl;
	    cerr << "ERROR:" << endl;
	    cerr << "ERROR:" << endl << endl;
	    exit(MKDIR_ERROR);
	  }
      strncpy(o->owner, owner_name, MAX_NAME_LENGTH);
      o->file_root = NULL;
      o->left = NULL;
      o->right = NULL;
      if (trail == NULL)
	root = o;
      else if (sc_val < 0)
	trail->left = o;
      else
	trail->right = o;
    }
  return o;
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: FindOrAddFile(const char* filename,                             *
*                           fileinfo*&  root)                                 *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function, given a pointer to the root of a file name binary tree,    *
*   attempts to find a specified file.  If the file is found, then a pointer  *
*   to the node is returned.  If the file could not be found, then a new      *
*   node is created for that file name, and a pointer to this new node is     *
*   returned.                                                                 *
*                                                                             *
*   This is done "optimistically" under the assumption that Condor will not   *
*   ask for non-existent checkpoint files.  Thus, this process of creating    *
*   new nodes on the fly is advantageous when storing new checkpoint files.   *
*                                                                             *
*   If a dynamic memory allocation request fails, then the program will be    *
*   terminated.                                                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        const char* filename - a pointer to the name of a checkpoint file    *
*        fileinfo*&  root     - a reference to a pointer to the root of a     *
*                               file name binary tree                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        fileinfo* - a pointer to the desired file name node (which may have  *
*                    been created by the routine)                             *
*                                                                             *
******************************************************************************/


fileinfo* FileStat::FindOrAddFile(const char* filename,
				  fileinfo*&  root)
{
  fileinfo* f;
  fileinfo* trail=NULL;
  int       sc_val;

  f = root;
  while ((f != NULL) && ((sc_val=strncmp(filename, f->filename, 
					 MAX_CONDOR_FILENAME_LENGTH)) != 0))
    {
      trail = f;
      if (sc_val < 0)
	f = f->left;
      else
	f = f->right;
    }
  sprintf(pathname, "%s/%s", pathname, filename); 
  if (f == NULL)
    {
      f = new fileinfo;
      if (f == NULL)
	{
	  cerr << endl << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR: cannot make new fileinfo node" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR:" << endl << endl;
	  exit(DYNAMIC_ALLOCATION);
	}
      strncpy(f->filename, filename, MAX_CONDOR_FILENAME_LENGTH);
      f->left = NULL;
      f->right = NULL;
      f->current.version = 0;
      f->current.filesize = 0;
      f->current.location = NOT_PRESENT;
      f->backup.version = 0;
      f->backup.filesize = 0;
      f->backup.location = NOT_PRESENT;
      if (trail == NULL)
	root = f;
      else if (sc_val < 0)
	trail->left = f;
      else
	trail->right = f;
    }
  return f;
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: Hash(const char* machine_name)                                  *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function performs a hash based on an machine's IP address.  It is    *
*   difficult to achieve a good hash on a string, so a machine's string is    *
*   converted into an IP address, and this IP address is hashed.              *
*                                                                             *
*   The actual hashing function is not extremely robust, but appears to yield *
*   fairly distributed values.  Each byte of an IP address (4 bytes) is added *
*   to form a sum.  This remainder of the division of the sum by 100 is the   *
*   returned hash value.                                                      *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        const char* machine_name - a pointer to the name of the submitting   *
*                                   machine                                   *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int - the bucket which the submitting machine hashes to              *
*                                                                             *
******************************************************************************/


int FileStat::Hash(const char* machine_name)
{
  struct hostent* h;
  u_char*         x;
  int             value=0;
  int             count;

  h = condor_gethostbyname((char*)machine_name);
  if (h == NULL)
    return(BAD_MACHINE_NAME);
  x = (u_char*) h->h_addr;
  for (count=0; count<4; count++)
    {
      value += *x;
      x++;
    }
  return(value%MAX_HASH_SIZE);
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: StoreFile(const char* machine_name,                             *
*                       const char* owner_name,                               *
*                       const char* filename,                                 *
*                       int         num_bytes)                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function is called when a checkpoint file has successfully been      *
*   saved on the Checkpoint Server.  It updates the data structure to reflect *
*   the new copy.                                                             *
*                                                                             *
*   In addition, this function may delete an older backup of the checkpoint   *
*   file.  The in-memory database will hold the current checkpoint file       *
*   information, and information about one backup checkpoint file.  When a    *
*   new checkpoint file is stored, it may be necessary to remove an older     *
*   checkpoint backup.  Because space on the Checkpoint Server is at a        *
*   premium, it is preferrable to discard a newer version stored on the       *
*   server rather than discard an older version on AFS.                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        const char* machine_name - a pointer to a string containing the      *
*                                   submitting machine's name                 *
*        const char* owner_name   - a pointer to a string containing the      *
*                                   owner's name                              *
*        const char* filename     - a pointer to a string containing the name *
*                                   of the checkpoint file                    *
*        int         num_bytes    - the size of the checkpoint file           *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int : CANNOT_DELETE_FILE - the new checkpoint file was stored, but   *
*                                   an older backup could not be deleted      *
*              CKPT_OK                 - the new checkpoint was stored, and an old *
*                                   backup checkpoint file was deleted (if    *
*                                   one exists)                               *
*                                                                             *
******************************************************************************/


int FileStat::StoreFile(const char* machine_name, 
			const char* owner_name, 
			const char* filename,
			int         num_bytes)
{
  machinenode* m;
  ownerinfo*   o;
  fileinfo*    f;
  char         temppath[MAX_PATHNAME_LENGTH];
  int          ret_code=CKPT_OK;

  m = FindOrAddMachine(machine_name);
  o = FindOrAddOwner(owner_name, m->owner_root);
  f = FindOrAddFile(filename, o->file_root);
  if (f->current.location == AFS)
    {
      if (f->backup.version != 0)
	{
	  if (f->backup.location == AFS)
	    sprintf(temppath, "%s/%s/%s/%s.%d", AFS_PREFIX, machine_name, 
		    owner_name, filename, f->backup.version);
	  else
	    sprintf(temppath, "/tmp/%s/%s/%s.%d", machine_name, owner_name,
		    filename, f->backup.version);
	  if (remove(temppath) != 0)
	    ret_code = CANNOT_DELETE_FILE;
	}
      f->backup = f->current;
    }
  else if (f->backup.location == AFS)
    {
      sprintf(temppath, "/tmp/%s/%s/%s.%d", machine_name, owner_name,
	      filename, f->current.version);
      if (remove(temppath) != 0)
	ret_code = CANNOT_DELETE_FILE;
      capacity_used -= f->current.filesize;
      capacity_free += f->current.filesize;
    }
  else
    {
      if (f->backup.version != 0)
	{
	  sprintf(temppath, "/tmp/%s/%s/%s.%d", machine_name, owner_name,
		  filename, f->backup.version);
	  if (remove(temppath) != 0)
	    ret_code = CANNOT_DELETE_FILE;
	}
      f->backup = f->current;      
      capacity_used -= f->backup.filesize;
      capacity_free += f->backup.filesize;
    }
  f->current.version++;
  f->current.location = CHKPT_SERVER;
  f->current.filesize = num_bytes;
  capacity_used += num_bytes;
  capacity_free -= num_bytes;
  return ret_code;
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: GetVersion(const char* machine_name,                            *
*                        const char* owner_name,                              *
*                        const char* filename)                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function is used to get the version number of the current checkpoint *
*   file.                                                                     *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        const char* machine_name - a pointer to a string containing the      *
*                                   submitting machine's name                 *
*        const char* owner_name   - a pointer to a string containing the      *
*                                   owner's name                              *
*        const char* filename     - a pointer to a string containing the name *
*                                   of the checkpoint file                    *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int - the version number of the current version of the checkpoint    *
*              file                                                           *
*                                                                             *
******************************************************************************/


int FileStat::GetVersion(const char* machine_name, 
			 const char* owner_name, 
			 const char* filename)
{
  machinenode* m;
  ownerinfo*   o;
  fileinfo*    f;

  m = FindOrAddMachine(machine_name);
  o = FindOrAddOwner(owner_name, m->owner_root);
  f = FindOrAddFile(filename, o->file_root);
  return(f->current.version);
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: DeleteFile(const char* machine_name,                            *
*                        const char* owner_name,                              *
*                        const char* filename)                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This large(!) function is used to delete all traces of a file from the    *
*   Checkpoint Server.  Usually, this function would only be called upon the  *
*   successful completion of a Condor job.                                    *
*                                                                             *
*   However, at this time, the Checkpoint Server does not delete any files    *
*   from the disk or in the in-memory data structure.  This is one of the     *
*   first changes to be made in the next version, but this module does fully  *
*   support file deletion.                                                    *
*                                                                             *
*   This routine is somewhat long, due to the fact that a file deletion may   *
*   cause the file name tree to be empty.  This in turn causes the owner node *
*   (which points to the root of the file name tree) to be deleted, which in  *
*   turn may cause the owner tree to be empty.  Then, the corresponding       *
*   machine node needs to be deleted.  As all of this is done to the          *
*   in-memory data structure, the corresponding files and directories on the  *
*   Checkpoint Server will be removed.                                        *
*                                                                             *
*   An iterative algorithm is preferred to a recursive algorithm primarily    *
*   because of speed.  (The entire data structure is "locked" as a deletion   *
*   takes place.)                                                             *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        const char* machine_name - the string representation of the          *
*                                   submitting machine's name                 *
*        const char* owner_name   - the owner of the file                     *
*        const char* filename     - the name of the file to be deleted        *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int : CANNOT_FIND_MACHINE     - the specified machine name does not  *
*                                        have a corresponding directory entry *
*              CANNOT_FIND_OWNER       - the specified owner does not have a  *
*                                        corresponding entry                  *
*              CANNOT_FIND_FILE        - the specified file is not in the     *
*                                        in-memory data structure             *
*              CANNOT_DELETE_FILE      - a file (either the current           *
*                                        checkpoint file or its backup) could *
*                                        not be deleted                       *
*              CANNOT_DELETE_DIRECTORY - an owner or machine name directory   *
*                                        could not be deleted                 *
*              CKPT_OK                      - all deletions (including the         *
*                                        "extended" ones for a file's owner   *
*                                        and the owner's machine) were        *
*                                        successfully completed               *
*                                                                             *
******************************************************************************/


int FileStat::DeleteFile(const char* machine_name,
			 const char* owner_name,
			 const char* filename)
{
  machinenode* m;
  machinenode* m_trail=NULL;
  machinenode* m_temp;
  machinenode* m_tt=NULL;
  ownerinfo*   o;
  ownerinfo*   o_trail=NULL;
  ownerinfo*   o_temp;
  ownerinfo*   o_tt=NULL;
  fileinfo*    f;
  fileinfo*    f_trail=NULL;
  fileinfo*    f_temp;
  fileinfo*    f_tt=NULL;
  int          h_val;
  int          sc_val;
  char         temppath[MAX_PATHNAME_LENGTH];

  h_val = Hash(machine_name);
  m = hash_table[h_val];
  while ((m != NULL) && ((sc_val=strncmp(machine_name, m->machine_name,
					 MAX_MACHINE_NAME_LENGTH)) != 0))
    {
      m_trail = m;
      if (sc_val < 0)
	m = m->left;
      else
	m = m->right;
    }
  if (m == NULL)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot find machine (" << machine_name << ')' << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      return(CANNOT_FIND_MACHINE);
    }
  o = m->owner_root;
  while ((o != NULL) && ((sc_val=strncmp(owner_name, o->owner, 
					 MAX_NAME_LENGTH)) != 0))
    {
      o_trail = o;
      if (sc_val < 0)
	o = o->left;
      else
	o = o->right;
    }
  if (o == NULL)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot find owner (" << owner_name << ") for job (on "
	   << machine_name << ')' << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      return(CANNOT_FIND_OWNER);
    }
  f = o->file_root;
  while ((f != NULL) && ((sc_val=strncmp(filename, f->filename, 
					 MAX_CONDOR_FILENAME_LENGTH)) != 0))
    {
      f_trail = f;
      if (sc_val < 0)
	f = f->left;
      else
	f = f->right;
    }
  if (f == NULL)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot find filename (" << filename << ") for job ("
	   << machine_name << ", " << owner_name << ')' << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      return(CANNOT_FIND_FILE);
    }
  f_temp = f->left;
  if (f_temp != NULL)
    {
      while (f_temp->right != NULL)
	{
	  f_tt = f_temp;
	  f_temp = f_temp->right;
	}
      if (f_tt != NULL)
	{
	  f_tt->right = f_temp->left;
	  f_temp->left = f->left;
	  f_temp->right = f->right;
	}
      else
	f_temp->right = f->right;
      if (f_trail == NULL)
	o->file_root = f_temp;
      else if (f_trail->left == f)
	f_trail->left = f_temp;
      else
	f_trail->right = f_temp;
    }
  else
    {
      if (f_trail == NULL)
	o->file_root = f->right;
      else if (f_trail->left == f)
	f_trail->left = f->right;
      else
	f_trail->right = f->right;
    }
  if (f->current.location == CHKPT_SERVER)
    {
      sprintf(temppath, "/tmp/%s/%s/%s.%d", machine_name, owner_name, filename,
	      f->current.version);
      if (remove(temppath) != 0)
	return(CANNOT_DELETE_FILE);
    }
  if (f->backup.location == CHKPT_SERVER)
    {
      sprintf(temppath, "/tmp/%s/%s/%s.%d", machine_name, owner_name, filename,
	      f->backup.version);
      if (remove(temppath) != 0)
	return(CANNOT_DELETE_FILE);
    }
  // Also need to delete files from AFS directories here
  delete f;
  if (o->file_root == NULL)
    {
      o_temp = o->left;
      if (o_temp != NULL)
	{
	  while (o_temp->right != NULL)
	    {
	      o_tt = o_temp;
	      o_temp = o_temp->right;
	    }
	  if (o_tt != NULL)
	    {
	      o_tt->right = o_temp->left;
	      o_temp->left = o->left;
	      o_temp->right = o->right;
	    }
	  else
	    o_temp->right = o->right;
	  if (o_trail == NULL)
	    m->owner_root = o_temp;
	  else if (o_trail->left == o)
	    o_trail->left = o_temp;
	  else
	    o_trail->right = o_temp;
	}
      else
	{
	  if (o_trail == NULL)
	    m->owner_root = o->right;
	  else if (o_trail->left == o)
	    o_trail->left = o->right;
	  else
	    o_trail->right = o->right;
	}
      sprintf(temppath, "/tmp/%s/%s", machine_name, owner_name);
      if (rmdir(temppath) != 0)
	return(CANNOT_DELETE_DIRECTORY);
      delete o;
      if (m->owner_root == NULL)
	{
	  m_temp = m->left;
	  if (m_temp != NULL)
	    {
	      while (m_temp->right != NULL)
		{
		  m_tt = m_temp;
		  m_temp = m_temp->right;
		}
	      if (m_tt != NULL)
		{
		  m_tt->right = m_temp->left;
		  m_temp->left = m->left;
		  m_temp->right = m->right;
		}
	      else
		m_temp->right = m->right;
	      if (m_trail == NULL)
		hash_table[h_val] = m_temp;
	      else if (m_trail->left == m)
		m_trail->left = m_temp;
	      else
		m_trail->right = m_temp;
	    }
	  else
	    {
	      if (m_trail == NULL)
		hash_table[h_val] = m->right;
	      else if (m_trail->left == m)
		m_trail->left = m->right;
	      else
		m_trail->right = m->right;
	    }
	  sprintf(temppath, "/tmp/%s", machine_name);
	  if (rmdir(temppath) != 0)
	    return(CANNOT_DELETE_DIRECTORY);
	  delete m;
	}
    }
  return(CKPT_OK);
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: FillPathname(char*       pathname,                              *
*                          const char* machine_name,                          *
*                          const char* owner_name,                            *
*                          const char* filename)                              *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function, given the submitting machine name, owner name, and         *
*   checkpoint file name, constructs the corresponding checkpoint file's      *
*   path at a designated location.  The machine name, owner, and file name    *
*   are not sufficient to generate a pathname; the file may be on AFS or the  *
*   Checkpoint Server, and has a version number appended to the file name.    *
*                                                                             *
*   After constructing the full pathname for the checkpoint file, this        *
*   function returns the current version number.                              *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        char*       pathname     - a pointer to where the pathname should be *
*                                   placed                                    *
*        const char* machine_name - a pointer to a string containing the      *
*                                   submitting machine's name                 *
*        const char* owner_name   - a pointer to a string containing the      *
*                                   owner's name                              *
*        const char* filename     - a pointer to a string containing the name *
*                                   of the checkpoint file                    *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int - the current checkpoint file's version number is returned       *
*                                                                             *
******************************************************************************/


int FileStat::FillPathname(char*       pathname,
			   const char* machine_name,
			   const char* owner_name,
			   const char* filename)
{
  machinenode* m;
  ownerinfo*   o;
  fileinfo*    f;

  m = FindOrAddMachine(machine_name);
  o = FindOrAddOwner(owner_name, m->owner_root);
  f = FindOrAddFile(filename, o->file_root);
  if (f->current.location == AFS)
    sprintf(pathname, "%s/%s/%s/%s.%d", AFS_PREFIX, machine_name, owner_name,
	    filename, f->current.version);
  else if (f->current.location == CHKPT_SERVER)
    sprintf(pathname, "/tmp/%s/%s/%s.%d", machine_name, owner_name, filename,
	    f->current.version);
  else
    sprintf(pathname, "no file");
  return(f->current.version);
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: KillMachineTree(machinenode* m)                                 *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function recursively performs a post-order traversal in order to     *
*   delete all nodes in a machine name tree.                                  *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        machinenode* p - a pointer to a machine name node                    *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::KillMachineTree(machinenode* p)
{
  if (p != NULL)
    {
      KillMachineTree(p->left);
      KillMachineTree(p->right);
      KillOwnerTree(p->owner_root);
      delete p;
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: KillOwnerTree(ownerinfo* p)                                     *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function recursively performs a post-order traversal in order to     *
*   delete all nodes in an owner name tree.                                   *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        ownerinfo* p - a pointer to an owner name node                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::KillOwnerTree(ownerinfo* p)
{
  if (p != NULL)
    {
      KillOwnerTree(p->left);
      KillOwnerTree(p->right);
      KillFileTree(p->file_root);
      delete p;
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: KillFileTree(fileinfo* p)                                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function recursively performs a post-order traversal in order to     *
*   delete all nodes in a file name tree.                                     *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        fileinfo* p - a pointer to a file name node                          *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::KillFileTree(fileinfo* p)
{
  if (p != NULL)
    {
      KillFileTree(p->left);
      KillFileTree(p->right);
      delete p;
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: Dump()                                                          *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function goes to each bucket in the hash table and calls the         *
*   MachineDump() function to print out all information about a machine name  *
*   binary tree.  (This, in turn, causes all owner and file information to be *
*   printed.)                                                                 *
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


void FileStat::Dump()
{
  int count;

  cout << "Dump of Checkpoint Server Files:" << endl;
  cout << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-" << endl;
  for (count=0; count<MAX_HASH_SIZE; count++)
    {
      cout << "Bucket #" << count << ':' << endl;
      MachineDump(hash_table[count]);
    }
  cout << "End of Checkpoint Server Dump" << endl;
  cout << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl << endl << endl;
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: MachineDump(machinenode* m)                                     *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function does a recursive in-order binary tree traversal to print    *
*   out all file information for each node in an owner name tree.  To print   *
*   out the file information, the FileDump() member function is invoked.      *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        ownerinfo* o - a pointer to an owner name node                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::MachineDump(machinenode* m)
{
  if (m != NULL)
    {
      MachineDump(m->left);
      cout << "    " << m->machine_name << ':' << endl;
      OwnerDump(m->owner_root);
      MachineDump(m->right);
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: OwnerDump(ownerinfo* o)                                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function does a recursive in-order binary tree traversal to print    *
*   out all file information for each node in an owner name tree.  To print   *
*   out the file information, the FileDump() member function is invoked.      *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        ownerinfo* o - a pointer to an owner name node                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::OwnerDump(ownerinfo* o)
{
  if (o != NULL)
    {
      OwnerDump(o->left);
      cout << "        " << o->owner << ':' << endl;
      FileDump(o->file_root);
      OwnerDump(o->right);
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: FileDump(fileinfo* p)                                           *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function does a recursive in-order binary tree traversal to print    *
*   all of the information in a file name tree.  Only information about the   *
*   current version is printed.                                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        fileinfo* p - a pointer to a file name node                          *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::FileDump(fileinfo* p)
{
  if (p != NULL)
    {
      FileDump(p->left);
      cout << "            " << p->filename << ':' << endl << "\t\tVersion:  " 
	   << p->current.version << endl;
      switch (p->current.location)
	{
	case CHKPT_SERVER: 
	  cout << "\t\tLocation: on Checkpoint Server" << endl;
	  break;
	case AFS:
	  cout << "\t\tLocation: on AFS" << endl; 
	  break;
	default:
	  cout << "\t\tNot present at this time" << endl;
	}
      FileDump(p->right);
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: LevelMachineDump(machinenode* m,                                *
*                              int          level,                            *
*                              int          target_level,                     *
*                              int&         offset)                           *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function is used to print out the first 4 levels of a machine name   *
*   binary tree in a tree-like format.  This is done recursively, starting at *
*   the root of the machine name tree (see DumpHashTree() for more details.   *
*   details).  This is very similar to the LevelFileDump() routine.           *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        machinenode* o            - a pointer to a machine name node         *
*        int          level        - the current level                        *
*        int          target_level - the desired level to print               *
*        int&         offset       - used to determine where to start         *
*                                    printing on a page                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::LevelMachineDump(machinenode* m,
				int          level,
				int          target_level,
				int&         offset)
{
  int spacing;

  spacing = 160/ipower(2, target_level);
  if (m != NULL)
    {
      if (level < target_level)
	{
	  LevelMachineDump(m->left, level+1, target_level, offset);
	  LevelMachineDump(m->right, level+1, target_level, offset);
	}
      else if (level == target_level)
	{
	  if (offset != 0)
	    {
	      cout << setw(offset+spacing) << m->machine_name;
	      offset = 0;
	    }
	  else
	    cout << setw(spacing) << m->machine_name;
	}
    }
  else if (level <= target_level)
    {
      if (offset != 0)
	{
	  cout << setw(offset+spacing*ipower(2, target_level-level)) << " ";
	  offset = 0;
	}
      else
	cout << setw(spacing*ipower(2, target_level-level)) << " ";
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: LevelOwnerDump(ownerinfo* o,                                    *
*                            int       level,                                 *
*                            int       target_level,                          *
*                            int&      offset)                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function is used to print out the first 4 levels of a owner name     *
*   binary tree in a tree-like format.  This is done recursively, starting at *
*   the root of the owner name tree (see DumpMachineTree() for more           *
*   details).  This is very similar to the LevelFileDump() routine.           *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        ownerinfo* o            - a pointer to an owner name node            *
*        int        level        - the current level                          *
*        int        target_level - the desired level to print                 *
*        int&       offset       - used to determine where to start printing  *
*                                  on a page                                  *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::LevelOwnerDump(ownerinfo* o,
			      int        level,
			      int        target_level,
			      int&       offset)
{
  int spacing;

  spacing = 160/ipower(2, target_level);
  if (o != NULL)
    {
      if (level < target_level)
	{
	  LevelOwnerDump(o->left, level+1, target_level, offset);
	  LevelOwnerDump(o->right, level+1, target_level, offset);
	}
      else if (level == target_level)
	{
	  if (offset != 0)
	    {
	      cout << setw(offset+spacing) << o->owner;
	      offset = 0;
	    }
	  else
	    cout << setw(spacing) << o->owner;
	}
    }
  else if (level <= target_level)
    {
      if (offset != 0)
	{
	  cout << setw(offset+spacing*ipower(2, target_level-level)) << " ";
	  offset = 0;
	}
      else
	cout << setw(spacing*ipower(2, target_level-level)) << " ";
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: LevelFileDump(fileinfo* p,                                      *
*                           int       level,                                  *
*                           int       target_level,                           *
*                           int&      offset)                                 *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function is used to print out the first 4 levels of a file name      *
*   binary tree in a tree-like format.  This is done recursively, starting at *
*   the root of the file name tree (see DumpOwnerTree() for more details).    *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        fileinfo* p            - a pointer to a file name node               *
*        int       level        - the current level                           *
*        int       target_level - the desired level to print                  *
*        int&      offset       - used to determine where to start printing   *
*                                 on a page                                   *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::LevelFileDump(fileinfo* p,
			     int       level,
			     int       target_level,
			     int&      offset)
{
  int spacing;

  spacing = 160/ipower(2, target_level);
  if (p != NULL)
    {
      if (level < target_level)
	{
	  LevelFileDump(p->left, level+1, target_level, offset);
	  LevelFileDump(p->right, level+1, target_level, offset);
	}
      else if (level == target_level)
	{
	  if (offset != 0)
	    {
	      cout << setw(offset+spacing) << p->filename;
	      offset = 0;
	    }
	  else
	    cout << setw(spacing) << p->filename;
	}
    }
  else if (level <= target_level)
    {
      if (offset != 0)
	{
	  cout << setw(offset+spacing*ipower(2, target_level-level)) << " ";
	  offset = 0;
	}
      else
	cout << setw(spacing*ipower(2, target_level-level)) << " ";
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: ipower(int base,                                                *
*                    int exp)                                                 *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This auxilliary function simply takes two integers and returns x to the   *
*   yth power, where x and y are parameters.  This behaves like the pow()     *
*   function (in <math.h>), but returns an integer result.                    *
*                                                                             *
*   This is used exclusively by the "Level----Dump()" functions to space the  *
*   node information when printing out levels.                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int base - the base field                                            *
*        int exp  - the exponent field                                        *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int - the result of taking (base) to the (exp) power                 *
*                                                                             *
******************************************************************************/


int FileStat::ipower(int base, 
		     int exp)
{
  int temp=1;
  int count;

  for (count=0; count<exp; count++)
    temp *= base;
  return (temp);
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: DumpHashTree(int bucket)                                        *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function calls upon the LevelMachineDump() function to print out the *
*   first 4 levels of the machine name binary tree which is "attached" to a   *
*   particular bucket in the hash table.                                      *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int bucket - the bucket number (0 to 99) whose machine name binary   *
*                     tree is to be printed                                   *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::DumpHashTree(int bucket)
{
  int          level;
  int          offset;
  int          OS;

  cout << endl << "Printing machine tree for hash bucket #" << bucket << ':'
       << endl;
  offset = -35;
  for (level=1; level<=MAX_PRINTABLE_LEVELS; level++)
    {
      OS = offset;
      LevelMachineDump(hash_table[bucket], 1, level, OS);
      cout << endl << endl;
      offset += ipower(2, MAX_PRINTABLE_LEVELS-level-1)*5;
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: DumpMachineTree(const char* machine_name)                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function calls upon the LevelOwnerDump() function to print out the   *
*   first 4 levels of the owner name binary tree belonging to a particular    *
*   machine.                                                                  *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        const char* machine_name - a pointer to the machine's name (string)  *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::DumpMachineTree(const char* machine_name)
{
  machinenode* m;
  int          level;
  int          offset;
  int          OS;

  m = FindOrAddMachine(machine_name);
  cout << endl << "Printing owner tree for [" << machine_name << "]:" << endl;
  offset = -35;
  for (level=1; level<=MAX_PRINTABLE_LEVELS; level++)
    {
      OS = offset;
      LevelOwnerDump(m->owner_root, 1, level, OS);
      cout << endl << endl;
      offset += ipower(2, MAX_PRINTABLE_LEVELS-level-1)*5;
    }
}




/* Class FileStat *************************************************************
*                                                                             *
*   Function: DumpOwnerTree(const char* machine_name,                         *
*                           const char* owner_name)                           *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function calls upon the LevelFileDump() function to print out the    *
*   first 4 levels of the file name binary tree belonging to a particular     *
*   owner.  (To uniquely identify an owner, the submitting machine is used    *
*   in conjunction with the owner's login name.                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        const char* machine_name - a pointer to the machine's name (string)  *
*        const char* owner_name   - a pointer to the owner's name (string)    *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void FileStat::DumpOwnerTree(const char* machine_name,
			     const char* owner_name)
{
  machinenode* m;
  ownerinfo*   o;
  int          level;
  int          offset;
  int          OS;

  m = FindOrAddMachine(machine_name);
  o = FindOrAddOwner(owner_name, m->owner_root);
  cout << endl << "Printing file tree for [" << machine_name << ", "
       << owner_name << "]:" << endl;
  offset = -35;
  for (level=1; level<=MAX_PRINTABLE_LEVELS; level++)
    {
      OS = offset;
      LevelFileDump(o->file_root, 1, level, OS);
      cout << endl << endl;
      offset += ipower(2, MAX_PRINTABLE_LEVELS-level-1)*5;
    }
}


























