/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
