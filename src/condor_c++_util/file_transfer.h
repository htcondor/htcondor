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

class FileTransfer;	// forward declatation


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
			  priv_state priv = PRIV_UNKNOWN,
			  bool use_file_catalog = true);

	int SimpleInit(ClassAd *Ad, bool want_check_perms, bool is_server, 
						 ReliSock *sock_to_use = NULL, 
						 priv_state priv = PRIV_UNKNOWN,
						 bool use_file_catalog = true);

	/** @param Ad contains filename remaps for downloaded files.
		       If NULL, turns off remaps.
		@return 1 on success, 0 on failure */
	int InitDownloadFilenameRemaps(ClassAd *Ad);

	/** @param session_id NULL (if should auto-negotiate) or
		       security session id to use for outgoing file transfer
		       commands */
	void setSecuritySession(char const *session_id);

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
		    success(true), in_progress(false), try_again(true), hold_code(0),
		    hold_subcode(0) {}
		filesize_t bytes;
		time_t duration;
		TransferType type;
		bool success;
		bool in_progress;
		bool try_again;
		int hold_code;
		int hold_subcode;
		MyString error_desc;
	};

	FileTransferInfo GetInfo() { return Info; }

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
	int InvokeFileTransferPlugin(CondorError &e, const char* URL, const char* dest);

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
	bool DelegateX509Credentials;
	bool PeerDoesTransferAck;
	bool PeerDoesGoAhead;
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
	FileCatalogHashTable* last_download_catalog;
	int ActiveTransferTid;
	time_t TransferStart;
	int TransferPipe[2];
	FileTransferHandler ClientCallback;
	Service* ClientCallbackClass;
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
	int clientSockTimeout;
	bool did_init;
	bool simple_init;
	ReliSock *simple_sock;
	MyString download_filename_remaps;
	bool m_use_file_catalog;
	TransferQueueContactInfo m_xfer_queue_contact_info;
	MyString m_jobid; // what job we are working on, for informational purposes
	char *m_sec_session_id;

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
	bool ReceiveTransferGoAhead(Stream *s,char const *fname,bool downloading,bool &go_ahead_always);

	// Receive message indicating that the peer is ready to receive the file.
	bool DoReceiveTransferGoAhead(Stream *s,char const *fname,bool downloading,bool &go_ahead_always,bool &try_again,int &hold_code,int &hold_subcode,MyString &error_desc, int alive_interval);

	// Obtain permission to receive a file download and then tell our
	// peer to go ahead and send it.
	// Save failure information with SaveTransferInfo().
	bool ObtainAndSendTransferGoAhead(DCTransferQueue &xfer_queue,bool downloading,Stream *s,char const *full_fname,bool &go_ahead_always);

	bool DoObtainAndSendTransferGoAhead(DCTransferQueue &xfer_queue,bool downloading,Stream *s,char const *full_fname,bool &go_ahead_always,bool &try_again,int &hold_code,int &hold_subcode,MyString &error_desc);

	// Report information about completed transfer from child thread.
	bool WriteStatusToTransferPipe(filesize_t total_bytes);
	ClassAd jobAd;
};

#endif

