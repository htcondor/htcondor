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
#ifdef WIN32
#include "perm.h"
#endif
#include "condor_uid.h"
#include "condor_ver_info.h"
#include "condor_classad.h"
#include "dc_transfer_queue.h"
#include <vector>
#include <map>

extern const char * const StdoutRemapName;
extern const char * const StderrRemapName;

class FileTransfer;	// forward declatation
class FileTransferItem;
class UploadExitInfo;
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


using TranskeyHashTable = std::map<std::string, FileTransfer *>;
using TransThreadHashTable = std::map<int, FileTransfer *>;
using FileCatalogHashTable = std::map<std::string, CatalogEntry>;

// Class to hold what we know about a file transfer plugin,
// both system plugins and user supplied plugins
// There will be one instance of this class with an empty path, and id of -1, and and bad_plugin=true
// That instance will be returned on failure from functions that return a plugin &
class FileTransferPlugin {
public:
	FileTransferPlugin(std::string_view _path, bool _from_job=false, bool _bad=false);

	bool multi_file() { return protocol_version > 1; }

	const std::string path;
	std::string name;
	ClassAd     ad;
	int         id{-1};   // set to -1 or the index into the plugin_ads vector
	bool        from_job{false};    // transferred by the job, we don't query these
	bool        was_queried{false}; // set to true once we attempted to query it once
	bool        bad_plugin{false};  // the null plugin, or any plugin that we could not query
	bool        has_failed_methods{false};
	unsigned char protocol_version{0}; // set to 0, 1 or 2 (for now), Pelican may be protocol 3 someday
};
using PluginHashTable = std::map<std::string, int, classad::CaseIgnLTStr>;

typedef int		(Service::*FileTransferHandlerCpp)(FileTransfer *);

enum FileTransferStatus {
	XFER_STATUS_UNKNOWN,
	XFER_STATUS_QUEUED,
	XFER_STATUS_ACTIVE,
	XFER_STATUS_DONE
};

enum class TransferAck {
	NONE,
	UPLOAD,
	DOWNLOAD,
	BOTH,
};

enum class TransferPluginResult {
	Success = 0,
	Error = 1,
	InvalidCredentials = 2,
	TimedOut = 3,
	ExecFailed = 4,
};

namespace htcondor {
class DataReuseDirectory;
}


// `TransferType` is currently being used by should be `TransferDirection`.
enum class TransferClass {
	none = 0,
	input = 1,
	output = 2,
	checkpoint = 3,
};


class FileTransfer final: public Service {

  public:

	typedef std::vector< ClassAd > PluginResultList;
	struct PluginInvocation {
		// Record our intentions.
		TransferClass transfer_class;
		std::string plugin_basename;
		std::vector<std::string> schemes;

		// Whole-plugin results as observed by HTCondor.
		TransferPluginResult result;
		time_t duration_in_seconds;
		int exit_code;
		bool exit_by_signal;
		int exit_signal;

		// No plug-in actually outputs this yet.
		// ClassAd wholePluginResultAd;

		// The per-URL records produced by the plug-in.  These are
		// currently stored elsewhere (pluginResultList).
		// std::vector< ClassAd > perURLRecords;
	};
	// typedef std::vector< PluginInvocation > PluginResultList;

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
	int UploadCheckpointFiles( int checkpointNumber, bool blocking = true );

	/** @return 1 on success, 0 on failure */
	int UploadFailureFiles( bool blocking = true );

		/** For non-blocking (i.e., multithreaded) transfers, the registered
			handler function will be called on each transfer completion.  The
			handler can call FileTransfer::GetInfo() for statistics on the
			last transfer.  It is safe for the handler to deallocate the
			FileTransfer object.
		*/
	void RegisterCallback(FileTransferHandlerCpp handler, Service* handlerclass, bool want_status_updates=false)
		{ 
			ClientCallbackCpp = handler; 
			ClientCallbackClass = handlerclass;
			ClientCallbackWantsStatusUpdates = want_status_updates;
		}

	enum TransferType { NoType, DownloadFilesType, UploadFilesType };

	struct FileTransferInfo {

		void addSpooledFile(char const *name_in_spool);

