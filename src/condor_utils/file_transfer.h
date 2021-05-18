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
#include "condor_classad.h"
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
#include <vector>

extern const char * const StdoutRemapName;
extern const char * const StderrRemapName;

class FileTransfer;	// forward declatation
class FileTransferItem;
typedef std::vector<FileTransferItem> FileTransferList;


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

enum class TransferPluginResult {
	Success = 0,
	Error = 1,
	InvalidCredentials = 2
};

namespace htcondor {
class DataReuseDirectory;
}


class FileTransfer final: public Service {

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

	/** Determine if this is a dataflow transfer (where the output files are 
	/ * newer than input files)
	 * 
	 * @return True if newer, False otherwse
	 */
	static bool IsDataflowJob(ClassAd *job_ad);

	/** @param session_id NULL (if should auto-negotiate) or
		       security session id to use for outgoing file transfer
		       commands */
	void setSecuritySession(char const *session_id);

	/** @param cred_dir The location of the credentials directory to be used
	 *  with the file transfer plugins.
	 */
	void setCredsDir(const std::string &cred_dir) {m_cred_dir = cred_dir;}

	/** @param socket The socket used to send syscall instructions */
	void setSyscallSocket(ReliSock* socket) {m_syscall_socket = socket;}

	/** @param reuse_dir: The DataReuseDirectory object to utilize for data reuse
	 *  lookups
	 */
	void setDataReuseDirectory(htcondor::DataReuseDirectory &reuse_dir) {m_reuse_dir = &reuse_dir;}

	/** Set the location of various ads describing the runtime environment.
	 *  Used by the file transfer plugins.
	 *
	 *  @param job_ad The location of the job ad.
	 *  @param machine_ad The location of the machine ad.
	 */
	void setRuntimeAds(const std::string &job_ad, const std::string &machine_ad)
	{m_job_ad = job_ad; m_machine_ad = machine_ad;}

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

	/** @return 1 on success, 0 on failure */
	int UploadCheckpointFiles( bool blocking = true );

	/** @return 1 on success, 0 on failure */
	int UploadFailureFiles( bool blocking = true );

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
		MyString tcp_stats;
	};

	FileTransferInfo GetInfo() { return Info; }

	inline bool IsServer() const {return user_supplied_key == FALSE;}

	inline bool IsClient() const {return user_supplied_key == TRUE;}

	static int HandleCommands(int command,Stream *s);

	static int Reaper(int pid, int exit_status);

	static bool SetServerShouldBlock( bool block );

		// Stop accepting new transfer requests for this instance of
		// the file transfer object.
		// Also abort this object's active transfer, if any.
	void stopServer();

		// If this file transfer object has a transfer running in
		// the background, kill it.
	void abortActiveTransfer();

	int Suspend() const;

	int Continue() const;

	float TotalBytesSent() const { return bytesSent; }

	float TotalBytesReceived() const { return bytesRcvd; };

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
	bool addFileToExceptionList( const char* filename );

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
	void SetPluginMappings( CondorError &e, const char* path );
	int InitializeSystemPlugins(CondorError &e);
	int InitializeJobPlugins(const ClassAd &job, CondorError &e);
	int AddJobPluginsToInputFiles(const ClassAd &job, CondorError &e, StringList &infiles) const;
	MyString DetermineFileTransferPlugin( CondorError &error, const char* source, const char* dest );
	TransferPluginResult InvokeFileTransferPlugin(CondorError &e, const char* URL, const char* dest, ClassAd* plugin_stats, const char* proxy_filename = NULL);
	TransferPluginResult InvokeMultipleFileTransferPlugin(CondorError &e, const std::string &plugin_path, const std::string &transfer_files_string, const char* proxy_filename, bool do_upload, std::vector<std::unique_ptr<ClassAd>> *);
	TransferPluginResult InvokeMultiUploadPlugin(const std::string &plugin_path, const std::string &transfer_files_string, ReliSock &sock, bool send_trailing_eom, CondorError &err,  long &upload_bytes);
    int OutputFileTransferStats( ClassAd &stats );
	std::string GetSupportedMethods(CondorError &err);
	void DoPluginConfiguration();

		// Convert directories with a trailing slash to a list of the contents
		// of the directory.  This is used so that ATTR_TRANSFER_INPUT_FILES
		// can be correctly interpreted by the schedd when handling spooled
		// inputs.  See the lengthy comment in the function body for additional
		// explanation of why this is necessary.
		// Returns false on failure and sets error_msg.
	static bool ExpandInputFileList( ClassAd *job, std::string &error_msg );
		// use this function when you don't want to party on the job ad like the above function does 
	static bool ExpandInputFileList( char const *input_list, char const *iwd, MyString &expanded_list, std::string &error_msg );

	// When downloading files, store files matching source_name as the name
	// specified by target_name.
	void AddDownloadFilenameRemap(char const *source_name,char const *target_name);

	// Add any number of download remaps, encoded in the form:
	// "source1 = target1; source2 = target2; ..."
	// or in other words, the format expected by the util function
	// filename_remap_find().
	void AddDownloadFilenameRemaps(char const *remaps);

	int GetUploadTimestamps(time_t * pStart, time_t * pEnd = NULL) const {
		if (uploadStartTime < 0)
			return false;
		if (pEnd) *pEnd = (time_t)uploadEndTime;
		if (pStart) *pStart = (time_t)uploadStartTime;
		return true;
	}

	bool GetDownloadTimestamps(time_t * pStart, time_t * pEnd = NULL) const {
		if (downloadStartTime < 0)
			return false;
		if (pEnd) *pEnd = (time_t)downloadEndTime;
		if (pStart) *pStart = (time_t)downloadStartTime;
		return true;
	}

	ClassAd *GetJobAd();

	bool transferIsInProgress() const { return ActiveTransferTid != -1; }

  protected:

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

	double uploadStartTime{-1}, uploadEndTime{-1};
	double downloadStartTime{-1}, downloadEndTime{-1};

	void CommitFiles();

	void FindChangedFiles();
	void DetermineWhichFilesToSend();

