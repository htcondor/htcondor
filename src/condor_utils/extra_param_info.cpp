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


#include "condor_common.h"
#include "condor_debug.h"
#include "extra_param_info.h"
#include "condor_string.h"

/*---------------------------------------------------------------------------
 *
 * ExtraParamInfo
 *    Information about a parameter that is in the condor config file.
 *    We track the filename and line number that it's defined in. Some
 *    parameters are not in files, but are defined elsewhere. We track
 *    that too.
 *
 *---------------------------------------------------------------------------*/

/*****************************************************************************
 *
 * Function: ExtraParamInfo constructor
 *
 ****************************************************************************/
ExtraParamInfo::ExtraParamInfo()
{
	_source      = None;
	_filename    = NULL;
	_line_number = -1;

	return;
}

/*****************************************************************************
 *
 * Function: ExtraParamInfo destructor
 *
 ****************************************************************************/
ExtraParamInfo::~ExtraParamInfo()
{
	if (_filename != NULL) {
		delete [] _filename;
		_filename = NULL;
	}

	return;
}


/*****************************************************************************
 *
 * Function: SetInfo
 * Purpose:  Set the file name and line number for this parameter information.
 *
 ****************************************************************************/
void ExtraParamInfo::SetInfo(
	const char *filename, 
	int line_number)
{
	if (filename != NULL) {
		if (_filename != NULL) {
			delete _filename;
		}
		_filename    = strnewp(filename);
		_source      = File;
		_line_number = line_number;
	}
	return;
}

/*****************************************************************************
 *
 * Function: SetInfo
 * Purpose:  For parameters that aren't in files, say where they are defined.
 *
 ****************************************************************************/
void ExtraParamInfo::SetInfo(
	ParamSource source)
{
	_filename    = NULL;
	_source      = source;
	_line_number = -1;
	return;
}

/*****************************************************************************
 *
 * Function: GetInfo
 * Purpose:  Find out where a parameter is defined. The source tells you if
 *           it is a file or not. You do NOT own the filename pointer, 
 *           so don't mess with it.
 *
 ****************************************************************************/
void ExtraParamInfo::GetInfo(
	ParamSource &source, 
	const char  *&filename, 
	int         &line_number)
{
	source      = _source;
	filename    = _filename;
	line_number = _line_number;
	return;
}

/*---------------------------------------------------------------------------
 *
 * ExtraParamTable
 *    A hash table of ExtraParamInfo, keyed on parameter names.
 *    This stores all of the information we have about parameters.
 *
 *---------------------------------------------------------------------------*/


/*****************************************************************************
 *
 * Function: ExtraParamTable constructor
 *
 ****************************************************************************/
ExtraParamTable::ExtraParamTable()
{
	table = new HashTable<MyString, ExtraParamInfo *>(300, MyStringHash,
													  updateDuplicateKeys);
	return;
}

/*****************************************************************************
 *
 * Function: ExtraParamTable destructor
 *
 ****************************************************************************/
ExtraParamTable::~ExtraParamTable()
{
	if (table != NULL) {
		ExtraParamInfo *info;
		table->startIterations();
		while (table->iterate(info)) {
			if (info) {
				delete info;
			}
		}
		delete table;
	    table = NULL;
	}
	return;
}

/*****************************************************************************
 *
 * Function: AddFileParam
 * Purpose:  Add the description of a parameter defined in a file to our 
 *           table.
 *
 ****************************************************************************/
void ExtraParamTable::
AddFileParam(
	const char *parameter, 
	const char *filename, 
	int line_number)
{
	MyString p(parameter);
	ExtraParamInfo *info = new ExtraParamInfo;

	if (info != NULL) {
		p.lower_case();
		ClearOldParam(p);
		info->SetInfo(filename, line_number);
		table->insert(p, info);
	}

	return;
}

/*****************************************************************************
 *
 * Function: AddInternalParam
 * Purpose:  Add the description of a parameter defined internally within
 *           Condor to our table.
 *
 ****************************************************************************/
void ExtraParamTable::
AddInternalParam(const char *parameter)
{
	MyString p(parameter);
	ExtraParamInfo *info = new ExtraParamInfo;
	
	if (info != NULL) {
		p.lower_case();
		ClearOldParam(p);
		info->SetInfo(ExtraParamInfo::Internal);
		table->insert(p, info);
	}
	return;
}

/*****************************************************************************
 *
 * Function: AddInternalParam
 * Purpose:  Add the description of a parameter defined in the user's
 *           environment to our table.
 *
 ****************************************************************************/
void ExtraParamTable::
AddEnvironmentParam(const char *parameter)
{
	MyString p(parameter);
	ExtraParamInfo *info = new ExtraParamInfo;
	
	if (info != NULL) {
		p.lower_case();
		ClearOldParam(p);
		info->SetInfo(ExtraParamInfo::Environment);
		table->insert(p, info);
	}
	return;
}

/*****************************************************************************
 *
 * Function: GetParam
 * Purpose:  Find out where a parameter is defined. "Filename" is overloaded,
 *           and if it's not really in a file, it contains a descriptive
 *           string that says where it's from. You'll know if this
 *           happens because the line number will be -1.
 *
 ****************************************************************************/
bool ExtraParamTable::
GetParam(
	const char *parameter, 
	MyString   &filename, 
	int        &line_number)
{
	bool           found_it;
	MyString       p(parameter);
	ExtraParamInfo *info;

	p.lower_case();
	if (table->lookup(p, info) == 0) {
		ExtraParamInfo::ParamSource info_source;
		const char *info_filename;

		info->GetInfo(info_source, info_filename, line_number);
		if (info_source == ExtraParamInfo::Internal) {
			filename = "<Internal>";
			line_number = -1;
		} else if (info_source == ExtraParamInfo::Environment) {
			filename = "<Environment>";
			line_number = -1;
		} else {
			filename = info_filename;
		}
		found_it = true;
	} else {
		filename    = "<Undefined>";
		line_number = -1;
		found_it = false;
	}
	return found_it;
}

/*****************************************************************************
 *
 * Function: ClearOldParam
 * Purpose:  Removes a parameter from the table, properly cleaning up
 *           the memory. This assumes that the parameter has already
 *           been converted to lower case.
 *
 ****************************************************************************/
void ExtraParamTable::
ClearOldParam(
	MyString &parameter)
{
	ExtraParamInfo *info;
	if (table->lookup(parameter, info) == 0) {
		table->remove(parameter);
		delete info; // It doesn't get cleaned up by the hash table
	}
	return;
}
