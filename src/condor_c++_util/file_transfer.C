/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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

#include "condor_common.h"
#include "condor_debug.h"
#include "string_list.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "file_transfer.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "basename.h"

#define FILE_TRANS_TIMEOUT 180

TranskeyHashTable* FileTransfer::TranskeyTable = NULL;
int FileTransfer::CommandsRegistered = FALSE;

// Hash function for pid table.
static int compute_transkey_hash(const MyString &key, int numBuckets) 
{
	return ( 45 % numBuckets );
}

FileTransfer::FileTransfer()
{
	Iwd = NULL;
	InputFiles = NULL;
	OutputFiles = NULL;
	TransSock = NULL;
	TransKey = NULL;

}

FileTransfer::~FileTransfer()
{
	if (Iwd) free(Iwd);
	if (InputFiles) delete InputFiles;
	if (OutputFiles) delete OutputFiles;
	if (TransSock) free(TransSock);
	if (TransKey) {
		// remove our key from the hash table
		if ( TranskeyTable ) {
			MyString key(TransKey);
			TranskeyTable->remove(key);
			if ( TranskeyTable->getNumElements() == 0 ) {
				// if hash table is empty, delete table as well
				delete TranskeyTable;
				TranskeyTable = NULL;
			}
		}		
		// and free the key as well
		free(TransKey);
	}	
}

int
FileTransfer::Init(ClassAd *Ad)
{
	char buf[ATTRLIST_MAX_EXPRESSION];

	dprintf(D_FULLDEBUG,"entering FileTransfer::Init\n");

	if (!TranskeyTable) {
		// initialize our hashtable
		if (!(TranskeyTable = new TranskeyHashTable(7, compute_transkey_hash)))
		{
			// failed to allocate our hashtable ?!?!
			return 0;
		}
		
	}

	// Note: we must register commands here instead of our constructor 
	// to ensure that daemonCore object has been initialized before we 
	// call Register_Command.
	if ( !CommandsRegistered ) {
		CommandsRegistered = TRUE;
		daemonCore->Register_Command(FILETRANS_UPLOAD,"FILETRANS_UPLOAD",
				(CommandHandler)&FileTransfer::HandleCommands,
				"FileTransfer::HandleCommands()",NULL,WRITE);
		daemonCore->Register_Command(FILETRANS_DOWNLOAD,"FILETRANS_DOWNLOAD",
				(CommandHandler)&FileTransfer::HandleCommands,
				"FileTransfer::HandleCommands()",NULL,WRITE);
	}

	if (Ad->LookupString("TRANSFER_KEY", buf) != 1) {
		return 0;
	}
	TransKey = strdup(buf);

	// insert this key into our hashtable if it is not already there
	MyString key(TransKey);
	FileTransfer *transobject;
	if ( TranskeyTable->lookup(key,transobject) < 0 ) {
		// this key is not in our hashtable; insert it
		if ( TranskeyTable->insert(key,this) < 0 ) {
			dprintf(D_FULLDEBUG,
					"FileTransfer::Init failed to insert key in our table\n");
			return 0;
		}
	}


	if (Ad->LookupString(ATTR_JOB_IWD, buf) != 1) {
		return 0;
	}
	Iwd = strdup(buf);

	if (Ad->LookupString("TRANSFER_INPUT_FILES", buf) == 1) {
		InputFiles = new StringList(buf);
	}

	if (Ad->LookupString("TRANSFER_OUTPUT_FILES", buf) == 1) {
		OutputFiles = new StringList(buf);
	}

	if (Ad->LookupString("TRANSFER_SOCKET", buf) == 1) {
		TransSock = strdup(buf);
	}

	return 1;
}

int
FileTransfer::DownloadFiles()
{
	ReliSock sock;
	
	dprintf(D_FULLDEBUG,"entering FileTransfer::DownloadFiles\n");
	
	sock.timeout(FILE_TRANS_TIMEOUT);

	if ( !sock.connect(TransSock,0) )
		return 0;

	sock.encode();

	if (!sock.snd_int(FILETRANS_UPLOAD,0) ||
		!sock.code(TransKey) ||
		!sock.end_of_message() ) {
		return 0;
	}

	return( Download(&sock) );
}

