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
#ifndef _FILE_TRANSFER_H
#define _FILE_TRANSFER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "MyString.h"
#include "HashTable.h"
#ifdef WIN32
#include "perm.h"
#endif
#include "condor_uid.h"
#include "condor_ver_info.h"

class FileTransfer;	// forward declatation

typedef HashTable <MyString, FileTransfer *> TranskeyHashTable;
typedef HashTable <int, FileTransfer *> TransThreadHashTable;

typedef int		(Service::*FileTransferHandler)(FileTransfer *);

class FileTransfer {

  public:

	FileTransfer();

	~FileTransfer();

	/** Initialize the object.  Must be called before any other methods.
		@param Ad ClassAd containing all attributes needed by the object,
		such as the list of files to transfer.  If this ad does not contain
		a transfer key, then one is generated and this object is considered
		to be a server.  If a transfer key exists in the ad, this object is
		considered to be a client.
		The FileTransfer Object will look up the following attributes
		from the ClassAd passed in via parameter 1:
				ATTR_CLUSTER_ID
				ATTR_JOB_CMD
				ATTR_JOB_ERROR
				ATTR_JOB_INPUT
				ATTR_JOB_IWD
				ATTR_JOB_OUTPUT
				ATTR_NT_DOMAIN
				ATTR_OWNER
				ATTR_PROC_ID
				ATTR_TRANSFER_EXECUTABLE
				ATTR_TRANSFER_INPUT_FILES
				ATTR_TRANSFER_INTERMEDIATE_FILES
				ATTR_TRANSFER_KEY
				ATTR_TRANSFER_OUTPUT_FILES
				ATTR_TRANSFER_SOCKET
		@param check_file_perms If true, before reading or writing to any file,
		a check is perfomed to see if the ATTR_OWNER attribute defined in the
		ClassAd has the neccesary read/write permission.
		@return 1 on success, 0 on failure */
	int Init( ClassAd *Ad, bool check_file_perms = false, 
			  priv_state priv = PRIV_UNKNOWN );

	int SimpleInit(ClassAd *Ad, bool want_check_perms, bool is_server, 
						 ReliSock *sock_to_use = NULL, 
						 priv_state priv = PRIV_UNKNOWN);

	/** @param Ad contains filename remaps for downloaded files.
		       If NULL, turns off remaps.
		@return 1 on success, 0 on failure */
	int InitDownloadFilenameRemaps(ClassAd *Ad);

	/** @return 1 on success, 0 on failure */
	int DownloadFiles(bool blocking=true);

	/** @return 1 on success, 0 on failure */
	int UploadFiles(bool blocking=true, bool final_transfer=true);

		/** For non-blocking (i.e., multithreaded) transfers, the registered
			handler function will be called on each transfer completion.  The
			handler can call FileTransfer::GetInfo() for statistics on the
			last transfer.  It is safe for the handler to deallocate the
			FileTransfer object.
		*/
	void RegisterCallback(FileTransferHandler handler, Service* handlerclass)
		{ 
			ClientCallback = handler; 
			ClientCallbackClass = handlerclass;
		}

	enum TransferType { NoType, DownloadFilesType, UploadFilesType };

	struct FileTransferInfo {
		FileTransferInfo() : bytes(0), duration(0), type(NoType),
			success(true), in_progress(false) {}
		filesize_t bytes;
		time_t duration;
		TransferType type;
		bool success;
		bool in_progress;
	};

	FileTransferInfo FileTransfer::GetInfo() { return Info; }

	inline bool IsServer() {return user_supplied_key == FALSE;}

	inline bool IsClient() {return user_supplied_key == TRUE;}

	static int HandleCommands(Service *,int command,Stream *s);

	static int Reaper(Service *, int pid, int exit_status);

	int Suspend();

	int Continue();

	float TotalBytesSent() { return bytesSent; }

	float TotalBytesReceived() { return bytesRcvd; };

	void RemoveInputFiles(const char *sandbox_path = NULL);

		/** Add the given filename to our list of output files to
			transfer back.  If we're not managing a list of output
			files, we return failure.  If we already have this file,
			we immediately return success.  Otherwise, we append the
			given filename to our list and return success.
			@param filename Name of file to add to our list
			@return false if we don't have a list, else true
		*/
	bool addOutputFile( const char* filename );

		/** Allows the client side of the filetransfer object to 
			point to a different server.
			@param transkey Value of ATTR_TRANSFER_KEY set by server
			@param transsock Value of ATTR_TRANSFER_SOCKET set by server
			@return true on success, false on failure
		*/
	bool changeServer( const char* transkey, const char* transsock );

		/** Specify the socket timeout to use on the client (starter)
			side of the FileTransfer.  Defaults to 30 seconds if
			unspecified.
			@param timeout Specified in seconds, a value of 0 means disable
			@return Previous timeout value
		*/
	int	setClientSocketTimeout(int timeout);

	void setTransferFilePermissions( bool value )
		{ TransferFilePermissions = value; }

	void setPeerVersion( const char *peer_version );
	void setPeerVersion( const CondorVersionInfo &peer_version );

	priv_state getDesiredPrivState( void ) { return desired_priv_state; };

  protected:

	int Download(ReliSock *s, bool blocking);
	int Upload(ReliSock *s, bool blocking);
	static int DownloadThread(void *arg, Stream *s);
	static int UploadThread(void *arg, Stream *s);

		/** Actually download the files.
			@return -1 on failure, bytes transferred otherwise
		*/
	int DoDownload( filesize_t *total_bytes, ReliSock *s);
	int DoUpload( filesize_t *total_bytes, ReliSock *s);

	void CommitFiles();
	void ComputeFilesToSend();
	float bytesSent, bytesRcvd;
	StringList* InputFiles;

	// When downloading files, store files matching source_name as the name
	// specified by target_name.
	void AddDownloadFilenameRemap(char const *source_name,char const *target_name);

	// Add any number of download remaps, encoded in the form:
	// "source1 = target1; source2 = target2; ..."
	// or in other words, the format expected by the util function
	// filename_remap_find().
	void AddDownloadFilenameRemaps(char const *remaps);
	

  private:

	bool TransferFilePermissions;
	char* Iwd;
	StringList* OutputFiles;
	StringList* EncryptInputFiles;
	StringList* EncryptOutputFiles;
	StringList* DontEncryptInputFiles;
	StringList* DontEncryptOutputFiles;
	StringList* IntermediateFiles;
	StringList* FilesToSend;
	StringList* EncryptFiles;
	StringList* DontEncryptFiles;
	char* SpooledIntermediateFiles;
	char* ExecFile;
	char* UserLogFile;
	char* X509UserProxy;
	char* TransSock;
	char* TransKey;
	char* SpoolSpace;
	char* TmpSpoolSpace;
	int user_supplied_key;
	bool upload_changed_files;
	int m_final_transfer_flag;
	time_t last_download_time;
	int ActiveTransferTid;
	time_t TransferStart;
	int TransferPipe[2];
	FileTransferHandler ClientCallback;
	Service* ClientCallbackClass;
	FileTransferInfo Info;
#ifdef WIN32
	perm* perm_obj;
#endif		
    priv_state desired_priv_state;
	bool want_priv_change;
	static TranskeyHashTable* TranskeyTable;
	static TransThreadHashTable* TransThreadTable;
	static int CommandsRegistered;
	static int SequenceNum;
	static int ReaperId;
	int clientSockTimeout;
	bool did_init;
	bool simple_init;
	ReliSock *simple_sock;
	MyString download_filename_remaps;
};

#endif
