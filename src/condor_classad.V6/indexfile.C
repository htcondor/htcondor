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

#include "common.h"
#include "indexfile.h"
#include <iostream>

BEGIN_NAMESPACE( classad );

int IndexFile::
dump_index()
{
   	hash_map<string,int,StringHash>::iterator m=Index.begin();
	cout << "in dump index the lenght= " << Index.size() << endl;
	while (m!=Index.end()){
		cout << "dump index  key= " << m->first << "  offset=" << m->second << endl;
		m++;   
	};
	return 1;
}

bool IndexFile::
TruncateStorageFile()
{
	int cur_set;
	int new_filed;
	char* filename="temp_file";
	char* logfilename="storagefile";
	hash_map<string,int,StringHash>::iterator ptr;
	if( ( new_filed = open(filename, O_RDWR | O_CREAT | O_APPEND, 0600 )) < 0 ) {
		CondorErrno = ERR_CACHE_FILE_ERROR;
		CondorErrMsg = "internal error:  unable to create the temp file in truncating storagefile";
		return( false );
	};
	
	for (ptr=Index.begin();ptr!=Index.end();ptr++){
		
		lseek(filed,ptr->second,SEEK_SET);
		char  k[1];
		string m;
		int l;
		
		while ((l=read(filed,k,1))>0){
			string n(k,1);
			if (n=="\n"){
				break; 
			} else {
				m=m+n;
			}           
		}
		m=m+'\n';
		if (m[0]!='*'){
			if (write(new_filed,(void *)(m.c_str()),m.size())<0){
				return false;
			} else {
				fsync(filed);
				ptr->second=cur_set; 
				cur_set+=m.size();
			}
		}
	}
	
	fsync(new_filed);
	if( rename(filename, logfilename) < 0 ) {
		CondorErrno = ERR_CACHE_FILE_ERROR;
		char buf[10];
		sprintf( buf, "%d", errno );
		CondorErrMsg = "failed to truncate storagefile: rename(" 
			+ string(filename) + " , " 
			+ string(logfilename) +", errno=" 
			+ string(buf);    
		return( false );
	}
	return true;
}


int IndexFile:: 
First(string &key)
{
	index_itr=Index.begin();
	if (index_itr != Index.end()){
		key=index_itr->first;
		return index_itr->second;
	} else {
		return -1;
	} 
}

int IndexFile:: 
Next(string &key)
{
	index_itr++;
	if (index_itr != Index.end()){
		key=index_itr->first;
		return index_itr->second;
	} else {
		return -1;
	} 
}

string IndexFile::
GetClassadFromFile(string key, int offset)
{
	if (filed != 0){
		int curset;
		curset = lseek(filed,offset,SEEK_SET);
		char  k[1];
		string m;
		int l;
		
		while ((l=read(filed,k,1))>0){
			string n(k,1);
			if (n=="\n"){
				break; 
			} else {
				m=m+n;
			}           
		}
		
		if (m[0] != '*'){
			return m;
		} else {
			return "";
		}
	}else{
	        return "";
	}
}

bool IndexFile::
UpdateIndex(string key, int offset)
{
	Index[key]=offset;
	//cout << Index[key] << endl;
	return true;
}

void IndexFile::
Init(int file_handler)
{
	filed=file_handler;
	Index.clear();
}

bool IndexFile::
WriteBack(string key, string ad)
{
	DeleteFromStorageFile(key);
	int k=lseek(filed,0,SEEK_END);
	Index[key]=k;
	ad=ad+"\n";
	if (write(filed,(void *)(ad.c_str()),ad.size())<0){
		return false;
	} else {
		fsync(filed);
		return true;
	};
    
}

bool IndexFile::
FindInFile(string key,tag &ptr)
{
	hash_map<string,int,StringHash>::iterator m=Index.find(key);
	if (m!=Index.end()){
		ptr.offset=m->second;
		return true; 
	} else {
		return false;
	}
}

bool IndexFile::
DeleteFromStorageFile(string key)
{
	hash_map<string,int,StringHash>::iterator m=Index.find(key);
	if (m!=Index.end()){
		int offset=m->second; 
		lseek(filed,offset,SEEK_SET);
		char  k[1];
		string m;
		int l;
		
		while ((l=read(filed,k,1))>0){
			string n(k,1);
			if (n=="\n"){
				break; 
			} else {
				m=m+n;
			}           
		}
		
		m[0]='*';
		m=m+'\n';
		lseek(filed,offset,SEEK_SET);
		write(filed,(void *)(m.c_str()),m.size());
		fsync(filed);
		Index.erase(key);
		return true;
	} else {
		return false;
	}
}

END_NAMESPACE



