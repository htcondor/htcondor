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
#include "condor_ckpt_name.h"
#include "condor_string.h"
#include "util_lib_proto.h"
#include "daemon.h"
#include "daemon_types.h"
#include "nullfile.h"
#include "condor_ver_info.h"

#define COMMIT_FILENAME ".ccommit.con"

// Filenames are case insensitive on Win32, but case sensitive on Unix
#ifdef WIN32
#	define file_strcmp _stricmp
#	define file_contains contains_anycase
#	define file_contains_withwildcard contains_anycase_withwildcard
#else
#	define file_strcmp strcmp
#	define file_contains contains
#	define file_contains_withwildcard contains_withwildcard
#endif

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
	FileTransfer *myobj;
};

struct download_info {
	FileTransfer *myobj;
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
	TransferFilePermissions = false;
	Iwd = NULL;
	InputFiles = NULL;
	OutputFiles = NULL;
	EncryptInputFiles = NULL;
	EncryptOutputFiles = NULL;
	DontEncryptInputFiles = NULL;
	DontEncryptOutputFiles = NULL;
	IntermediateFiles = NULL;
	SpooledIntermediateFiles = NULL;
	FilesToSend = NULL;
	EncryptFiles = NULL;
	DontEncryptFiles = NULL;
	ExecFile = NULL;
	UserLogFile = NULL;
	X509UserProxy = NULL;
	TransSock = NULL;
	TransKey = NULL;
	SpoolSpace = NULL;
	TmpSpoolSpace = NULL;
	user_supplied_key = FALSE;
	upload_changed_files = false;
	last_download_time = 0;
	ActiveTransferTid = -1;
	TransferStart = 0;
	ClientCallback = 0;
	TransferPipe[0] = TransferPipe[1] = -1;
	bytesSent = 0.0;
	bytesRcvd = 0.0;
	m_final_transfer_flag = FALSE;
#ifdef WIN32
	perm_obj = NULL;
#endif
	desired_priv_state = PRIV_UNKNOWN;
	want_priv_change = false;
	did_init = false;
	clientSockTimeout = 30;
	simple_init = true;
	simple_sock = NULL;
}