		const char * dump(std::string & out, const char * verbose_indent=nullptr) const {
			const char * pre = " ", * post = ",";
			if (verbose_indent) { pre = verbose_indent; post = "\n"; }
			out += " "; out += (type==DownloadFilesType) ? "down" : (type==UploadFilesType ? "up" : "?"); out += post;
			out += pre; out += "success=" + std::to_string(success); out += post;
			out += pre; out += "in_progress=" + std::to_string(in_progress); out += post;
			out += pre; out += "status=" + std::to_string(xfer_status); out += post;
			out += pre; out += "bytes=" + std::to_string(bytes); out += post;
			if (hold_code) {
				out += pre; out += "hold=" + std::to_string(hold_code) + "/" + std::to_string(hold_subcode); out += post;
			}
			if ( ! error_desc.empty()) {
				out += pre; out += "err="; out += error_desc; out += post;
			}
			return out.c_str();
		}

		filesize_t bytes{0};
		time_t duration{0};
		TransferType type{NoType};
		bool success{true};
		bool in_progress{false};
		FileTransferStatus xfer_status{XFER_STATUS_UNKNOWN};
		bool try_again{true};
		int hold_code{0};
		int hold_subcode{0};
		std::map<std::string, filesize_t, classad::CaseIgnLTStr> protocol_bytes;
		ClassAd stats;
		std::string error_desc;
			// List of files we created in remote spool.
			// This is intended to become SpooledOutputFiles.
		std::string spooled_files;
		std::string tcp_stats;
	};

	FileTransferInfo GetInfo() {
		return r_Info;
	}

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

	uint64_t TotalBytesSent() const { return bytesSent; }

	uint64_t TotalBytesReceived() const { return bytesRcvd; };
	//
	// Add the given filename to the list of "output" files.  Will
	// create the empty list of output files if necessary; never
	// fails (unless the sytem is out of memory).
	//
	void addOutputFile( const char* filename );

	// Add the given filename to the list of "failure" files.
	void addFailureFile( const char* filename );

	// Check if we have failure files
	bool hasFailureFiles() const { return !FailureFiles.empty(); }

	//
	// Add the given path or URL to the list of checkpoint files.  The file
	// will be transferred to the named destination* in the sandbox.
	//
	// *: At present, the basename of the destination must be the same
	//    as the basename of the source.
	//
	// This function should only ever be
	// called by the shadow during initial file transfer to the starter.
	//
	// The caller must ensure that pathsAlreadyPreserved is empty the
	// first time and is preserved between calls.
	//
	void addCheckpointFile(
		const std::string & source, const std::string & destination,
		std::set< std::string > & pathsAlreadyPreserved
	);

	//
	// As addCheckpointFile(), but for input files.
	//
	void addInputFile(
		const std::string & source, const std::string & destination,
		std::set< std::string > & pathsAlreadyPreserved
	);

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
	bool addFileToExceptionList( const char *filename );

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

	void setRenameExecutable(bool rename_exec)
	{ PeerRenamesExecutable = rename_exec; }

	priv_state getDesiredPrivState( void ) { return desired_priv_state; };

	void setTransferQueueContactInfo(char const *contact);

	// returns nullptr on error, a Pointer to a member of the filetransfer_plugins vector on success
	FileTransferPlugin & InsertPlugin(std::string_view plugin_path, bool from_job);
	// lookup plugin by id, returns a ref to the Plugin for the given id, or a ref to the null plugin
	FileTransferPlugin & Plugin(int plugin_id) {
		if (plugin_id >= 0 && plugin_id < (int)plugin_ads.size()) { return plugin_ads[plugin_id]; }
		return null_plugin_ad;
	}
	// lookup plugin by path, returns a ref to the null plugin if not found
	FileTransferPlugin & Plugin(std::string_view path) { 
		auto found = plugin_ads_by_path.find(std::string(path));
		if (found != plugin_ads_by_path.end()) { return Plugin(found->second); }
		return null_plugin_ad;
	}
	// add an entry in the plugin_table for each schema/method that point back to the given plugin
	// optionally test the methods and report any methods the failed the testing (and thus were not added)
	void AddPluginMappings(const std::string& methods, FileTransferPlugin & plugin, bool test, std::string & failed_methods);
		// Run a test invocation of URL schema using plugin.  Will attempt to download
		// the URL specified by config param `schema`_TEST_URL to a temporary directory.
	bool TestPlugin(const std::string &schema, FileTransferPlugin & plugin);
		// Run a specific file transfer plugin, specified by `path`, to determine which schemas are
		// supported.  If `enable_testing` is true, then additionally test if the plugin is functional.
	void InsertPluginAndMappings( CondorError &e, const char* path, bool enable_testing );
		// Initialize and probe the plugins from the condor configuration.  If `enable_testing`
		// is set to true, then potentially test the plugins as well.
	int InitializeSystemPlugins(CondorError &e, bool enable_testing);
	int InitializeJobPlugins(const ClassAd &job, CondorError &e);
	int AddJobPluginsToInputFiles(const ClassAd &job, CondorError &e, std::vector<std::string> &infiles) const;
	FileTransferPlugin & DetermineFileTransferPlugin( CondorError &error, const char* source, const char* dest );
	TransferPluginResult InvokeFileTransferPlugin(CondorError &e, int &exit_code, const char* URL, const char* dest, ClassAd* plugin_stats, const char* proxy_filename = NULL);

