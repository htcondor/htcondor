#ifndef FILEINDEX2_H
#define FILEINDEX2_H


#include "constants2.h"
#include "typedefs2.h"
#include "network2.h"


const int MAX_HASH_SIZE = 100;


extern "C" int mkdir();
extern "C" int rmdir();


typedef struct file_node
{
  char            file_name[MAX_CONDOR_FILENAME_LENGTH];
  file_info_node* file_data;
  file_node*      left;
  file_node*      right;
} file_node;


typedef struct owner_node
{
  char        owner_name[MAX_NAME_LENGTH];
  file_node*  file_root;
  owner_node* left;
  owner_node* right;
} owner_node;


typedef struct machine_node
{
  struct in_addr machine_IP;
  owner_node*    owner_root;
  machine_node*  left;
  machine_node*  right;
} machine_node;


class FileIndex
{
  private:
    double        capacity_used;
    machine_node* hash_table[MAX_HASH_SIZE];
    int Hash(struct in_addr machine_IP);
    machine_node* FindOrAddMachine(struct in_addr machine_IP,
                                   int            AddFlag);
    owner_node* FindOrAddOwner(machine_node* m,
                               const char*   owner_name,
                               int           AddFlag);
    file_node* FindOrAddFile(file_node*& file_root,
			     const char* file_name,
			     int         AddFlag);
    void DestroyFileTree(file_node*& file_root);
    void DestroyOwnerTree(owner_node*& owner_root);
    void DestroyMachineTree(machine_node*& machine_root);
    void DestroyIndex();
    void MIDump(machine_node* m_ptr);
    void OIDump(owner_node* o_ptr);
    void FIDump(file_node* f_ptr);

  public:
    FileIndex();
    ~FileIndex();
    int Exists(struct in_addr machine_IP,
               const char*    owner_name,
               const char*    file_name);
    int LockStatus(struct in_addr machine_IP,
		   const char*    owner_name,
		   const char*    file_name);
    int AddNewFile(struct in_addr  machine_IP,
		   const char*     owner_name,
		   const char*     file_name,
		   file_info_node* file_info_ptr);
    file_node* GetFileNode(struct in_addr machine_IP,
			   const char*    owner_name,
			   const char*    file_name);
    file_info_node* GetFileInfo(struct in_addr machine_IP,
				const char*    owner_name,
				const char*    file_name);
    int RenameFile(struct in_addr  machine_IP,
		   const char*     owner_name,
		   const char*     file_name,
		   const char*     new_file_name,
		   file_info_node* file_info_ptr);
    int DeleteFile(struct in_addr machine_IP,
		   const char*    owner_name,
		   const char*    file_name);
    int RemoveFile(struct in_addr machine_IP,
		   const char*    owner_name,
		   const char*    file_name);
    void IndexDump();
};


#endif








