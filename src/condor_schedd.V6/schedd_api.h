/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#ifndef _schedd_api_h
#define _schedd_api_h

#include "condor_common.h"
#include "condor_classad.h"
#include "CondorError.h"
#include "MyString.h"
#include "HashTable.h"
#include "directory.h"
#include "list.h"

#include "soap_scheddH.h"

/**
   `A JobFile is a file that resides in a Job's spool. JobFile objects
   are used for writing to and reading from spool files.
 */
class JobFile
{
 public:
	JobFile();
	~JobFile();

		/// A file descriptor.
	int file;
		/// The file's name.
	MyString name;
		/// The file's size, in bytes.
	int size;
		/// The current offset in the file.
	int currentOffset;
};

/**
   A FileInfo object contains basic infomation about a file.
 */
class FileInfo
{
 public:
		/**
		   Construct a new FileInfo object. Be sure to call initialize()!
		 */
	FileInfo();
		/**
		   Free all memory allocated in the constructor.
		*/
	~FileInfo();

		/**
		   Initialize the FileInfo object. The name is strdup()ed and
		   free()d in the destructor.

		   @param name The name of the file.
		   @param size The number of bytes in the file.
		 */
	int initialize(const char *name, unsigned long size);

		/// The file's name.
	char *name;
		/// The size, in bytes, of the file.
	unsigned long size;
};

/**
   A Job, quite simply. It provides access its spool, including
   listing, reading and writing spool files. It also provides a
   mechanism for submission and aborting a submission.
 */
class Job
{
 public:
		/**
		   Construct a Job with the given cluster and job ids.

		   @param clusterId The cluster the Job is in.
		   @param jobId The Job's id.
		 */
	Job(int clusterId, int jobId);

		/**
		   Destruct the Job. This does not abort the job or remove any
		   files. This does clean up all JobFiles, but does not remove
		   the files themselves. To remove files use abort().
		 */
	~Job();

		/**
		   Initialize the Job, which includes creating the Job's spool
		   directory.
		 */
	int initialize(CondorError &errstack);

		/**
		   Submit the Job. The submission process includes collecting
		   the declared files and adding them to the jobAd.

		   @param jobAd The Job's ClassAd.
		 */
	int submit(const struct condor__ClassAdStruct &jobAd,
			   CondorError &errstack);

		/**
		   Declare a file in the Job's spool.

		   @param name The file's name.
		   @param size The file's size, in bytes.
		 */
	int declare_file(const MyString &name,
					 int size,
					 CondorError &errstack);

		/**
		   Put a file in the Job's spool. Actually, put a chunk of a
		   file's data into the file. Errors are reported if the file
		   was not declared with declare_file(), if the offset or
		   offset + data_length are outside the declared bounds of the
		   file.

		   @param name The file's name, identifing where to write data.
		   @param offset The place to seek to in the file for writing data.
		   @param data The data itself.
		   @param data_length The length of the data.
		 */
	int put_file(const MyString &name,
				 int offset,
				 char * data,
				 int data_length,
				 CondorError &errstack);

		/**
		   Get a piece of a file in the Job's spool. Errors happen
		   when the file does not exist, the offset is out of the
		   file's bounds, or the offset+length is out of the file's
		   bounds.

		   @param name The name of the spool file.
		   @param offset The location to seek to in the file.
		   @param length The amount of data to be read from the file.
		   @param data The resulting data.
		 */
	int get_file(const MyString &name,
				 int offset,
				 int length,
				 unsigned char * &data,
				 CondorError &errstack);

		/**
		   List the spool directory. This provides FileInfo for each
		   file in the Job's spool.
		 */
	int get_spool_list(List<FileInfo> &file_list,
					   CondorError &errstack);

		/**
		   Abort the Job's submission process. This will not terminate
		   the Job if it has already been submitted. This deletes all
		   memory associated with the Job and the Job's spool files.
		 */
	int abort(CondorError &errstack);

		/**
		   Get the cluster this Job lives in.
		 */
	int getClusterID();

 protected:
	int clusterId;
	int jobId;
	HashTable<MyString, JobFile> declaredFiles;
	MyString spoolDirectory;
};

#endif
