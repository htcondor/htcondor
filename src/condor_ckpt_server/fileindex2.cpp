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


#include "condor_common.h"
#include "condor_debug.h"
#include "fileindex2.h"
#include "gen_lib.h" 

#include <iostream>
using namespace std;

FileIndex::FileIndex()
{
  int index;

  capacity_used = 0.0;
  for (index=0; index<MAX_HASH_SIZE; index++)
    hash_table[index] = NULL;
}


FileIndex::~FileIndex()
{
  DestroyIndex();
}


int FileIndex::Hash(struct in_addr machine_IP)
{
  int   val;
  unsigned char* c_ptr;

  c_ptr = (unsigned char*) &machine_IP.s_addr;
  val = *c_ptr + *(c_ptr+1) + *(c_ptr+2) + *(c_ptr+3);
  return val%MAX_HASH_SIZE;
}


machine_node* FileIndex::FindOrAddMachine(struct in_addr machine_IP,
                                          int            AddFlag)
{
  int           hash_bucket;
  int           temp;
  machine_node* trail=NULL;
  machine_node* ptr;
  char          pathname[MAX_PATHNAME_LENGTH];

  hash_bucket = Hash(machine_IP);
  ptr = hash_table[hash_bucket];
  while ((ptr != NULL) && (ptr->machine_IP.s_addr != machine_IP.s_addr))
    {
      trail = ptr;
      temp = (ptr->machine_IP.s_addr < machine_IP.s_addr);
      if (temp)
        ptr = ptr->right;
      else
        ptr = ptr->left;
    }
  if (AddFlag == ADD)
    {
      if (ptr == NULL)
        {
          ptr = new machine_node;
          if (ptr == NULL)
            {
              dprintf(D_ALWAYS, "ERROR: cannot allocate sufficient dynamic memory\n");
              exit(DYNAMIC_ALLOCATION);
            }
          ptr->left = NULL;
          ptr->right = NULL;
          ptr->machine_IP = machine_IP;
          ptr->owner_root = NULL;
          if (trail == NULL)
            hash_table[hash_bucket] = ptr;
          else if (temp)
            trail->right = ptr;
          else
            trail->left = ptr;
          sprintf(pathname, "%s%s", LOCAL_DRIVE_PREFIX, inet_ntoa(machine_IP));
	  errno = 0;
          if (mkdir(pathname, (mode_t) INT_MAX) != 0)
            if (errno != EEXIST)
              {
                dprintf(D_ALWAYS, "ERROR: cannot make directory '%s'\n", pathname);
                exit(MKDIR_ERROR);
              }
        }
    }
  return ptr;
}


owner_node* FileIndex::FindOrAddOwner(machine_node* m,
                                      const char*   owner_name,
                                      int           AddFlag)
{
  owner_node* trail=NULL;
  owner_node* ptr;
  int         temp;
  char        pathname[MAX_PATHNAME_LENGTH];

  ptr = m->owner_root;
  while ((ptr != NULL) && ((temp=strncmp(owner_name, ptr->owner_name,
                                         MAX_NAME_LENGTH)) != 0))
    {
      trail = ptr;
      if (temp < 0)
        ptr = ptr->left;
      else
        ptr = ptr->right;
    }
  if (AddFlag == ADD)
    {
      if (ptr == NULL)
        {
          ptr = new owner_node;
          if (ptr == NULL)
            {
              dprintf(D_ALWAYS, "ERROR: cannot allocate sufficient dynamic memory\n");
              exit(DYNAMIC_ALLOCATION);
            }
          ptr->left = NULL;
          ptr->right = NULL;
          strncpy(ptr->owner_name, owner_name, MAX_NAME_LENGTH);
          ptr->file_root = NULL;
          if (trail == NULL)
            m->owner_root = ptr;
          else if (temp < 0)
            trail->left = ptr;
          else
            trail->right = ptr;
          sprintf(pathname, "%s%s/%s", LOCAL_DRIVE_PREFIX, 
		  inet_ntoa(m->machine_IP), owner_name);
	  errno = 0;
          if (mkdir(pathname, (mode_t) INT_MAX) != 0)
            if (errno != EEXIST)
              {
                dprintf(D_ALWAYS, "ERROR: cannot make directory '%s'\n", pathname);
                exit(MKDIR_ERROR);
              }
        }
    }
  return ptr;
}


