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
#define CondorLogOp_BeginTransaction	105

//! Definition of End Transaction Command Type Constant
#define CondorLogOp_EndTransaction		106


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
	int					op_type;		//!< command type

	char				*key;			//!< key
	char				*mytype;		//!< mytype
	char				*targettype;	//!< targettype
	char				*name;			//!< name
	char				*value;			//!< attribut value
	
private:
	int					valcmp(char* str1, char* str2);
};

#endif /* _CLASSADLOGENTRY_H_ */
