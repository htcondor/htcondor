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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dap_srb.h"
#include "dap_constants.h"
#include "dap_error.h"
#include "dap_url.h"
#include "dap_srb_util.h"
// Stork SRB transfers files > 2GB
#ifndef _LARGEFILE_SOURCE
#error _LARGEFILE_SOURCE not defined!
#endif
#ifndef _LARGEFILE64_SOURCE
#error _LARGEFILE64_SOURCE not defined!
#endif
#if _FILE_OFFSET_BITS != 64
#error _FILE_OFFSET_BITS not 64
#endif
#define BUF_SIZE_BYTES 	(BUF_SIZE * sizeof(char) )
#define DATATYPE        "ascii text"		/* FIXME: hard-coded */
#if 0
#define RESOURCE        "sfs-tape2-npaci"	/* FIXME: hard-coded */
#endif

/* ============================================================================
 * Transfer a file from local disk to SRB
 * ==========================================================================*/

static int
transfer_from_file_to_srb(const char *_src, const char *_dest)
{
    srbConn *conn = NULL;
    int out_fd = -1, in_fd = -1;
    char *buf = NULL;
    unsigned long filesize = 0;
    int bytes_read, bytes_written, total_bytes;
    struct stat64 srcStat;
    struct stat64 destStat;
	int catType = MDAS_CATALOG;	// Hard coded. FIXME
	int rtnVal = DAP_ERROR;
	char *resource = NULL;

	// Parse URLs.
	Url srcParsed(_src);	// parse the standard URL fields
	Url destParsed(_dest);	// parse the standard URL fields
	if (! srcParsed.parsed() ) {
		fprintf(stderr, "parse src URL '%s' error\n",
				srcParsed.urlNoPassword()
		);
		goto EXIT;
	}
	if (! destParsed.parsed() ) {
		fprintf(stderr, "parse dest URL '%s' error\n",
				destParsed.urlNoPassword()
		);
		goto EXIT;
	}
	destParsed.parseSrb();	// parse the SRB URL fields
	if (! destParsed.parsedSrb() ) {
		fprintf(stderr, "parse dest URL SRB fields'%s' error\n",
				destParsed.urlNoPassword()
		);
		goto EXIT;
	}

	resource = strdup(srbResource());		//de-const
	if (! resource) {
		fprintf(stderr,
				"unable to read defaultResource from $HOME/.srb/.MdasEnv\n");
		goto EXIT;
	}

    //get the local filesize info
	if (stat64(srcParsed.path(), &srcStat) < 0) {
		fprintf(stdout,"stat64 source %s: %s\n",
				srcParsed.urlNoPassword(), strerror(errno) );
		goto EXIT;
	}
	filesize = srcStat.st_size;
	if (! S_ISREG(srcStat.st_mode)) {
		fprintf(stdout,"stat source %s: not a regular file\n",
				srcParsed.urlNoPassword() );
		goto EXIT;
	}

    // open the input file for reading
    in_fd = open(srcParsed.path(), O_RDONLY|O_LARGEFILE);
    if (in_fd < 0){
		fprintf(stdout,"open source %s: %s\n",
				srcParsed.path(), strerror(errno));
		goto EXIT;
    }

    //connect to SRB server
    conn = srbConnect(
		destParsed.host(),		// srbHost
		destParsed.port(),		// srbPort
		destParsed.password(),	// srbAuth
		destParsed.srbUser(),	// userName
		destParsed.srbMdasDomain(),	// domainName
		"ENCRYPT1",				// authScheme
		NULL					// serverDn, GSI auth only
	);

    if (clStatus(conn) != CLI_CONNECTION_OK) {
      fprintf(stderr,"srbConnect failed.\n");
      fprintf(stderr,"=================================\n");
      fprintf(stderr,"%s",clErrorMessage(conn));
      fprintf(stderr,"=================================\n");
        srb_perror (2, clStatus(conn), "", SRB_RCMD_ACTION|SRB_LONG_MSG);
      fprintf(stderr,"=================================\n");
      fprintf(stderr,"host = '%s'\n", destParsed.host() );
      fprintf(stderr,"port = '%s'\n", destParsed.port() );
      //fprintf(stderr,"auth = '%s'\n", destParsed.password() );
      fprintf(stderr,"user = '%s'\n", destParsed.srbUser() );
      fprintf(stderr,"domainName = '%s'\n", destParsed.srbMdasDomain() );
      fprintf(stderr,"authScheme = '%s'\n", "ENCRYPT1");
      fprintf(stderr,"GSI server DN = '%p'\n", (void *)NULL);
      goto EXIT;
    }

    //open the output file 
    out_fd = srbObjCreate(
		conn,					// conn
		catType,				// catType, HARD-CODED! FIXME
		destParsed.basename(),	// objID
		DATATYPE,				// dataTypeName, HARD-CODED! FIXME
		resource,				// resourceName
		destParsed.dirname(),	// collectionName
		NULL,					// pathName
		filesize				// dataSize
	);

    if (out_fd < 0)  { //error
    //if (out_fd != 0)  { //error
      fprintf(stderr,"srbObjCreate failed.\n");
      fprintf(stderr,"%s",clErrorMessage(conn));
      fprintf(stderr,"conn = %p\n", conn);
      fprintf(stderr,"catType = %d\n", catType);
      fprintf(stderr,"object = '%s'\n", destParsed.basename() );
      fprintf(stderr,"dataType = '%s'\n", DATATYPE);
      fprintf(stderr,"resource = '%s'\n", resource);
      fprintf(stderr,"collection = '%s'\n", destParsed.dirname() );
      fprintf(stderr,"pathname = '%p'\n", (void *)NULL);
      //fprintf(stderr,"dataSize = '%lu'\n", filesize);
      fprintf(stderr,"dataSize = '%lu'\n", (unsigned long)0);
      return DAP_ERROR;
    }
    

    //transfer data
    buf = new char[ BUF_SIZE ];
	total_bytes = 0;
    while (1) {
		bytes_read = read(in_fd, buf, BUF_SIZE_BYTES);
		if (bytes_read < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				fprintf(stderr, "%s:%d read source %s: %s\n",
					__FILE__, __LINE__,
					srcParsed.urlNoPassword(), strerror(errno));
				goto EXIT;
			}
		} else if (bytes_read == 0) {
			break;
		}
		bytes_written = srbObjWrite (conn, out_fd, buf, bytes_read);
		total_bytes += bytes_written;
		if (bytes_written != bytes_read) {
			fprintf(stderr,"srbobjWrite failed, wrote %d/%d bytes\n",
					bytes_written, total_bytes);
			fprintf(stderr,"%s", clErrorMessage(conn));
			goto EXIT;
		}
	}

	// Close the target SRB object
    if (srbObjClose (conn, out_fd) < 0) {
      fprintf(stderr,"srbObjClose failed.\n");
      fprintf(stderr,"%s",clErrorMessage(conn));
	  goto EXIT;
	} else {
		out_fd = -1;	// required for next srbObjStat()
	}

    //get dest file stat from SRB after the transfer
	if (srbObjStat64(conn, catType, destParsed.path(), &destStat) < 0) {
		fprintf(stderr, "stat remote srb file %s\n", destParsed.path() );
		fprintf(stderr,"%s", clErrorMessage(conn));
		goto EXIT;
    }    

	if (! S_ISREG(destStat.st_mode)) {	// regular file
		fprintf(stderr, "remote path %s not a regular file\n",
			destParsed.path() );
		goto EXIT;
	}

	printf ("size of  src (%s) = %lu\n", srcParsed.urlNoPassword(), filesize);
	printf ("size of dest (%s) = %lu\n", destParsed.urlNoPassword(),
			(unsigned long)destStat.st_size);
	if (destStat.st_size != filesize) {
		fprintf(stderr, "file size mismatch\n");
		goto EXIT;
	}

	// Successful file transfer.
	rtnVal = DAP_SUCCESS;

