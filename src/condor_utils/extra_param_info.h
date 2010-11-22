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


#ifndef EXTRA_PARAM_INFO_H
#define EXTRA_PARAM_INFO_H

#include "HashTable.h"
#include "MyString.h"

class ExtraParamInfo
{
 public:
	enum ParamSource
	{
		None,
		File,
		Environment,
		Internal
	};

	ExtraParamInfo();
	~ExtraParamInfo();

	void SetInfo(const char *filename, int line_number);
	void SetInfo(ParamSource source);
	void GetInfo(ParamSource &source, const char *&filename, int &line_number);
 private:
	ParamSource  _source;
	char         *_filename;
	int          _line_number;
};

class ExtraParamTable
{
 public:
	ExtraParamTable();
	virtual ~ExtraParamTable();
	virtual void AddFileParam(const char *parameter, const char *filename, 
					  int line_number);
	virtual void AddInternalParam(const char *parameter);
	virtual void AddEnvironmentParam(const char *parameter);
	virtual bool GetParam(const char *parameter, 
				  MyString &filename, int &line_number);
 private:
	HashTable<MyString, ExtraParamInfo *> *table;

	void ClearOldParam(MyString &parameter);
};

class DummyExtraParamTable : public ExtraParamTable
{
 public:
	DummyExtraParamTable() {}
	virtual ~DummyExtraParamTable() {}
	virtual void AddFileParam(const char * /*parameter*/ , const char *  /*filename*/, 
					  int  /*line_number*/ ) {}
	virtual void AddInternalParam(const char *  /*parameter*/ ) {}
	virtual void AddEnvironmentParam(const char *  /*parameter*/ ) {}
	virtual bool GetParam(const char *  /*parameter*/ , 
				  MyString &filename, int &line_number) {line_number=-1;filename="unknown";return true;}
};

#endif /* EXTRA_PARAM_INFO_H */
