#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <string>
#include <map>
#include <hash_map>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"
BEGIN_NAMESPACE( classad );

typedef struct{
  int offset;
} tag; 

struct eqstr
{
  bool operator()(const char* s1, const char* s2) const
  {
    return strcmp(s1, s2) == 0;
  }
};

class IndexFile {
 public:
  void Init(int file_handler);
  bool FindInFile(string key,tag &offset);
  /** the cache in mem is full, so we write one classad back to file
      and wri
      @param s_id  the pointer to the ID
  */
  bool UpdateIndex(string key);
  bool WriteBack(string key, string ad);  
  //should delete it from file and index
  bool DeleteFromStorageFile(string key);
  bool UpdateIndex(string key, int offset);
  int First(string &key);
  int Next(string &key);
  string GetClassadFromFile(string key, int offset);
  bool  TruncateStorageFile();
  int  dump_index();
 private:
  hash_map<string,int,StringHash> Index;
  hash_map<string,int,StringHash>::iterator index_itr;
  int filed;
};

END_NAMESPACE






