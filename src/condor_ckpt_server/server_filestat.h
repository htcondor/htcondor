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
*   File:    server_filestat.h                                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This module contains prototypes and type definitions for the              *
*   server_filestat module.  This module is used to keep track of all of the  *
*   checkpoint files used by the Checkpoint Server.                           *
*                                                                             *
******************************************************************************/


#ifndef SERVER_FILESTAT_H
#define SERVER_FILESTAT_H


/* Header Files */

#include "server_constants.h"
#include "server_typedefs.h"
#include <sys/types.h>




/* Module-wide Constants */

const int MAX_HASH_SIZE = 100;
const int MAX_PRINTABLE_LEVELS = 4;
const int AFS = 0;
const int CHKPT_SERVER = 1;
const int NOT_PRESENT = -1;
const int MAX_DIRNAME_LENGTH = 256;
const int MAX_PATHNAME_LENGTH = 2000;
const int BAD_MACHINE_NAME = -1000;




/* Type Definitions */

typedef struct versioninfo
{
  u_short   version;
  u_lint    filesize;
  short int location;  
} versioninfo;




typedef struct fileinfo
{
  char        filename[MAX_CONDOR_FILENAME_LENGTH];
  versioninfo current, backup;
  fileinfo*   left;
  fileinfo*   right;
} fileinfo;




typedef struct ownerinfo
{
  char       owner[MAX_NAME_LENGTH];
  fileinfo*  file_root;
  ownerinfo* left;
  ownerinfo* right;
} ownerinfo;




typedef struct machinenode
{
  char         machine_name[MAX_MACHINE_NAME_LENGTH];
  ownerinfo*   owner_root;
  machinenode* left;
  machinenode* right;
} machinenode;