#ifdef HAVE_HTTP_PUBLIC_FILES
	int AddInputFilenameRemaps(ClassAd *Ad);
#endif
	uint64_t bytesSent{0}, bytesRcvd{0};
	StringList* InputFiles{nullptr};

  private:

	bool uploadCheckpointFiles{false};
	bool uploadFailureFiles{false};
	bool TransferFilePermissions{false};
	bool DelegateX509Credentials{false};
	bool PeerDoesTransferAck{false};
	bool PeerDoesGoAhead{false};
	bool PeerUnderstandsMkdir{false};
	bool PeerDoesXferInfo{false};
	bool PeerDoesReuseInfo{false};
	bool PeerDoesS3Urls{false};
	bool TransferUserLog{false};
	char* Iwd{nullptr};
	StringList* ExceptionFiles{nullptr};
	StringList* OutputFiles{nullptr};
	StringList* EncryptInputFiles{nullptr};
	StringList* EncryptOutputFiles{nullptr};
	StringList* DontEncryptInputFiles{nullptr};
	StringList* DontEncryptOutputFiles{nullptr};
	StringList* IntermediateFiles{nullptr};
	StringList* FilesToSend{nullptr};
	StringList* EncryptFiles{nullptr};
	StringList* DontEncryptFiles{nullptr};

	StringList* CheckpointFiles{nullptr};
	StringList* EncryptCheckpointFiles{nullptr};
	StringList* DontEncryptCheckpointFiles{nullptr};

	char* OutputDestination{nullptr};
	char* SpooledIntermediateFiles{nullptr};
	char* ExecFile{nullptr};
	char* UserLogFile{nullptr};
	char* X509UserProxy{nullptr};
	MyString JobStdoutFile;
	MyString JobStderrFile;
	char* TransSock{nullptr};
	char* TransKey{nullptr};
	char* SpoolSpace{nullptr};
	char* TmpSpoolSpace{nullptr};
	int user_supplied_key{false};
	bool upload_changed_files{false};
	int m_final_transfer_flag{false};
	time_t last_download_time{0};
	FileCatalogHashTable* last_download_catalog{nullptr};
	int ActiveTransferTid{-1};
	time_t TransferStart{0};
	int TransferPipe[2] {-1, -1};
	bool registered_xfer_pipe{false};
	FileTransferHandler ClientCallback{nullptr};
	FileTransferHandlerCpp ClientCallbackCpp{nullptr};
	Service* ClientCallbackClass{nullptr};
	bool ClientCallbackWantsStatusUpdates{false};
	FileTransferInfo Info;
	PluginHashTable* plugin_table{nullptr};
	std::map<MyString, bool> plugins_multifile_support;
	std::map<std::string, bool> plugins_from_job;
	bool I_support_filetransfer_plugins{false};
	bool I_support_S3{false};
	bool multifile_plugins_enabled{false};
#ifdef WIN32
	perm* perm_obj{nullptr};