FileTransfer::~FileTransfer()
{
	if (daemonCore && ActiveTransferTid >= 0) {
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
	if (UserLogFile) free(UserLogFile);
	if (X509UserProxy) free(X509UserProxy);
	if (SpoolSpace) free(SpoolSpace);
	if (TmpSpoolSpace) free(TmpSpoolSpace);
	if (InputFiles) delete InputFiles;
	if (OutputFiles) delete OutputFiles;
	if (EncryptInputFiles) delete EncryptInputFiles;
	if (EncryptOutputFiles) delete EncryptOutputFiles;
	if (DontEncryptInputFiles) delete DontEncryptInputFiles;
	if (DontEncryptOutputFiles) delete DontEncryptOutputFiles;
	if (IntermediateFiles) delete IntermediateFiles;
	if (SpooledIntermediateFiles) delete SpooledIntermediateFiles;
	// Note: do _not_ delete FileToSend!  It points to OutputFile or Intermediate.
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
#ifdef WIN32
	if (perm_obj) delete perm_obj;
#endif

}

int
FileTransfer::SimpleInit(ClassAd *Ad, bool want_check_perms, bool is_server, 
						 ReliSock *sock_to_use, priv_state priv ) 
{
	char buf[ATTRLIST_MAX_EXPRESSION];
	char *dynamic_buf = NULL;

	if( did_init ) {
			// no need to except, just quietly return success
		return 1;
	}

	user_supplied_key = is_server ? FALSE : TRUE;

	dprintf(D_FULLDEBUG,"entering FileTransfer::SimpleInit\n");

	desired_priv_state = priv;
    if ( priv == PRIV_UNKNOWN ) {
        want_priv_change = false;
    } else {
        want_priv_change = true;
    }

	simple_sock = sock_to_use;

	// user must give us an initial working directory.
	if (Ad->LookupString(ATTR_JOB_IWD, buf) != 1) {
		return 0;
	}
	Iwd = strdup(buf);

	// if the user want us to check file permissions, pull out the Owner
	// from the classad and instantiate a perm object.
	if ( want_check_perms ) {
		if (Ad->LookupString(ATTR_OWNER, buf) != 1) {
			// no owner specified in ad
			return 0;		
		}
#ifdef WIN32
		// lookup the domain
		char ntdomain[80];
		char *p_ntdomain = ntdomain;
		if (Ad->LookupString(ATTR_NT_DOMAIN, ntdomain) != 1) {
			// no nt domain specified in the ad; assume local account
			p_ntdomain = NULL;
		}
		perm_obj = new perm();
		if ( !perm_obj->init(buf,p_ntdomain) ) {
			// could not find the owner on this system; perm object
			// already did a dprintf so we don't have to.
			delete perm_obj;
			perm_obj = NULL;
			return 0;
		} 
#endif
	}

	// Set InputFiles to be ATTR_TRANSFER_INPUT_FILES plus 
	// ATTR_JOB_INPUT, ATTR_JOB_CMD, and ATTR_ULOG_FILE if simple_init.
	dynamic_buf = NULL;
	if (Ad->LookupString(ATTR_TRANSFER_INPUT_FILES, &dynamic_buf) == 1) {
		InputFiles = new StringList(dynamic_buf,",");
		free(dynamic_buf);
		dynamic_buf = NULL;
	} else {
		InputFiles = new StringList(NULL,",");
	}
	if (Ad->LookupString(ATTR_JOB_INPUT, buf) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf) ) {			
			if ( !InputFiles->file_contains(buf) )
				InputFiles->append(buf);			
		}
	}
	if ( Ad->LookupString(ATTR_ULOG_FILE, buf) == 1 ) {
		UserLogFile = strdup(condor_basename(buf));
			// add to input files if sending from submit to the schedd
		if ( (simple_init) && (!nullFile(buf)) ) {			
			if ( !InputFiles->file_contains(buf) )
				InputFiles->append(buf);			
		}
	}
	if ( Ad->LookupString(ATTR_X509_USER_PROXY, buf) == 1 ) {
		X509UserProxy = strdup(basename(buf));
			// add to input files
		if ( !nullFile(buf) ) {			
			if ( !InputFiles->file_contains(buf) )
				InputFiles->append(buf);			
		}
	}

	if ( (IsServer() || (IsClient() && simple_init))&& 
		 (Ad->LookupString(ATTR_JOB_CMD, buf) == 1) ) 
	{
		// stash the executable name for comparison later, so
		// we know that this file should be called condor_exec on the
		// client machine.  if an executable for this cluster exists
		// in the spool dir, use it instead.

		// Only check the spool directory if we're the server.
		// Note: This will break Condor-C jobs if the executable is ever
		//   spooled the old-fashioned way (which doesn't happen currently).
		if ( IsServer() ) {
			char *source;
			char *Spool;
			int Cluster = 0;
			int Proc = 0;

			Ad->LookupInteger(ATTR_CLUSTER_ID, Cluster);
			Ad->LookupInteger(ATTR_PROC_ID, Proc);
			Spool = param("SPOOL");
			if ( Spool ) {

				SpoolSpace = strdup( gen_ckpt_name(Spool,Cluster,Proc,0) );
				TmpSpoolSpace = (char*)malloc( strlen(SpoolSpace) + 10 );
				sprintf(TmpSpoolSpace,"%s.tmp",SpoolSpace);

				priv_state saved_priv = PRIV_UNKNOWN;
				if( want_priv_change ) {
					saved_priv = set_priv( desired_priv_state );
				}

				if( (mkdir(SpoolSpace,0777) < 0) ) {
					if( errno != EEXIST ) {
						dprintf( D_ALWAYS, "FileTransfer::Init(): "
								 "mkdir(%s) failed, %s (errno: %d)\n",
								 SpoolSpace, strerror(errno), errno );
					}
				}
				if( (mkdir(TmpSpoolSpace,0777) < 0) ) {
					if( errno != EEXIST ) {
						dprintf( D_ALWAYS, "FileTransfer::Init(): "
								 "mkdir(%s) failed, %s (errno: %d)\n",
								 TmpSpoolSpace, strerror(errno), errno );
					}
				}
				source = gen_ckpt_name(Spool,Cluster,ICKPT,0);
				free(Spool);
				Spool = NULL;
				if ( access(source,F_OK | X_OK) >= 0 ) {
					// we can access an executable in the spool dir
					ExecFile = strdup(source);
				}
				if( want_priv_change ) {
					ASSERT( saved_priv != PRIV_UNKNOWN );
					set_priv( saved_priv );
				}
			}
		}

		if ( !ExecFile ) {
			// apparently the executable is not in the spool dir.
			// so we must make certain the user has permission to read
			// this file; if so, we can record it as the Executable to send.
#ifdef WIN32
			if ( perm_obj && (perm_obj->read_access(buf) != 1) ) {
				// we do _not_ have permission to read this file!!
				dprintf(D_ALWAYS,
					"FileTrans: permission denied reading %s\n",buf);
				return 0;
			}
#endif
			ExecFile = strdup(buf);
		}

		// If we don't already have this on our list of things to transfer, 
		// and we haven't set TRANSFER_EXECTUABLE to false, send it along.
		// If we didn't set TRANSFER_EXECUTABLE, default to true 

		int xferExec;
		if(!Ad->LookupBool(ATTR_TRANSFER_EXECUTABLE,xferExec)) {
			xferExec=1;
		}

		if ( xferExec && !InputFiles->contains(ExecFile) ) {
			InputFiles->append(ExecFile);	
		}	
	}

	// Set OutputFiles to be ATTR_TRANSFER_OUTPUT_FILES if specified.
	// If not specified, set it to send whatever files have changed.
	// Also add in ATTR_JOB_OUPUT plus ATTR_JOB_ERROR, if we're not
	// streaming them, and if we're using a fixed list of output
	// files.
	dynamic_buf = NULL;
	if (Ad->LookupString(ATTR_TRANSFER_OUTPUT_FILES, &dynamic_buf) == 1) {
		OutputFiles = new StringList(dynamic_buf,",");
		free(dynamic_buf);
		dynamic_buf = NULL;
	} else {
		// send back new/changed files after the run
		upload_changed_files = true;
	}
	// and now check stdout/err
	int streaming = 0;
	if(Ad->LookupString(ATTR_JOB_OUTPUT, buf) == 1 ) {
		Ad->LookupBool( ATTR_STREAM_OUTPUT, streaming );
		if( ! streaming && ! upload_changed_files && ! nullFile(buf) ) {
				// not streaming it, add it to our list if we're not
				// just going to transfer anything that was changed.
				// only add to list if not NULL_FILE (i.e. /dev/null)
			if( OutputFiles ) {
				if( !OutputFiles->file_contains(buf) ) {
					OutputFiles->append( buf );
				}
			} else {
				OutputFiles = new StringList(buf,",");
			}
		}
	}
		// re-initialize this flag so we don't use stale info from
		// ATTR_STREAM_OUTPUT if ATTR_STREAM_ERROR isn't defined
	streaming = 0;
	if( Ad->LookupString(ATTR_JOB_ERROR, buf) == 1 ) {
		Ad->LookupBool( ATTR_STREAM_ERROR, streaming );
		if( ! streaming && ! upload_changed_files && ! nullFile(buf) ) {
				// not streaming it, add it to our list if we're not
				// just going to transfer anything that was changed.
				// only add to list if not NULL_FILE (i.e. /dev/null)
			if( OutputFiles ) {
				if( !OutputFiles->file_contains(buf) ) {
					OutputFiles->append( buf );
				}
			} else {
				OutputFiles = new StringList(buf,",");
			}
		}
	}

	// Set EncryptInputFiles to be ATTR_ENCRYPT_INPUT_FILES if specified.
	if (Ad->LookupString(ATTR_ENCRYPT_INPUT_FILES, buf) == 1) {
		EncryptInputFiles = new StringList(buf,",");
	} else {
		EncryptInputFiles = new StringList(NULL,",");
	}

	// Set EncryptOutputFiles to be ATTR_ENCRYPT_OUTPUT_FILES if specified.
	if (Ad->LookupString(ATTR_ENCRYPT_OUTPUT_FILES, buf) == 1) {
		EncryptOutputFiles = new StringList(buf,",");
	} else {
		EncryptOutputFiles = new StringList(NULL,",");
	}

	// Set DontEncryptInputFiles to be ATTR_DONT_ENCRYPT_INPUT_FILES if specified.
	if (Ad->LookupString(ATTR_DONT_ENCRYPT_INPUT_FILES, buf) == 1) {
		DontEncryptInputFiles = new StringList(buf,",");
	} else {
		DontEncryptInputFiles = new StringList(NULL,",");
	}

	// Set DontEncryptOutputFiles to be ATTR_DONT_ENCRYPT_OUTPUT_FILES if specified.
	if (Ad->LookupString(ATTR_DONT_ENCRYPT_OUTPUT_FILES, buf) == 1) {
		DontEncryptOutputFiles = new StringList(buf,",");
	} else {
		DontEncryptOutputFiles = new StringList(NULL,",");
	}

	int spool_completion_time = 0;
	Ad->LookupInteger(ATTR_STAGE_IN_FINISH,spool_completion_time);
	last_download_time = spool_completion_time;

	did_init = true;
	return 1;
}