EXIT:
    if (in_fd >= 0) close(in_fd);
	if (buf) delete []buf;
    if (out_fd >= 0) srbObjClose (conn, out_fd);
    if (conn) clFinish(conn);
    if (resource) free(resource);
	printf("transfer status: %s\n",
			(rtnVal == DAP_SUCCESS) ? "success" : "FAIL");
	return rtnVal;
}

/* ============================================================================
 * Transfer an SRB file to local disk
 * ==========================================================================*/
static int
transfer_from_srb_to_file(const char *_src, const char *_dest)
{
    int in_fd = -1, out_fd = -1;
    char *buf = NULL;
    int bytes_read, bytes_written, total_bytes;
    //int ret = DAP_ERROR;
    struct stat64 filestat;
    srbConn *conn = NULL;
    struct stat64 statbuf, destStat;
    struct stat64 dest_dirstat;
	char *cmd = NULL;
	int rtnVal = DAP_ERROR;
	int catType = MDAS_CATALOG;	// Hard coded. FIXME

	// Parse URLs.
	Url srcParsed(_src);	// parse the standard URL fields
	Url destParsed(_dest);	// parse the standard URL fields
	if (! srcParsed.parsed() ) {
		fprintf(stderr, "parse src URL '%s' error\n",
				srcParsed.urlNoPassword()
		);
		goto EXIT;
	}
	if (! destParsed.parsed() ) {
		fprintf(stderr, "parse dest URL '%s' error\n",
				destParsed.urlNoPassword()
		);
		goto EXIT;
	}
	srcParsed.parseSrb();	// parse the SRB URL fields
	if (! srcParsed.parsedSrb() ) {
		fprintf(stderr, "parse src URL SRB fields'%s' error\n",
				destParsed.urlNoPassword()
		);
		goto EXIT;
	}

	//if destination file exists, unlink it.
    if (!lstat64(destParsed.path(), &filestat)){	// don't follow symlinks
      if (unlink( destParsed.path() )) {
		fprintf(stderr, "unlink existing destination file %s: %s\n",
				destParsed.path(), strerror(errno) );
		goto EXIT;
	  }
    }

	//check if dest directory exists, if not create
	if (stat64(destParsed.dirname(), &dest_dirstat)){
		// Ensure full path of destination directory exists.
		// TODO: eliminate this shell command.  Currently, this is the best way
		// to support creating multiple directories.  Replace with mkdir()
		// system call
		const char *mkdir = "mkdir -p ";
		cmd =
			new char[ strlen(mkdir) + strlen(destParsed.dirname() ) + 1];
		strcpy(cmd, mkdir);
		strcat(cmd, destParsed.dirname() );
		if (system(cmd) == -1){
			fprintf(stdout,"Error in creating directory : %s\n",
				destParsed.dirname()
			);
			goto EXIT;
		}
	}

    buf = new char[ BUF_SIZE ];
    //fprintf(stdout, "Transfering file:%s to local disk ..\n", src_file);

    //connect to SRB server
    conn = srbConnect(
		srcParsed.host(),		// srbHost
		srcParsed.port(),		// srbPort
		srcParsed.password(),	// srbAuth
		srcParsed.srbUser(),	// userName
		srcParsed.srbMdasDomain(),	// domainName
		"ENCRYPT1",				// authScheme
		NULL					// serverDn, GSI auth only
	);

    if (clStatus(conn) != CLI_CONNECTION_OK) {
      fprintf(stderr,"srbConnect failed.\n");
      fprintf(stderr,"=================================\n");
      fprintf(stderr,"%s",clErrorMessage(conn));
      fprintf(stderr,"=================================\n");
        srb_perror (2, clStatus(conn), "", SRB_RCMD_ACTION|SRB_LONG_MSG);
      fprintf(stderr,"=================================\n");
      fprintf(stderr,"host = '%s'\n", srcParsed.host() );
      fprintf(stderr,"port = '%s'\n", srcParsed.port() );
      //fprintf(stderr,"auth = '%s'\n", srcParsed.password() );
      fprintf(stderr,"user = '%s'\n", srcParsed.srbUser() );
      fprintf(stderr,"domainName = '%s'\n", srcParsed.srbMdasDomain() );
      fprintf(stderr,"authScheme = '%s'\n", "ENCRYPT1");
      fprintf(stderr,"GSI server DN = '%p'\n", (void *)NULL);
      goto EXIT;
    }

    //open the input file 
    in_fd = srbObjOpen(
		conn,					// conn
		srcParsed.basename(),	// objID
		O_RDONLY|O_LARGEFILE,	// oflags
		srcParsed.dirname()	// collectionName
	);
    if (in_fd < 0)  { //error
      fprintf(stderr, "can't open collection %s object %s\n",
		  srcParsed.dirname(),
		  srcParsed.basename()
	 );
      fprintf(stderr,"%s",clErrorMessage(conn));
      goto EXIT;
    }


    //create and open file for writing
	// TODO: replace hard coded file mode with appropriate value.  Note that
	// daemoncore sets the umask to 022 .
    out_fd = open(
		destParsed.path(),
		O_WRONLY | O_CREAT | O_LARGEFILE,
		00777	//  TODO: replace hard coded file mode with appropriate value.
				// Note that // daemoncore sets the umask to 022 .

	);
    if (out_fd < 0){
      fprintf(stderr,
		  "Error in creating file %s: %s\n",
		  destParsed.path(), strerror(errno) );
      goto EXIT;
    }

	// data transfer loop
	total_bytes = 0;
	while (1) {
		bytes_read = srbObjRead(conn, in_fd, buf, BUF_SIZE_BYTES );
		if (bytes_read < 0) {
			fprintf(stderr, "%s:%d read source %s: %s\n",
				__FILE__, __LINE__,
				srcParsed.urlNoPassword(), clErrorMessage(conn));
			goto EXIT;
		} else if (bytes_read == 0) {
			break;
		}
		bytes_written = write(out_fd, buf, bytes_read);
		total_bytes += bytes_written;
		if (bytes_written != bytes_read) {
			fprintf(stderr,"write failed, wrote %d/%d bytes: %s\n",
					bytes_written, total_bytes, strerror(errno) );
			goto EXIT;
		}
	}

    if (close(out_fd)) {
      fprintf(stderr,"close %s failed: %s\n", destParsed.urlNoPassword(),
			  strerror(errno) );
	  goto EXIT;
	} else {
		out_fd = -1;	// required for next stat()
	}

    //get dest file stat
	if (stat64(destParsed.path(), &destStat) < 0) {
		fprintf(stderr, "stat dest file %s: %s\n",
				destParsed.path(), strerror(errno) );
		goto EXIT;
    }    

    //get src file stat from SRB
    if	(	srbObjStat64(
				conn,
				catType,			// FIXME: hard-coded
				srcParsed.path(),
				&statbuf
			)
			< 0
		) {
		fprintf(stderr, "can't stat srb file %s: %s\n",
			srcParsed.path(), clErrorMessage(conn) );
		goto EXIT;
    }    

	if (! S_ISREG(statbuf.st_mode)) {	// regular file
		fprintf(stderr, "remote path %s not a regular file\n",
			srcParsed.path() );
		goto EXIT;
	}
	printf ("size of  src (%s) = %lu\n", srcParsed.urlNoPassword(),
			(unsigned long)statbuf.st_size);
	printf ("size of dest (%s) = %lu\n", destParsed.urlNoPassword(),
			(unsigned long)destStat.st_size);
	if (destStat.st_size != destStat.st_size) {
		fprintf(stderr, "file size mismatch\n");
		goto EXIT;
	}

	// Successful file transfer.
	rtnVal = DAP_SUCCESS;

EXIT:
	if (cmd) delete []cmd;
    if (in_fd >= 0) srbObjClose(conn, in_fd);
	if (buf) delete []buf;
    if (out_fd >= 0) close (out_fd);
    if (conn) clFinish(conn);
	printf("transfer status: %s\n",
			(rtnVal == DAP_SUCCESS) ? "success" : "FAIL");
	return rtnVal;
}

