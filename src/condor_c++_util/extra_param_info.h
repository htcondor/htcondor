/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
	~ExtraParamTable();
	void AddFileParam(const char *parameter, const char *filename, 
					  int line_number);
	void AddInternalParam(const char *parameter);
	void AddEnvironmentParam(const char *parameter);
	bool GetParam(const char *parameter, 
				  MyString &filename, int &line_number);
 private:
	HashTable<MyString, ExtraParamInfo *> *table;

	void ClearOldParam(MyString &parameter);
};

#endif /* EXTRA_PARAM_INFO_H */
