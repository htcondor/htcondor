/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 *********************************************************************/

#include <fcntl.h>
#include <stdio.h>
#include <string>
#include <map>
#include "classad_stl.h"
#include <sys/types.h>

BEGIN_NAMESPACE( classad )

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
	std::string GetClassadFromFile(std::string key, int offset);
	bool  TruncateStorageFile();
	int  dump_index();
 private:
	classad_hash_map<std::string,int,StringHash> Index;
	classad_hash_map<std::string,int,StringHash>::iterator index_itr;
	int filed;
};

END_NAMESPACE