int
FileTransfer::UploadFiles()
{
	ReliSock sock;

	dprintf(D_FULLDEBUG,"entering FileTransfer::UploadFiles\n");

	sock.timeout(FILE_TRANS_TIMEOUT);

	if ( !sock.connect(TransSock,0) )
		return 0;

	sock.encode();

	if (!sock.snd_int(FILETRANS_DOWNLOAD,0) ||
		!sock.code(TransKey) ||
		!sock.end_of_message() ) {
		return 0;
	}

	dprintf(D_FULLDEBUG,
					"FileTransfer::UploadFiles: sent TransKey=%s\n",TransKey);

	return( Upload(&sock,OutputFiles) );
}

int
FileTransfer::HandleCommands(Service *, int command, Stream *s)
{
	FileTransfer *transobject;
	char transkeybuf[_POSIX_PATH_MAX];
	char *transkey = transkeybuf;

	dprintf(D_FULLDEBUG,"entering FileTransfer::HandleCommands\n");

	if ( s->type() != Stream::reli_sock ) {
		// the FileTransfer object only works on TCP, not UDP
		return 0;
	}
	ReliSock *sock = (ReliSock *) s;

	sock->timeout(FILE_TRANS_TIMEOUT);

	if (!sock->code(transkey) ||
		!sock->end_of_message() ) {
		dprintf(D_FULLDEBUG,
			    	"FileTransfer::HandleCommands failed to read transkey\n");
		return 0;
	}
	dprintf(D_FULLDEBUG,
					"FileTransfer::HandleCommands read transkey=%s\n",transkey);

	MyString key(transkey);
	if ( TranskeyTable->lookup(key,transobject) < 0 ) {
		// invalid transkey sent; send back 0 for failure
		sock->snd_int(0,1);	// sends a "0" then an end_of_record
		dprintf(D_FULLDEBUG,"transkey is invalid!\n");
		return FALSE;
	}

	switch (command) {
		case FILETRANS_UPLOAD:
			transobject->Upload(sock,transobject->InputFiles);
			break;
		case FILETRANS_DOWNLOAD:
			transobject->Download(sock);
			break;
		default:
			dprintf(D_ALWAYS,
				"FileTransfer::HandleCommands: unrecognized command %d\n",
				command);
			return 0;
			break;
	}

	return 1;
}


int
FileTransfer::Download(ReliSock *s)
{
	int reply;
	char filename[_POSIX_PATH_MAX];
	char* p_filename = filename;
	char fullname[_POSIX_PATH_MAX];

	dprintf(D_FULLDEBUG,"entering FileTransfer::Download\n");
	
	s->decode();

	for (;;) {
		if (!s->code(reply))
			return 0;
		if (!s->end_of_message())
			return 0;
		if ( !reply )
			break;
		if ( !s->code(p_filename) )
			return 0;
		sprintf(fullname,"%s%c%s",Iwd,DIR_DELIM_CHAR,filename);
		if ( !s->get_file(fullname) )
			return 0;
		if ( !s->end_of_message() )
			return 0;
	}

	return 1;
}

int
FileTransfer::Upload(ReliSock *s, StringList *filelist)
{
	char *filename;
	char *basefilename;
	char fullname[_POSIX_PATH_MAX];

	dprintf(D_FULLDEBUG,"entering FileTransfer::Upload\n");
	
	s->encode();
	
	if ( filelist ) {
		filelist->rewind();
	}

	while ( filelist && (filename=filelist->next()) ) {
		if (!s->snd_int(1,FALSE))
			return 0;
		if (!s->end_of_message())
			return 0;
		basefilename = basename(filename);
		if ( !s->code(basefilename) )
			return 0;

		if ( filename[0] != '/' && filename[0] != '\\' && filename[1] != ':' ) {
			sprintf(fullname,"%s%c%s",Iwd,DIR_DELIM_CHAR,filename);
		} else {
			strcpy(fullname,filename);
		}

		if ( !s->put_file(fullname) )
			return 0;
		if ( !s->end_of_message() )
			return 0;
	}

	// tell our peer we have nothing more to send
	s->snd_int(0,TRUE);

	return 1;
}
