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

#ifndef _FILE_TRANSFER_H
#define _FILE_TRANSFER_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "MyString.h"
#include "HashTable.h"
#ifdef WIN32
#include "perm.h"
#endif
#include "condor_uid.h"
#include "condor_ver_info.h"
#include "condor_classad.h"
#include "dc_transfer_queue.h"
#include <list>


extern const char * const StdoutRemapName;
extern const char * const StderrRemapName;

class FileTransfer;	// forward declatation
class FileTransferItem;
typedef std::list<FileTransferItem> FileTransferList;


struct CatalogEntry {
	time_t		modification_time;
	filesize_t	filesize;

	// if support for hashes is added, it requires memory management of the
	// data pointed to.  (hash table iterator copies Values, so you'll need a
	// copy operator that does a deep copy, or use a large enough static array)
	//
	//void		*hash;
	//void		hash[SIZE_OF_HASH];
	//
	//
	// likewise if you add signatures and all that PKI jazz.
	//
	// ZKM
};


typedef HashTable <MyString, FileTransfer *> TranskeyHashTable;
typedef HashTable <int, FileTransfer *> TransThreadHashTable;
typedef HashTable <MyString, CatalogEntry *> FileCatalogHashTable;
typedef HashTable <MyString, MyString> PluginHashTable;

typedef int		(Service::*FileTransferHandlerCpp)(FileTransfer *);
typedef int		(*FileTransferHandler)(FileTransfer *);

enum FileTransferStatus {
	XFER_STATUS_UNKNOWN,
	XFER_STATUS_QUEUED,
	XFER_STATUS_ACTIVE,
	XFER_STATUS_DONE
};