//----------------------- MAIN BODY --------------------------------------
int
main(int argc, char *argv[])
{
	int transfer_status;

	if (argc < 3){
		fprintf(stderr,
			"==============================================================\n");
		fprintf(stderr,
			"USAGE: %s <src_url> <dest_url>\n", argv[0]);
		fprintf(stderr,
			"==============================================================\n");
		exit(-1);
	}

	const char *_src = argv[1];
	const char *_dest = argv[2];

	// Parse URLs.
	Url srcParsed(_src);	// parse the standard URL fields
	Url destParsed(_dest);	// parse the standard URL fields
	if (! srcParsed.parsed() ) {
		fprintf(stderr, "parse src URL '%s' error\n",
				srcParsed.urlNoPassword() );
		exit(-1);
	}
	if (! destParsed.parsed() ) {
		fprintf(stderr, "parse dest URL '%s' error\n",
				destParsed.urlNoPassword() );
		exit(-1);
	}
  
  fprintf(stdout, "Transfering from %s to %s\n",
		  srcParsed.urlNoPassword() ,
		  destParsed.urlNoPassword() );

  if (!strcmp(srcParsed.scheme(), "srb") &&
		  !strcmp(destParsed.scheme() , "file")){
    transfer_status = transfer_from_srb_to_file(_src, _dest);
  }
  else if (!strcmp(srcParsed.scheme(), "file") &&
		  !strcmp(destParsed.scheme(), "srb")){
    transfer_status = transfer_from_file_to_srb(_src, _dest);
  }
  else{
    fprintf(stderr,
			"Error: Transfer from %s to %s using this module not supported!\n", 
	    srcParsed.scheme(), destParsed.scheme());
    exit(-1);
  }
  return transfer_status;
  
}

