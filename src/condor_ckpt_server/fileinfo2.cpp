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
#include "fileinfo2.h"
#include "network2.h"
#include "gen_lib.h"
#include <iostream>
#include <fstream>
#include <iomanip>
using namespace std;

FileInformation::FileInformation()
{
  head = NULL;
  tail = NULL;
  num_files = 0;
}


FileInformation::~FileInformation()
{
  DeleteFileInfo();
}


void FileInformation::DeleteFileInfo()
{
  file_info_node* p;
  file_info_node* trail;

  p = head;
  while (p != NULL)
    {
      trail = p;
      p = p->next;
      delete trail;
      num_files--;
    }
  head = NULL;
  tail = NULL;
}


int FileInformation::RemoveFileInfo(file_info_node* d_ptr)
{
	if (d_ptr) {
		if ((head == d_ptr) && (tail == d_ptr)) {
			head = NULL;
			tail = NULL;
        } else if (head == d_ptr) {
			head = head->next;
			head->prev = NULL;
        } else if (tail == d_ptr) {
			tail = tail->prev;
			tail->next = NULL;
        } else {
			d_ptr->prev->next = d_ptr->next;
			d_ptr->next->prev = d_ptr->prev;
        }
		delete d_ptr;
    }
	num_files--;
	return (REMOVED_FILE);
}


file_info_node* FileInformation::AddFileInfo(struct in_addr machine_IP,
					     const char*    owner_name,
					     const char*    file_name,
					     u_lint         file_size,
					     file_state     state,
					     time_t*        last_modified_ptr)
{
  file_info_node* n;
  char*           temp_name;

  n = new file_info_node;
  if (n == NULL)
    {
	  dprintf(D_ALWAYS, 
			  "ERROR: unable to allocate memory for the file information\n");
      exit(DYNAMIC_ALLOCATION);
    }
  memcpy((char*) &n->data.machine_IP, &machine_IP, sizeof(struct in_addr));
  temp_name = GetIPName(machine_IP);
  strncpy(n->data.machine_IP_name, temp_name, MAX_MACHINE_NAME_LENGTH);
  strncpy(n->data.owner_name, owner_name, MAX_NAME_LENGTH);
  strncpy(n->data.file_name, file_name, MAX_CONDOR_FILENAME_LENGTH);
  n->data.size = file_size;
  n->data.last_modified_time = time(NULL);
  n->data.state = state;
  n->next = NULL;
  if (head == NULL)
    {
      head = n;
      tail = n;
      n->prev = NULL;
    }
  else
    {
      tail->next = n;
      n->prev = tail;
      tail = n;
    }
  num_files++;
  return n;
}


int FileInformation::RenameFileInfo(file_info_node* r_ptr,
                                    const char*     new_file_name)
{
	if (r_ptr != NULL)
		strcpy(r_ptr->data.file_name, new_file_name);
	return CKPT_OK;
}


void FileInformation::PrintFileInfo()
{
  const char ch1='=';
  const char ch2='-';

  file_info_node* p;
  u_lint             count=0;

  cout << setw(53) << "Submitting" << endl;
  cout << setw(15) << "File Name" << setw(37) << "Machine" << setw(19)
       << "Owner Name" << setw(20) << "File Size" << setw(5) << " "
       << "Time Last Modified (local)" << endl;
  MakeLongLine(ch1, ch2);
  p = head;
  while (p != NULL)
    {
      cout << setw(6) << " " << setw(STATUS_FILENAME_LENGTH-1) 
	   << setiosflags(ios::left) << p->data.file_name 
	   << setw(5) << " "
	   << setw(STATUS_MACHINE_NAME_LENGTH-1) << resetiosflags(ios::right)
           << p->data.machine_IP_name << setw(5) << " "
	   << setw(STATUS_OWNER_NAME_LENGTH-1) << setiosflags(ios::left) 
	   << p->data.owner_name << resetiosflags(ios::left) 
	   << setw(15) << p->data.size << setw(6) << " " 
	   << ctime(&p->data.last_modified_time) << endl;
      p = p->next;
      count++;
    }
  MakeLongLine(ch1, ch2);
  if (count != num_files)
    {
	  dprintf(D_ALWAYS, "WARNING incorrect count for the number of files "
			  "(count = %lu, num = %lu)\n", 
			  (unsigned long) count, (unsigned long) num_files);
    }
}

/*
                                           Submitting
      File Name                              Machine         Owner Name           File Size     Time Last Modified (local)
      XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX     NNN.NNN.NNN.NNN     XXXXXXXXXXXXXXX     NNNNNNNNNN      XXXXXXXXXXXXXXXXXXXXXXXXX

*/


