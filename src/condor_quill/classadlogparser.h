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
#ifndef _CLASSADLOGPARSER_H_
#define _CLASSADLOGPARSER_H_

#include "condor_common.h"
#include "condor_io.h"
#include "quill_enums.h"

//used to distinguish between first and successive calls
#define IMPOSSIBLE_OFFSET -10000

//! ClassAdLogParser
/*! \brief Parser for ClassAd Log file
 *
 *  It actually reads and parses ClassAd Log file (job_queue.log)
 */
class ClassAdLogParser
{
public:

		//! constructor	
	ClassAdLogParser();
		//! destructor	
	~ClassAdLogParser();
	
		//
		// accessors
		//
	
	    //! get the file handles
	int     getFileDescriptor();
	FILE *  getFilePointer();

		//! set the file handles
	void    setFileDescriptor(int fd);
	void    setFilePointer(FILE *fp);

		//! return the current ClassAd log entry
	ClassAdLogEntry* 	getLastCALogEntry();
		//! return the last ClassAd log entry
	ClassAdLogEntry* 	getCurCALogEntry();
		//! set the next offset. IMPOSSIBLE_OFFSET is just a default value	
	void 	setNextOffset(long offset = IMPOSSIBLE_OFFSET);
	//!	set a job queue file name
	void	setJobQueueName(char* jqn);
	//!	get a job queue file name
	char*	getJobQueueName();
	//!	get a current file offset
	long	getCurOffset();
	//!	set a current file offset
	void 	setCurOffset(long offset);
	//!	get a current classad log entry data as a New ClassAd command
	QuillErrCode 	getNewClassAdBody(char*& key, 
									  char*& mytype, 
									  char*& targettype);
	//!	get a current classad log entry data as a Destroy ClassAd command
	QuillErrCode 	getDestroyClassAdBody(char*& key);
	//!	get a current classad log entry data as a Set Attribute command
	QuillErrCode 	getSetAttributeBody(char*& key, 
										char*& name, 
										char*& value);
	//!	get a current classad log entry data as a Delete Attribute command
	QuillErrCode 	getDeleteAttributeBody(char*& key, char*& name);

	//!	get the body of a historical sequence number command
	QuillErrCode	getLogHistoricalSNBody(char*& seqnum, char*& timestamp);

	//! read a classad log entry in the current offset of a file
	FileOpErrCode readLogEntry(int &op_type, bool ex = false);

	FileOpErrCode openFile(bool ex = false);
	FileOpErrCode closeFile(bool ex = false);

private:
		//
		// helper functions
		// 
	int	readHeader(int fd, int& op_type);
	int 	readHeader(FILE *fp, int& op_type);
	int 	readword(FILE *fp, char *&);
	int 	readword(int, char *&);
	int 	readline(FILE *fp, char *&);
	int 	readline(int, char *&);


	int	    readLogHistoricalSNBody(int fd);
	int	    readLogHistoricalSNBody(FILE *fp);	
	int 	readNewClassAdBody(int fd);
	int 	readNewClassAdBody(FILE *fp);
	int 	readDestroyClassAdBody(int fd);
	int 	readDestroyClassAdBody(FILE *fp);
	int 	readSetAttributeBody(int fd);
	int 	readSetAttributeBody(FILE *fp);
	int 	readDeleteAttributeBody(int fd);
	int 	readDeleteAttributeBody(FILE *fp);
	int 	readBeginTransactionBody(int fd);
	int 	readBeginTransactionBody(FILE *fp);
	int 	readEndTransactionBody(int fd);
	int 	readEndTransactionBody(FILE *fp);
		
		//
		// data
		//	
	char	job_queue_name[_POSIX_PATH_MAX];//!< job queue log file path
	long	nextOffset;						//!< next offset

	ClassAdLogEntry		curCALogEntry; 	//!< current ClassAd log entry
	ClassAdLogEntry		lastCALogEntry; //!< last ClassAd log entry 

	FILE 	*log_fp;
	int	    log_fd;
};

#endif /* _CLASSADLOGPARSER_H_ */