file_node* FileIndex::FindOrAddFile(file_node*& file_root,
				    const char* file_name,
				    int         AddFlag)
{
  file_node* trail=NULL;
  file_node* ptr;
  int        temp;

  ptr = file_root;
  while ((ptr != NULL) && ((temp=strncmp(file_name, ptr->file_name, 
					 MAX_CONDOR_FILENAME_LENGTH)) != 0))
    {
      trail = ptr;
      if (temp < 0)
	ptr = ptr->left;
      else
	ptr = ptr->right;
    }
  if (AddFlag == ADD)
    {
      if (ptr == NULL)
	{
	  ptr = new file_node;
	  if (ptr == NULL)
	    {
		  dprintf(D_ALWAYS, "ERROR: cannot allocate sufficient dynamic memory\n");
	      exit(DYNAMIC_ALLOCATION);
	    }
	  ptr->left = NULL;
	  ptr->right = NULL;
	  strncpy(ptr->file_name, file_name, MAX_CONDOR_FILENAME_LENGTH);
	  ptr->file_data = NULL;
	  if (trail == NULL)
	    file_root = ptr;
	  else if (temp < 0)
	    trail->left = ptr;
	  else
	    trail->right = ptr;
	}
    }
  return ptr;
}


int FileIndex::Exists(struct in_addr machine_IP,
                      const char*    owner_name,
                      const char*    file_name)
{
  machine_node* m;
  owner_node*   o;
  file_node*    f;

  m = FindOrAddMachine(machine_IP, 0);
  if (m == NULL)
    return DOES_NOT_EXIST;
  o = FindOrAddOwner(m, owner_name, 0);
  if (o == NULL)
    return DOES_NOT_EXIST;
  f = FindOrAddFile(o->file_root, file_name, 0);
  if (f == NULL)
    return DOES_NOT_EXIST;
  if (f->file_data == NULL)
    {
      dprintf(D_ALWAYS, "ERROR: IMDS index is inconsistent\n");
      exit(IMDS_INDEX_ERROR);
    }
  return EXISTS;
}


void FileIndex::DestroyFileTree(file_node*& file_root)
{
  if (file_root != NULL)
    {
      DestroyFileTree(file_root->left);
      DestroyFileTree(file_root->right);
      delete file_root;
      file_root = NULL;
    }
}


void FileIndex::DestroyOwnerTree(owner_node*& owner_root)
{
  if (owner_root != NULL)
    {
      DestroyOwnerTree(owner_root->left);
      DestroyOwnerTree(owner_root->right);
      DestroyFileTree(owner_root->file_root);
      delete owner_root;
      owner_root = NULL;
    }
}


void FileIndex::DestroyMachineTree(machine_node*& machine_root)
{
  if (machine_root != NULL)
    {
      DestroyMachineTree(machine_root->left);
      DestroyMachineTree(machine_root->right);
      DestroyOwnerTree(machine_root->owner_root);
      delete machine_root;
      machine_root = NULL;
    }
}


void FileIndex::DestroyIndex()
{
  int bucket;

  for (bucket=0; bucket<MAX_HASH_SIZE; bucket++)
    DestroyMachineTree(hash_table[bucket]);
}


int FileIndex::AddNewFile(struct in_addr  machine_IP,
			  const char*     owner_name,
			  const char*     file_name,
			  file_info_node* file_info_ptr)
{
  machine_node* m;
  owner_node*   o;
  file_node*    f;

  m = FindOrAddMachine(machine_IP, ADD);
  o = FindOrAddOwner(m, owner_name, ADD);
  f = FindOrAddFile(o->file_root, file_name, ADD);
  f->file_data = file_info_ptr;
  return CREATED;
}


file_node* FileIndex::GetFileNode(struct in_addr machine_IP,
				  const char*    owner_name,
				  const char*    file_name)
{
  machine_node* m;
  owner_node*   o;
  file_node*    f;

  m = FindOrAddMachine(machine_IP, 0);
  if (m == NULL)
    return NULL;
  o = FindOrAddOwner(m, owner_name, 0);
  if (o == NULL)
    return NULL;
  f = FindOrAddFile(o->file_root, file_name, 0);
  if (f == NULL)
    return NULL;
  else
    return f;
}


file_info_node* FileIndex::GetFileInfo(struct in_addr machine_IP,
				       const char*    owner_name,
				       const char*    file_name)
{
  file_node* f;

  f = GetFileNode(machine_IP, owner_name, file_name);
  if (f == NULL)
    return NULL;
  else
    return f->file_data;
}


int FileIndex::RenameFile(struct in_addr  machine_IP,
						  const char*     owner_name,
						  const char*     file_name,
						  const char*     new_file_name,
						  file_info_node* file_info_ptr)
{
	int A_code;
	
	if (Exists(machine_IP, owner_name, file_name) != EXISTS)
		return DOES_NOT_EXIST;
	A_code = AddNewFile(machine_IP, owner_name, new_file_name, file_info_ptr);
	if (A_code == EXISTS)
		return FILE_ALREADY_EXISTS;
	(void) DeleteFile(machine_IP, owner_name, file_name);
/*	return RENAMED; */
	return 0;
}