class FileTransfer: public Service {

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
			  priv_state priv = PRIV_UNKNOWN,
			  bool use_file_catalog = true);

	int SimpleInit(ClassAd *Ad, bool want_check_perms, bool is_server, 
						 ReliSock *sock_to_use = NULL, 
						 priv_state priv = PRIV_UNKNOWN,
						 bool use_file_catalog = true,
						 bool is_spool = false);

	/** @param Ad contains filename remaps for downloaded files.
		       If NULL, turns off remaps.
		@return 1 on success, 0 on failure */
	int InitDownloadFilenameRemaps(ClassAd *Ad);

	/** @param session_id NULL (if should auto-negotiate) or
		       security session id to use for outgoing file transfer
		       commands */
	void setSecuritySession(char const *session_id);

		/** Set limits on how much data will be sent/received per job
			(i.e. per call to DoUpload() or DoDownload()).  The job is
			put on hold if the limit is exceeded.  The files are sent
			in the usual order, and the file that hit the limit will
			be missing its tail.
		 */
	void setMaxUploadBytes(filesize_t MaxUploadBytes);
	void setMaxDownloadBytes(filesize_t MaxUploadBytes);

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
	void RegisterCallback(FileTransferHandler handler, bool want_status_updates=false)
		{ 
			ClientCallback = handler; 
			ClientCallbackWantsStatusUpdates = want_status_updates;
		}
	void RegisterCallback(FileTransferHandlerCpp handler, Service* handlerclass, bool want_status_updates=false)
		{ 
			ClientCallbackCpp = handler; 
			ClientCallbackClass = handlerclass;
			ClientCallbackWantsStatusUpdates = want_status_updates;
		}

	enum TransferType { NoType, DownloadFilesType, UploadFilesType };

	struct FileTransferInfo {
		FileTransferInfo() : bytes(0), duration(0), type(NoType),
		    success(true), in_progress(false), xfer_status(XFER_STATUS_UNKNOWN),
			try_again(true), hold_code(0), hold_subcode(0) {}

		void addSpooledFile(char const *name_in_spool);

		filesize_t bytes;
		time_t duration;
		TransferType type;
		bool success;
		bool in_progress;
		FileTransferStatus xfer_status;
		bool try_again;
		int hold_code;
		int hold_subcode;
		MyString error_desc;
			// List of files we created in remote spool.
			// This is intended to become SpooledOutputFiles.
		MyString spooled_files;
	};

	FileTransferInfo GetInfo() { return Info; }

	inline bool IsServer() {return user_supplied_key == FALSE;}

	inline bool IsClient() {return user_supplied_key == TRUE;}

	static int HandleCommands(Service *,int command,Stream *s);

	static int Reaper(Service *, int pid, int exit_status);

	static bool SetServerShouldBlock( bool block );

		// Stop accepting new transfer requests for this instance of
		// the file transfer object.
		// Also abort this object's active transfer, if any.
	void stopServer();

		// If this file transfer object has a transfer running in
		// the background, kill it.
	void abortActiveTransfer();

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

		/** Add the given filename to our list of exceptions.  These
			files to will not be transfer back, even if they meet the
			criteria to be returned.  If we already have this file,
			we immediately return success.  Otherwise, we append the
			given filename to our list and return success.
			NOTE: This list trumps any the addition of any file in it,
			meaning dynamically added files, modified files, etc. that
			are listed in it will be completely ignored.
			@param filename Name of file to add to our list
			@return always true
			*/
	bool addFileToExeptionList( const char* filename );

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

	void setTransferQueueContactInfo(char const *contact);

	void InsertPluginMappings(MyString methods, MyString p);
	MyString DeterminePluginMethods( CondorError &e, const char* path );
	int InitializePlugins(CondorError &e);
	int InvokeFileTransferPlugin(CondorError &e, const char* URL, const char* dest, const char* proxy_filename = NULL);
	MyString GetSupportedMethods();

		// Convert directories with a trailing slash to a list of the contents
		// of the directory.  This is used so that ATTR_TRANSFER_INPUT_FILES
		// can be correctly interpreted by the schedd when handling spooled
		// inputs.  See the lengthy comment in the function body for additional
		// explanation of why this is necessary.
		// Returns false on failure and sets error_msg.
	static bool ExpandInputFileList( ClassAd *job, MyString &error_msg );

	// When downloading files, store files matching source_name as the name
	// specified by target_name.
	void AddDownloadFilenameRemap(char const *source_name,char const *target_name);

	// Add any number of download remaps, encoded in the form:
	// "source1 = target1; source2 = target2; ..."
	// or in other words, the format expected by the util function
	// filename_remap_find().
	void AddDownloadFilenameRemaps(char const *remaps);

	int GetUploadTimestamps(time_t * pStart, time_t * pEnd = NULL) {
		if (uploadStartTime < 0)
			return false;
		if (pEnd) *pEnd = uploadEndTime;
		if (pStart) *pStart = uploadStartTime;
		return true;
	}

	bool GetDownloadTimestamps(time_t * pStart, time_t * pEnd = NULL) {
		if (downloadStartTime < 0)
			return false;
		if (pEnd) *pEnd = downloadEndTime;
		if (pStart) *pStart = downloadStartTime;
		return true;
	}

	ClassAd *GetJobAd();

	bool transferIsInProgress() { return ActiveTransferTid != -1; }

  protected:

		/** 
		 * Connect the given socket object to the relevant server.
		 */
	int DownloadConnect(ReliSock &sock);

	int Download(ReliSock *s, bool blocking);
	int Upload(ReliSock *s, bool blocking);
	static int DownloadThread(void *arg, Stream *s);
	static int UploadThread(void *arg, Stream *s);
	int TransferPipeHandler(int p);
	bool ReadTransferPipeMsg();
	void UpdateXferStatus(FileTransferStatus status);

		/** Actually download the files.
			@return -1 on failure, bytes transferred otherwise
		*/
	int DoDownload( filesize_t *total_bytes, ReliSock *s);
	int DoUpload( filesize_t *total_bytes, ReliSock *s);

	time_t uploadStartTime, uploadEndTime;
	time_t downloadStartTime, downloadEndTime;

	void CommitFiles();
	void ComputeFilesToSend();
	float bytesSent, bytesRcvd;
	StringList* InputFiles;

  private:

	bool TransferFilePermissions;
	bool DelegateX509Credentials;
	bool PeerDoesTransferAck;
	bool PeerDoesGoAhead;
	bool PeerUnderstandsMkdir;
	bool PeerDoesXferInfo;
	bool TransferUserLog;
	char* Iwd;
	StringList* ExceptionFiles;
	StringList* OutputFiles;
	StringList* EncryptInputFiles;
	StringList* EncryptOutputFiles;
	StringList* DontEncryptInputFiles;
	StringList* DontEncryptOutputFiles;
	StringList* IntermediateFiles;
	StringList* FilesToSend;
	StringList* EncryptFiles;
	StringList* DontEncryptFiles;
	std::string m_network_name;
	char* OutputDestination;
	char* SpooledIntermediateFiles;
	char* ExecFile;
	char* UserLogFile;
	char* X509UserProxy;
	MyString JobStdoutFile;
	MyString JobStderrFile;
	char* TransSock;
	char* TransKey;
	char* SpoolSpace;
	char* TmpSpoolSpace;
	int user_supplied_key;
	bool upload_changed_files;
	int m_final_transfer_flag;
	time_t last_download_time;
	FileCatalogHashTable* last_download_catalog;
	int ActiveTransferTid;
	time_t TransferStart;
	int TransferPipe[2];
	bool registered_xfer_pipe;
	FileTransferHandler ClientCallback;
	FileTransferHandlerCpp ClientCallbackCpp;
	Service* ClientCallbackClass;
	bool ClientCallbackWantsStatusUpdates;
	FileTransferInfo Info;
	PluginHashTable* plugin_table;
	bool I_support_filetransfer_plugins;
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
	static bool ServerShouldBlock;
	int clientSockTimeout;
	bool did_init;
	bool simple_init;
	ReliSock *simple_sock;
	MyString download_filename_remaps;
	bool m_use_file_catalog;
	TransferQueueContactInfo m_xfer_queue_contact_info;
	MyString m_jobid; // what job we are working on, for informational purposes
	char *m_sec_session_id;
	filesize_t MaxUploadBytes;
	filesize_t MaxDownloadBytes;

	// stores the path to the proxy after one is received
	MyString LocalProxyName;

	// called to construct the catalog of files in a direcotry
	bool BuildFileCatalog(time_t spool_time = 0, const char* iwd = NULL, FileCatalogHashTable **catalog = NULL);

	// called to lookup the catalog entry of file
	bool LookupInFileCatalog(const char *fname, time_t *mod_time, filesize_t *filesize);

	// Called internally by DoUpload() in order to handle common wrapup tasks.
	int ExitDoUpload(filesize_t *total_bytes, ReliSock *s, priv_state saved_priv, bool socket_default_crypto, bool upload_success, bool do_upload_ack, bool do_download_ack, bool try_again, int hold_code, int hold_subcode, char const *upload_error_desc,int DoUpload_exit_line);

	// Send acknowledgment of success/failure after downloading files.
	void SendTransferAck(Stream *s,bool success,bool try_again,int hold_code,int hold_subcode,char const *hold_reason);

	// Receive acknowledgment of success/failure after downloading files.
	void GetTransferAck(Stream *s,bool &success,bool &try_again,int &hold_code,int &hold_subcode,MyString &error_desc);

	// Stash transfer success/failure info that will be propagated back to
	// caller of file transfer operation, using GetInfo().
	void SaveTransferInfo(bool success,bool try_again,int hold_code,int hold_subcode,char const *hold_reason);

	// Receive message indicating that the peer is ready to receive the file
	// and save failure information with SaveTransferInfo().
	bool ReceiveTransferGoAhead(Stream *s,char const *fname,bool downloading,bool &go_ahead_always,filesize_t &peer_max_transfer_bytes);

	// Receive message indicating that the peer is ready to receive the file.
	bool DoReceiveTransferGoAhead(Stream *s,char const *fname,bool downloading,bool &go_ahead_always,filesize_t &peer_max_transfer_bytes,bool &try_again,int &hold_code,int &hold_subcode,MyString &error_desc, int alive_interval);

	// Obtain permission to receive a file download and then tell our
	// peer to go ahead and send it.
	// Save failure information with SaveTransferInfo().
	bool ObtainAndSendTransferGoAhead(DCTransferQueue &xfer_queue,bool downloading,Stream *s,filesize_t sandbox_size,char const *full_fname,bool &go_ahead_always);

	bool DoObtainAndSendTransferGoAhead(DCTransferQueue &xfer_queue,bool downloading,Stream *s,filesize_t sandbox_size,char const *full_fname,bool &go_ahead_always,bool &try_again,int &hold_code,int &hold_subcode,MyString &error_desc);

	std::string GetTransferQueueUser();

	// Report information about completed transfer from child thread.
	bool WriteStatusToTransferPipe(filesize_t total_bytes);
	ClassAd jobAd;

	bool ExpandFileTransferList( StringList *input_list, FileTransferList &expanded_list );

		// This function generates a list of files to transfer, including
		// directories to create and their full contents.
		// Arguments:
		// src_path  - the path of the file to be transferred
		// dest_dir  - the directory to write the file to
		// iwd       - relative paths are relative to this path
		// max_depth - how deep to recurse (-1 for infinite)
		// expanded_list - the list of files to transfer
	static bool ExpandFileTransferList( char const *src_path, char const *dest_dir, char const *iwd, int max_depth, FileTransferList &expanded_list );

	static bool ExpandInputFileList( char const *input_list, char const *iwd, MyString &expanded_list, MyString &error_msg );

		// Returns true if path is a legal path for our peer to tell us it
		// wants us to write to.  It must be a relative path, containing
		// no ".." elements.
	bool LegalPathInSandbox(char const *path,char const *sandbox);

		// Returns true if specified path points into the spool directory.
		// This does not do an existence check for the file.
	bool outputFileIsSpooled(char const *fname);

	void callClientCallback();
};

// returns 0 if no expiration
time_t GetDesiredDelegatedJobCredentialExpiration(ClassAd *job);

// returns 0 if renewal of lifetime is not needed
time_t GetDelegatedProxyRenewalTime(time_t expiration_time);

void GetDelegatedProxyRenewalTime(ClassAd *jobAd);

#endif

