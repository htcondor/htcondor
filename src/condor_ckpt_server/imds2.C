#include <time.h>
#include <iostream.h>
#include <iomanip.h>
#include <fstream.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "imds2.h"
#include "fileinfo2.h"
#include "fileindex2.h"



u_lint IMDS::GetNumFiles()
{
  return FileStats.GetNumFiles();
}


int IMDS::LockStatus(struct in_addr machine_IP,
		     const char*    owner_name,
		     const char*    file_name)
{
  return Index.LockStatus(machine_IP, owner_name, file_name);
}


void IMDS::Lock(struct in_addr machine_IP,
		const char*    owner_name,
		const char*    file_name,
		file_state     state)
{
  file_node*      file_ptr;
  file_info_node* file_data;

  file_ptr = Index.GetFileNode(machine_IP, owner_name, file_name);
  if (file_ptr != NULL)
    {
      file_data = file_ptr->file_data;
      file_data->lock = EXCLUSIVE_LOCK;
      file_data->data.state = state;
    }
}


void IMDS::Unlock(struct in_addr machine_IP,
		  const char*    owner_name,
		  const char*    file_name)
{
  file_node*      file_ptr;
  file_info_node* file_data;

  file_ptr = Index.GetFileNode(machine_IP, owner_name, file_name);
  if (file_ptr != NULL)
    {
      file_data = file_ptr->file_data;
      file_data->lock = UNLOCKED;
      file_data->data.state = ON_SERVER;
    }
}


int IMDS::AddFile(struct in_addr machine_IP,
		  const char*    owner_name,
		  const char*    file_name,
		  u_lint         file_size,
		  file_state     state)
{
  file_node*      file_ptr;
  file_info_node* file_data;
  char            pathname[MAX_PATHNAME_LENGTH];

  file_ptr = Index.GetFileNode(machine_IP, owner_name, file_name);
  if (file_ptr != NULL)
    {
      file_data = file_ptr->file_data;
      if (file_data->lock != UNLOCKED)
	return FILE_LOCKED;
      sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX, 
	      file_data->data.machine_IP_name, owner_name, file_name);
      if ((state == NOT_PRESENT) || (state == LOADING))
	remove(pathname);                        // Do not care if fails
      file_data->data.size = file_size;
      file_data->data.last_modified_time = time(NULL);
      file_data->data.state = state;
      file_data->lock = EXCLUSIVE_LOCK;
    }
  else
    {
      file_data = FileStats.AddFileInfo(machine_IP, owner_name, file_name, 
					file_size, state, NULL);
      (void) Index.AddNewFile(machine_IP, owner_name, file_name, file_data);
    }
  return CREATED;
}


int IMDS::RenameFile(struct in_addr machine_IP,
		     const char*    owner_name,
		     const char*    file_name,
		     const char*    new_file_name)
{
  file_node*      old_fn;
  file_node*      new_fn;
  file_info_node* old_file_ptr;
  file_info_node* new_file_ptr;
  char            new_pathname[MAX_PATHNAME_LENGTH];
  char            old_pathname[MAX_PATHNAME_LENGTH];

  old_fn = Index.GetFileNode(machine_IP, owner_name, file_name);
  if (old_fn == NULL)
    return DOES_NOT_EXIST;
  old_file_ptr = old_fn->file_data;
  if (old_file_ptr->lock != UNLOCKED)
    return FILE_LOCKED;
  sprintf(old_pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX, 
	  old_file_ptr->data.machine_IP_name, owner_name, file_name);
  sprintf(new_pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX, 
	  old_file_ptr->data.machine_IP_name, owner_name, new_file_name);
  new_fn = Index.GetFileNode(machine_IP, owner_name, new_file_name);
  if (new_fn == NULL)
    {
      if (rename(old_pathname, new_pathname) != 0)
	return CANNOT_RENAME_FILE;
      strncpy(old_file_ptr->data.machine_IP_name, new_file_name, 
	      MAX_CONDOR_FILENAME_LENGTH);
      old_file_ptr->data.last_modified_time = time(NULL);
      (void) Index.AddNewFile(machine_IP, owner_name, new_file_name, 
			      old_file_ptr);
    }
  else
    {
      new_file_ptr = new_fn->file_data;
      if (new_file_ptr->lock != UNLOCKED)
	return FILE_LOCKED;
      if (rename(old_pathname, new_pathname) != 0)
	return CANNOT_RENAME_FILE;
      new_file_ptr->data.size = old_file_ptr->data.size;
      new_file_ptr->data.state = old_file_ptr->data.state;
      new_file_ptr->data.last_modified_time = time(NULL);
      (void) FileStats.RemoveFileInfo(old_file_ptr);
    }
  (void) Index.DeleteFile(machine_IP, owner_name, file_name);
  return RENAMED;
}


u_short IMDS::RemoveFile(struct in_addr machine_IP,
			 const char*    owner_name,
			 const char*    file_name)
{
  file_node* file_ptr;
  int        d_ret_code;
  int        i_ret_code;

  file_ptr = Index.GetFileNode(machine_IP, owner_name, file_name);
  if (file_ptr == NULL)
    return DOES_NOT_EXIST;
  d_ret_code = FileStats.RemoveFileInfo(file_ptr->file_data);
  if (d_ret_code != REMOVED_FILE)
    return (u_short) d_ret_code;
  i_ret_code = Index.DeleteFile(machine_IP, owner_name, file_name);
  return (u_short) i_ret_code;
}


void IMDS::TransferFileInfo(int socket_desc)
{
  FileStats.TransferFileInfo(socket_desc);
}