int FileIndex::DeleteFile(struct in_addr machine_IP,
						  const char*    owner_name,
						  const char*    file_name)
{
	machine_node* machine_trail=NULL;
	machine_node* machine_ptr;
	int           m_temp;
	machine_node* machine_rep_trail=NULL;
	machine_node* machine_rep_ptr;
	owner_node*   owner_trail=NULL;
	owner_node*   owner_ptr;
	int           o_temp;
	owner_node*   owner_rep_trail=NULL;
	owner_node*   owner_rep_ptr;
	file_node*    file_trail=NULL;
	file_node*    file_ptr;
	int           f_temp;
	file_node*    file_rep_trail=NULL;
	file_node*    file_rep_ptr;
	int           hash_bucket;
/*	int           retCode=REMOVED_FILE; */
	int           retCode=0;
	char          machine_name[MAX_MACHINE_NAME_LENGTH];
	char          pathname[MAX_PATHNAME_LENGTH];
	
	hash_bucket = Hash(machine_IP);
	machine_ptr = hash_table[hash_bucket];
	while ((machine_ptr != NULL) && 
		   (machine_ptr->machine_IP.s_addr != machine_IP.s_addr)) {
		machine_trail = machine_ptr;
		m_temp = (machine_IP.s_addr < machine_ptr->machine_IP.s_addr);
		if (m_temp)
			machine_ptr = machine_ptr->left;
		else
			machine_ptr = machine_ptr->right;
    }
	if (machine_ptr == NULL) {
		dprintf(D_ALWAYS, "ERROR: IMDS inconsistency; file exists but cannot be found\n");
		exit(IMDS_INDEX_ERROR);
    }
	owner_ptr = machine_ptr->owner_root;
	while ((owner_ptr != NULL) && ((o_temp=strncmp(owner_name, 
												   owner_ptr->owner_name, 
												   MAX_NAME_LENGTH)) != 0)) {
		owner_trail = owner_ptr;
		if (o_temp < 0)
			owner_ptr = owner_ptr->left;
		else
			owner_ptr = owner_ptr->right;
    }
	if (owner_ptr == NULL) {
		dprintf(D_ALWAYS, "ERROR: IMDS inconsistency; file exists but cannot be found\n");
		exit(IMDS_INDEX_ERROR);
    }
	file_ptr = owner_ptr->file_root;
	while ((file_ptr != NULL) && ((f_temp=strncmp(file_name, 
												  file_ptr->file_name, 
												  MAX_CONDOR_FILENAME_LENGTH)) 
								  != 0)) {
		file_trail = file_ptr;
		if (f_temp < 0)
			file_ptr = file_ptr->left;
		else
			file_ptr = file_ptr->right;
    }
	if (file_ptr == NULL) {
		dprintf(D_ALWAYS, "ERROR: IMDS inconsistency; file exists but cannot be found\n");
		exit(IMDS_INDEX_ERROR);
    }

	strcpy(machine_name, inet_ntoa(file_ptr->file_data->data.machine_IP));

	sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX, machine_name, 
			owner_name, file_name);
	dprintf(D_ALWAYS, "    Deleting file: %s\n", pathname);
	if (unlink(pathname)) {
		retCode = CANNOT_DELETE_FILE;
	}

	file_rep_ptr = file_ptr->left;
	if (file_rep_ptr == NULL) {
		if (file_trail == NULL)
			owner_ptr->file_root = file_ptr->right;
		else if (file_trail->left == file_ptr)
			file_trail->left = file_ptr->right;
		else
			file_trail->right = file_ptr->right;
    } else {
		while (file_rep_ptr->right != NULL) {
			file_rep_trail = file_rep_ptr;
			file_rep_ptr = file_rep_ptr->right;
		}
		if (file_rep_trail == NULL)
			file_rep_ptr->right = file_ptr->right;
		else {
			file_rep_trail->right = file_rep_ptr->left;
			file_rep_ptr->left = file_ptr->left;
			file_rep_ptr->right = file_ptr->right;
		}
		if (file_trail == NULL)
			owner_ptr->file_root = file_rep_ptr;
		else if (file_trail->left == file_ptr)
			file_trail->left = file_rep_ptr;
		else
			file_trail->right = file_rep_ptr;
    }
	delete file_ptr;
	if (owner_ptr->file_root == NULL) {
		owner_rep_ptr = owner_ptr->left;
		if (owner_rep_ptr == NULL) {
			if (owner_trail == NULL)
				machine_ptr->owner_root = owner_ptr->right;
			else if (owner_trail->left == owner_ptr)
				owner_trail->left = owner_ptr->right;
			else
				owner_trail->right = owner_ptr->right;
		}
		else {
			while (owner_rep_ptr->right != NULL) {
				owner_rep_trail = owner_rep_ptr;
				owner_rep_ptr = owner_rep_ptr->right;
			}
			if (owner_rep_trail == NULL)
				owner_rep_ptr->right = owner_ptr->right;
			else {
				owner_rep_trail->right = owner_rep_ptr->left;
				owner_rep_ptr->left = owner_ptr->left;
				owner_rep_ptr->right = owner_ptr->right;
			}
			if (owner_trail == NULL)
				machine_ptr->owner_root = owner_rep_ptr;
			else if (owner_trail->left == owner_ptr)
				owner_trail->left = owner_rep_ptr;
			else
				owner_trail->right = owner_rep_ptr;
		}
		delete owner_ptr;
		if (machine_ptr->owner_root == NULL) {
			sprintf(pathname, "%s%s/%s", LOCAL_DRIVE_PREFIX, machine_name, 
					owner_name);
			if (rmdir(pathname) != 0)
/*				retCode = CANNOT_DELETE_DIRECTORY; */ ;
			machine_rep_ptr = machine_ptr->left;
			if (machine_rep_ptr == NULL) {
				if (machine_trail == NULL)
					hash_table[hash_bucket] = machine_ptr->right;
				else if (machine_trail->left == machine_ptr)
					machine_trail->left = machine_ptr->right;
				else
					machine_trail->right = machine_ptr->right;
			} else {
				while (machine_rep_ptr->right != NULL) {
					machine_rep_trail = machine_rep_ptr;
					machine_rep_ptr = machine_rep_ptr->right;
				}
				if (machine_rep_trail == NULL)
					machine_rep_ptr->right = machine_ptr->right;
				else {
					machine_rep_trail->right = machine_rep_ptr->left;
					machine_rep_ptr->left = machine_ptr->left;
					machine_rep_ptr->right = machine_ptr->right;
				}
				if (machine_trail == NULL)
					hash_table[hash_bucket] = machine_rep_ptr;
				else if (machine_trail->left == machine_ptr)
					machine_trail->left = machine_rep_ptr;
				else
					machine_trail->right = machine_rep_ptr;
			}
			delete machine_ptr;
			if ((hash_table[hash_bucket] == NULL) && 
				(retCode != CANNOT_DELETE_DIRECTORY)) {
				sprintf(pathname, "%s%s", LOCAL_DRIVE_PREFIX, machine_name);
				if (rmdir(pathname) != 0)
/*					retCode = CANNOT_DELETE_DIRECTORY; */ ;
			}
		}
    }
	return retCode;
}


