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
#ifndef _FILE_TRANSFER_H
#define _FILE_TRANSFER_H

#include "MyString.h"
#include "HashTable.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

class FileTransfer;	// forward declatation

typedef HashTable <MyString, FileTransfer *> TranskeyHashTable;
typedef HashTable <int, FileTransfer *> TransThreadHashTable;

typedef int		(*FileTransferHandler)(FileTransfer *);

class FileTransfer {

  public:

	FileTransfer::FileTransfer();

	FileTransfer::~FileTransfer();

	/** @return 1 on success, 0 on failure */
	int FileTransfer::Init(ClassAd *Ad);

	/** @return 1 on success, 0 on failure */
	int FileTransfer::DownloadFiles(bool blocking=true);

	/** @return 1 on success, 0 on failure */
	int FileTransfer::UploadFiles(bool blocking=true);

		/** For non-blocking (i.e., multithreaded) transfers, the registered
			handler function will be called on each transfer completion.  The
			handler can call FileTransfer::GetInfo() for statistics on the
			last transfer.  It is safe for the handler to deallocate the
			FileTransfer object.
		*/
	void FileTransfer::RegisterCallback(FileTransferHandler handler)
		{ ClientCallback = handler; }

	enum TransferType { NoType, DownloadFilesType, UploadFilesType };

	struct FileTransferInfo {
		FileTransferInfo() : bytes(0), duration(0), type(NoType),
			success(true), in_progress(false) {}
		int bytes;
		time_t duration;
		TransferType type;
		bool success;
		bool in_progress;
	};

	FileTransferInfo FileTransfer::GetInfo() { return Info; }

	inline bool FileTransfer::IsServer() {return user_supplied_key == FALSE;}

	inline bool FileTransfer::IsClient() {return user_supplied_key == TRUE;}

	static int FileTransfer::HandleCommands(Service *,int command,Stream *s);

	static int FileTransfer::Reaper(Service *, int pid, int exit_status);

	int FileTransfer::Suspend();

	int FileTransfer::Continue();

  protected:

	int FileTransfer::Download(ReliSock *s, bool blocking);
	int FileTransfer::Upload(ReliSock *s, StringList *filelist, bool blocking);
	static int FileTransfer::DownloadThread(void *arg, Stream *s);
	static int FileTransfer::UploadThread(void *arg, Stream *s);

		/** Actually download the files.
			@return -1 on failure, bytes transferred otherwise
		*/
	static int FileTransfer::DoDownload(ReliSock *s, char *Iwd);
	static int FileTransfer::DoUpload(ReliSock *s, StringList *filelist,
									  char *Iwd, char *Exec);

  private:

	char* Iwd;
	StringList* InputFiles;
	StringList* OutputFiles;
	char* ExecFile;
	char* TransSock;
	char* TransKey;
	int user_supplied_key;
	bool upload_changed_files;
	time_t last_download_time;
	int ActiveTransferTid;
	time_t TransferStart;
	int TransferPipe[2];
	FileTransferHandler ClientCallback;
	FileTransferInfo Info;

		
	static TranskeyHashTable* TranskeyTable;
	static TransThreadHashTable* TransThreadTable;
	static int CommandsRegistered;
	static int SequenceNum;
	static int ReaperId;
};

#endif
