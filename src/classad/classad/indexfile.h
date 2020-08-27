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

#ifndef __CLASSAD_INDEXFILE_H__
#define __CLASSAD_INDEXFILE_H__

#include <fcntl.h>
#include <stdio.h>
#include <string>
#include <map>
#include "classad/classad_containers.h"
#include <sys/types.h>

namespace classad {

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
	bool FindInFile(std::string key,tag &offset);
	/** the cache in mem is full, so we write one classad back to file
		and wri
		@param s_id  the pointer to the ID
	*/
	bool UpdateIndex(std::string key);
	bool WriteBack(std::string key, std::string ad);  
	//should delete it from file and index
	bool DeleteFromStorageFile(std::string key);
	bool UpdateIndex(std::string key, int offset);
	int First(std::string &key);
	int Next(std::string &key);
	std::string GetClassadFromFile(std::string key, int offset) const;
	bool  TruncateStorageFile();
	int  dump_index();
 private:
	typedef classad_map<std::string,int> index_type;
	typedef classad_map<std::string,int>::iterator index_itr_type;
	index_type Index;
	index_itr_type index_itr;
	int filed;
};

}

#endif //__CLASSAD_INDEXFILE_H__