	TransferPluginResult InvokeMultipleFileTransferPlugin(
		CondorError &e, int &exit_code, bool &exit_by_signal, int &exit_signal,
		FileTransferPlugin & plugin, const std::string &transfer_files_string,
		std::vector<ClassAd> & resultAds,
		const char* proxy_filename, bool do_upload
	);
	TransferPluginResult InvokeMultiUploadPlugin(
		FileTransferPlugin & plugin,
		int &exit_code, bool &exit_by_signal, int &exit_signal,
		const std::string &transfer_files_string, ReliSock &sock,
		bool send_trailing_eom, CondorError &err, long long &upload_bytes
	);

	void AggregateThisTransferStats( ClassAd &stats );
	filesize_t UpdateTransferStatsTotals(filesize_t cedar_total_bytes);
	filesize_t GetURLSizeBytes();
	int LogThisTransferStats( ClassAd &stats );
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
	static bool ExpandInputFileList( char const *input_list, char const *iwd, std::string &expanded_list, std::string &error_msg );

	// When downloading files, store files matching source_name as the name
	// specified by target_name.
	void AddDownloadFilenameRemap(char const *source_name,char const *target_name);

	// Add any number of download remaps, encoded in the form:
	// "source1 = target1; source2 = target2; ..."
	// or in other words, the format expected by the util function
	// filename_remap_find().
	void AddDownloadFilenameRemaps(const std::string &remaps);

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

	const std::vector<FileTransferPlugin>& getPlugins() const {
		return const_cast< const std::vector<FileTransferPlugin> & >(plugin_ads);
	}

	bool transferIsInProgress() const { return ActiveTransferTid != -1; }

	const std::unordered_map< std::string, std::string > & GetProxyByMethodMap() { return proxy_by_method; }

	const PluginResultList & getPluginResultList();

  protected:

	// Because FileTransferItem doesn't store the destination file name
	// (only the directory), this doesn't actually work right.
	void addSandboxRelativePath( const std::string & source,
		const std::string & destination, FileTransferList & ftl,
		std::set< std::string > & pathsAlreadyPreserved
	);

	// There's three places where we need to know this.
	bool shouldSendStderr();
	bool shouldSendStdout();

	int Download(ReliSock *s, bool blocking);
	int Upload(ReliSock *s, bool blocking);
	int Reap(int exit_status); // called by the static Reaper the reaping
	static int DownloadThread(void *arg, Stream *s);
	static int UploadThread(void *arg, Stream *s);
	int TransferPipeHandler(int p);
	bool ReadTransferPipeMsg();
	void UpdateXferStatus(FileTransferStatus status);
	bool SendPluginOutputAd( const ClassAd & plugin_output_ad );


		/** Actually download the files. Download and Upload above call these
			@return -1 on failure, bytes transferred otherwise
		*/
	filesize_t DoDownload(ReliSock *s);
	filesize_t DoUpload(ReliSock *s);
	filesize_t DoCheckpointUploadFromStarter(ReliSock * s);
	filesize_t DoCheckpointUploadFromShadow(ReliSock * s);
	filesize_t DoNormalUpload(ReliSock * s);

	typedef struct _ft_protocol_bits_struct {
		filesize_t peer_max_transfer_bytes = {-1};
		bool I_go_ahead_always = {false};
		bool peer_goes_ahead_always = {false};
		bool socket_default_crypto = {true};
	} _ft_protocol_bits;

	// Do the final computation of which files we'll be transferring.  This
	// _includes_ starting the file-transfer protocol and executing the
	// data reuse and S3 URL signing/conversion routines.
	//
	// This is the first half of the old DoUpload().
	int computeFileList(
	    ReliSock * s, FileTransferList & filelist,
	    std::unordered_set<std::string> & skip_files,
	    filesize_t & sandbox_size,
	    DCTransferQueue & xfer_queue,
	    _ft_protocol_bits & protocolState,
	    bool using_output_destination
	);

