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

#ifdef _NO_CONDOR_
#include <stdlib.h> // for free
#include <string.h> // for strdup, strcmp
#else
#include "condor_common.h"
#include "condor_io.h"
#endif

#include "ClassAdLogEntry.h"
#include "ClassAdLogParser.h"

ClassAdLogEntry::ClassAdLogEntry()
{
		// initialization
	key = mytype = targettype = name = value = NULL;	
	offset = next_offset = op_type = 0;

}

ClassAdLogEntry::~ClassAdLogEntry()
{
		// free allocated pointers
	if (key != NULL) free(key);	
	if (mytype != NULL) free(mytype);	
	if (targettype != NULL) free(targettype);	
	if (name != NULL) free(name);	
	if (value != NULL) free(value);	
}


//! initialize the variables
void
ClassAdLogEntry::init(int opType)
{
	op_type = opType;

		// free pointers
	if (key != NULL) {
		free(key);	
		key = NULL;
	}

	if (mytype != NULL) {
		free(mytype);	
		mytype = NULL;
	}

	if (targettype != NULL) {
		free(targettype);	
		targettype = NULL;
	}

	if (name != NULL) {
		free(name);	
		name = NULL;
	}

	if (value != NULL) {
		free(value);	
		value = NULL;
	}
}

/*!
	assignment operator
*/
ClassAdLogEntry&
ClassAdLogEntry::operator=(const ClassAdLogEntry& from)
{
	offset = from.offset;
	next_offset = from.next_offset;

	if (key != NULL) free(key);
	key = NULL;
	if(from.key != NULL) {
		key = strdup(from.key);
	}

	if (mytype != NULL) free(mytype);
	mytype = NULL;
	if(from.mytype != NULL) {
		mytype = strdup(from.mytype);
	}

	if (targettype != NULL) free(targettype);
	targettype = NULL;
	if(from.targettype != NULL) {
		targettype = strdup(from.targettype);
	}

	if (name != NULL) free(name);
	name = NULL;
	if(from.name != NULL) {
		name = strdup(from.name);
	}

	if (value != NULL) free(value);
	value = NULL;
	if(from.value != NULL) {
		value = strdup(from.value);
	}

	return (*this);
}

/*!
	check if the parameter ClassAdLogEntry is equal

	\return equality test resuls
			0: not equal
			1: equal
*/
int
ClassAdLogEntry::equal(ClassAdLogEntry* caLogEntry)
{

	if (caLogEntry->op_type == op_type) {
		switch(caLogEntry->op_type) {
		case CondorLogOp_NewClassAd: // key, mytype, targettyp
			if (valcmp(caLogEntry->key, key) == 0 &&
				valcmp(caLogEntry->mytype, mytype) == 0 &&
				valcmp(caLogEntry->targettype, targettype) == 0) {
				return 1;
			}
			else {
				return 0;
			}
			break;
		case CondorLogOp_DestroyClassAd: // key
			if (valcmp(caLogEntry->key, key) == 0) {
				return 1;
			}
			else {
				return 0;
			}
			break;
		case CondorLogOp_SetAttribute: // key, name, value
			if (valcmp(caLogEntry->key, key) == 0 &&
				valcmp(caLogEntry->name, name) == 0 &&
				valcmp(caLogEntry->value, value) == 0) {
				return 1;
			}
			else {
				return 0;
			}
			break;
		case CondorLogOp_DeleteAttribute: // key, name
			if (valcmp(caLogEntry->key, key) == 0 &&
				valcmp(caLogEntry->name, name) == 0) {
				return 1;
			}
			else {
				return 0;
			}
			break;
		case CondorLogOp_BeginTransaction:
			return 1;
			break;
		case CondorLogOp_EndTransaction:
			return 1;
			break;
		case CondorLogOp_LogHistoricalSequenceNumber: // key, value
			if (valcmp(caLogEntry->key, key) == 0 &&
				valcmp(caLogEntry->value, value) == 0) {
				return 1;
			}
			else {
				return 0;
			}
			break;
		}
	}

	return 0;
}

/*!
	compare two strings

	\return 
	0 if two strings are equal.
	If not:
		1. 
			If both of them aren't NULL,
			it returns the result of strcmp(str1, str2).
		2.
			if str1 is NULL and str2 isn't NULL, it returns -1.
			if str1 isn't NULL and str2 is NULL, it returns 1.
*/
int
ClassAdLogEntry::valcmp(char* str1, char* str2)
{
	if (str1 == NULL) {
		if (str2 == NULL) {
			return 0;
		}
		else {
			return 1;
		}
	}
	else {
		if (str2 == NULL) {
			return -1;
		}
		else {
			return strcmp(str1, str2);
		}
	}

	return -2; // This case doesn't happen.
}
