/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#include <fcntl.h>
#include <stdio.h>
#include <string>
#include <map>
#if (__GNUC__<3)
#include <hash_map>
#else
#include <ext/hash_map>
#endif
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
	std::hash_map<std::string,int,StringHash> Index;
	std::hash_map<std::string,int,StringHash>::iterator index_itr;
	int filed;
};

END_NAMESPACE