	// Execute the CEDAR file-transfer protocol and call plug-ins as
	// appropriate.
	//
	// This is the second half of the old DoUpload().
	filesize_t uploadFileList(
		ReliSock * s, const FileTransferList & filelist,
		std::unordered_set<std::string> & skip_files,
		const filesize_t & sandbox_size,
		DCTransferQueue & xfer_queue,
		_ft_protocol_bits & protocolState
	);

	double uploadStartTime{-1}, uploadEndTime{-1};
	double downloadStartTime{-1}, downloadEndTime{-1};

	void CommitFiles();

	void FindChangedFiles();
	void DetermineWhichFilesToSend();

#ifdef HAVE_HTTP_PUBLIC_FILES
	int AddInputFilenameRemaps(ClassAd *Ad);
#endif
	uint64_t bytesSent{0}, bytesRcvd{0};
	std::vector<std::string> InputFiles;

  private:

	PluginResultList pluginResultList;

	int checkpointNumber{-1};
	bool uploadCheckpointFiles{false};
	bool uploadFailureFiles{false};
	bool inHandleCommands{false};
	bool TransferFilePermissions{false};
	bool DelegateX509Credentials{false};
	bool PeerDoesTransferAck{false};
	bool PeerDoesGoAhead{false};
	bool PeerUnderstandsMkdir{false};
	bool PeerDoesXferInfo{false};
	bool PeerDoesReuseInfo{false};
	bool PeerDoesS3Urls{false};
	bool PeerRenamesExecutable{true};
	bool PeerKnowsProtectedURLs{false};
	bool TransferUserLog{false};
	char* Iwd{nullptr};
#ifdef WIN32
	std::vector<istring> ExceptionFiles;
#else
	std::vector<std::string> ExceptionFiles;
#endif
	std::vector<std::string> OutputFiles;
	std::vector<std::string> EncryptInputFiles;
	std::vector<std::string> EncryptOutputFiles;
	std::vector<std::string> DontEncryptInputFiles;
	std::vector<std::string> DontEncryptOutputFiles;
	std::vector<std::string> IntermediateFiles;
	std::vector<std::string>* FilesToSend{nullptr};
	std::vector<std::string>* EncryptFiles{nullptr};
	std::vector<std::string>* DontEncryptFiles{nullptr};

	std::vector<std::string> CheckpointFiles;
	std::vector<std::string> EncryptCheckpointFiles;
	std::vector<std::string> DontEncryptCheckpointFiles;
	std::vector<std::string> FailureFiles;

	char* OutputDestination{nullptr};
	char* SpooledIntermediateFiles{nullptr};
	char* ExecFile{nullptr};
	char* UserLogFile{nullptr};
	char* X509UserProxy{nullptr};
	std::string JobStdoutFile;
	std::string JobStderrFile;
	char* TransSock{nullptr};
	char* TransKey{nullptr};
	char* SpoolSpace{nullptr};
	std::string TmpSpoolSpace;
	int user_supplied_key{false};
	bool upload_changed_files{false};
	int m_final_transfer_flag{false};
	time_t last_download_time{0};
	FileCatalogHashTable last_download_catalog;
	int ActiveTransferTid{-1};
	time_t TransferStart{0};
	int TransferPipe[2] {-1, -1};
	bool registered_xfer_pipe{false};
	FileTransferHandlerCpp ClientCallbackCpp{nullptr};
	Service* ClientCallbackClass{nullptr};
	bool ClientCallbackWantsStatusUpdates{false};
	FileTransferInfo r_Info; // result info, used by top level stuff
	FileTransferInfo i_Info; // internal info, used by worker functions talking through a pipe
	FileTransferInfo & workInfo() { return (TransferPipe[1] < 0) ? r_Info : i_Info; }
	FileTransferPlugin null_plugin_ad{"", false, true}; // this is returned when we need to return a & and there is no plugin
	std::vector<FileTransferPlugin> plugin_ads;
	// map plugin path to entries in the table of plugins above
	std::map<std::string, int> plugin_ads_by_path;
	// map URL prefixes (methods) to entries in the table of plugins above
	PluginHashTable* plugin_table{nullptr};
	bool I_support_filetransfer_plugins{false};
	bool I_support_S3{false};
	bool multifile_plugins_enabled{false};
	bool m_has_protected_url{false};
#ifdef WIN32
	perm* perm_obj{nullptr};
#endif
	priv_state desired_priv_state{PRIV_UNKNOWN};
	bool want_priv_change{false};
	static TranskeyHashTable TranskeyTable;
	static TransThreadHashTable TransThreadTable;
	static int CommandsRegistered;
	static int SequenceNum;
	static int ReaperId;
	static bool ServerShouldBlock;
	int clientSockTimeout{30};
	bool did_init{false};
	bool simple_init{true};
	ReliSock *simple_sock{nullptr};
	ReliSock *m_syscall_socket{nullptr};
	std::string download_filename_remaps;
	bool m_use_file_catalog{true};
	TransferQueueContactInfo m_xfer_queue_contact_info;
	std::string m_jobid; // what job we are working on, for informational purposes
	char *m_sec_session_id{nullptr};
	std::string m_cred_dir;
	std::string m_job_ad;
	std::string m_machine_ad;
	filesize_t MaxUploadBytes{-1};  // no limit by default
	filesize_t MaxDownloadBytes{-1};