/******************************************************************************
*                                                                             *
*   Class: FileStat()                                                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This class is used to keep track of all of the checkpoint files used by   *
*   the checkpoint server.  This data structure is held entirely in memory,   *
*   and is essential for generating the checkpoint file names.                *
*                                                                             *
*   Condor uses a unique filename for each checkpoint file on a submitting    *
*   machine.  This holds the checkpoint files using 4 pieces of data:         *
*                                                                             *
*       1) the submitting machine name                                        *
*       2) the owner of the checkpointed job                                  *
*       3) the Condor filename of the checkpoint file                         *
*       4) a version number for a checkpoint file                             *
*                                                                             *
*   It is assumed that the machine name and owner name uniquely specify the   *
*   actual owner of a file.  (Here at the University of Wisconsin in Madison, *
*   the owner name (i.e., login name) is unique.  However, people outside of  *
*   the university may have the same login names as people at this            *
*   university.                                                               *
*                                                                             *
*   The data structure used to hold the checkpoint file data was created for  *
*   performance on one hand, and ease of translating the indices to create    *
*   the pathname of the checkpoint file.                                      *
*                                                                             *
*   On the first level, a submitting machine's IP address is hashed into      *
*   buckets.  (The 0.5 Beta version has 100 buckets.)  Each bucket has a      *
*   pointer to a binary tree based on the machine name (a string).  This      *
*   first level is hashed for performance, and a tree is used for hash        *
*   collisions because it is faster than a linked list.                       *
*                                                                             *
*   For each bucket in the hash table, there is a binary tree sorted by       *
*   machine name.  Each of the nodes in this tree has a pointer to the root   *
*   of a binary tree sorted by owner name.  A binary tree is chosen over a    *
*   second hash table since it is difficult to hash using a string.  (The     *
*   machine names were hashed using their IP addresses.)                      *
*                                                                             *
*   Each node in the binary tree of owner names on a machine has a pointer to *
*   the root of a binary tree sorted on Condor checkpoint file names.  Again, *
*   a binary tree is chosen due to difficulties with hashing on a string.     *
*                                                                             *
*   To recap, the file information is kept in a layered data structure which  *
*   has a hash table at the top where each bucket points to the root of a     *
*   binary tree (sorted by machine name) where each node points to the root   *
*   of a binary tree (sorted by owner name) where each node points to the     *
*   root of a binary tree (sorted by file name).  Each node in the final      *
*   final binary tree contains information about a file including a version   *
*   number (used exclusively by the Checkpoint Server), size of the file,     *
*   location of the file (nowhere, on the server, or on AFS), and the         *
*   accumulated time of Condor execution.  In addition, information about a   *
*   file backup is also maintained by the data structure.                     *
*                                                                             *
*   Another benefit of using binary trees is that the files are kept in a     *
*   directory structure analagous to the binary trees in a format:            *
*                                                                             *
*       /tmp/<machine name>/<owner name>/<file name>.<version>                *
*                                                                             *
*   if the file is kept on the server.                                        *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Features:                                                                 *
*       1) File lookups are fast.  It is arguable than having multiple layers *
*          of hashing would provide faster lookup times, but hashing strings  *
*          may cause long collision chains, and does not mirror the actual    *
*          directory tree on the Checkpoint Server.                           *
*       2) A backup file of a checkpoint is always kept.  This backup file    *
*          may exist on the Checkpoint Server or on AFS (the network file     *
*          system used at the University of Wisconsin).                       *
*       3) It is assumed that Checkpoint Server file space is at a premium.   *
*          Therefore, if an older backup exists on AFS, then a newer backup   *
*          on the server may be deleted when a new checkpoint file is stored. *
*       4) Additional routines to print out the information stored in this    *
*          in-memory data structure.  There are also routines to print the    *
*          contents of binary trees in a tree-like fashion for up to 4 levels *
*          in the tree.                                                       *
*       5) This data structure is based on the "optimistic" approach that     *
*          Condor does not make errors.  For example, there is no "Search"    *
*          member function in the classs, but there are "FindOrAdd" functions *
*          instead.  If a file cannot be found, then the data structure and   *
*          subdirectories are altered/made assuming that the file will be     *
*          placed on the Checkpoint Server.  This optimism (i.e., that Condor *
*          will not attempt to restart non-existent checkpoint files, etc)    *
*          makes it faster.                                                   *
*       6) By using dynamic structures, the in-memory data structure may grow *
*          to be very large with O(log n) access times.                       *
*       7) By appending a version number to the unique checkpoint file name   *
*          (it is unique for a submitting machine), we can keep track of      *
*          which is the most up-to-date version in case of a system crash.    *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Known Bugs:                                                               *
*       1) Although there is code provided to remove a checkpoint file (and   *
*          its backup) when a Condor job completes, this function is not      *
*          called.  (See the server module.)                                  *
*                                                                             *
*       2) The optimistic assumption may not be correct.                      *
*                                                                             *
*          Bug fix: Make separate functions for finding and adding a file.    *
*                                                                             *
*       3) The in-memory data structure is volatile, and cannot be saved to   *
*          disk.  Thus, all data will be lost inthe event of a system crash.  *
*                                                                             *
*          Bug fix: In a later release, additional routine will be made for   *
*                   saving and restoring the in-memory data structure.  These *
*                   routines will be written in a manner which preserves the  *
*                   actual ordering of the trees.                             *
*                                                                             *
*       4) Crash recovery is not supported.                                   *
*                                                                             *
*          Bug fix: The log file may be used to keep track of which file      *
*                   transfers were complete at the time of the crash.  In a   *
*                   later version, a function may be written which scans the  *
*                   log and the directory structure to automatically          *
*                   regenerate the in-memory data structure (and delete       *
*                   partially transfered files).                              *
*                                                                             *
*       5) The Checkpoint Server may be filled to capacity.                   *
*                                                                             *
*          Bug fix: There are already plans to implement a process whose job  *
*                   is to move files from the Checkpoint Server to AFS (or    *
*                   any other file system).  An algorithm has been made for   *
*                   selecting which files should be moved, and the in-memory  *
*                   data structure supports files on AFS.  (Changes will need *
*                   to be made to many of the structures used in the current  *
*                   implementation.)                                          *
*                                                                             *
*       6) When a machine contacts the Checkpoint Server, its IP address is   *
*          obtained.  This IP address is used to generate a machine name      *
*          using name servers (via the gethostbyaddr() function).  If no      *
*          name can be generated, then the program will terminate with an     *
*          error that gethostbyaddr() failed.                                 *
*                                                                             *
*          Bug fix: This problem should only occur if a name server goes down *
*                   or if a remote machine not on the local Condor pool       *
*                   attempts to checkpoint to the server.  If gethostbyaddr() *
*                   fails, then it is possible to generate a string name for  *
*                   the machine based on its IP address.  It is also possible *
*                   to queue the request and attempt to get the machine name  *
*                   at a later time (say, after 1 minute).                    *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Data Members:                                                             *
*       1) capacity      - an approximation of the amount of memory on the    *
*                          Checkpoint Server local disk                       *
*       2) capacity_free - the amount of memory free on the local disk of the *
*                          Checkpoint Server                                  *
*       3) capacity_used - the amount of disk space used on the Checkpoint    *
*                          Server                                             *
*       4) pathname      - a character array used for generating pathnames    *
*       5) hash_table    - a hash table in which every bucket points at the   *
*                          root of a binary tree sorted by machine name (a    *
*                          string)                                            *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Member Functions:                                                         *
*       1)  FindOrAddMachine - this function searches for a certain machine   *
*                              name.  If it cannot be found, it is created in *
*                              the machine name binary tree                   *
*       2)  FindOrAddOwner   - this function searches for a machine's owner.  *
*                              If the owner cannot be found, the owner is     *
*                              created in the in-memory data structure (and   *
*                              on the Checkpoint Server disk)                 *
*       3)  FindOrAddFile    - this function, given a machine name and owner, *
*                              searches the appropriate file binary tree.  If *
*                              it cannot be found, an entry in the in-memory  *
*                              data structure is created                      *
*       4)  Hash             - this function hashes an IP address and returns *
*                              a value between 0 and 99                       *
*       5)  StoreFile        - this function is called when a file has        *
*                              successfully been received by the Checkpoint   *
*                              Server.  It updates the in-memory database,    *
*                              and erases an old backup file (if too many     *
*                              exist)                                         *
*       6)  GetVersion       - this returns the current version number of a   *
*                              checkpoint file                                *
*       7)  DeleteFile       - this function removes all data pertaining to a *
*                              checkpoint file                                *
*       8)  FillPathname     - this function builds a pathname to the newest  *
*                              version of a checkpoint file                   *
*       9)  KillMachineTree  - this function, given the root of a machine     *
*                              name tree, recursively deletes all nodes in    *
*                              the binary tree, as well as the corresponding  *
*                              directories                                    *
*       10) KillOwnerTree    - given the root of an owner name tree, this     *
*                              function will recursively remove all nodes in  *
*                              the tree along with the corresponding          *
*                              directories                                    *
*       11) KillFileTree     - given the root of a file name tree, this will  *
*                              delete all nodes in the tree.  It will also    *
*                              remove all corresponding files                 *
*       12) Dump             - used for debugging, this function prints out   *
*                              the contents of the entire data structure      *
*                              unformatted)                                   *
*       13) MachineDump      - given the root of a machine name tree, this    *
*                              function will dump out information pertaining  *
*                              to each node in the tree.  For each node, it   *
*                              will dump the owners on a machine, as well as  *
*                              their files                                    *
*       14) OwnerDump        - given the root of an owner name tree, this     *
*                              function dumps all of the owner data.  For     *
*                              each node, the owner's file name data is also  *
*                              dumped                                         *
*       15) FileDump         - this function dumps all of the files in a file *
*                              name tree given the root of the binary tree    *
*       16) LevelMachineDump - this function will dump out the machines in    *
*                              a machine tree, but is limited to a depth of   *
*                              4 levels                                       *
*       17) LevelOwnerDump   - this function will dump out the owners in an   *
*                              owner name tree, but is limited to a depth of  *
*                              4 levels (80 columns)                          *
*       18) LevelFileDump    - this function will dump out the files in a     *
*                              file name tree in a tree-like fashion.         *
*                              However, it is limited to a size of 4 levels   *
*       19) ipower           - an auxilliary function which returns an        *
*                              integer, calculated as x to the yth power      *
*                              (where both x and y are integers)              *
*       20) DumpHashTree     - this function, used primarily for debugging,   *
*                              dumps the contents of a hash bucket (i.e., it  *
*                              prints the contents of a machine name binary   *
*                              tree) in a tree format (up to 4 levels)        *
*       21) DumpMachineTree  - this function, used primarily for debugging,   *
*                              dumps the contents of a machine node (i.e.,    *
*                              it prints out an owner binary tree) in a tree  *
*                              format (up to 4 levels)                        *
*       22) DumpOwnerTree    - this function, used primarily for debugging,   *
*                              dumps the contents of a owner node (i.e.,      *
*                              it prints out an owner's filename binary tree) *
*                              in a tree format (up to 4 levels)              *
*                                                                             *
******************************************************************************/


