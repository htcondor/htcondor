#include "fileinfo2.h"
#include "network2.h"
#include "gen_lib.h"
#include <iostream.h>
#include <iomanip.h>
#include <fstream.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>


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
  if (num_files != 0)
    {
      cerr << endl << "WARNING:" << endl;
      cerr << "WARNING:" << endl;
      cerr << "WARNING: upon deletion, incorrect number of files maintained"
           << endl;
      cerr << "WARNING:" << endl;
      cerr << "WARNING:" << endl;
    }
}


int FileInformation::RemoveFileInfo(file_info_node* d_ptr)
{

  if (d_ptr->lock != UNLOCKED)
    return FILE_LOCKED;
  if (d_ptr != NULL)
    {
      if ((head == d_ptr) && (tail == d_ptr))
        {
          head = NULL;
          tail = NULL;
        }
      else if (head == d_ptr)
        {
          head = head->next;
          head->prev = NULL;
        }
      else if (tail == d_ptr)
        {
          tail = tail->prev;
          tail->next = NULL;
        }
      else
        {
          d_ptr->prev->next = d_ptr->next;
          d_ptr->next->prev = d_ptr->prev;
        }
      delete d_ptr;
      num_files--;
    }
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
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: unable to allocate memory for the file information" 
	   << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
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
  n->lock = EXCLUSIVE_LOCK;
  return n;
}


int FileInformation::RenameFileInfo(file_info_node* r_ptr,
                                    const char*     new_file_name)
{
	if (r_ptr != NULL)
		if (r_ptr->lock != UNLOCKED)
			return FILE_LOCKED;
		else
			strcpy(r_ptr->data.file_name, new_file_name);
/*	return RENAMED; */
	return OK;
}


void FileInformation::PrintFileInfo()
{
  const char ch1='=';
  const char ch2='-';

  file_info_node* p;
  int             count=0;

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
      cerr << endl << "WARNING:" << endl;
      cerr << "WARNING:" << endl;
      cerr << "WARNING: incorrect count for the number of files" << endl;
      cerr << "WARNING:" << endl;
      cerr << "WARNING:" << endl;
    }
}

/*
                                           Submitting
      File Name                              Machine         Owner Name           File Size     Time Last Modified (local)
      XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX     NNN.NNN.NNN.NNN     XXXXXXXXXXXXXXX     NNNNNNNNNN      XXXXXXXXXXXXXXXXXXXXXXXXX

*/


void FileInformation::TransferFileInfo(int socket_desc)
{
  int              max_buf;
  file_state_info* buffer;
  int              buf_count;
  file_info_node*  ptr;
  int              temp_len;

  if (num_files == 0)
    exit(CHILDTERM_NO_FILE_STATUS);
  max_buf = (DATA_BUFFER_SIZE/sizeof(file_state_info)) * 
                          sizeof(file_state_info);
  if (max_buf < num_files)
    max_buf = num_files;
  buffer = new file_state_info[max_buf];
  if (buffer == NULL)
    exit(DYNAMIC_ALLOCATION);
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
      temp_len = net_write(socket_desc, (char*) buffer, 
			   sizeof(file_state_info) * buf_count);
      if (temp_len != (sizeof(file_state_info) * buf_count))
	exit(CHILDTERM_ERROR_ON_STATUS_WRITE);
    }
  delete [] buffer;
}


void FileInformation::WriteFileInfoToDisk()
{
  ofstream        outfile(TEMP_FILE_INFO_FILENAME);
  file_info_node* fin_ptr;
  int             count=0;
  char            pathname1[MAX_PATHNAME_LENGTH];
  char            pathname2[MAX_PATHNAME_LENGTH];

  if (outfile.fail())
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: unable to write in-memory data structure to file" 
	   << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
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
        cerr << endl << "ERROR:" << endl;
        cerr << "ERROR:" << endl;
        cerr << "ERROR: cannot access IMDS file" << endl;
        cerr << "ERROR:" << endl;
        cerr << "ERROR:" << endl;
        exit(CANNOT_DELETE_IMDS_FILE);
      }
  errno = 0;
  if (rename(pathname1, pathname2) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: unable to rename IMDS file" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "errno = " << errno << endl;
      exit(CANNOT_RENAME_IMDS_FILE);
    }
  if (count != num_files)
    {
      cerr << endl << "WARNING:" << endl;
      cerr << "WARNING:" << endl;
      cerr << "WARNING: incorrect count for the number of files" << endl;
      cerr << "WARNING:" << endl;
      cerr << "WARNING:" << endl;
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