void FileInformation::TransferFileInfo(int socket_desc)
{
  u_lint           max_buf;
  file_state_info* buffer;
  u_lint           buf_count;
  file_info_node*  ptr;
  u_lint           temp_len;

  if (num_files == 0) {
	dprintf(D_ALWAYS, "ERROR: num_files is zero.\n");
    exit(CHILDTERM_NO_FILE_STATUS);
  }
  max_buf = (DATA_BUFFER_SIZE/sizeof(file_state_info)) * 
                          sizeof(file_state_info);
  if (max_buf < num_files)
    max_buf = num_files;
  buffer = new file_state_info[max_buf];
  if (buffer == NULL) {
	dprintf(D_ALWAYS, "ERROR: Couldn't allocate file_state_info.\n");
    exit(DYNAMIC_ALLOCATION);
  }
  ptr = head;
  while (ptr != NULL)
    {
      buf_count = 0;
      while ((ptr != NULL) && (buf_count < max_buf))
	{
	  strncpy(buffer[buf_count].file_name, ptr->data.file_name, 
		  STATUS_FILENAME_LENGTH);
	  strncpy(buffer[buf_count].owner_name, ptr->data.owner_name,
		  STATUS_OWNER_NAME_LENGTH);
	  strncpy(buffer[buf_count].machine_IP_name, ptr->data.machine_IP_name,
		  STATUS_MACHINE_NAME_LENGTH);
	  buffer[buf_count].size = htonl(ptr->data.size);
	  buffer[buf_count].last_modified_time = 
	                     htonl((u_lint) ptr->data.last_modified_time);
	  buffer[buf_count].state = htons((u_short) ptr->data.state);
	  buf_count++;
	  ptr = ptr->next;
	}

	/* this code path appears never to be taken so I'm not doing the 32/64
		bit backwards compatibility work for this structure being written
		onto the socket. Why? Cause this is all a dirty hack. However, if
		we do take this code path, we can dump out some information just
		in case so the admin knows to send mail to condor-admin about it. 
		The way to fix it is to propogate the FDContext down from SendStatus
		to here and update protocol.cpp with the right send/recv functions. */
	 dprintf(D_ALWAYS, "FileInformation::TransferFileInfo(): Calling "
	 					"net_write() on a file_state_info structure. "
						"This functionality needs 32/64 bit work!\n");

      temp_len = net_write(socket_desc, (char*) buffer, 
			   sizeof(file_state_info) * buf_count);
      if (temp_len != (sizeof(file_state_info) * buf_count)) {
		  dprintf(D_ALWAYS, "ERROR: Write failed, line %d\n", __LINE__);
		  exit(CHILDTERM_ERROR_ON_STATUS_WRITE);
	  }
    }
  delete [] buffer;
}


void FileInformation::WriteFileInfoToDisk()
{
  ofstream        outfile(TEMP_FILE_INFO_FILENAME);
  file_info_node* fin_ptr;
  u_lint          count=0;
  char            pathname1[MAX_PATHNAME_LENGTH];
  char            pathname2[MAX_PATHNAME_LENGTH];

  if (outfile.fail())
    {
      dprintf(D_ALWAYS, 
			  "ERROR: unable to write in-memory data structure to file\n");
      exit(CANNOT_WRITE_IMDS_FILE);
    }
  fin_ptr = head;
  while (fin_ptr != NULL)
    {
      if ((fin_ptr->data.state == ON_SERVER) || 
	  (fin_ptr->data.state == XMITTING))
	outfile << fin_ptr->data.machine_IP.s_addr << endl
	        << fin_ptr->data.owner_name << endl
		<< fin_ptr->data.file_name << endl
                << fin_ptr->data.size << endl
                << fin_ptr->data.last_modified_time << endl << endl;
      fin_ptr = fin_ptr->next;
      count++;
    }
  outfile.close();
  sprintf(pathname1, "%s%s", LOCAL_DRIVE_PREFIX, FILE_INFO_FILENAME);
  sprintf(pathname2, "%s%s", LOCAL_DRIVE_PREFIX, TEMP_FILE_INFO_FILENAME);
  errno = 0;
  if (remove(pathname1) == -1)
    if (errno == EACCES)
      {
		dprintf(D_ALWAYS, "ERROR: cannot access IMDS file\n");
        exit(CANNOT_DELETE_IMDS_FILE);
      }
  errno = 0;
  if (rename(pathname1, pathname2) != 0)
    {
      dprintf(D_ALWAYS, "ERROR: unable to rename IMDS file\n");
      exit(CANNOT_RENAME_IMDS_FILE);
    }
  if (count != num_files)
    {
      dprintf(D_ALWAYS, "WARNING: incorrect count for the number of files\n");
    }
}


void FileInformation::LoadFileInfoFromDisk()
{
  ifstream       infile(FILE_INFO_FILENAME);
  struct in_addr machine_IP;
  char           owner_name[MAX_NAME_LENGTH];
  char           file_name[MAX_CONDOR_FILENAME_LENGTH];
  u_lint         size;
  time_t         last_modified_time;

  if (!infile.fail())
    {
      if (head != NULL)
        DeleteFileInfo();
      infile >> machine_IP.s_addr;
      while (!infile.eof())
        {
          infile >> owner_name >> file_name >> size >> last_modified_time;
          AddFileInfo(machine_IP, owner_name, file_name, size, UNKNOWN,
                      &last_modified_time);
          infile >> machine_IP.s_addr;
        }
      infile.close();
    }
}


