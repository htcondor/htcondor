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
#include "directory.h"
#include "condor_config.h"

#define CONDOR_EXEC "condor_exec"

TranskeyHashTable* FileTransfer::TranskeyTable = NULL;
TransThreadHashTable *FileTransfer::TransThreadTable = NULL;
int FileTransfer::CommandsRegistered = FALSE;
int FileTransfer::SequenceNum = 0;
int FileTransfer::ReaperId = -1;


// Utils from the util_lib that aren't prototyped
extern "C" {
	int		get_random_int();
	int		set_seed( int );
}

struct upload_info {
	StringList *filelist;
	char *Iwd;
	int pipe;
	char *ExecFile;
};

struct download_info {
	char *Iwd;
	int pipe;
};

// Hash function for pid table.
static int compute_transkey_hash(const MyString &key, int numBuckets) 
{
	return ( key.Hash() % numBuckets );
}

static int compute_transthread_hash(const int &pid, int numBuckets) 
{
	return ( pid % numBuckets );
}

FileTransfer::FileTransfer()
{
	Iwd = NULL;
	InputFiles = NULL;
	OutputFiles = NULL;
	ExecFile = NULL;
	TransSock = NULL;
	TransKey = NULL;
	user_supplied_key = FALSE;
	upload_changed_files = false;
	last_download_time = 0;
	ActiveTransferTid = -1;
	TransferStart = 0;
	ClientCallback = NULL;
	TransferPipe[0] = TransferPipe[1] = -1;
}