	// stores the path to the proxy after one is received
	std::string LocalProxyName;

	// called to construct the catalog of files in a direcotry
	bool BuildFileCatalog(time_t spool_time = 0, const char* iwd = NULL, FileCatalogHashTable *catalog = NULL);

	// called to lookup the catalog entry of file
	bool LookupInFileCatalog(const char *fname, time_t *mod_time, filesize_t *filesize);

	// Called internally by DoUpload() in order to handle common wrapup tasks.
	int
	ExitDoUpload(ReliSock *s, bool socket_default_crypto, priv_state saved_priv, DCTransferQueue & xfer_queue, filesize_t bytes, UploadExitInfo& xfer_info);

	// Send acknowledgment of success/failure after downloading files.
	void SendTransferAck(Stream *s,bool success,bool try_again,int hold_code,int hold_subcode,char const *hold_reason);

	// Receive acknowledgment of success/failure after downloading files.
	void GetTransferAck(Stream *s,bool &success,bool &try_again,int &hold_code,int &hold_subcode,std::string &error_desc);

	// Stash transfer success/failure info that will be propagated back to
	// caller of file transfer operation, using GetInfo().
	void SaveTransferInfo(bool success,bool try_again,int hold_code,int hold_subcode,char const *hold_reason);

	// Receive message indicating that the peer is ready to receive the file
	// and save failure information with SaveTransferInfo().
	bool ReceiveTransferGoAhead(Stream *s,char const *fname,bool downloading,bool &go_ahead_always,filesize_t &peer_max_transfer_bytes);

	// Receive message indicating that the peer is ready to receive the file.
	bool DoReceiveTransferGoAhead(Stream *s,char const *fname,bool downloading,bool &go_ahead_always,filesize_t &peer_max_transfer_bytes,bool &try_again,int &hold_code,int &hold_subcode,std::string &error_desc, int alive_interval);

	// Obtain permission to receive a file download and then tell our
	// peer to go ahead and send it.
	// Save failure information with SaveTransferInfo().
	bool ObtainAndSendTransferGoAhead(DCTransferQueue &xfer_queue,bool downloading,Stream *s,filesize_t sandbox_size,char const *full_fname,bool &go_ahead_always);

	bool DoObtainAndSendTransferGoAhead(DCTransferQueue &xfer_queue,bool downloading,Stream *s,filesize_t sandbox_size,char const *full_fname,bool &go_ahead_always,bool &try_again,int &hold_code,int &hold_subcode,std::string &error_desc);

	std::string GetTransferQueueUser();

	void ReceiveAliveMessage();

	// Report information about completed transfer from child thread.
	bool WriteStatusToTransferPipe(filesize_t total_bytes);
	ClassAd jobAd;

	//
	// As of this writing, this function should only ever be called from
	// DoUpload().  It converts from a std::vector<std::string> of entries to a
	// a FileTransferList, a std::vector of FileTransferItems.  The
	// FileTransferList is a sequence of commands, and should not be
	// reordered except by operator < (and probably not even then),
	// because -- for example -- directories must be created before the
	// file that live in them.
	//
	bool ExpandFileTransferList( std::vector<std::string> *input_list, FileTransferList &expanded_list, bool preserveRelativePaths, const char* queue = nullptr );