#endif
	priv_state desired_priv_state{PRIV_UNKNOWN};
	bool want_priv_change{false};
	static TranskeyHashTable* TranskeyTable;
	static TransThreadHashTable* TransThreadTable;
	static int CommandsRegistered;
	static int SequenceNum;
	static int ReaperId;
	static bool ServerShouldBlock;
	int clientSockTimeout{30};
	bool did_init{false};
	bool simple_init{true};
	ReliSock *simple_sock{nullptr};
	ReliSock *m_syscall_socket{nullptr};
	MyString download_filename_remaps;
	bool m_use_file_catalog{true};
	TransferQueueContactInfo m_xfer_queue_contact_info;
	MyString m_jobid; // what job we are working on, for informational purposes
	char *m_sec_session_id{nullptr};
	std::string m_cred_dir;
	std::string m_job_ad;
	std::string m_machine_ad;
	filesize_t MaxUploadBytes{-1};  // no limit by default
	filesize_t MaxDownloadBytes{-1};

	// stores the path to the proxy after one is received
	MyString LocalProxyName;

	// Object to manage reuse of any data locally.
	htcondor::DataReuseDirectory *m_reuse_dir{nullptr};

	// called to construct the catalog of files in a direcotry
	bool BuildFileCatalog(time_t spool_time = 0, const char* iwd = NULL, FileCatalogHashTable **catalog = NULL);

	// called to lookup the catalog entry of file
	bool LookupInFileCatalog(const char *fname, time_t *mod_time, filesize_t *filesize);

	// Called internally by DoUpload() in order to handle common wrapup tasks.
	int ExitDoUpload(const filesize_t *total_bytes, int numFiles, ReliSock *s, priv_state saved_priv, bool socket_default_crypto, bool upload_success, bool do_upload_ack, bool do_download_ack, bool try_again, int hold_code, int hold_subcode, char const *upload_error_desc,int DoUpload_exit_line);

	// Send acknowledgment of success/failure after downloading files.
	void SendTransferAck(Stream *s,bool success,bool try_again,int hold_code,int hold_subcode,char const *hold_reason);

	// Receive acknowledgment of success/failure after downloading files.
	void GetTransferAck(Stream *s,bool &success,bool &try_again,int &hold_code,int &hold_subcode,MyString &error_desc) const;

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

	bool DoObtainAndSendTransferGoAhead(DCTransferQueue &xfer_queue,bool downloading,Stream *s,filesize_t sandbox_size,char const *full_fname,bool &go_ahead_always,bool &try_again,int &hold_code,int &hold_subcode,std::string &error_desc);

	std::string GetTransferQueueUser();

	// Report information about completed transfer from child thread.
	bool WriteStatusToTransferPipe(filesize_t total_bytes);
	ClassAd jobAd;

	bool ExpandFileTransferList( StringList *input_list, FileTransferList &expanded_list, bool preserveRelativePaths );

		// This function generates a list of files to transfer, including
		// directories to create and their full contents.
		// Arguments:
		// src_path  - the path of the file to be transferred
		// dest_dir  - the directory to write the file to
		// iwd       - relative paths are relative to this path
		// max_depth - how deep to recurse (-1 for infinite)
		// expanded_list - the list of files to transfer
	static bool ExpandFileTransferList( char const *src_path, char const *dest_dir, char const *iwd, int max_depth, FileTransferList &expanded_list, bool preserveRelativePaths );

        // Function internal to ExpandFileTransferList() -- called twice there.
    static bool ExpandParentDirectories( const char *src_path, const char *iwd, FileTransferList & expanded_list );

		// Returns true if path is a legal path for our peer to tell us it
		// wants us to write to.  It must be a relative path, containing
		// no ".." elements.
	bool LegalPathInSandbox(char const *path,char const *sandbox);

		// Returns true if specified path points into the spool directory.
		// This does not do an existence check for the file.
	bool outputFileIsSpooled(char const *fname);

	void callClientCallback();

		// Manages the information about a single file to potentially reuse.
	class ReuseInfo
	{
	public:
		ReuseInfo(const std::string &filename,
			const std::string &checksum,
			const std::string &checksum_type,
			const std::string &tag,
			uint64_t size)
		: m_size(size),
		m_filename(filename),
		m_checksum(checksum),
		m_checksum_type(checksum_type),
		m_tag(tag)
		{}

		const std::string &filename() const {return m_filename;}
		const std::string &checksum() const {return m_checksum;}
		const std::string &checksum_type() const {return m_checksum_type;}
		uint64_t size() const {return m_size;}

	private:
		const uint64_t m_size{0};
		const std::string m_filename;
		const std::string m_checksum;
		const std::string m_checksum_type;
		const std::string m_tag;
	};

		// A list of all the potential files to reuse
	std::vector<ReuseInfo> m_reuse_info;
		// Any errors occurred when parsing a data manifest.  The parsing
		// occurs quite early in the upload protocol, before there is a reasonable
		// context to respond with an error message.  Hence, we stash errors here
		// and report them when it is reasonable to generate a hold message for the
		// user.
	CondorError m_reuse_info_err;

	// Parse the contents of a data manifest file for data reuse.
	// Returns true on success; false otherwise.  In the case of a failure, the
	// err object is filled in with an appropriate error message.
	bool ParseDataManifest();
};

// returns 0 if no expiration
time_t GetDesiredDelegatedJobCredentialExpiration(ClassAd *job);

// returns 0 if renewal of lifetime is not needed
time_t GetDelegatedProxyRenewalTime(time_t expiration_time);

void GetDelegatedProxyRenewalTime(ClassAd *jobAd);

#endif