FileTransfer::~FileTransfer()
{
	if (ActiveTransferTid >= 0) {
		dprintf(D_ALWAYS, "FileTransfer object destructor called during "
				"active transfer.  Cancelling transfer.\n");
		daemonCore->Kill_Thread(ActiveTransferTid);
		TransThreadTable->remove(ActiveTransferTid);
		ActiveTransferTid = -1;
	}
	if (TransferPipe[0] >= 0) close(TransferPipe[0]);
	if (TransferPipe[1] >= 0) close(TransferPipe[1]);
	if (Iwd) free(Iwd);
	if (ExecFile) free(ExecFile);
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
				// delete our Thread table as well, which should be empty now
				delete TransThreadTable;
				TransThreadTable = NULL;
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

	if (Iwd) {
		EXCEPT("FileTransfer::Init called twice!");
	}

	if (ActiveTransferTid >= 0) {
		EXCEPT("FileTransfer::Init called during active transfer!");
	}

	if (!TranskeyTable) {
		// initialize our hashtable
		if (!(TranskeyTable = new TranskeyHashTable(7, compute_transkey_hash)))
		{
			// failed to allocate our hashtable ?!?!
			return 0;
		}
		
	}

	if (!TransThreadTable) {
		// initialize our thread hashtable
		if (!(TransThreadTable =
			  new TransThreadHashTable(7, compute_transthread_hash))) {
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
		ReaperId = daemonCore->Register_Reaper("FileTransfer::Reaper",
							(ReaperHandler)&FileTransfer::Reaper,
							"FileTransfer::Reaper()",NULL);
		if (ReaperId == 1) {
			EXCEPT("FileTransfer::Reaper() can not be the default reaper!\n");
		}

		// we also need to initialize the random number generator.  since
		// this only has to happen once, and we will only be in this section
		// of the code once (because the CommandsRegistered flag is static),
		// initialize the C++ random number generator here as well.
		set_seed( time(NULL) + (unsigned)this + (unsigned)Ad );
	}

	if (Ad->LookupString(ATTR_TRANSFER_KEY, buf) != 1) {
		char tempbuf[80];
		// classad did not already have a TRANSFER_KEY, so
		// generate a new one.  It must be unique and not guessable.
		sprintf(tempbuf,"%x#%x%x%x",++SequenceNum,(unsigned)time(NULL),
			get_random_int(),get_random_int());
		TransKey = strdup(tempbuf);
		user_supplied_key = FALSE;
		sprintf(tempbuf,"%s=\"%s\"",ATTR_TRANSFER_KEY,TransKey);
		Ad->InsertOrUpdate(tempbuf);

		// since we generated the key, it is only good on our socket.
		// so update TRANSFER_SOCK now as well.
		char *mysocket = daemonCore->InfoCommandSinfulString();
		ASSERT(mysocket);
		sprintf(tempbuf,"%s=\"%s\"",ATTR_TRANSFER_SOCKET,mysocket);
		Ad->InsertOrUpdate(tempbuf);
	} else {
		// Here the ad we were given already has a Transfer Key.
		TransKey = strdup(buf);
		user_supplied_key = TRUE;
	}

	// user must give us an initial working directory.
	if (Ad->LookupString(ATTR_JOB_IWD, buf) != 1) {
		return 0;
	}
	Iwd = strdup(buf);

	// Set InputFiles to be ATTR_TRANSFER_INPUT_FILES plus 
	// ATTR_JOB_INPUT, ATTR_JOB_CMD.
	if (Ad->LookupString(ATTR_TRANSFER_INPUT_FILES, buf) == 1) {
		InputFiles = new StringList(buf);
	}
	if (Ad->LookupString(ATTR_JOB_INPUT, buf) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( strcmp(buf,NULL_FILE) != 0 ) {
			if ( InputFiles ) 
				InputFiles->append(buf);
			else
				InputFiles = new StringList(buf);
		}
	}
	if (Ad->LookupString(ATTR_JOB_CMD, buf) == 1) {
		// stash the executable name for comparison later, so
		// we know that this file should be called condor_exec on the
		// client machine.  if an executable for this cluster exists
		// in the spool dir, use it instead.
		char source[_POSIX_PATH_MAX];
		char *Spool;
		int Cluster = 0;

		Ad->LookupInteger(ATTR_CLUSTER_ID, Cluster);
		Spool = param("SPOOL");
		if ( Spool ) {
			sprintf(source, "%s%ccluster%d.ickpt.subproc0", Spool, 
				DIR_DELIM_CHAR,Cluster);
			free(Spool);
			if ( access(source,F_OK | X_OK) >= 0 ) {
				// we can access an executable in the spool dir
				ExecFile = strdup(source);
			}
		}
		if ( !ExecFile ) {
			ExecFile = strdup(buf);
		}
		if ( InputFiles ) 
			InputFiles->append(ExecFile);
		else
			InputFiles = new StringList(ExecFile);
	}


	// Set OutputFiles to be ATTR_TRANSFER_OUTPUT_FILES if specified.
	// If not specified, set it to send whatever files have changed.
	// Also add in ATTR_JOB_OUPUT plus ATTR_JOB_ERROR.
	if (Ad->LookupString(ATTR_TRANSFER_OUTPUT_FILES, buf) == 1) {
		OutputFiles = new StringList(buf);
	} else {
		// send back new/changed files after the run
		upload_changed_files = true;
	}
	// and now check stdout/err
	if (Ad->LookupString(ATTR_JOB_OUTPUT, buf) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( strcmp(buf,NULL_FILE) != 0 ) {
			if ( OutputFiles ) 
				OutputFiles->append(buf);
			else
				OutputFiles = new StringList(buf);
		}
	}
	if (Ad->LookupString(ATTR_JOB_ERROR, buf) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( strcmp(buf,NULL_FILE) != 0 ) {
			if ( OutputFiles ) 
				OutputFiles->append(buf);
			else
				OutputFiles = new StringList(buf);
		}
	}

	if (Ad->LookupString(ATTR_TRANSFER_SOCKET, buf) != 1) {
		return 0;		
	}
	TransSock = strdup(buf);

	// if we are acting as the server side, insert this key 
	// into our hashtable if it is not already there.
	if ( IsServer() ) {
		MyString key(TransKey);
		FileTransfer *transobject;
		if ( TranskeyTable->lookup(key,transobject) < 0 ) {
			// this key is not in our hashtable; insert it
			if ( TranskeyTable->insert(key,this) < 0 ) {
				dprintf(D_ALWAYS,
					"FileTransfer::Init failed to insert key in our table\n");
				return 0;
			}
		} else {
			// this key is already in our hashtable; this is a programmer error!
			EXCEPT("FileTransfer: Duplicate TransferKeys!");
		}
	}

	return 1;
}

int
FileTransfer::DownloadFiles(bool blocking)
{
	ReliSock sock;
	int ret_value;
	
	dprintf(D_FULLDEBUG,"entering FileTransfer::DownloadFiles\n");

	if (ActiveTransferTid >= 0) {
		EXCEPT("FileTransfer::DownloadFiles called during active transfer!\n");
	}

	// Make certain Init() was called.
	if ( Iwd == NULL ) {
		EXCEPT("FileTransfer: Init() never called");
	}

	// This method should only be called on the client side, so if
	// we are the server side, there is a programmer error -- do EXCEPT.
	if ( IsServer() ) {
		EXCEPT("FileTransfer: DownloadFiles called on server side");
	}

	if ( !sock.connect(TransSock,0) )
		return 0;

	sock.encode();

	if (!sock.snd_int(FILETRANS_UPLOAD,0) ||
		!sock.code(TransKey) ||
		!sock.end_of_message() ) {
		return 0;
	}

	ret_value = Download(&sock,blocking);

	// If Download was successful (it returns 1 on success) and
	// upload_changed_files is true, then we must record the current
	// time in last_download_time so in UploadFiles we have a timestamp
	// to compare.  If it is a non-blocking download, we do all this
	// in the thread reaper.
	if ( blocking && ret_value == 1 && upload_changed_files ) {
		time(&last_download_time);
		// Now sleep for 1 second.  If we did not do this, then jobs
		// which run real quickly (i.e. less than a second) would not
		// have their output files uploaded.  The real reason we must
		// sleep here is time_t is only at the resolution on 1 second.
		sleep(1);
	}

	return ret_value;
}

int
FileTransfer::UploadFiles(bool blocking)
{
	ReliSock sock;
	StringList *files_to_send = NULL;
	StringList changed_files;

	dprintf(D_FULLDEBUG,"entering FileTransfer::UploadFiles\n");

	if (ActiveTransferTid >= 0) {
		EXCEPT("FileTransfer::UpLoadFiles called during active transfer!\n");
	}

	// Make certain Init() was called.
	if ( Iwd == NULL ) {
		EXCEPT("FileTransfer: Init() never called");
	}

	// This method should only be called on the client side, so if
	// we are the server side, there is a programmer error -- do EXCEPT.
	if ( IsServer() ) {
		EXCEPT("FileTransfer: UploadFiles called on server side");
	}

	if ( upload_changed_files && last_download_time > 0 ) {
		// Here we will upload only files in the Iwd which have changed
		// since we downloaded last.  We only do this is 
		// upload_changed_files it true, and if last_download_time > 0
		// which means we have already downloaded something.
		Directory dir(Iwd);
		const char *f;
		while ( (f=dir.Next()) ) {
			// if this file is has been modified since last download,
			// add it to the list of files to transfer.
			if ( dir.GetModifyTime() > last_download_time ) {
				dprintf(D_FULLDEBUG,"Sending changed file %s, mod=%ld, dow=%ld\n",
					f,dir.GetModifyTime(),last_download_time);
				changed_files.append(f);
				files_to_send = &changed_files;
			}
		}
	} else {
		// Here we want to upload the files listed in OutputFiles.
		files_to_send = OutputFiles;
	}
	
	// Optimization: files_to_send now contains the files to upload.
	// If files_to_send is NULL, then we have nothing to send, so
	// we can return with SUCCESS immedidately.
	if ( files_to_send == NULL ) {
		return 1;
	}

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

	return( Upload(&sock,files_to_send,blocking) );
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

	if (!sock->code(transkey) ||
		!sock->end_of_message() ) {
		dprintf(D_FULLDEBUG,
			    	"FileTransfer::HandleCommands failed to read transkey\n");
		return 0;
	}
	dprintf(D_FULLDEBUG,
					"FileTransfer::HandleCommands read transkey=%s\n",transkey);

	MyString key(transkey);
	if ( (TranskeyTable == NULL) || 
		 (TranskeyTable->lookup(key,transobject) < 0) ) {		
		// invalid transkey sent; send back 0 for failure
		sock->snd_int(0,1);	// sends a "0" then an end_of_record
		dprintf(D_FULLDEBUG,"transkey is invalid!\n");
		// sleep for 5 seconds to prevent brute-force attack on guessing key
		sleep(5);
		return FALSE;
	}

	switch (command) {
		case FILETRANS_UPLOAD:
			transobject->Upload(sock,transobject->InputFiles,false);
			break;
		case FILETRANS_DOWNLOAD:
			transobject->Download(sock,false);
			break;
		default:
			dprintf(D_ALWAYS,
				"FileTransfer::HandleCommands: unrecognized command %d\n",
				command);
			return 0;
			break;
	}

	return 1;
	// return KEEP_STREAM;
}


int
FileTransfer::Reaper(Service *, int pid, int exit_status)
{
	FileTransfer *transobject;
	if ( TransThreadTable->lookup(pid,transobject) < 0) {
		dprintf(D_ALWAYS, "unknown pid %d in FileTransfer::Reaper!\n", pid);
		return FALSE;
	}
	transobject->ActiveTransferTid = -1;
	TransThreadTable->remove(pid);

	transobject->Info.duration = time(NULL)-transobject->TransferStart;
	transobject->Info.in_progress = false;
	if (WEXITSTATUS(exit_status) > 0) {
		dprintf(D_ALWAYS, "File transfer completed successfully.\n");
		transobject->Info.success = true;
	} else {
		dprintf(D_ALWAYS, "File transfer failed (status=%d).\n",exit_status);
		transobject->Info.success = false;
	}

	read(transobject->TransferPipe[0], (char *)&transobject->Info.bytes,
		 sizeof(int));
	close(transobject->TransferPipe[0]);
	close(transobject->TransferPipe[1]);
	transobject->TransferPipe[0] = transobject->TransferPipe[1] = -1;

	// If Download was successful (it returns 1 on success) and
	// upload_changed_files is true, then we must record the current
	// time in last_download_time so in UploadFiles we have a timestamp
	// to compare.  If it is a non-blocking download, we do all this
	// in the thread reaper.
	if ( transobject->Info.success && transobject->upload_changed_files &&
		 transobject->IsClient() && transobject->Info.type == DownloadFilesType ) {
		time(&(transobject->last_download_time));
		// Now sleep for 1 second.  If we did not do this, then jobs
		// which run real quickly (i.e. less than a second) would not
		// have their output files uploaded.  The real reason we must
		// sleep here is time_t is only at the resolution on 1 second.
		sleep(1);
	}

	if (transobject->ClientCallback) {
		dprintf(D_FULLDEBUG,
				"Calling client FileTransfer handler function.\n");
		transobject->ClientCallback(transobject);
	}

	return TRUE;
}

int
FileTransfer::Download(ReliSock *s, bool blocking)
{
	dprintf(D_FULLDEBUG,"entering FileTransfer::Download\n");
	
	if (ActiveTransferTid >= 0) {
		EXCEPT("FileTransfer::Download called during active transfer!\n");
	}

	Info.duration = 0;
	Info.type = DownloadFilesType;
	Info.success = true;
	Info.in_progress = true;
	TransferStart = time(NULL);

	if (blocking) {

		Info.bytes = DoDownload((ReliSock *)s, Iwd);
		Info.duration = time(NULL)-TransferStart;
		Info.success = (Info.bytes >= 0);
		Info.in_progress = false;
		return Info.success;

	} else {

		// make a pipe to communicate with our thread
		if (pipe(TransferPipe) < 0) {
			dprintf(D_ALWAYS, "pipe failed with errno %d in "
					"FileTransfer::Upload\n", errno);
			return FALSE;
		}

		download_info *info = (download_info *)malloc(sizeof(download_info));
		info->Iwd = Iwd;
		info->pipe = TransferPipe[1]; // write end
		ActiveTransferTid = daemonCore->
			Create_Thread((ThreadStartFunc)&FileTransfer::DownloadThread,
						  (void *)info, s, ReaperId);
		if (ActiveTransferTid == FALSE) {
			dprintf(D_ALWAYS,
					"Failed to create FileTransfer DownloadThread!\n");
			ActiveTransferTid = -1;
			free(info);
			return FALSE;
		}
		// daemonCore will free(info) when the thread exits
		TransThreadTable->insert(ActiveTransferTid, this);

	}
	
	return 1;
}

int
FileTransfer::DownloadThread(void *arg, Stream *s)
{
	dprintf(D_FULLDEBUG,"entering FileTransfer::DownloadThread\n");
	download_info *info = (download_info *)arg;
	int total_bytes = DoDownload((ReliSock *)s, info->Iwd);
	write(info->pipe, (char *)&total_bytes, sizeof(int));
	return (total_bytes >= 0);
}

int
FileTransfer::DoDownload(ReliSock *s, char *Iwd)
{
	int reply, bytes, total_bytes = 0;
	char filename[_POSIX_PATH_MAX];
	char* p_filename = filename;
	char fullname[_POSIX_PATH_MAX];

	dprintf(D_FULLDEBUG,"entering FileTransfer::DoDownload\n");

	s->decode();

	for (;;) {
		if (!s->code(reply))
			return -1;
		if (!s->end_of_message())
			return -1;
		if ( !reply )
			break;
		if ( !s->code(p_filename) )
			return -1;
		sprintf(fullname,"%s%c%s",Iwd,DIR_DELIM_CHAR,filename);
		if ( ((bytes = s->get_file(fullname)) < 0) )
			return -1;
		if ( !s->end_of_message() )
			return -1;
		total_bytes += bytes;
	}

	return total_bytes;
}

int
FileTransfer::Upload(ReliSock *s, StringList *filelist, bool blocking)
{
	dprintf(D_FULLDEBUG,"entering FileTransfer::Upload\n");

	if (ActiveTransferTid >= 0) {
		EXCEPT("FileTransfer::Upload called during active transfer!\n");
	}

	Info.duration = 0;
	Info.type = UploadFilesType;
	Info.success = true;
	Info.in_progress = true;
	TransferStart = time(NULL);

	if (blocking) {

		Info.bytes = DoUpload((ReliSock *)s, filelist, Iwd, ExecFile);
		Info.duration = time(NULL)-TransferStart;
		Info.success = (Info.bytes >= 0);
		Info.in_progress = false;
		return Info.success;

	} else {

		// make a pipe to communicate with our thread
		if (pipe(TransferPipe) < 0) {
			dprintf(D_ALWAYS, "pipe failed with errno %d in "
					"FileTransfer::Upload\n", errno);
			return FALSE;
		}

		upload_info *info = (upload_info *)malloc(sizeof(upload_info));
		info->filelist = filelist;
		info->Iwd = Iwd;
		info->pipe = TransferPipe[1];	// write end
		info->ExecFile = ExecFile;
		ActiveTransferTid = daemonCore->
			Create_Thread((ThreadStartFunc)&FileTransfer::UploadThread,
						  (void *)info, s, ReaperId);
		if (ActiveTransferTid == FALSE) {
			dprintf(D_ALWAYS, "Failed to create FileTransfer UploadThread!\n");
			free(info);
			ActiveTransferTid = -1;
			return FALSE;
		}
		// daemonCore will free(info) when the thread exits
		TransThreadTable->insert(ActiveTransferTid, this);
	}
		
	return 1;
}

int
FileTransfer::UploadThread(void *arg, Stream *s)
{
	dprintf(D_FULLDEBUG,"entering FileTransfer::UploadThread\n");
	upload_info *info = (upload_info *)arg;
	int total_bytes = DoUpload((ReliSock *)s, info->filelist, info->Iwd,
		info->ExecFile);
	write(info->pipe, (char *)&total_bytes, sizeof(int));
	return (total_bytes >= 0);
}

int
FileTransfer::DoUpload(ReliSock *s, StringList *filelist, char *Iwd, 
					   char *Exec)
{
	char *filename;
	char *basefilename;
	char fullname[_POSIX_PATH_MAX];
	int bytes, total_bytes=0;
	
	dprintf(D_FULLDEBUG,"entering FileTransfer::DoUpload\n");

	s->encode();
	
	if ( filelist ) {
		filelist->rewind();
	}

	while ( filelist && (filename=filelist->next()) ) {

		dprintf(D_FULLDEBUG,"DoUpload: send file %s\n",filename);

		if (!s->snd_int(1,FALSE)) {
			dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
			return -1;
		}
		if (!s->end_of_message()) {
			dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
			return -1;
		}

		if ( Exec && ( strcmp(Exec,filename)==0 ) ) {
			// this file is the job executable
			basefilename = CONDOR_EXEC ;
		} else {
			// this file is _not_ the job executable
			basefilename = basename(filename);
		}

		if ( !s->code(basefilename) ) {
			dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
			return -1;
		}

		if ( filename[0] != '/' && filename[0] != '\\' && filename[1] != ':' ){
			sprintf(fullname,"%s%c%s",Iwd,DIR_DELIM_CHAR,filename);
		} else {
			strcpy(fullname,filename);
		}

		if ( ((bytes = s->put_file(fullname)) < 0) ) {
			dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
			return -1;
		}

		if ( !s->end_of_message() ) {
			dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
			return -1;
		}

		total_bytes += bytes;
	}

	// tell our peer we have nothing more to send
	s->snd_int(0,TRUE);

	dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
	return total_bytes;
}

int
FileTransfer::Suspend()
{
	int result = TRUE;	// return TRUE if there currently is no thread

	if (ActiveTransferTid != -1 ) {
		result = daemonCore->Suspend_Thread(ActiveTransferTid);
	}

	return result;
}

int
FileTransfer::Continue()
{
	int result = TRUE;	// return TRUE if there currently is no thread

	if (ActiveTransferTid != -1 ) {
		result = daemonCore->Continue_Thread(ActiveTransferTid);
	}

	return result;
}