int
FileTransfer::Init( ClassAd *Ad, bool want_check_perms, priv_state priv ) 
{
	char buf[ATTRLIST_MAX_EXPRESSION];
	char *dynamic_buf = NULL;

	ASSERT( daemonCore );	// full Init require DaemonCore methods

	if( did_init ) {
			// no need to except, just quietly return success
		return 1;
	}

	dprintf(D_FULLDEBUG,"entering FileTransfer::Init\n");

	simple_init = false;

	if (!TranskeyTable) {
		// initialize our hashtable
		if (!(TranskeyTable = new TranskeyHashTable(7, compute_transkey_hash)))
		{
			// failed to allocate our hashtable ?!?!
			return 0;
		}
		
	}

	if (ActiveTransferTid >= 0) {
		EXCEPT("FileTransfer::Init called during active transfer!");
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
	if ( !CommandsRegistered  ) {
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
		set_seed( time(NULL) + (unsigned long)this + (unsigned long)Ad );
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
		char *mysocket = global_dc_sinful();
		ASSERT(mysocket);
		sprintf(tempbuf,"%s=\"%s\"",ATTR_TRANSFER_SOCKET,mysocket);
		Ad->InsertOrUpdate(tempbuf);
	} else {
		// Here the ad we were given already has a Transfer Key.
		TransKey = strdup(buf);
		user_supplied_key = TRUE;
	}

		// Init all the file lists, etc.
	if ( !SimpleInit(Ad, want_check_perms, IsServer(),
			NULL, priv ) ) 
	{
		return 0;
	}

		// At this point, we'd better have a transfer socket
	if (Ad->LookupString(ATTR_TRANSFER_SOCKET, buf) != 1) {
		return 0;		
	}
	TransSock = strdup(buf);


	// If we are acting as the server side and we are uploading
	// any changed files, make a list of "intermediate" files
	// stored in our spool space (i.e. if transfer_files=ALWAYS).
	// This list is stored in the ClassAd which is sent to the 
	// client side, so that when the client does a final transfer
	// it can send changed files from that run + all intermediate
	// files.  -Todd Tannenbaum <tannenba@cs.wisc.edu> 6/8/01
	buf[0] = '\0';
	if ( IsServer() && upload_changed_files ) {
		CommitFiles();
		MyString filelist;
		const char* current_file = NULL;
		bool print_comma = false;
			// if desired_priv_state is PRIV_UNKNOWN, the Directory
			// object treats that just like we do: don't switch... 
		Directory spool_space( SpoolSpace, desired_priv_state );
		while ( (current_file=spool_space.Next()) ) {
			if ( UserLogFile && 
				 !file_strcmp(UserLogFile,current_file) ) 
			{
					// dont send UserLog file to the starter
				continue;
			}				
			if ( last_download_time > 0 &&
				 spool_space.GetModifyTime() <= last_download_time ) 
			{
					// Make certain file isn't just an input file
				continue;
			}
			if ( print_comma ) {
				filelist += ",";
			} else {
				print_comma = true;
			}
			filelist += current_file;			
		}
		if ( print_comma ) {
			// we know that filelist has at least one entry, so
			// insert it as an attribute into the ClassAd which
			// will get sent to our peer.
			MyString intermediateFilesBuf;
			intermediateFilesBuf.sprintf( "%s=\"%s\"",
				ATTR_TRANSFER_INTERMEDIATE_FILES,filelist.Value());
			Ad->InsertOrUpdate(intermediateFilesBuf.Value());
			dprintf(D_FULLDEBUG,"%s\n",buf);
		}
	}
	if ( IsClient() && upload_changed_files ) {
		dynamic_buf = NULL;
		Ad->LookupString(ATTR_TRANSFER_INTERMEDIATE_FILES,&dynamic_buf);
		dprintf(D_FULLDEBUG,"%s=\"%s\"\n",
				ATTR_TRANSFER_INTERMEDIATE_FILES,
				dynamic_buf ? dynamic_buf : "(none)");
		if ( dynamic_buf ) {
			SpooledIntermediateFiles = strnewp(dynamic_buf);
			free(dynamic_buf);
			dynamic_buf = NULL;
		}
	}
	

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

	did_init = true;
	
	return 1;
}

int
FileTransfer::DownloadFiles(bool blocking)
{
	int ret_value;
	ReliSock sock;
	ReliSock *sock_to_use;

	dprintf(D_FULLDEBUG,"entering FileTransfer::DownloadFiles\n");

	if (ActiveTransferTid >= 0) {
		EXCEPT("FileTransfer::DownloadFiles called during active transfer!\n");
	}

	// Make certain Init() was called.
	if ( Iwd == NULL ) {
		EXCEPT("FileTransfer: Init() never called");
	}

	if (!simple_init) {
		// This method should only be called on the client side, so if
		// we are the server side, there is a programmer error -- do EXCEPT.
		if ( IsServer() ) {
			EXCEPT("FileTransfer: DownloadFiles called on server side");
		}

		sock.timeout(clientSockTimeout);

		Daemon d( DT_ANY, TransSock );

		if ( !sock.connect(TransSock,0) ) {
			dprintf( D_ALWAYS, "FileTransfer: Unable to connect to server "
					 "%s\n", TransSock );
			return FALSE;
		}

		d.startCommand(FILETRANS_UPLOAD, &sock, 0);

		sock.encode();

		if ( !sock.code(TransKey) ||
			!sock.end_of_message() ) {
			return 0;
		}

		sock_to_use = &sock;
	} else {
		ASSERT(simple_sock);
		sock_to_use = simple_sock;
	}

	ret_value = Download(sock_to_use,blocking);

	// If Download was successful (it returns 1 on success) and
	// upload_changed_files is true, then we must record the current
	// time in last_download_time so in UploadFiles we have a timestamp
	// to compare.  If it is a non-blocking download, we do all this
	// in the thread reaper.
	if ( !simple_init && blocking && ret_value == 1 && upload_changed_files ) {
		time(&last_download_time);
		// Now sleep for 1 second.  If we did not do this, then jobs
		// which run real quickly (i.e. less than a second) would not
		// have their output files uploaded.  The real reason we must
		// sleep here is time_t is only at the resolution on 1 second.
		sleep(1);
	}

	return ret_value;
}


void
FileTransfer::ComputeFilesToSend()
{
	if (IntermediateFiles) delete(IntermediateFiles);
	IntermediateFiles = NULL;
	FilesToSend = NULL;
	EncryptFiles = NULL;
	DontEncryptFiles = NULL;
	
	if ( upload_changed_files && last_download_time > 0 ) {
		// Here we will upload only files in the Iwd which have changed
		// since we downloaded last.  We only do this if 
		// upload_changed_files it true, and if last_download_time > 0
		// which means we have already downloaded something.

		// If this is the final transfer, be certain to send back
		// not only the files which have been modified during this run,
		// but also the files which have been modified during
		// previous runs (i.e. the SpooledIntermediateFiles).
		if ( m_final_transfer_flag && SpooledIntermediateFiles ) {
			IntermediateFiles = 
				new StringList(SpooledIntermediateFiles,",");
			FilesToSend = IntermediateFiles;
			EncryptFiles = EncryptOutputFiles;
			DontEncryptFiles = DontEncryptOutputFiles;
		}
	
			// if desired_priv_state is PRIV_UNKNOWN, the Directory
			// object treats that just like we do: don't switch... 
		Directory dir( Iwd, desired_priv_state );

		const char *f;
		while( (f=dir.Next()) ) {
			// don't send back condor_exec.exe
			if ( file_strcmp(f,CONDOR_EXEC)==MATCH )
				continue;
			if ( file_strcmp(f,"condor_exec.bat")==MATCH )
				continue;

			// for now, skip all subdirectory names until we add
			// subdirectory support into FileTransfer.
			if ( dir.IsDirectory() )
				continue;
							
			// if this file is has been modified since last download,
			// add it to the list of files to transfer.
			if ( dir.GetModifyTime() > last_download_time ) {
				dprintf( D_FULLDEBUG, 
						 "Sending changed file %s, mod=%ld, dow=%ld\n",	
						 f, dir.GetModifyTime(), last_download_time );
				if (!IntermediateFiles) {
					// Initialize it with intermediate files
					// which we already have spooled.  We want to send
					// back these files + any that have changed this time.
					IntermediateFiles = new StringList(NULL,",");
					FilesToSend = IntermediateFiles;
					EncryptFiles = EncryptOutputFiles;
					DontEncryptFiles = DontEncryptOutputFiles;
				}
				// now append changed file to list only if not already there 
				if ( IntermediateFiles->file_contains(f) == FALSE ) {
					IntermediateFiles->append(f);
				}
			}
		}
	}
}

void
FileTransfer::RemoveInputFiles(const char *sandbox_path)
{
	char *old_iwd;
	int old_transfer_flag;
	StringList do_not_remove;
	const char *f;

	if (!sandbox_path) {
		ASSERT(SpoolSpace);
		sandbox_path = SpoolSpace;
	}

	// See if the sandbox_path exists.  If it does not, we're done.
	if ( !IsDirectory(sandbox_path) ) {
		return;
	}

	old_iwd = Iwd;
	old_transfer_flag = m_final_transfer_flag;

	Iwd = strdup(sandbox_path);
	m_final_transfer_flag = 1;

	ComputeFilesToSend();

	// if FilesToSend is still NULL, then the user did not
	// want anything sent back via modification date.  
	if ( FilesToSend == NULL ) {
		FilesToSend = OutputFiles;
		EncryptFiles = EncryptOutputFiles;
		DontEncryptFiles = DontEncryptOutputFiles;
	}

	// Make a new list that only contains file basenames.
	FilesToSend->rewind();
	while ( (f=FilesToSend->next()) ) {
		do_not_remove.append( basename(f) );
	}

	// Now, remove all files in the sandbox_path EXCEPT
	// for files in list do_not_remove.
	Directory dir( sandbox_path, desired_priv_state );

	while( (f=dir.Next()) ) {
			// for now, skip all subdirectory names until we add
			// subdirectory support into FileTransfer.
		if( dir.IsDirectory() ) {
			continue;
		}
			
			// skip output files
		if ( do_not_remove.file_contains(f) == TRUE ) {
			continue;
		}

			// if we made it here, we are looking at an "input" file.
			// so remove it.
		dir.Remove_Current_File();
	}

	m_final_transfer_flag = old_transfer_flag;
	free(Iwd);
	Iwd = old_iwd;

	return;
}


int
FileTransfer::UploadFiles(bool blocking, bool final_transfer)
{
    ReliSock sock;
	ReliSock *sock_to_use;

	StringList changed_files(NULL,",");

	dprintf(D_FULLDEBUG,
		"entering FileTransfer::UploadFiles (final_transfer=%d)\n",
		final_transfer ? 1 : 0);

	if (ActiveTransferTid >= 0) {
		EXCEPT("FileTransfer::UpLoadFiles called during active transfer!\n");
	}

	// Make certain Init() was called.
	if ( Iwd == NULL ) {
		EXCEPT("FileTransfer: Init() never called");
	}

	// This method should only be called on the client side, so if
	// we are the server side, there is a programmer error -- do EXCEPT.
	if ( !simple_init && IsServer() ) {
		EXCEPT("FileTransfer: UploadFiles called on server side");
	}

	// set flag saying if this is the last upload (i.e. job exited)
	m_final_transfer_flag = final_transfer ? 1 : 0;

	// figure out what to send based upon modification date
	ComputeFilesToSend();

	// if FilesToSend is still NULL, then the user did not
	// want anything sent back via modification date.  so
	// send the input or output sandbox, depending what 
	// direction we are going.
	if ( FilesToSend == NULL ) {
		if ( simple_init ) {
			if ( IsClient() ) {
				// condor_submit sending to the schedd
				FilesToSend = InputFiles;
				EncryptFiles = EncryptInputFiles;
				DontEncryptFiles = DontEncryptInputFiles;
			} else {
				// schedd sending to condor_transfer_data
				FilesToSend = OutputFiles;
				EncryptFiles = EncryptOutputFiles;
				DontEncryptFiles = DontEncryptOutputFiles;
			}
		} else {
			// starter sending back to the shadow
			FilesToSend = OutputFiles;
			EncryptFiles = EncryptOutputFiles;
			DontEncryptFiles = DontEncryptOutputFiles;
		}

	}

	if ( !simple_init ) {
		// Optimization: files_to_send now contains the files to upload.
		// If files_to_send is NULL, then we have nothing to send, so
		// we can return with SUCCESS immedidately.
		if ( FilesToSend == NULL ) {
			return 1;
		}

		sock.timeout(clientSockTimeout);

		Daemon d( DT_ANY, TransSock );

		if ( !sock.connect(TransSock,0) ) {
			dprintf( D_ALWAYS, "FileTransfer: Unable to connect to server "
					 "%s\n", TransSock );
			return FALSE;
		}

		d.startCommand(FILETRANS_DOWNLOAD, &sock);

		sock.encode();

		if ( !sock.code(TransKey) ||
			!sock.end_of_message() ) {
			return 0;
		}

		dprintf( D_FULLDEBUG,
				 "FileTransfer::UploadFiles: sent TransKey=%s\n", TransKey );

		sock_to_use = &sock;
	} else {
		ASSERT(simple_sock);
		sock_to_use = simple_sock;
	}


	int retval = Upload(sock_to_use,blocking);

	return( retval );
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

	// turn off timeouts on sockets, since our peer could get suspended
	// (like in the case of the starter sending files back to the shadow)
	sock->timeout(0);

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
			// We want to upload all files listed as InputFiles,
			// but first append to InputFiles any files in our SpoolSpace.
			// And before we do that, call CommitFiles() to finish any
			// previous commit which may have been prematurely aborted.
			{
			const char *currFile;
			transobject->CommitFiles();
			Directory spool_space( transobject->SpoolSpace, 
								   transobject->getDesiredPrivState() );
			while ( (currFile=spool_space.Next()) ) {
				if (transobject->UserLogFile && 
						!file_strcmp(transobject->UserLogFile,currFile)) 
				{
						// Don't send the userlog from the shadow to starter
					continue;
				} else {
						// We aren't looking at the userlog file... ship it!
					transobject->InputFiles->append(spool_space.GetFullPath());
				}
			}
			transobject->FilesToSend = transobject->InputFiles;
			transobject->EncryptFiles = transobject->EncryptInputFiles;
			transobject->DontEncryptFiles = transobject->DontEncryptInputFiles;
			transobject->Upload(sock,true);		// blocking = true for now...
			}
			break;
		case FILETRANS_DOWNLOAD:
			transobject->Download(sock,true);	// blocking = true for now...
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
	if( WIFSIGNALED(exit_status) ) {
		dprintf( D_ALWAYS, "File transfer failed (killed by signal=%d)\n",
				 WTERMSIG(exit_status) );
		transobject->Info.success = false;
	} else {
		if( WEXITSTATUS(exit_status) != 0 ) {
			dprintf( D_ALWAYS, "File transfer completed successfully.\n" );
			transobject->Info.success = true;
		} else {
			dprintf( D_ALWAYS, "File transfer failed (status=%d).\n",
					 WEXITSTATUS(exit_status) );
			transobject->Info.success = false;
		}
	}

	read( transobject->TransferPipe[0],
		  (char *)&transobject->Info.bytes,
		  sizeof( filesize_t) );
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
		((transobject->ClientCallbackClass)->*(transobject->ClientCallback))(transobject);
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

		int status = DoDownload( &Info.bytes, (ReliSock *) s );
		Info.duration = time(NULL)-TransferStart;
		Info.success = ( status >= 0 );
		Info.in_progress = false;
		return Info.success;

	} else {

		ASSERT( daemonCore );

		// make a pipe to communicate with our thread
		if (pipe(TransferPipe) < 0) {
			dprintf(D_ALWAYS, "pipe failed with errno %d in "
					"FileTransfer::Upload\n", errno);
			return FALSE;
		}

		download_info *info = (download_info *)malloc(sizeof(download_info));
		info->myobj = this;
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
	filesize_t	total_bytes;

	dprintf(D_FULLDEBUG,"entering FileTransfer::DownloadThread\n");
	FileTransfer * myobj = ((download_info *)arg)->myobj;
	int status = myobj->DoDownload( &total_bytes, (ReliSock *)s );
	write( myobj->TransferPipe[1],	// write-end of pipe
		   (char *)&total_bytes,
		   sizeof(filesize_t) );
	return ( status == 0 );
}


/*
  Define a macro to restore our priv state (if needed) and return.  We
  do this so we don't leak priv states in functions where we need to
  be in our desired state.
*/

#define return_and_resetpriv(i)                     \
    if( saved_priv != PRIV_UNKNOWN )                \
        _set_priv(saved_priv,__FILE__,__LINE__,1);  \
    return i;


int
FileTransfer::DoDownload( filesize_t *total_bytes, ReliSock *s)
{
	int rc;
	int reply;
	filesize_t bytes;
	char filename[_POSIX_PATH_MAX];
	char* p_filename = filename;
	char fullname[_POSIX_PATH_MAX];
	int final_transfer;

	priv_state saved_priv = PRIV_UNKNOWN;
	*total_bytes = 0;

	// we want to tell get_file() to perform an fsync (i.e. flush to disk)
	// the files we download if we are the client & we will need to upload
	// the changed files later on.  why do we need this fsync, you ask?  	
	// because we figure out which files have changed by looking at the file's
	// modification time.  on some operating systems, the file modification time
	// is _not_ when it was really modified, but when the modifications are actually
	// commited to disk.  thus we must fsync in order to make certain we do not think
	// that files like condor_exec.exe have been modified, etc. -Todd <tannenba@cs>
	bool want_fsync = ( ((IsClient() && !simple_init) ||  // starter receiving
						 (IsServer() && simple_init))     // schedd receiving
						 && upload_changed_files );

	dprintf(D_FULLDEBUG,"entering FileTransfer::DoDownload sync=%d\n",
					want_fsync ? 1 : 0);

	s->decode();

	// find out if encryption is enabled by default or not on this socket
	bool socket_default_crypto = s->get_encryption();

	// find out if this is the final download.  if so, we put the files
	// into the user's Iwd instead of our SpoolSpace.
	if( !s->code(final_transfer) ) {
		return_and_resetpriv( -1 );
	}
//	dprintf(D_FULLDEBUG,"TODD filetransfer DoDownload final_transfer=%d\n",final_transfer);
	if( !s->end_of_message() ) {
		return_and_resetpriv( -1 );
	}	
	for (;;) {
		if( !s->code(reply) ) {
			return_and_resetpriv( -1 );
		}
		if( !s->end_of_message() ) {
			return_and_resetpriv( -1 );
		}
		dprintf( D_SECURITY, "FILETRANSFER: incoming file_command is %i\n", reply);
		if( !reply ) {
			break;
		}
		if (reply == 1) {
			s->set_crypto_mode(socket_default_crypto);
		} else if (reply == 2) {
			s->set_crypto_mode(true);
		} else if (reply == 3) {
			s->set_crypto_mode(false);
		}

		if( !s->code(p_filename) ) {
			return_and_resetpriv( -1 );
		}

			/*
			  if we want to change priv states but haven't done so
			  yet, set it now.  we only need to do this once since
			  we're no longer doing any hard-coded insanity with
			  PRIV_CONDOR and everything can either be done in our
			  existing priv state (want_priv_change == FALSE) or in
			  the priv state we were told to use... Derek, 2005-04-21
			*/
		if( want_priv_change && saved_priv == PRIV_UNKNOWN ) {
			saved_priv = set_priv( desired_priv_state );
		}

		if( final_transfer || IsClient() ) {
			sprintf(fullname,"%s%c%s",Iwd,DIR_DELIM_CHAR,filename);
#ifdef WIN32
			// check for write permission on this file, if we are supposed to check
			if ( perm_obj && (perm_obj->write_access(fullname) != 1) ) {
				// we do _not_ have permission to write this file!!
				dprintf(D_ALWAYS,"DoDownload: Permission denied to write file %s!\n",
					fullname);
				dprintf(D_FULLDEBUG,"DoDownload: exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}
#endif
		} else {
			sprintf(fullname,"%s%c%s",TmpSpoolSpace,DIR_DELIM_CHAR,filename);
		}
	
		// On WinNT and apparently, some Unix, too, even doing an
		// fsync on the file does not get rid of the lazy-write
		// behavior which in the modification time being set a few
		// seconds into the future.  Instead of sleeping for 30+
		// seconds here in the starter & thus delaying the start of
		// the job, we call _utime to manually set the modification
		// time of the file we just wrote backwards in time by a few
		// minutes!  MLOP!! Since we are doing this, we may as well
		// not bother to fsync every file.
//		dprintf(D_FULLDEBUG,"TODD filetransfer DoDownload fullname=%s\n",fullname);
		if ( TransferFilePermissions ) {
			rc = s->get_file_with_permissions( &bytes, fullname );
		} else {
			rc = s->get_file( &bytes, fullname );
		}
		if( rc < 0 ) {
			return_and_resetpriv( -1 );
		}
		if ( want_fsync ) {
			struct utimbuf timewrap;

			time_t current_time = time(NULL);
			timewrap.actime = current_time;		// set access time to now
			timewrap.modtime = current_time - 180;	// set modify time to 3 min ago

			utime(fullname,&timewrap);
		}

		if( !s->end_of_message() ) {
			return_and_resetpriv( -1 );
		}
		*total_bytes += bytes;
	}

	// go back to the state we were in before file transfer
	s->set_crypto_mode(socket_default_crypto);

#ifdef WIN32
		// unsigned __int64 to float is not implemented on Win32
	bytesRcvd += (float)(signed __int64)(*total_bytes);
#else
	bytesRcvd += (*total_bytes);
#endif

	if ( !final_transfer && IsServer() ) {
		char buf[ATTRLIST_MAX_EXPRESSION];
		int fd;
		// we just stashed all the files in the TmpSpoolSpace.
		// write out the commit file.

		sprintf(buf,"%s%c%s",TmpSpoolSpace,DIR_DELIM_CHAR,COMMIT_FILENAME);
#if defined(WIN32)
		if ((fd = ::open(buf, O_WRONLY | O_CREAT | O_TRUNC | 
			_O_BINARY | _O_SEQUENTIAL, 0644)) < 0)
#else
		if ((fd = ::open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
#endif
		{
			dprintf(D_ALWAYS, 
				"FileTransfer::DoDownload failed to write commit file\n");
			return_and_resetpriv( -1 );
		}

		memset(buf,0,sizeof(buf));
		// copy in list of files to remove here
		::write(fd,buf,1+strlen(buf));
		::close(fd);

		// Now actually perform the commit.
		CommitFiles();
	}

	return_and_resetpriv( 0 );
}

void
FileTransfer::CommitFiles()
{
	char buf[_POSIX_PATH_MAX];
	char newbuf[_POSIX_PATH_MAX];
	const char *file;

	if ( IsClient() ) {
		return;
	}

	priv_state saved_priv = PRIV_UNKNOWN;
	if( want_priv_change ) {
		saved_priv = set_priv( desired_priv_state );
	}

	Directory tmpspool( TmpSpoolSpace, desired_priv_state );

	sprintf(buf,"%s%c%s",TmpSpoolSpace,DIR_DELIM_CHAR,COMMIT_FILENAME);
	if ( access(buf,F_OK) >= 0 ) {
		// the commit file exists, so commit the files.
		while ( (file=tmpspool.Next()) ) {
			// don't commit the commit file!
			if ( file_strcmp(file,COMMIT_FILENAME) == MATCH )
				continue;
			sprintf(buf,"%s%c%s",TmpSpoolSpace,DIR_DELIM_CHAR,file);
			sprintf(newbuf,"%s%c%s",SpoolSpace,DIR_DELIM_CHAR,file);
			if ( rotate_file(buf,newbuf) < 0 ) {
				EXCEPT("FileTransfer CommitFiles Failed -- What Now?!?!");
			}
		}
		// TODO: remove files specified in commit file
	}

	// We have now commited the files in tmpspool, if we were supposed to.
	// So now blow away tmpspool.
	tmpspool.Remove_Entire_Directory();
	if( want_priv_change ) {
		ASSERT( saved_priv != PRIV_UNKNOWN );
		set_priv( saved_priv );
	}
}

int
FileTransfer::Upload(ReliSock *s, bool blocking)
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
		DoUpload( &Info.bytes, (ReliSock *)s);
		Info.duration = time(NULL)-TransferStart;
		Info.success = (Info.bytes >= 0);
		Info.in_progress = false;
		return Info.success;

	} else {

		ASSERT( daemonCore );

		// make a pipe to communicate with our thread
		if (pipe(TransferPipe) < 0) {
			dprintf(D_ALWAYS, "pipe failed with errno %d in "
					"FileTransfer::Upload\n", errno);
			return FALSE;
		}

		upload_info *info = (upload_info *)malloc(sizeof(upload_info));
		info->myobj = this;
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
	FileTransfer * myobj = ((upload_info *)arg)->myobj;
	filesize_t	total_bytes;
	int status = myobj->DoUpload( &total_bytes, (ReliSock *)s );
	write( myobj->TransferPipe[1],	// write end
		   (char *)&total_bytes,
		   sizeof(filesize_t) );
	return ( status >= 0 );
}

int
FileTransfer::DoUpload(filesize_t *total_bytes, ReliSock *s)
{
	int rc;
	char *filename;
	char *basefilename;
	char fullname[_POSIX_PATH_MAX];
	filesize_t bytes;
	bool is_the_executable;
	StringList * filelist = FilesToSend;

	*total_bytes = 0;
	dprintf(D_FULLDEBUG,"entering FileTransfer::DoUpload\n");

	priv_state saved_priv = PRIV_UNKNOWN;
	if( want_priv_change ) {
		saved_priv = set_priv( desired_priv_state );
	}

	s->encode();

	// tell the server if this is the final transfer our not.
	// if it is the final transfer, the server places the files
	// into the user's Iwd.  if not, the files go into SpoolSpace.
	if( !s->code(m_final_transfer_flag) ) {
		return_and_resetpriv( -1 );
	}
	if( !s->end_of_message() ) {
		return_and_resetpriv( -1 );
	}
	if ( filelist ) {
		filelist->rewind();
	}

	// get ourselves a local copy that will be cleaned up if we exit
	char *tmpSpool = param("SPOOL");
	char Spool[_POSIX_PATH_MAX];
	if (tmpSpool) {
		strcpy (Spool, tmpSpool);
		free (tmpSpool);
	}

	// record the state it was in when we started... the "default" state
	bool socket_default_crypto = s->get_encryption();

	if( want_priv_change && saved_priv == PRIV_UNKNOWN ) {
		saved_priv = set_priv( desired_priv_state );
	}
	while ( filelist && (filename=filelist->next()) ) {

		dprintf(D_FULLDEBUG,"DoUpload: send file %s\n",filename);

		// now we send an int to the other side to indicate the next
		// action.  historically, we sent either 1 or 0.  zero meant
		// we were finished and there are no more files to send.  any
		// non-zero value means there is at least one more file.
		//
		// this has been expanded with two new values which indicate
		// encryption settings per-file.  the new values are:
		// 0 - finished
		// 1 - use socket default (on or off) for next file
		// 2 - force encryption on for next file.
		// 3 - force encryption off for next file.

		// default to the socket default
		int file_command = 1;
		
		// find out if this file is in DontEncryptFiles
		if ( DontEncryptFiles->file_contains_withwildcard(filename) ) {
			// turn crypto off for this file (actually done below)
			file_command = 3;
		}

		// now find out if this file is in EncryptFiles.  if it was
		// also in DontEncryptFiles, that doesn't matter, this will
		// override.
		if ( EncryptFiles->file_contains_withwildcard(filename) ) {
			// turn crypto on for this file (actually done below)
			file_command = 2;
		}

		dprintf ( D_SECURITY, "FILETRANSFER: outgoing file_command is %i for %s\n",
				file_command, filename );

		if( !s->snd_int(file_command,FALSE) ) {
			dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}
		if( !s->end_of_message() ) {
			dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}

		// now enable the crypto decision we made:
		if (file_command == 1) {
			s->set_crypto_mode(socket_default_crypto);
		} else if (file_command == 2) {
			s->set_crypto_mode(true);
		} else if (file_command == 3) {
			s->set_crypto_mode(false);
		}

		if ( ExecFile && !simple_init && (file_strcmp(ExecFile,filename)==0 )) {
			// this file is the job executable
			is_the_executable = true;
			basefilename = (char *)CONDOR_EXEC ;
		} else {
			// this file is _not_ the job executable
			is_the_executable = false;
			basefilename = basename(filename);
		}

		if( !s->code(basefilename) ) {
			dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}

		if( filename[0] != '/' && filename[0] != '\\' && filename[1] != ':' ){
			sprintf(fullname,"%s%c%s",Iwd,DIR_DELIM_CHAR,filename);
		} else {
			strcpy(fullname,filename);
		}

		// check for read permission on this file, if we are supposed to check.
		// do not check the executable, since it is likely sitting in the SPOOL
		// directory.
#ifdef WIN32
		if( perm_obj && !is_the_executable &&
			(perm_obj->read_access(fullname) != 1) ) {
			// we do _not_ have permission to read this file!!
			dprintf(D_ALWAYS,"DoUpload: Permission denied to read file %s!\n",
				fullname);
			dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}
#endif

		if ( TransferFilePermissions ) {
			rc = s->put_file_with_permissions( &bytes, fullname );
		} else {
			rc = s->put_file( &bytes, fullname );
		}
		if( rc < 0 ) {
			/* if we fail to transfer a file, EXCEPT so the other side can */
			/* try again. SC2000 hackery. _WARNING_ - I think Keller changed */
			/* all of this. -epaulson 11/22/2000 */
			EXCEPT("DoUpload: Failed to send file %s, exiting at %d\n",
				fullname,__LINE__);
			return_and_resetpriv( -1 );
		}

		if( !s->end_of_message() ) {
			dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}

		*total_bytes += bytes;
	}

	// tell our peer we have nothing more to send
	s->snd_int(0,TRUE);

	// go back to the state we were in before file transfer
	s->set_crypto_mode(socket_default_crypto);

	dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);

#ifdef WIN32
		// unsigned __int64 to float not implemented on Win32
	bytesSent += (float)(signed __int64)*total_bytes;
#else 
	bytesSent += *total_bytes;
#endif

	return_and_resetpriv( 0 );
}

int
FileTransfer::Suspend()
{
	int result = TRUE;	// return TRUE if there currently is no thread

	if (ActiveTransferTid != -1 ) {
		ASSERT( daemonCore );
		result = daemonCore->Suspend_Thread(ActiveTransferTid);
	}

	return result;
}

int
FileTransfer::Continue()
{
	int result = TRUE;	// return TRUE if there currently is no thread

	if (ActiveTransferTid != -1 ) {
		ASSERT( daemonCore );
		result = daemonCore->Continue_Thread(ActiveTransferTid);
	}

	return result;
}


bool
FileTransfer::addOutputFile( const char* filename )
{
	if( ! OutputFiles ) {
		return false;
	}
	if( OutputFiles->contains(filename) ) {
		return true;
	}
	OutputFiles->append( filename );
	return true;
}

bool
FileTransfer::changeServer(const char* transkey, const char* transsock)
{

	if ( transkey ) {
		if (TransKey) {
			free(TransKey);
		}
		TransKey = strdup(transkey);
	}

	if ( transsock ) {
		if (TransSock) {
			free(TransSock);
		}
		TransSock = strdup(transsock);
	}

	return true;
}

int	
FileTransfer::setClientSocketTimeout(int timeout)
{
	int old_val = clientSockTimeout;
	clientSockTimeout = timeout;
	return old_val;
}

/* This function must be called by both peers */
void
FileTransfer::setPeerVersion( const char *peer_version )
{
	CondorVersionInfo vi( peer_version );

	setPeerVersion( vi );
}

/* This function must be called by both peers */
void
FileTransfer::setPeerVersion( const CondorVersionInfo &peer_version )
{
	if ( peer_version.built_since_version(6,7,7) ) {
		TransferFilePermissions = true;
	} else {
		TransferFilePermissions = false;
	}
}


