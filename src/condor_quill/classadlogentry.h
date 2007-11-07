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

#ifndef _CLASSADLOGENTRY_H_
#define _CLASSADLOGENTRY_H_


//! Definition of New ClassAd Command Type Constant
#define CondorLogOp_NewClassAd			101

//! Definition of Destroy ClassAd Command Type Constant
#define CondorLogOp_DestroyClassAd		102


//! Definition of Set Attribute Command Type Constant
#define CondorLogOp_SetAttribute		103

//! Definition of Delete Attribute Command Type Constant
#define CondorLogOp_DeleteAttribute		104

//! Definition of Begin Transaction Command Type Constant
#define CondorLogOp_BeginTransaction		105

//! Definition of End Transaction Command Type Constant
#define CondorLogOp_EndTransaction		106

//! Definition of End Transaction Command Type Constant
#define CondorLogOp_LogHistoricalSequenceNumber	107


//! ClassAdLogEntry
/*! \brief this models each ClassAd Log Entry
 *
 * It includes data and a couple of accessors.
 */
class ClassAdLogEntry
{
public:
	//! constructor	
	ClassAdLogEntry();
	//! destructor	
	~ClassAdLogEntry();

	//! initialization
	//! \param opType command type (operation type)
	void				init(int opType);

		// opeartors
	//! assigment operator
	ClassAdLogEntry& 	operator=(const ClassAdLogEntry&);
	//! eqaul check operator
	int				 	equal(ClassAdLogEntry* caLogEntry);

		// data
	long				offset;			//!< offset of this entry
	long				next_offset;	//!< offset of the next entry
	int				op_type;		//!< command type

	char				*key;			//!< key
	char				*mytype;		//!< mytype
	char				*targettype;	//!< targettype
	char				*name;			//!< name
	char				*value;			//!< attribut value
	
private:
	int					valcmp(char* str1, char* str2);
};

#endif /* _CLASSADLOGENTRY_H_ */