class FileStat
{
private:
  double capacity;
  double capacity_free;
  double capacity_used;
  char   pathname[MAX_PATHNAME_LENGTH];
  machinenode* hash_table[MAX_HASH_SIZE];
//  int StoreFS();
//  int LoadFS();
  machinenode* FindOrAddMachine(const char* machine_name);
  ownerinfo* FindOrAddOwner(const char* owner_name, ownerinfo*& root);
  fileinfo* FindOrAddFile(const char* filename, fileinfo*& root);
  int Hash(const char* machine_name);
  void KillMachineTree(machinenode* p);
  void KillOwnerTree(ownerinfo* p);
  void KillFileTree(fileinfo* p);
  void FileDump(fileinfo* p);
  void OwnerDump(ownerinfo* p);
  void MachineDump(machinenode* p);
  void LevelFileDump(fileinfo* p,
		     int       level,
		     int       target_level,
		     int&      offset);
  void LevelOwnerDump(ownerinfo* o,
		      int        level,
		      int        target_level,
		      int&       offset);
  void LevelMachineDump(machinenode* m,
			int          level,
			int          target_level,
			int&         offset);
  int ipower(int base, int exp);

public:
  FileStat();
  ~FileStat();
  int StoreFile(const char* machine_name, 
		const char* owner_name, 
		const char* filename,
		int         num_bytes);
  int GetVersion(const char* machine_name, 
		 const char* owner_name, 
		 const char* filename);
  int DeleteFile(const char* machine_name,
		 const char* owner_name,
		 const char* filename);
  void Dump();
  void DumpOwnerTree(const char* machine_name,
		     const char* owner_name);
  void DumpMachineTree(const char* machine_name);
  void DumpHashTree(int bucket);
  int FillPathname(char*       pathname,
		   const char* machine_name,
		   const char* owner_name,
		   const char* filename);
};


#endif