int FileIndex::RemoveFile(struct in_addr machine_IP,
			  const char*    owner_name,
			  const char*    file_name)
{
  if (Exists(machine_IP, owner_name, file_name) != EXISTS)
    return DOES_NOT_EXIST;
  return DeleteFile(machine_IP, owner_name, file_name);
}


void FileIndex::FIDump(file_node* f_ptr)
{
  if (f_ptr != NULL)
    {
      FIDump(f_ptr->left);
      cout << "\t\t\tFile Name: " << f_ptr->file_data->data.file_name << endl;
      cout << "\t\t\tFile Size: " << f_ptr->file_data->data.size << endl;
      cout << "\t\t\tStatus:    ";
      switch (f_ptr->file_data->data.state)
	{
	case NOT_PRESENT: 
	  cout << "not present on server" << endl;
	  break;
	case ON_SERVER:
	  cout << "on checkpoint server" << endl;
	  break;
	case LOADING:
	  cout << "loading" << endl;
	  break;
	case XMITTING:
	  cout << "transmitting" << endl;
	  break;
	default:
	  cout << "unknown" << endl;
	}
      cout << endl;
      FIDump(f_ptr->right);
    }
}


void FileIndex::OIDump(owner_node* o_ptr)
{
  if (o_ptr != NULL)
    {
      OIDump(o_ptr->left);
      cout << "\t\tOwner: " << o_ptr->owner_name << endl;
      FIDump(o_ptr->file_root);
      OIDump(o_ptr->right);
    }
}



void FileIndex::MIDump(machine_node* m_ptr)
{
  if (m_ptr != NULL)
    {
      MIDump(m_ptr->left);
      cout << "\tMachine: " << inet_ntoa(m_ptr->machine_IP) << endl;
      OIDump(m_ptr->owner_root);
      MIDump(m_ptr->right);
    }
}


void FileIndex::IndexDump()
{
  int count;

  MakeLine('=', '-');
  cout << "Start of File Index Dump" << endl;
  MakeLine('-', '-');
  for (count=0; count<MAX_HASH_SIZE; count++)
    if (hash_table[count] != NULL)
      {
	cout << endl << "*** Bucket #" << count << ": ***" << endl;
	MIDump(hash_table[count]);
      }
  MakeLine('-', '-');
  cout << "End of File Index Dump" << endl;
  MakeLine('=', '-');
  cout << endl << endl;
}