		// This function generates a list of files to transfer, including
		// directories to create and their full contents.
		// Arguments:
		// src_path  - the path of the file to be transferred
		// dest_dir  - the directory to write the file to
		// iwd       - relative paths are relative to this path
		// max_depth - how deep to recurse (-1 for infinite)
		// expanded_list - the list of files to transfer
		// preserve_relative_paths - if false, use the old behavior and
		//      transfer src_path to its basename. if true, and
		//      src_path is a relative path, transfer it to the same
		//      relative path
		// SpoolSpace - if preserve_relative_paths is set, then a
		//      src_path which begins with this path is treated as a
		//      relative path.  This allows us to preserve the relative
		//      paths of files stored in SPOOL, whether from
		//      self-checkpointing, ON_EXIT_OR_EVICT, or remote input
		//      file spooling.
	static bool ExpandFileTransferList( char const *src_path, char const *dest_dir, char const *iwd, int max_depth, FileTransferList &expanded_list, bool preserveRelativePaths, char const *SpoolSpace, std::set<std::string> & pathsAlreadyPreserved, const char* queue = nullptr);

		// Function internal to ExpandFileTransferList() -- called twice there.
		// The SpoolSpace argument is only necessary because this function
		// calls back into  ExpandFileTransferList(); see that function
		// for details.
	static bool ExpandParentDirectories( const char *src_path, const char *iwd, FileTransferList & expanded_list, const char *SpoolSpace, std::set<std::string> & pathsAlreadyPreserved );

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

    // We need a little more control over checkpoint files than we do
    // for normal input URLs.  Because of the design of the rest of the
    // code, this list still can't directly specify an arbitrary file
    // name for a URL, but we don't need that for checkpoints.
    //
    // (It might be possible to _fake_ such a targeting using input file
    // but I'm quite sure we don't tell the multi-file downloaders about
    // the remaps, which means it's impossible in the general case to
    // collisions.)
    FileTransferList checkpointList;

    FileTransferList inputList;

    std::unordered_map< std::string, std::string > proxy_by_method;
};

class UploadExitInfo {
public:
	friend class FileTransfer;
	UploadExitInfo() = default;

	UploadExitInfo & doAck(TransferAck newAck) { ack = newAck; return *this; }
	UploadExitInfo & retry() { try_again = true; return *this; }
	UploadExitInfo & success() { upload_success = true; return *this; }
	UploadExitInfo & line(int line) { exit_line = line; return *this; }
	UploadExitInfo & files(int count) { xfered_files = count; return *this; }
	UploadExitInfo & setError(const std::string& desc, int code, int subcode = -1) {
		error_desc = desc;
		hold_code = code;
		hold_subcode = subcode;
		return *this;
	}
	UploadExitInfo & noError() {
		error_desc.clear();
		hold_code = 0;
		hold_subcode = 0;
		return *this;
	}

    const std::string & getErrorDescription() { return error_desc; }
    void setErrorDescription( const std::string & desc ) { error_desc = desc; }

	bool checkAck(TransferAck check) { return ack == TransferAck::BOTH || ack == check; }

	const char* ackStr() {
		if (ack == TransferAck::NONE) { return "NONE"; }
		else if (ack == TransferAck::UPLOAD) { return "UPLOAD"; }
		else if (ack == TransferAck::DOWNLOAD) { return "DOWNLOAD"; }
		else if (ack == TransferAck::BOTH) { return "BOTH"; }
		else { return "UNKOWN"; }
	}

	std::string displayStr() {
		std::string temp;
		formatstr(temp, "Success = %s | Error[%d.%d] = '%s' | Ack = %s | Line = %d | Files = %d | Retry = %s",
		          upload_success ? "True" : "False", hold_code, hold_subcode, error_desc.c_str(),
		          ackStr(), exit_line, xfered_files, try_again ? "True" : "False");
		return temp;
	}

private:
	std::string error_desc = "";
	int hold_code = -1;
	int hold_subcode = -1;
	TransferAck ack = TransferAck::NONE;
	int exit_line = 0;
	int xfered_files = 0;
	bool upload_success = false;
	bool try_again = false;
};

// returns 0 if no expiration
time_t GetDesiredDelegatedJobCredentialExpiration(ClassAd *job);

// returns 0 if renewal of lifetime is not needed
time_t GetDelegatedProxyRenewalTime(time_t expiration_time);

void GetDelegatedProxyRenewalTime(ClassAd *jobAd);

#endif

