/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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


#include "condor_common.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "file_transfer.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "basename.h"
#include "directory.h"
#include "condor_config.h"
#include "spooled_job_files.h"
#include "util_lib_proto.h"
#include "daemon.h"
#include "daemon_types.h"
#include "nullfile.h"
#include "condor_ver_info.h"
#include "globus_utils.h"
#include "filename_tools.h"
#include "condor_holdcodes.h"
#include "mk_cache_links.h"
#include "subsystem_info.h"
#include "condor_url.h"
#include "my_popen.h"
#include "file_transfer_stats.h"
#include "utc_time.h"
#define HAVE_DATE_REUSE_DIR 1 // TODO: disable and remove data reuse hooks in file transfer
#ifdef HAVE_DATA_REUSE_DIR
#include "data_reuse.h"
#endif
#include "AWSv4-utils.h"
#include "AWSv4-impl.h"
#include "condor_random_num.h"
#include "condor_sys.h"
#include "limit_directory_access.h"
#include "checksum.h"
#include "shortfile.h"
#include "fcloser.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>

const char * const StdoutRemapName = "_condor_stdout";
const char * const StderrRemapName = "_condor_stderr";

// Transfer commands are sent from the upload side to the download side.
// 0 - finished
// 1 - use socket default (on or off) for next file
// 2 - force encryption on for next file.
// 3 - force encryption off for next file.
// 4 - do an x509 credential delegation (using the socket default)
// 5 - send a URL and have the download side fetch it
// 6 - send a request to make a directory
// 999 - send a classad telling what to do.
//
// 999 subcommands (999 is followed by a filename and then a ClassAd):
// 7 - ClassAd contains information about a URL upload performed by
//     the upload side.
// 8 - ClassAd contains information about a list of files which will be
//     sent later that may be eligible for reuse.  This is command requires
//     a response indicating if the download side already has one of the
//     files available.
// 9 - ClassAd contains a list of URLs that need to be signed for the uploader
//     to proceed.
enum class TransferCommand {
	Unknown = -1,
	Finished = 0,
	XferFile = 1,
	EnableEncryption = 2,
	DisableEncryption = 3,
	XferX509 = 4,
	DownloadUrl = 5,
	Mkdir = 6,
	Other = 999
};

enum class TransferSubCommand {
	Unknown = -1,
	UploadUrl = 7,
	ReuseInfo = 8,
	SignUrls = 9
};

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
bool FileTransfer::ServerShouldBlock = true;

const char PLUGIN_OUTPUT_AD = 2;
const char FINAL_UPDATE_XFER_PIPE_CMD = 1;
const char IN_PROGRESS_UPDATE_XFER_PIPE_CMD = 0;

/**
 * The `FileTransferItem` represents a single work item for the DoUpload
 * side of the file transfer obejct to perform.
 *
 * All state information about the file transfer should be kept here.
 *
 * Importantly, the FileTransferItem implements the `<` operator, allowing
 * it to be sorted in a list.  This allows, for example, all the CEDAR-based
 * transfers to be performed prior to the non-CEDAR transfers.
 */
class FileTransferItem {
public:
	const std::string &srcName() const { return m_src_name; }
	const std::string &destDir() const { return m_dest_dir; }
	const std::string &destUrl() const { return m_dest_url; }
	const std::string &srcScheme() const { return m_src_scheme; }
	const std::string &xferQueue() const { return m_xfer_queue; }
	filesize_t fileSize() const { return m_file_size; }
	void setDestDir(const std::string &dest) { m_dest_dir = dest; }
	void setXferQueue(const std::string &queue) { m_xfer_queue = queue; }
	void setFileSize(filesize_t new_size) { m_file_size = new_size; }
	void setDomainSocket(bool value) { is_domainsocket = value; }
	void setSymlink(bool value) { is_symlink = value; }
	void setDirectory(bool value) { is_directory = value; }
	bool isDomainSocket() const {return is_domainsocket;}
	bool isSymlink() const {return is_symlink;}
	bool isDirectory() const {return is_directory;}
	bool isSrcUrl() const {return !m_src_scheme.empty();}
	bool isDestUrl() const {return !m_dest_scheme.empty();}
	bool hasQueue() const { return !m_xfer_queue.empty(); }
	condor_mode_t fileMode() const {return m_file_mode;}
	void setFileMode(condor_mode_t new_mode) {m_file_mode = new_mode;}

	void setSrcName(const std::string &src) {
		m_src_name = src;
		const char *scheme_end = IsUrl(src.c_str());
		if (scheme_end) {
			m_src_scheme = std::string(src.c_str(), scheme_end - src.c_str());
		}
	}

	void setDestUrl(const std::string &dest_url) {
		m_dest_url = dest_url;
		const char *scheme_end = IsUrl(dest_url.c_str());
		if (scheme_end) {
			m_dest_scheme = std::string(dest_url.c_str(), scheme_end - dest_url.c_str());
		}
	}

    //
    // This function is used by std::stable_sort() in FileTransfer::DoUpload()
    // to group transfers.  This function used to also sort all transfers,
    // but it no longer does; now it only sorts URLs.  (It should probably stop
    // doing that, too, but that's another ticket.)  This is because
    // ExpandeFileTransferList() -- which converts a list of source names
    // into a std::vector<FileTransferItem> -- creates new non-URL
    // FileTransferItems when it "expands" the name of a directory into
    // the FileTransferItem which creates the directory and the
    // FileTransferItems which populate the directory.  Obviously, creating
    // the directory must come before populating it.
    //
    // Before, this almost always happened because the "source" of a directory
    // would always sort before the source of any of the files in the
    // directory.  However, after HTCONDOR-583 fixed a problem where file
    // transfer would ignore directories in SPOOL, it became very easy to
    // write job submit files which would have two different "sources" (the
    // SPOOL and the input directory) for the same directory.  When
    // ExpandFileTransferList() removes duplicate directories, it removes
    // the directories which appear later in the list, because the directory
    // needs to be created before any files in it (and the input directory,
    // for example, could have a file in it that's not in SPOOL).  Sorting
    // the list of non-URL FileTransferItems may undo this ordering (for
    // instance, if SPOOL sorts before the IWD, a file from SPOOL would be
    // transferred before the directory "sourced" in IWD would be created,
    // because transfers from IWD are listed first (to make sure that files
    // in SPOOL win)).
    //

	bool operator<(const FileTransferItem &other) const {
		// Ordering of transfers:
		// - Destination URLs first (allows these plugins to alter CEDAR transfers on
		//   stageout)
		// - CEDAR-based transfers (move any credentials prior to source URLs; assume
		//   credentials are already present for stageout).
		// - Source URLs last.
		//      - Protected URLs (require a transfer queues permission)
		//      - All other source URLs

		auto is_dest_url = !m_dest_scheme.empty();
		auto other_is_dest_url = !other.m_dest_scheme.empty();
		if (is_dest_url && !other_is_dest_url) {
			return true;
		}
		if (!is_dest_url && other_is_dest_url) {
			return false;
		}
		if (is_dest_url) {
			if (m_dest_scheme == other.m_dest_scheme) {
				return false;
			} else {
				return m_dest_scheme < other.m_dest_scheme;
			}
		}

		auto is_src_url = !m_src_scheme.empty();
		auto other_is_src_url = !other.m_src_scheme.empty();
		if (is_src_url && !other_is_src_url) {
			return false;
		}
		if (!is_src_url && other_is_src_url) {
			return true;
		}
		if (is_src_url) { // Both are URLs
			// Check if src has specified queue for permissions
			if (hasQueue() && !other.hasQueue()) {
				return true;
			} else if (!hasQueue() && other.hasQueue()) {
				return false;
			} else if (hasQueue() && other.hasQueue() && m_xfer_queue != other.m_xfer_queue) {
				return m_xfer_queue < other.m_xfer_queue;
			}
			if (m_src_scheme == other.m_src_scheme) {
				return false;
			} else {
				return m_src_scheme < other.m_src_scheme;
			}
		}
		return false;
	}

private:
	std::string m_src_scheme;
	std::string m_dest_scheme;
	std::string m_src_name;
	std::string m_dest_dir;
	std::string m_dest_url;
	std::string m_xfer_queue;
	bool is_domainsocket{false};
	bool is_directory{false};
	bool is_symlink{false};
	condor_mode_t m_file_mode{NULL_FILE_PERMISSIONS};
	filesize_t m_file_size{0};
};

void
dPrintFileTransferList( int flags, const FileTransferList & list, const std::string & header ) {
	std::string message = header;
	for( auto & i: list ) {
		formatstr_cat( message, " %s -> '%s' [%s],",
			i.srcName().c_str(), i.destDir().c_str(), i.destUrl().c_str()
		);
	}
	// Don't print the trailing comma.
	if( message[message.size() - 1] == ',' ) {
	    message.erase(message.size() - 1);
    }
	dprintf( flags, "%s\n", message.c_str() );
}

const int GO_AHEAD_FAILED = -1; // failed to contact transfer queue manager
const int GO_AHEAD_UNDEFINED = 0;
//const int GO_AHEAD_ONCE = 1;    // send one file and ask again
				// Currently, there is no usage of GO_AHEAD_ONCE; if we have a
				// token, we assume it lasts forever.

const int GO_AHEAD_ALWAYS = 2;  // send all files without asking again


struct upload_info {
	FileTransfer *myobj;
};

struct download_info {
	FileTransfer *myobj;
};

FileTransfer::FileTransfer()
{
}

FileTransfer::~FileTransfer()
{
	dprintf(D_ZKM, "FileTransfer destructor %p daemonCore=%p\n", this, daemonCore);
	if (daemonCore && ActiveTransferTid >= 0) {
		dprintf(D_ALWAYS, "FileTransfer object destructor called during "
				"active transfer.  Cancelling transfer.\n");
		abortActiveTransfer();
	}
	if (daemonCore && (TransferPipe[0] >= 0)) {
		if( registered_xfer_pipe ) {
			registered_xfer_pipe = false;
			daemonCore->Cancel_Pipe(TransferPipe[0]);
		}
		daemonCore->Close_Pipe(TransferPipe[0]);
	}
	if (daemonCore && (TransferPipe[1] >= 0)) daemonCore->Close_Pipe(TransferPipe[1]);
	if (Iwd) free(Iwd);
	if (ExecFile) free(ExecFile);
	if (UserLogFile) free(UserLogFile);
	if (X509UserProxy) free(X509UserProxy);
	if (SpoolSpace) free(SpoolSpace);
	if (OutputDestination) free(OutputDestination);
	if (SpooledIntermediateFiles) free(SpooledIntermediateFiles);
	// Note: do _not_ delete FileToSend!  It points to OutputFile or Intermediate.
	if (last_download_catalog) {
		// iterate through and delete entries
		CatalogEntry *entry_pointer;
		last_download_catalog->startIterations();
		while(last_download_catalog->iterate(entry_pointer)) {
			delete entry_pointer;
		}
		delete last_download_catalog;
	}
	if (TransSock) free(TransSock);
	stopServer();
	// Do not delete the TransThreadTable. There may be other FileTransfer
	// objects out there planning to use it.
	//if( TransThreadTable && TransThreadTable->getNumElements() == 0 ) {
	//	delete TransThreadTable;
	//	TransThreadTable = NULL;
	//}
#ifdef WIN32
	if (perm_obj) delete perm_obj;
#endif
	free(m_sec_session_id);
	delete plugin_table;
}

inline bool
FileTransfer::shouldSendStdout() {
	bool streaming = false;
	jobAd.LookupBool( ATTR_STREAM_OUTPUT, streaming );
	if( ! streaming && ! nullFile( JobStdoutFile.c_str() ) ) { return true; }
	return false;
}

inline bool
FileTransfer::shouldSendStderr() {
	bool streaming = false;
	jobAd.LookupBool( ATTR_STREAM_ERROR, streaming );
	if( ! streaming && ! nullFile( JobStderrFile.c_str() ) ) { return true; }
	return false;
}

int
FileTransfer::SimpleInit(ClassAd *Ad, bool want_check_perms, bool is_server,
						 ReliSock *sock_to_use, priv_state priv,
						 bool use_file_catalog, bool is_spool)
{
	char buf[ATTRLIST_MAX_EXPRESSION];
	char *dynamic_buf = NULL;
	std::string buffer;

	jobAd = *Ad;	// save job ad

	if( did_init ) {
			// no need to except, just quietly return success
		return 1;
	}

	user_supplied_key = is_server ? FALSE : TRUE;

	dprintf(D_FULLDEBUG,"entering FileTransfer::SimpleInit\n");

	/* in the case of SimpleInit being called inside of Init, this will
		simply assign the same value to itself. */
	m_use_file_catalog = use_file_catalog;

	desired_priv_state = priv;
    if ( priv == PRIV_UNKNOWN ) {
        want_priv_change = false;
    } else {
        want_priv_change = true;
    }

	simple_sock = sock_to_use;

	// user must give us an initial working directory.
	if (Ad->LookupString(ATTR_JOB_IWD, buf, sizeof(buf)) != 1) {
		dprintf(D_FULLDEBUG,
			"FileTransfer::SimpleInit: Job Ad did not have an iwd!\n");
		return 0;
	}
	Iwd = strdup(buf);

	// if the user want us to check file permissions, pull out the Owner
	// from the classad and instantiate a perm object.
	if ( want_check_perms ) {
		if (Ad->LookupString(ATTR_OWNER, buf, sizeof(buf)) != 1) {
			// no owner specified in ad
			dprintf(D_FULLDEBUG,
				"FileTransfer::SimpleInit: Job Ad did not have an owner!\n");
			return 0;
		}
#ifdef WIN32
		// lookup the domain
		char ntdomain[80];
		char *p_ntdomain = ntdomain;
		if (Ad->LookupString(ATTR_NT_DOMAIN, ntdomain, sizeof(ntdomain)) != 1) {
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
	m_reuse_info.clear();
	m_reuse_info_err.clear();

	// Set InputFiles to be ATTR_TRANSFER_INPUT_FILES plus
	// ATTR_JOB_INPUT, ATTR_JOB_CMD, and ATTR_ULOG_FILE if simple_init.
	dynamic_buf = NULL;
	if (Ad->LookupString(ATTR_TRANSFER_INPUT_FILES, &dynamic_buf) == 1) {
		InputFiles = split(dynamic_buf, ",");
		free(dynamic_buf);
		dynamic_buf = NULL;
	}

	// Check for protected input queue list attribute
	ExprTree *tree = Ad->Lookup(ATTR_TRANSFER_Q_URL_IN_LIST);
	if (tree) {
		if (tree->GetKind() == ClassAd::ExprTree::EXPR_LIST_NODE) {
			m_has_protected_url = true;
		} else {
			dprintf(D_FULLDEBUG, "FileTransfer::SimpleInit: Job Ad attribute %s is not type list node.\n",
			        ATTR_TRANSFER_Q_URL_IN_LIST);
			return 0;
		}
	}

	std::vector<std::string> PubInpFiles;
	if (Ad->LookupString(ATTR_PUBLIC_INPUT_FILES, &dynamic_buf) == 1) {
		// Add PublicInputFiles to InputFiles list.
		// If these files will be transferred via web server cache,
		// they will be removed from InputFiles.
		PubInpFiles = split(dynamic_buf, ",");
		free(dynamic_buf);
		dynamic_buf = NULL;
		for (auto& path : PubInpFiles) {
			if (!file_contains(InputFiles, path))
				InputFiles.emplace_back(path);
		}
	}
	if (Ad->LookupString(ATTR_JOB_INPUT, buf, sizeof(buf)) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(buf) ) {
			if ( !file_contains(InputFiles, buf) )
				InputFiles.emplace_back(buf);
		}
	}

	// If we are spooling, we want to ignore URLs
	// We want the file transfer plugin to be invoked at the starter, not the schedd.
	// See https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=2162
	if (IsClient() && simple_init && is_spool) {
		std::erase_if(InputFiles, [](auto& f) {return IsUrl(f.c_str());});

			// We want to spool the manifest file from client to schedd on
			// submit; this way, the reuse information is available for job startup
		std::string manifest_file;
		if (jobAd.EvaluateAttrString("DataReuseManifestSHA256", manifest_file))
		{
			if (!file_contains(InputFiles, manifest_file))
				InputFiles.emplace_back(manifest_file);
		}
		if (!ParseDataManifest()) {
			m_reuse_info.clear();
		}
			// If we need to reuse data to the worker, we might also benefit from
			// not spooling when reuse is an option.
		for (const auto &info : m_reuse_info) {
			if (!file_contains(InputFiles, info.filename()))
				InputFiles.emplace_back(info.filename());
		}


		std::string list = join(InputFiles, ",");
		dprintf(D_FULLDEBUG, "Input files: %s\n", list.c_str());
	}
#ifdef HAVE_HTTP_PUBLIC_FILES
	else if (IsServer() && !is_spool && param_boolean("ENABLE_HTTP_PUBLIC_FILES", false)) {
		// For files to be cached, change file names to URLs
		ProcessCachedInpFiles(Ad, InputFiles, PubInpFiles);
	}
#endif

	if ( Ad->LookupString(ATTR_ULOG_FILE, buf, sizeof(buf)) == 1 ) {
		UserLogFile = strdup(condor_basename(buf));
		// For 7.5.6 and earlier, we want to transfer the user log as
		// an input file if we're in condor_submit. Otherwise, we don't.
		// At this point, we don't know what version our peer is,
		// so we have to delay this decision until UploadFiles().
	}
	if ( Ad->LookupString(ATTR_X509_USER_PROXY, buf, sizeof(buf)) == 1 ) {
		X509UserProxy = strdup(buf);
			// add to input files
		if ( !nullFile(buf) ) {
			if ( !file_contains(InputFiles, buf) )
				InputFiles.emplace_back(buf);
		}
	}
	if ( Ad->LookupString(ATTR_OUTPUT_DESTINATION, buf, sizeof(buf)) == 1 ) {
		OutputDestination = strdup(buf);
		dprintf(D_FULLDEBUG, "FILETRANSFER: using OutputDestination %s\n", buf);
	}

	// there are a few places below where we need the value of the SPOOL
	// knob if we're the server. we param for it once here, and free it
	// at the end of this function
	//
	char* Spool = NULL;
	if ( IsServer() ) {
		Spool = param("SPOOL");
	}

	// if we're the server, initialize the SpoolSpace and TmpSpoolSpace
	// member variables
	//
	int Cluster = 0;
	int Proc = 0;
	Ad->LookupInteger(ATTR_CLUSTER_ID, Cluster);
	Ad->LookupInteger(ATTR_PROC_ID, Proc);
	formatstr(m_jobid, "%d.%d", Cluster, Proc);
	if ( IsServer() && Spool ) {

		SpooledJobFiles::getJobSpoolPath(Ad, buffer);
		SpoolSpace = strdup(buffer.c_str());
		formatstr(TmpSpoolSpace,"%s.tmp",SpoolSpace);
	}

	Ad->LookupString(ATTR_JOB_CMD, buffer);
	if ( (IsServer() || (IsClient() && simple_init)) )
	{
		// TODO: If desired_priv_state isn't PRIV_UNKNOWN, shouldn't
		//   we switch priv state for these file checks?

		// stash the executable name for comparison later, so
		// we know that this file should be called condor_exec on the
		// client machine.  if an executable for this cluster exists
		// in the spool dir, use it instead.

		// Only check the spool directory if we're the server.
		// Note: This will break Condor-C jobs if the executable is ever
		//   spooled the old-fashioned way (which doesn't happen currently).
		if ( IsServer() && Spool ) {
			ExecFile = GetSpooledExecutablePath(Cluster, Spool);
			if ( access(ExecFile,F_OK | X_OK) < 0 ) {
				free(ExecFile); ExecFile = NULL;
			}
		}

		if ( !ExecFile ) {
			// apparently the executable is not in the spool dir.
			// so we must make certain the user has permission to read
			// this file; if so, we can record it as the Executable to send.
#ifdef WIN32
			// buffer doesn't refer to a real file when this code is executed in the SCHEDD when spooling
			// so instead of failing here, we just don't bother with the access test in that case.
			if ( !simple_init && perm_obj && (perm_obj->read_access(buffer.c_str()) != 1) ) {
				// we do _not_ have permission to read this file!!
				dprintf(D_ALWAYS,
				        "FileTrans: permission denied reading %s\n",buffer.c_str());
				return 0;
			}
#endif
			ExecFile = strdup(buffer.c_str());
		}

		// If we don't already have this on our list of things to transfer,
		// and we haven't set TRANSFER_EXECTUABLE to false, send it along.
		// If we didn't set TRANSFER_EXECUTABLE, default to true

		bool xferExec;
		if(!Ad->LookupBool(ATTR_TRANSFER_EXECUTABLE,xferExec)) {
			xferExec=true;
		}

		if ( xferExec && !file_contains(InputFiles, ExecFile) &&
			 !file_contains(PubInpFiles, ExecFile)) {
			// Don't add exec file if it already is in cached list
			InputFiles.emplace_back(ExecFile);
		}

		// Special case for condor_submit -i 
		std::string OrigExecFile;
		Ad->LookupString(ATTR_JOB_ORIG_CMD, OrigExecFile);
		if ( !OrigExecFile.empty() && !file_contains(InputFiles, OrigExecFile) && !file_contains(PubInpFiles, OrigExecFile)) {
			// Don't add origexec file if it already is in cached list
			InputFiles.emplace_back(OrigExecFile);
		}
	} else if ( IsClient() && !simple_init ) {
		ExecFile = strdup( condor_basename(buffer.c_str()) );
	}

	// Set OutputFiles to be ATTR_SPOOLED_OUTPUT_FILES if specified, otherwise
	// set OutputFiles to be ATTR_TRANSFER_OUTPUT_FILES if specified.
	// If not specified, set it to send whatever files have changed.
	// Also add in ATTR_JOB_OUPUT plus ATTR_JOB_ERROR, if we're not
	// streaming them, and if we're using a fixed list of output
	// files.
	dynamic_buf = NULL;
	if (Ad->LookupString(ATTR_SPOOLED_OUTPUT_FILES, &dynamic_buf) == 1 ||
		Ad->LookupString(ATTR_TRANSFER_OUTPUT_FILES, &dynamic_buf) == 1)
	{
		OutputFiles = split(dynamic_buf, ",");
		free(dynamic_buf);
		dynamic_buf = NULL;
	} else {
		// send back new/changed files after the run
		upload_changed_files = true;
	}

	if( Ad->LookupString( ATTR_JOB_OUTPUT, JobStdoutFile ) ) {
		if( (! upload_changed_files) && shouldSendStdout() ) {
			if(! file_contains( OutputFiles, JobStdoutFile )) {
				OutputFiles.emplace_back( JobStdoutFile );
			}
		}
	}

	if( Ad->LookupString( ATTR_JOB_ERROR, JobStderrFile ) ) {
		if( (! upload_changed_files) && shouldSendStderr() ) {
			if(! file_contains( OutputFiles, JobStderrFile )) {
				OutputFiles.emplace_back( JobStderrFile );
			}
		}
	}

		// add the spooled user log to the list of files to xfer
		// (i.e. when sending output to condor_transfer_data)
	std::string ulog;
	if( jobAd.LookupString(ATTR_ULOG_FILE,ulog) ) {
		if( outputFileIsSpooled(ulog.c_str()) ) {
			if( !file_contains(OutputFiles, ulog) ) {
				OutputFiles.emplace_back(ulog);
			}
		}
	}

	// Set EncryptInputFiles to be ATTR_ENCRYPT_INPUT_FILES if specified.
	if (Ad->LookupString(ATTR_ENCRYPT_INPUT_FILES, buf, sizeof(buf)) == 1) {
		EncryptInputFiles = split(buf, ",");
	}

	// Set EncryptOutputFiles to be ATTR_ENCRYPT_OUTPUT_FILES if specified.
	if (Ad->LookupString(ATTR_ENCRYPT_OUTPUT_FILES, buf, sizeof(buf)) == 1) {
		EncryptOutputFiles = split(buf, ",");
	}

	// Set DontEncryptInputFiles to be ATTR_DONT_ENCRYPT_INPUT_FILES if specified.
	if (Ad->LookupString(ATTR_DONT_ENCRYPT_INPUT_FILES, buf, sizeof(buf)) == 1) {
		DontEncryptInputFiles = split(buf, ",");
	}

	// Set DontEncryptOutputFiles to be ATTR_DONT_ENCRYPT_OUTPUT_FILES if specified.
	if (Ad->LookupString(ATTR_DONT_ENCRYPT_OUTPUT_FILES, buf, sizeof(buf)) == 1) {
		DontEncryptOutputFiles = split(buf, ",");
	}

	if (Ad->LookupString(ATTR_FAILURE_FILES, buf, sizeof(buf)) == 1) {
		FailureFiles = split(buf, ",");

		if( shouldSendStdout() ) {
			if(! file_contains(FailureFiles, JobStdoutFile) ) {
				FailureFiles.emplace_back( JobStdoutFile );
			}
		}

		if( shouldSendStderr() ) {
			if(! file_contains(FailureFiles, JobStderrFile) ) {
				FailureFiles.emplace_back( JobStderrFile );
			}
		}
	}

	// We need to know whether to apply output file remaps or not.
	// The case where we want to apply them is when we are the shadow
	// or anybody else who is writing the files to their final
	// location.  We do not want to apply them if we are the shadow
	// and this job was submitted with 'condor_submit -s' or something
	// similar, because we are writing to the spool directory, and the
	// filenames in the spool directory should be the same as they are
	// in the execute dir, or we have trouble when we try to write
	// files that get mapped to subdirectories within the spool
	// directory.

	// Unfortunately, we can't tell for sure whether we are a client
	// who should be doing remaps, so clients who do want it (like
	// condor_transfer_data), should explicitly call
	// InitDownloadFilenameRemaps().

	bool spooling_output = false;
	{
		if (Iwd && Spool) {
			if(!strncmp(Iwd,Spool,strlen(Spool))) {
				// We are in the spool directory.
				// Wish there was a better way to find this out!
				spooling_output = true;
			}
		}
	}

	if(!spooling_output) {
		if(IsServer()) {
			if(!InitDownloadFilenameRemaps(Ad)) return 0;
		}
#ifdef HAVE_HTTP_PUBLIC_FILES
		else if( !simple_init ) {
			// Only add input remaps for starter receiving
			AddInputFilenameRemaps(Ad);
		}
#endif
	}

	// get plugin configuration (enabled/disabled)
	DoPluginConfiguration();

	// if there are job plugins, add them to the list of input files.
	CondorError e;
	AddJobPluginsToInputFiles(*Ad, e, InputFiles);

	int spool_completion_time = 0;
	Ad->LookupInteger(ATTR_STAGE_IN_FINISH,spool_completion_time);
	last_download_time = spool_completion_time;
	if(IsServer()) {
		BuildFileCatalog(last_download_time);
	} else {
		BuildFileCatalog();
	}

	if ( Spool ) {
		free(Spool);
	}

	did_init = true;
	return 1;
}

int
FileTransfer::InitDownloadFilenameRemaps(ClassAd *Ad) {
	std::string remap_fname;
	std::string ulog_fname;

	dprintf(D_FULLDEBUG,"Entering FileTransfer::InitDownloadFilenameRemaps\n");

	download_filename_remaps = "";
	if(!Ad) return 1;

	// when downloading files from the job, apply output name remaps
	if (Ad->LookupString(ATTR_TRANSFER_OUTPUT_REMAPS,remap_fname)) {
		AddDownloadFilenameRemaps(remap_fname.c_str());
	}

	// If a client is receiving spooled output files which include a
	// user job log file with a directory component, add a remap.
	// Otherwise, the user log will end up in the iwd, which is wrong.
	if (IsClient() && Ad->LookupString(ATTR_ULOG_FILE, ulog_fname) &&
		ulog_fname.find(DIR_DELIM_CHAR) != std::string::npos) {

		std::string full_name;
		if (fullpath(ulog_fname.c_str())) {
			full_name = ulog_fname;
		} else {
			Ad->LookupString(ATTR_JOB_IWD, full_name);
			full_name += DIR_DELIM_CHAR;
			full_name += ulog_fname;
		}
		AddDownloadFilenameRemap(condor_basename(full_name.c_str()), full_name.c_str());
	}

	if(!download_filename_remaps.empty()) {
		dprintf(D_FULLDEBUG, "FileTransfer: output file remaps: %s\n",download_filename_remaps.c_str());
	}
	return 1;
}

bool
FileTransfer::IsDataflowJob( ClassAd *job_ad ) {

	int newest_input_timestamp = -1;
	int oldest_output_timestamp = -1;
	std::set<int> input_timestamps;
	std::set<int> output_timestamps;
	std::string executable_file;
	std::string iwd;
	std::string input_files;
	std::string output_files;
	std::string stdin_file;
	std::string token;
	struct stat file_stat;

	// Lookup the working directory
	job_ad->LookupString( ATTR_JOB_IWD, iwd );


	// Parse the list of input files
	job_ad->LookupString( ATTR_TRANSFER_INPUT_FILES, input_files );
	for (const auto &token: StringTokenIterator(input_files, ",")) {
		// Skip any file path that looks like a URL or transfer plugin related
		if ( token.find( "://" ) == std::string::npos ) {
			// Stat each file. Paths can be relative or absolute.
			std::string input_filename;
			if ( token.find_last_of( DIR_DELIM_CHAR ) != std::string::npos ) {
				input_filename = token;
			}
			else {
				input_filename = iwd + DIR_DELIM_CHAR + token;
			}

			if ( stat( input_filename.c_str(), &file_stat ) == 0 ) {
				input_timestamps.insert( file_stat.st_mtime );
			}
		}
	}

	// The executable is an input file for purposes of this analysis.
	job_ad->LookupString( ATTR_JOB_CMD, executable_file );
	if ( stat( executable_file.c_str(), &file_stat ) == 0 ) {
		input_timestamps.insert( file_stat.st_mtime );
	} else {
		// The container universe doesn't need a real executable
		// to run a job, but we'll worry about supporting that if
		// anyone ever asks for it.
		return false;
	}

	// The standard input file, if any, is an input file for this analysis.
	job_ad->LookupString( ATTR_JOB_INPUT, stdin_file );
	if ( !stdin_file.empty() && stdin_file != "/dev/null" ) {
		if ( stat( stdin_file.c_str(), &file_stat ) == 0 ) {
			input_timestamps.insert( file_stat.st_mtime );
		} else {
			return false;
		}
	}


	// Parse the list of output files
	job_ad->LookupString( ATTR_TRANSFER_OUTPUT_FILES, output_files );
	for (const auto &token: StringTokenIterator(output_files, ",")) {
		// Stat each file. Add the last-modified timestamp to set of timestamps.
		std::string output_filename;
		if ( token.find_last_of( DIR_DELIM_CHAR ) != std::string::npos ) {
			output_filename = token;
		}
		else {
			output_filename = iwd + DIR_DELIM_CHAR + token;
		}

		if ( stat( output_filename.c_str(), &file_stat ) == 0 ) {
			output_timestamps.insert( file_stat.st_mtime );
		}
		else {
			// Failure to stat this output file suggests the file doesn't exist.
			// A job must have all declared outputs to be a dataflow job. Abort.
			return false;
		}
	}


	if ( !input_timestamps.empty() ) {
		newest_input_timestamp = *input_timestamps.rbegin();

		// If the oldest output file is more recent than the newest input file,
		// then this is a dataflow job.
		if ( !output_timestamps.empty() ) {
			oldest_output_timestamp = *output_timestamps.begin();
			return oldest_output_timestamp > newest_input_timestamp;
		}
	}

	return false;
}

#ifdef HAVE_HTTP_PUBLIC_FILES
int
FileTransfer::AddInputFilenameRemaps(ClassAd *Ad) {
	dprintf(D_FULLDEBUG,"Entering FileTransfer::AddInputFilenameRemaps\n");

	if(!Ad) {
		dprintf(D_FULLDEBUG, "FileTransfer::AddInputFilenameRemaps -- job ad null\n");
	  	return 1;
	}

	download_filename_remaps = "";
	char *remap_fname = NULL;

	// when downloading files from the job, apply input name remaps
	if (Ad->LookupString(ATTR_TRANSFER_INPUT_REMAPS,&remap_fname)) {
		AddDownloadFilenameRemaps(remap_fname);
		free(remap_fname);
		remap_fname = NULL;
	}
	if(!download_filename_remaps.empty()) {
		dprintf(D_FULLDEBUG, "FileTransfer: input file remaps: %s\n",download_filename_remaps.c_str());
	}
	return 1;
}
#endif

int
FileTransfer::Init(
	ClassAd *Ad,
	bool want_check_perms /* false */,
	priv_state priv /* PRIV_UNKNOWN */,
	bool use_file_catalog /* = true */)
{
	char buf[ATTRLIST_MAX_EXPRESSION];
	char *dynamic_buf = NULL;

	ASSERT( daemonCore );	// full Init require DaemonCore methods

	if( did_init ) {
			// no need to except, just quietly return success
		return 1;
	}

	dprintf(D_FULLDEBUG,"entering FileTransfer::Init\n");

	m_use_file_catalog = use_file_catalog;

	simple_init = false;

	if (!TranskeyTable) {
		// initialize our hashtable
		if (!(TranskeyTable = new TranskeyHashTable(hashFunction)))
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
			  new TransThreadHashTable(hashFuncInt))) {
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
				&FileTransfer::HandleCommands,
				"FileTransfer::HandleCommands()",WRITE);
		daemonCore->Register_Command(FILETRANS_DOWNLOAD,"FILETRANS_DOWNLOAD",
				&FileTransfer::HandleCommands,
				"FileTransfer::HandleCommands()",WRITE);
		ReaperId = daemonCore->Register_Reaper("FileTransfer::Reaper",
							&FileTransfer::Reaper,
							"FileTransfer::Reaper()");
		if (ReaperId == 1) {
			EXCEPT("FileTransfer::Reaper() can not be the default reaper!");
		}
	}

	if (Ad->LookupString(ATTR_TRANSFER_KEY, buf, sizeof(buf)) != 1) {
		char tempbuf[80];
		// classad did not already have a TRANSFER_KEY, so
		// generate a new one.  It must be unique and not guessable.
		snprintf(tempbuf,sizeof(tempbuf),"%x#%x%x%x",++SequenceNum,(unsigned)time(NULL),
			get_csrng_int(), get_csrng_int());
		TransKey = strdup(tempbuf);
		user_supplied_key = FALSE;
		Ad->Assign(ATTR_TRANSFER_KEY,TransKey);

		// since we generated the key, it is only good on our socket.
		// so update TRANSFER_SOCK now as well.
		char const *mysocket = global_dc_sinful();
		ASSERT(mysocket);
		Ad->Assign(ATTR_TRANSFER_SOCKET,mysocket);
	} else {
		// Here the ad we were given already has a Transfer Key.
		TransKey = strdup(buf);
		user_supplied_key = TRUE;
	}

		// Init all the file lists, etc.
	if ( !SimpleInit(Ad, want_check_perms, IsServer(),
			NULL, priv, m_use_file_catalog ) )
	{
		return 0;
	}

		// It is important that we call SimpleInit above before calling
		// InitializeJobPlugins because that sets up the plugin config
	if(IsClient()) {
		CondorError e;
		if(-1 == InitializeJobPlugins(*Ad, e)) {
			return 0;
		}
	}

		// At this point, we'd better have a transfer socket
	if (Ad->LookupString(ATTR_TRANSFER_SOCKET, buf, sizeof(buf)) != 1) {
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
		std::string filelist;
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

			time_t mod_time;
			filesize_t filesize;
			if ( LookupInFileCatalog(current_file, &mod_time, &filesize) ) {
				// Make certain file isn't just an input file

				// if filesize is -1, it's a special flag meaning to compare
				// the file in the old way, i.e. only checking if it is newer
				// than the stored spool_date.
				if(filesize==-1) {
					if(spool_space.GetModifyTime() <= mod_time) {
						dprintf( D_FULLDEBUG,
					 		"Not including file %s, t: %ld<=%ld, s: N/A\n",
					 		current_file, spool_space.GetModifyTime(), mod_time);
						continue;
					}
					// fall through and include file
				}
				else if((spool_space.GetModifyTime()==mod_time) &&
						(spool_space.GetFileSize()==filesize) ) {
					dprintf( D_FULLDEBUG,
						 "Not including file %s, t: %ld, "
						 "s: " FILESIZE_T_FORMAT "\n",
						 current_file, spool_space.GetModifyTime(), spool_space.GetFileSize());
					continue;
				}
				dprintf( D_FULLDEBUG,
					 "Including changed file %s, t: %ld, %ld, "
					 "s: " FILESIZE_T_FORMAT ", " FILESIZE_T_FORMAT "\n",
					 current_file,
					 spool_space.GetModifyTime(), mod_time,
					 spool_space.GetFileSize(), filesize );
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
			Ad->InsertAttr(ATTR_TRANSFER_INTERMEDIATE_FILES, filelist);
			dprintf(D_FULLDEBUG,"%s=\"%s\"\n",ATTR_TRANSFER_INTERMEDIATE_FILES,
					filelist.c_str());
		}
	}
	if ( IsClient() && upload_changed_files ) {
		dynamic_buf = NULL;
		Ad->LookupString(ATTR_TRANSFER_INTERMEDIATE_FILES,&dynamic_buf);
		dprintf(D_FULLDEBUG,"%s=\"%s\"\n",
				ATTR_TRANSFER_INTERMEDIATE_FILES,
				dynamic_buf ? dynamic_buf : "(none)");
		if ( dynamic_buf ) {
			SpooledIntermediateFiles = strdup(dynamic_buf);
			free(dynamic_buf);
			dynamic_buf = NULL;
		}
	}


	// if we are acting as the server side, insert this key
	// into our hashtable if it is not already there.
	if ( IsServer() ) {
		std::string key(TransKey);
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
		EXCEPT("FileTransfer::DownloadFiles called during active transfer!");
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

		if (IsDebugLevel(D_COMMAND)) {
			dprintf (D_COMMAND, "FileTransfer::DownloadFiles(%s,...) making connection to %s\n",
				getCommandStringSafe(FILETRANS_UPLOAD), TransSock ? TransSock : "NULL");
		}

		Daemon d( DT_ANY, TransSock );

		if ( !d.connectSock(&sock,0) ) {
			dprintf( D_ALWAYS, "FileTransfer: Unable to connect to server "
					 "%s\n", TransSock );
			Info.success = 0;
			Info.in_progress = false;
			formatstr( Info.error_desc, "FileTransfer: Unable to connect to server %s",
					 TransSock );
			return FALSE;
		}

		CondorError err_stack;
		if ( !d.startCommand(FILETRANS_UPLOAD, &sock, 0, &err_stack, NULL, false, m_sec_session_id) ) {
			Info.success = 0;
			Info.in_progress = 0;
			formatstr( Info.error_desc, "FileTransfer: Unable to start "
					   "transfer with server %s: %s", TransSock,
					   err_stack.getFullText().c_str() );
		}

		sock.encode();

		if ( !sock.put_secret(TransKey) ||
			!sock.end_of_message() ) {
			Info.success = 0;
			Info.in_progress = false;
			formatstr( Info.error_desc, "FileTransfer: Unable to start transfer with server %s",
					 TransSock );
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
		BuildFileCatalog();
		// Now sleep for 1 second.  If we did not do this, then jobs
		// which run real quickly (i.e. less than a second) would not
		// have their output files uploaded.  The real reason we must
		// sleep here is time_t is only at the resolution on 1 second.
		sleep(1);
	}

	return ret_value;
}


void
FileTransfer::FindChangedFiles()
{
	std::vector<std::string> final_files_to_send;

	// Here we will upload only files in the Iwd which have changed
	// since we downloaded last.  We only do this if
	// upload_changed_files it true, and if last_download_time > 0
	// which means we have already downloaded something.

	// If this is the final transfer, be certain to send back
	// not only the files which have been modified during this run,
	// but also the files which have been modified during
	// previous runs (i.e. the SpooledIntermediateFiles).
	if ( m_final_transfer_flag && SpooledIntermediateFiles ) {
		final_files_to_send = split(SpooledIntermediateFiles, ",");
	}

		// if desired_priv_state is PRIV_UNKNOWN, the Directory
		// object treats that just like we do: don't switch...
	Directory dir( Iwd, desired_priv_state );

	const char *proxy_file = NULL;
	std::string proxy_file_buf;
	if(jobAd.LookupString(ATTR_X509_USER_PROXY, proxy_file_buf)) {
		proxy_file = condor_basename(proxy_file_buf.c_str());
	}

	const char *f;
	while( (f=dir.Next()) ) {
		// don't send back the executable
		if ( ExecFile && MATCH == file_strcmp ( f, ExecFile ) ) {
			dprintf ( D_FULLDEBUG, "Skipping %s\n", f );
			continue;
		}
		if( proxy_file && file_strcmp(f, proxy_file) == MATCH ) {
			dprintf( D_FULLDEBUG, "Skipping %s\n", f );
			continue;
		}

		// for now, skip all subdirectory names until we add
		// subdirectory support into FileTransfer.
		if ( dir.IsDirectory() && ! file_contains(OutputFiles, f) ) {
			dprintf( D_FULLDEBUG, "Skipping dir %s\n", f );
			continue;
		}

		// if this file is has been modified since last download,
		// add it to the list of files to transfer.
		bool send_it = false;

		// look up the file name in the catalog.  if it does not exist, it
		// is a new file, and is always transfered back.  if it the
		// filename does already exist in the catalog, then the
		// modification date and filesize parameters are filled in.
		// if either has changed, transfer the file.

		filesize_t filesize;
		time_t modification_time;
		if ( !LookupInFileCatalog(f, &modification_time, &filesize) ) {
			// file was not found.  send it.
			dprintf( D_FULLDEBUG,
					"Sending new file %s, time==%ld, size==%ld\n",
					f, dir.GetModifyTime(), (long) dir.GetFileSize() );
			send_it = true;
		}
		else if (file_contains(final_files_to_send, f)) {
			dprintf( D_FULLDEBUG,
					"Sending previously changed file %s\n", f);
			send_it = true;
		}
		else if (file_contains(OutputFiles, f)) {
			dprintf(D_FULLDEBUG,
				    "Sending dynamically added output file %s\n",
				    f);
			send_it = true;
		}
		else if (filesize == -1) {
			// this is a special block of code that should eventually go
			// away.  essentially, setting the filesize to -1 means that
			// we only transfer the file if the timestamp is newer than
			// the spool date stored in the job ad (how it's always worked
			// in the past).  once the FileCatalog is stored persistently
			// somewhere, this mode of operation can go away.
			if (dir.GetModifyTime() > modification_time) {
				// include the file if the time stamp is greater than
				// the spool date (stored in modification_time).
				dprintf( D_FULLDEBUG,
					"Sending changed file %s, t: %ld, %ld, "
					"s: " FILESIZE_T_FORMAT ", N/A\n",
					f, dir.GetModifyTime(), modification_time,
					dir.GetFileSize());
				send_it = true;
			} else {
				// if filesize was -1 but the timestamp was earlier than
				// modification_time, do NOT include the file.
				dprintf( D_FULLDEBUG,
					"Skipping file %s, t: %ld<=%ld, s: N/A\n",
					f, dir.GetModifyTime(), modification_time);
				continue;
			}
		}
		else if ((filesize != dir.GetFileSize()) ||
				(modification_time != dir.GetModifyTime()) ) {
			// file has changed in size or modification time.  this
			// doesn't catch the case where the file was modified
			// without changing size and is then back-dated.  use a hash
			// or something if that's truly needed, and compare the
			// checksums.
			dprintf( D_FULLDEBUG,
				"Sending changed file %s, t: %ld, %ld, "
				"s: " FILESIZE_T_FORMAT ", " FILESIZE_T_FORMAT "\n",
				f, dir.GetModifyTime(), modification_time,
				dir.GetFileSize(), filesize );
			send_it = true;
		}
		else {
			dprintf( D_FULLDEBUG,
					"Skipping file %s, t: %" PRIi64"==%" PRIi64
					", s: %" PRIi64"==%" PRIi64"\n",
					f,
					(PRIi64_t)dir.GetModifyTime(),
					(PRIi64_t)modification_time,
					(PRIi64_t)dir.GetFileSize(),
					(PRIi64_t)filesize );
			continue;
		}
		if(send_it) {
			// now append changed file to list only if not already there
			if ( file_contains(IntermediateFiles, f) == FALSE ) {
				IntermediateFiles.emplace_back(f);
			}
		}
	}

	if (!IntermediateFiles.empty()) {
		// Initialize it with intermediate files
		// which we already have spooled.  We want to send
		// back these files + any that have changed this time.
		FilesToSend = &IntermediateFiles;
		EncryptFiles = &EncryptOutputFiles;
		DontEncryptFiles = &DontEncryptOutputFiles;
	}
}

int
FileTransfer::UploadCheckpointFiles( int checkpointNumber, bool blocking ) {
	this->checkpointNumber = checkpointNumber;

	// This is where we really want to separate "I understand the job ad"
	// from "I can operate the protocol".  Until then, just set a member
	// variable to make the special-case behavior happen.
	uploadCheckpointFiles = true;
	int rv = UploadFiles( blocking, false );
	uploadCheckpointFiles = false;
	return rv;
}

int
FileTransfer::UploadFailureFiles( bool blocking ) {
	uploadFailureFiles = true;
	int rv = UploadFiles( blocking, true );
	uploadFailureFiles = false;
	return rv;
}

void
FileTransfer::DetermineWhichFilesToSend() {
	// IntermediateFiles is dynamically allocated (some jobs never use it).
	IntermediateFiles.clear();

	// These are always pointers to vectors owned by this object.
	FilesToSend = NULL;
	EncryptFiles = NULL;
	DontEncryptFiles = NULL;

	// We're doing this allocation on the fly because we expect most jobs
	// won't specify a checkpoint list.
	if( uploadCheckpointFiles ) {
		std::string checkpointList;
		if( jobAd.LookupString( ATTR_CHECKPOINT_FILES, checkpointList ) ) {
			CheckpointFiles = split(checkpointList);

			// This should Just Work(TM), but I haven't tested it yet and
			// I don't know that anybody will every actually use it.
			EncryptCheckpointFiles.clear();
			DontEncryptCheckpointFiles.clear();

			//
			// If we're not streaming ATTR_JOB_OUTPUT or ATTR_JOB_ERROR,
			// send them along as well.  If ATTR_CHECKPOINT_FILES is set,
			// it's an explicit list, so we don't have to worry about
			// implicitly sending the file twice.
			//
			if( shouldSendStdout() ) {
				if(! file_contains(CheckpointFiles, JobStdoutFile) ) {
					CheckpointFiles.emplace_back(JobStdoutFile);
				}
			}

			if( shouldSendStderr() ) {
				if(! file_contains(CheckpointFiles, JobStderrFile) ) {
					CheckpointFiles.emplace_back(JobStderrFile);
				}
			}

			// Yes, this is stupid, but it'd be a big change to fix.
			FilesToSend = &CheckpointFiles;
			EncryptFiles = &EncryptCheckpointFiles;
			DontEncryptFiles = &DontEncryptCheckpointFiles;

			return;
		}
	}

	if( uploadFailureFiles ) {
		FilesToSend = &FailureFiles;
		return;
	}

	if ( upload_changed_files && last_download_time > 0 ) {
		FindChangedFiles();
	}

	// if FilesToSend is still NULL, then the user did not
	// want anything sent back via modification date.  so
	// send the input or output sandbox, depending what
	// direction we are going.
	if ( FilesToSend == NULL ) {
		if ( simple_init ) {
			if ( IsClient() ) {
				// condor_submit sending to the schedd
				FilesToSend = &InputFiles;
				EncryptFiles = &EncryptInputFiles;
				DontEncryptFiles = &DontEncryptInputFiles;
			} else {
				// schedd sending to condor_transfer_data
				FilesToSend = &OutputFiles;
				EncryptFiles = &EncryptOutputFiles;
				DontEncryptFiles = &DontEncryptOutputFiles;
			}
		} else {
			// starter sending back to the shadow
			FilesToSend = &OutputFiles;
			EncryptFiles = &EncryptOutputFiles;
			DontEncryptFiles = &DontEncryptOutputFiles;
		}

	}
}


int
FileTransfer::UploadFiles(bool blocking, bool final_transfer)
{
	ReliSock sock;
	ReliSock *sock_to_use;

	dprintf(D_FULLDEBUG,
		"entering FileTransfer::UploadFiles (final_transfer=%d)\n",
		final_transfer ? 1 : 0);

	if (ActiveTransferTid >= 0) {
		EXCEPT("FileTransfer::UpLoadFiles called during active transfer!");
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

	// If we're a client talking to a 7.5.6 or older schedd, we want
	// to send the user log as an input file.
	if ( UserLogFile && TransferUserLog && simple_init && !nullFile( UserLogFile ) ) {
		if ( !file_contains( InputFiles, UserLogFile ) )
			InputFiles.emplace_back( UserLogFile );
	}

	// set flag saying if this is the last upload (i.e. job exited)
	m_final_transfer_flag = final_transfer ? 1 : 0;

	DetermineWhichFilesToSend();

	if ( !simple_init ) {
		// Optimization: files_to_send now contains the files to upload.
		// If files_to_send is NULL, then we have nothing to send, so
		// we can return with SUCCESS immedidately.
		if ( FilesToSend == NULL ) {
			return 1;
		}

		sock.timeout(clientSockTimeout);

		if (IsDebugLevel(D_COMMAND)) {
			dprintf (D_COMMAND, "FileTransfer::UploadFiles(%s,...) making connection to %s\n",
				getCommandStringSafe(FILETRANS_DOWNLOAD), TransSock ? TransSock : "NULL");
		}

		Daemon d( DT_ANY, TransSock );

		if ( !d.connectSock(&sock,0) ) {
			dprintf( D_ALWAYS, "FileTransfer: Unable to connect to server "
					 "%s\n", TransSock );
			Info.success = 0;
			Info.in_progress = false;
			formatstr( Info.error_desc, "FileTransfer: Unable to connect to server %s",
					 TransSock );
			return FALSE;
		}

		CondorError err_stack;
		if ( !d.startCommand(FILETRANS_DOWNLOAD, &sock, clientSockTimeout, &err_stack, NULL, false, m_sec_session_id) ) {
			Info.success = 0;
			Info.in_progress = 0;
			formatstr( Info.error_desc, "FileTransfer: Unable to start "
					   "transfer with server %s: %s", TransSock,
					   err_stack.getFullText().c_str() );
		}

		sock.encode();

		if ( !sock.put_secret(TransKey) ||
			!sock.end_of_message() ) {
			Info.success = 0;
			Info.in_progress = false;
			formatstr( Info.error_desc, "FileTransfer: Unable to start transfer with server %s",
					 TransSock );
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
FileTransfer::HandleCommands(int command, Stream *s)
{
	FileTransfer *transobject;
	char *transkey = NULL;

	dprintf(D_FULLDEBUG,"entering FileTransfer::HandleCommands\n");

	if ( s->type() != Stream::reli_sock ) {
		// the FileTransfer object only works on TCP, not UDP
		return 0;
	}
	ReliSock *sock = (ReliSock *) s;

	// turn off timeouts on sockets, since our peer could get suspended
	// (like in the case of the starter sending files back to the shadow)
	sock->timeout(0);

	// code() allocates memory for the string if the pointer is NULL.
	if (!sock->get_secret(transkey) ||
		!sock->end_of_message() ) {
		dprintf(D_FULLDEBUG,
			    	"FileTransfer::HandleCommands failed to read transkey\n");
		if (transkey) free(transkey);
		return 0;
	}
	dprintf(D_FULLDEBUG,
					"FileTransfer::HandleCommands read transkey=%s\n",transkey);

	std::string key(transkey);
	free(transkey);
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
			transobject->CommitFiles();

			std::string checkpointDestination;
			if(! transobject->jobAd.LookupString( "CheckpointDestination", checkpointDestination )) {
                const char *currFile;
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
						//
						// Do NOT try to avoid duplicating files here.  Because
						// we append the SPOOL entries to the input list, they'll
						// win even if the de-duplication code in DoUpload()
						// doesn't detect them.  This avoids a bunch of ugly code
						// duplication, and also a bug caused by assuming all
						// entries in SPOOL were files, so that directories at
						// the root of SPOOL would be removed from the input list,
						// causing an incomplete transfer if the user hadn't
						// put the whole directory in TransferCheckpointFiles.
						const char * filename = spool_space.GetFullPath();
						// dprintf( D_ZKM, "[FT] Appending SPOOL filename %s to input files.\n", filename );
						transobject->InputFiles.emplace_back(filename);
					}
				}
			}

			// Similarly, we want to look through any data reuse file and treat them as input
			// files.  We must handle the manifest here in order to ensure the manifest files
			// are treated in the same manner as anything else that appeared on transfer_input_files
			if (!transobject->ParseDataManifest()) {
				transobject->m_reuse_info.clear();
			}
			for (const auto &info : transobject->m_reuse_info) {
				if (!file_contains(transobject->InputFiles, info.filename()))
					transobject->InputFiles.emplace_back(info.filename());
			}

			// dprintf( D_ZKM, "HandleCommands(): InputFiles = %s\n", transobject->InputFiles.to_string().c_str() );
			transobject->FilesToSend = &transobject->InputFiles;
			transobject->EncryptFiles = &transobject->EncryptInputFiles;
			transobject->DontEncryptFiles = &transobject->DontEncryptInputFiles;

			transobject->inHandleCommands = true;
			if(! checkpointDestination.empty()) { transobject->uploadCheckpointFiles = true; }
			transobject->Upload(sock,ServerShouldBlock);
			if(! checkpointDestination.empty()) { transobject->uploadCheckpointFiles = false; }
			transobject->inHandleCommands = false;
			}
			break;
		case FILETRANS_DOWNLOAD:
			transobject->Download(sock,ServerShouldBlock);
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


bool
FileTransfer::SetServerShouldBlock( bool block )
{
	bool old_value = ServerShouldBlock;
	ServerShouldBlock = block;
	return old_value;
}

int
FileTransfer::Reaper(int pid, int exit_status)
{
	FileTransfer *transobject;
	if (!TransThreadTable || TransThreadTable->lookup(pid,transobject) < 0) {
		dprintf(D_ALWAYS, "unknown pid %d in FileTransfer::Reaper!\n", pid);
		return FALSE;
	}
	transobject->ActiveTransferTid = -1;
	TransThreadTable->remove(pid);

	transobject->Info.duration = time(NULL)-transobject->TransferStart;
	transobject->Info.in_progress = false;
	if( WIFSIGNALED(exit_status) ) {
		transobject->Info.success = false;
		transobject->Info.try_again = true;
		formatstr(transobject->Info.error_desc, "File transfer failed (killed by signal=%d)", WTERMSIG(exit_status));
		if( transobject->registered_xfer_pipe ) {
			transobject->registered_xfer_pipe = false;
			daemonCore->Cancel_Pipe(transobject->TransferPipe[0]);
		}
		dprintf( D_ALWAYS, "%s\n", transobject->Info.error_desc.c_str() );
	} else {
		if( WEXITSTATUS(exit_status) == 1 ) {
			dprintf( D_ALWAYS, "File transfer completed successfully.\n" );
			transobject->Info.success = true;
		} else {
			dprintf( D_ALWAYS, "File transfer failed (status=%d).\n",
					 WEXITSTATUS(exit_status) );
			transobject->Info.success = false;
		}
	}

		// Close the write end of the pipe so we don't block trying
		// to read from it if the child closes it prematurely.
		// We don't do this until this late stage in the game, because
		// in windows everything currently happens in the main thread.
	if( transobject->TransferPipe[1] != -1 ) {
		daemonCore->Close_Pipe(transobject->TransferPipe[1]);
		transobject->TransferPipe[1] = -1;
	}

		// if we haven't already read the final status update, do it now
	if( transobject->registered_xfer_pipe ) {
		// It's possible that the pipe contains a progress update message
		// followed by the final update message. Keep reading until we
		// get the final message or encounter an error.
		do {
			transobject->ReadTransferPipeMsg();
		} while ( transobject->Info.success &&
				  transobject->Info.xfer_status != XFER_STATUS_DONE );
	}

	if( transobject->registered_xfer_pipe ) {
		transobject->registered_xfer_pipe = false;
		daemonCore->Cancel_Pipe(transobject->TransferPipe[0]);
	}

	daemonCore->Close_Pipe(transobject->TransferPipe[0]);
	transobject->TransferPipe[0] = -1;

	if ( transobject->Info.success ) {
		if ( transobject->Info.type == DownloadFilesType ) {
			transobject->downloadEndTime = condor_gettimestamp_double();

		} else if ( transobject->Info.type == UploadFilesType ) {
			transobject->uploadEndTime = condor_gettimestamp_double();

		}
	}

	// If Download was successful (it returns 1 on success) and
	// upload_changed_files is true, then we must record the current
	// time in last_download_time so in UploadFiles we have a timestamp
	// to compare.  If it is a non-blocking download, we do all this
	// in the thread reaper.
	if ( transobject->Info.success && transobject->upload_changed_files &&
		 transobject->IsClient() && transobject->Info.type == DownloadFilesType ) {
		time(&(transobject->last_download_time));
		transobject->BuildFileCatalog(0, transobject->Iwd, &(transobject->last_download_catalog));
		// Now sleep for 1 second.  If we did not do this, then jobs
		// which run real quickly (i.e. less than a second) would not
		// have their output files uploaded.  The real reason we must
		// sleep here is time_t is only at the resolution on 1 second.
		sleep(1);
	}

	transobject->callClientCallback();

	return TRUE;
}

void
FileTransfer::callClientCallback()
{
	if (ClientCallbackCpp) {
		dprintf(D_FULLDEBUG,
				"Calling client FileTransfer handler function.\n");
		((ClientCallbackClass)->*(ClientCallbackCpp))(this);
	}
}

bool
FileTransfer::ReadTransferPipeMsg()
{
	int n;

	char cmd=0;
	n = daemonCore->Read_Pipe( TransferPipe[0], &cmd, sizeof(cmd) );
	if(n != sizeof( cmd )) goto read_failed;

	if( cmd == IN_PROGRESS_UPDATE_XFER_PIPE_CMD ) {
		int i=XFER_STATUS_UNKNOWN;
		n = daemonCore->Read_Pipe( TransferPipe[0],
								   (char *)&i,
								   sizeof( int ) );
		if(n != sizeof( int )) goto read_failed;
		Info.xfer_status = (FileTransferStatus)i;

		if( ClientCallbackWantsStatusUpdates ) {
			callClientCallback();
		}
	} else if( cmd == FINAL_UPDATE_XFER_PIPE_CMD ) {
		Info.xfer_status = XFER_STATUS_DONE;

		n = daemonCore->Read_Pipe( TransferPipe[0],
								   (char *)&Info.bytes,
								   sizeof( filesize_t) );
		if(n != sizeof( filesize_t )) goto read_failed;
		if( Info.type == DownloadFilesType ) {
			bytesRcvd += Info.bytes;
		}
		else {
			bytesSent += Info.bytes;
		}

		n = daemonCore->Read_Pipe( TransferPipe[0],
								   (char *)&Info.try_again,
								   sizeof( bool ) );
		if(n != sizeof( bool )) goto read_failed;


		n = daemonCore->Read_Pipe( TransferPipe[0],
								   (char *)&Info.hold_code,
								   sizeof( int ) );
		if(n != sizeof( int )) goto read_failed;

		n = daemonCore->Read_Pipe( TransferPipe[0],
								   (char *)&Info.hold_subcode,
								   sizeof( int ) );
		if(n != sizeof( int )) goto read_failed;

		int stats_len = 0;
		n = daemonCore->Read_Pipe( TransferPipe[0],
								   (char *)&stats_len,
								   sizeof( int ) );
		if(n != sizeof( int )) goto read_failed;
		if (stats_len) {
			char *stats_buf = new char[stats_len+1];
			n = daemonCore->Read_Pipe( TransferPipe[0],
									stats_buf,
									stats_len );
			if(n != stats_len) {
				delete [] stats_buf;
				goto read_failed;
			}
			stats_buf[stats_len] = '\0';
			classad::ClassAdParser parser;
			parser.ParseClassAd(stats_buf, Info.stats);
			delete [] stats_buf;
		}

		int error_len = 0;
		n = daemonCore->Read_Pipe( TransferPipe[0],
								   (char *)&error_len,
								   sizeof( int ) );
		if(n != sizeof( int )) goto read_failed;

		if(error_len) {
			char *error_buf = new char[error_len];
			ASSERT(error_buf);

			n = daemonCore->Read_Pipe( TransferPipe[0],
									   error_buf,
									   error_len );
			if(n != error_len) {
				delete [] error_buf;
				goto read_failed;
			}

			// The client should have null terminated this, but
			// let's write the null just in case it didn't
			error_buf[error_len - 1] = '\0';

			Info.error_desc = error_buf;

			delete [] error_buf;
		}

		int spooled_files_len = 0;
		n = daemonCore->Read_Pipe( TransferPipe[0],
								   (char *)&spooled_files_len,
								   sizeof( int ) );
		if(n != sizeof( int )) goto read_failed;

		if(spooled_files_len) {
			char *spooled_files_buf = new char[spooled_files_len];
			ASSERT(spooled_files_buf);

			n = daemonCore->Read_Pipe( TransferPipe[0],
									   spooled_files_buf,
									   spooled_files_len );
			if(n != spooled_files_len) {
				delete [] spooled_files_buf;
				goto read_failed;
			}
			// The sender should be sending a null terminator,
			// but let's not rely on that.
			spooled_files_buf[spooled_files_len-1] = '\0';
			Info.spooled_files = spooled_files_buf;

			delete [] spooled_files_buf;
		}

		if( registered_xfer_pipe ) {
			registered_xfer_pipe = false;
			daemonCore->Cancel_Pipe(TransferPipe[0]);
		}
	} else if( cmd == PLUGIN_OUTPUT_AD ) {
		// Read the length of the serialization of the pipe.
		int size_of_ad = 0;
		n = daemonCore->Read_Pipe( TransferPipe[0],
		                           &size_of_ad,
		                           sizeof( int ) );
		if( n != sizeof(int) ) {
			goto read_failed;
		}

		// Allocate the buffer, make sure it will be zero-terminated.
		char * plugin_output_ad_string = new char[size_of_ad + 1];
		ASSERT( plugin_output_ad_string );
		plugin_output_ad_string[size_of_ad] = '\0';

		// Fill the buffer.  It may take more than one read.  The
		// second read should only block very briefly -- at most long
		// enough for the child process to be rescheduled and reenter
		// the kernel.  If that's not brief enough, we'll need to
		// start managing state or more likely use a coroutine.
		int total_read = 0;
		while( total_read < size_of_ad ) {
			n = daemonCore->Read_Pipe( TransferPipe[0],
			                           plugin_output_ad_string + total_read,
			                           size_of_ad );
			if( n <= 0 ) { goto read_failed; }
			total_read += n;
		}
		if( total_read > size_of_ad ) {
			delete [] plugin_output_ad_string;
			goto read_failed;
		}

		classad::ClassAdParser cap;
		pluginResultList.emplace_back();
		const bool parse_full_string = true;
		bool parsed_plugin_output_ad = cap.ParseClassAd(
			plugin_output_ad_string, pluginResultList.back(), parse_full_string
		);
		ASSERT(parsed_plugin_output_ad);

		delete [] plugin_output_ad_string;
	} else {
		EXCEPT("Invalid file transfer pipe command %d",cmd);
	}

	return true;

 read_failed:
	Info.success = false;
	Info.try_again = true;
	if( Info.error_desc.empty() ) {
		formatstr(Info.error_desc,"Failed to read status report from file transfer pipe (errno %d): %s",errno,strerror(errno));
		dprintf(D_ALWAYS,"%s\n",Info.error_desc.c_str());
	}
	if( registered_xfer_pipe ) {
		registered_xfer_pipe = false;
		daemonCore->Cancel_Pipe(TransferPipe[0]);
	}

	return false;
}

bool
FileTransfer::SendPluginOutputAd( const ClassAd & plugin_output_ad ) {
	// Do we have a pipe to which to write?
	if( TransferPipe[1] == -1 ) { return false; }

	// Write the command.
	char cmd = PLUGIN_OUTPUT_AD;
	int n = daemonCore->Write_Pipe( TransferPipe[1], & cmd, sizeof(cmd) );
	if( n != sizeof(cmd) ) { return false; }

	// Serialize the ClassAd.
	std::string plugin_output_ad_string;
	classad::ClassAdUnParser caup;
	caup.Unparse( plugin_output_ad_string, & plugin_output_ad );

	// Write the size of the serialization.
	int size_of_ad = plugin_output_ad_string.size();
	n = daemonCore->Write_Pipe(
		TransferPipe[1], & size_of_ad, sizeof(size_of_ad)
	);
	if( n != sizeof(size_of_ad) ) { return false; }

	// Write the serialization.
	n = daemonCore->Write_Pipe(
		TransferPipe[1],
		plugin_output_ad_string.c_str(),
		plugin_output_ad_string.size()
	);
	// I'm also asserting that our ads aren't >= 2GB.
	ASSERT( n == (int)plugin_output_ad_string.size() );

	return true;
}


void
FileTransfer::UpdateXferStatus(FileTransferStatus status)
{
	if( Info.xfer_status != status ) {
		bool write_failed = false;
		if( TransferPipe[1] != -1 ) {
			int n;
			char cmd = IN_PROGRESS_UPDATE_XFER_PIPE_CMD;

			n = daemonCore->Write_Pipe( TransferPipe[1],
										&cmd,
										sizeof(cmd) );
			if(n != sizeof(cmd)) write_failed = true;

			if(!write_failed) {
				int i = status;
				n = daemonCore->Write_Pipe( TransferPipe[1],
											(char *)&i,
											sizeof(int) );
				if(n != sizeof(int)) write_failed = true;
			}
		}

		if( !write_failed ) {
			Info.xfer_status = status;
		}
	}
}

int
FileTransfer::TransferPipeHandler(int p)
{
	ASSERT( p == TransferPipe[0] );

	return ReadTransferPipeMsg();
}

int
FileTransfer::Download(ReliSock *s, bool blocking)
{
	dprintf(D_FULLDEBUG,"entering FileTransfer::Download\n");

	if (ActiveTransferTid >= 0) {
		EXCEPT("FileTransfer::Download called during active transfer!");
	}

	Info.duration = 0;
	Info.type = DownloadFilesType;
	Info.success = true;
	Info.in_progress = true;
	Info.xfer_status = XFER_STATUS_UNKNOWN;
	Info.stats.Clear();
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
		if (!daemonCore->Create_Pipe(TransferPipe,true)) {
			dprintf(D_ALWAYS, "Create_Pipe failed in "
					"FileTransfer::Download\n");
			return FALSE;
		}

		if (-1 == daemonCore->Register_Pipe(TransferPipe[0],
											"Download Results",
											(PipeHandlercpp)&FileTransfer::TransferPipeHandler,
											"TransferPipeHandler",
											this)) {
			dprintf(D_ALWAYS,"FileTransfer::Download() failed to register pipe.\n");
			return FALSE;
		}
		else {
			registered_xfer_pipe = true;
		}

		download_info *info = (download_info *)malloc(sizeof(download_info));
		ASSERT ( info );
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
		dprintf(D_FULLDEBUG,
				"FileTransfer: created download transfer process with id %d\n",
				ActiveTransferTid);
		// daemonCore will free(info) when the thread exits
		TransThreadTable->insert(ActiveTransferTid, this);

		downloadStartTime = condor_gettimestamp_double();

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

	if(!myobj->WriteStatusToTransferPipe(total_bytes)) {
		return 0;
	}
	return ( status == 0 );
}

void
FileTransfer::AddDownloadFilenameRemap(char const *source_name,char const *target_name) {
	if(!download_filename_remaps.empty()) {
		download_filename_remaps += ";";
	}
	download_filename_remaps += source_name;
	download_filename_remaps += "=";
	download_filename_remaps += target_name;
}

void
FileTransfer::AddDownloadFilenameRemaps(char const *remaps) {
	if(!download_filename_remaps.empty()) {
		download_filename_remaps += ";";
	}
	download_filename_remaps += remaps;
}


bool
shadow_safe_mkdir_impl(
	const std::filesystem::path & prefix,
	const std::filesystem::path & suffix,
	mode_t mode
) {
	// We additionally interpret LIMIT_DIRECTORY_ACCESS as controlling where
	// the shadow may make directories.  Specifically, we allow the shadow
	// to make a directory if it would be allowed to write to that path if
	// it were a file.
	//
	// We do NOT allow the shadow to make any directory named in
	// LIMIT_DIRECTORY_ACCESS; I implemented this an earlier version of the
	// code for testing purposes, but it grossly violated Mat's principle of
	// least astonishment.  (It seems either grossly unhelpful -- since the
	// directory will likely be created with the wrong permissions -- or more
	// likely to be a typo than anything the admin actually wanted to happen.)
	// dprintf( D_ZKM, "shadow_safe_mkdir_impl(%s, %s)\n", prefix.c_str(), suffix.c_str() );

	//
	// Algorithm.
	//
	// We are given a prefix, of which at most the last component does not
	// exist.  We are given a suffix, any component of which may not exist.
	//
	// (1)  We check `prefix` and (if necessary) each component of `suffix` to
	//      find the first path component that doesn't exist, updating a
	//      partial path as go along.  If every component exists, then the
	//      whole path exists and we're done.
	// (2)  Otherwise, we try to re-establish the invariant:
	//      A) Call allow_shadow_access() on the partial path to see if we're
	//         allowed to create it.  If not, return false.
	//      B) Call mkdir() to create it.  If we fail, return false.
	//      C) If there's not another path component, we're done; return true.
	//      D) Otherwise, the partial path now meets the invariant for
	//         the prefix.  Construct the corresponding suffix and recurse.
	//         (We could unlink() the directory we just created if the
	//         recursion fails, but it's not at all clear if that's the right
	//         thing to do, given the possible failure conditions.)
	//

	// Step (1).
	std::filesystem::path partial_path( prefix );
	auto next_path_component_iter = suffix.begin();
	while( std::filesystem::exists( partial_path ) ) {
		// dprintf( D_ZKM, "shadow_safe_mkdir_impl(%s, %s), (1): partial_path = %s, next_path_component_iter = %s\n", prefix.c_str(), suffix.c_str(), partial_path.c_str(), next_path_component_iter->c_str() );
		if( next_path_component_iter == suffix.end() ) {
			// Then the directory we're trying to create exists.  Great!
			// (This should only happen if somebody else has created it
			// between when we checked in shadow_safe_mkdir() and here.)
			// We don't check allow_shadow_access() here because we're not
			// writing anything now, and it'll be called for each individual
			// file write anyway.
			return true;
		}

		partial_path = partial_path / (* next_path_component_iter);
		++next_path_component_iter;
	}

	// Step (2A).
	// dprintf( D_ZKM, "shadow_safe_mkdir_impl(%s, %s), (2A): partial_path = %s, next_path_component_iter = %s\n", prefix.c_str(), suffix.c_str(), partial_path.c_str(), next_path_component_iter->c_str() );
	if(! allow_shadow_access( partial_path.string().c_str() )) {
		errno = EACCES;
		return false;
	}

	// Step (2B).  We can see EEXIST if, for instance, some other shadow
	// is going through this logic at the same time.  In general, I'm not
	// going to worry if we get ENOENT; if some other process is deleting
	// directories where you want to write your output data, I think it's
	// entirely appropriate for us to fail to write there.
	int rv = mkdir( partial_path.string().c_str(), mode );
	if( rv != 0 && errno != EEXIST ) {
		return false;
	}

	// Step (2C).
	// dprintf( D_ZKM, "shadow_safe_mkdir_impl(%s, %s), (2C): partial_path = %s, next_path_component_iter = %s\n", prefix.c_str(), suffix.c_str(), partial_path.c_str(), next_path_component_iter->c_str() );
	if( next_path_component_iter == suffix.end() ) { return true; }

	// Step (2D).
	std::filesystem::path remainder;
	for( ; next_path_component_iter != suffix.end(); ++next_path_component_iter ) {
		remainder /= (* next_path_component_iter);
	}

	// dprintf( D_ZKM, "shadow_safe_mkdir_impl(%s, %s), (2D): partial_path = %s, remainder = %s\n", prefix.c_str(), suffix.c_str(), partial_path.c_str(), remainder.c_str() );
	return shadow_safe_mkdir_impl( partial_path, remainder, mode );
}


bool
shadow_safe_mkdir( const std::string & dir, mode_t mode, priv_state priv ) {
	std::filesystem::path path(dir);
	if(! path.has_root_path()) {
		dprintf( D_ALWAYS, "Internal logic error: shadow_safe_mkdir() called with relative path.  Refusing to make the directory.\n" );
		errno = EINVAL;
		return false;
	}

	TemporaryPrivSentry sentry;
	if( priv != PRIV_UNKNOWN ) { set_priv( priv ); }

	// If the path already exists, we have nothing to do.
	if( std::filesystem::exists( path ) ) { return true; }

	return shadow_safe_mkdir_impl( path.root_path(), path.relative_path(), mode );
}

/*
  Define a macro to restore our priv state (if needed) and return.  We
  do this so we don't leak priv states in functions where we need to
  be in our desired state.
*/

#ifdef HAVE_DATA_REUSE_DIR
#define return_and_resetpriv(i)                     \
    if( saved_priv != PRIV_UNKNOWN )                \
        _set_priv(saved_priv,__FILE__,__LINE__,1);  \
    if ( m_reuse_dir && !reservation_id.empty() ) { \
        CondorError err;                            \
        if (!m_reuse_dir->ReleaseSpace(reservation_id, err)) { \
            dprintf(D_FULLDEBUG, "Failed to release space: %s\n", \
                err.getFullText().c_str());         \
        }                                           \
    }                                               \
    return i;
#else
#define return_and_resetpriv(i)                     \
    if( saved_priv != PRIV_UNKNOWN )                \
        _set_priv(saved_priv,__FILE__,__LINE__,1);  \
    return i;
#endif

int
FileTransfer::DoDownload( filesize_t *total_bytes_ptr, ReliSock *s)
{
	filesize_t bytes=0;
	filesize_t peer_max_transfer_bytes=0;
	std::string filename;;
	std::string fullname;
	int final_transfer = 0;
	bool download_success = true;
	bool try_again = true;
	int hold_code = 0;
	int hold_subcode = 0;
	std::string error_buf;
	int delegation_method = 0; /* 0 means this transfer is not a delegation. 1 means it is.*/
	time_t start, elapsed;
	int numFiles = 0;
	ClassAd pluginStatsAd;
	int plugin_exit_code = 0;
	bool deferred_checkpoint_error = false;

	// At the beginning of every download and every upload.
	pluginResultList.clear();

	// Variable for deferred transfers, used to transfer multiple files at once
	// by certain filte transfer plugins. These need to be scoped to the full
	// function.
	bool isDeferredTransfer = false;
	classad::ClassAdUnParser unparser;
	std::map<std::string, std::string> deferredTransfers;
	std::unique_ptr<classad::ClassAd> thisTransfer( new classad::ClassAd() );

	bool I_go_ahead_always = false;
	bool peer_goes_ahead_always = false;
	DCTransferQueue xfer_queue(m_xfer_queue_contact_info);
	std::function<void(void)> f {[this] { this->ReceiveAliveMessage(); }};
	s->SetXferAliveCallback(f);
	CondorError errstack;

	priv_state saved_priv = PRIV_UNKNOWN;
	*total_bytes_ptr = 0;

	downloadStartTime = condor_gettimestamp_double();

#ifdef HAVE_DATA_REUSE_DIR
	/* Track the potential data reuse
		 */
	std::vector<ReuseInfo> reuse_info;
	std::string reservation_id;
#endif

		// When we are signing URLs, we want to make sure that the requested
		// prefix is valid.
	std::vector<std::string> output_url_prefixes;
	std::string checkpointDestination;
	if( jobAd.LookupString( "CheckpointDestination", checkpointDestination ) ) {
		dprintf(D_FULLDEBUG, "DoDownload: Valid output URL prefix: %s\n", checkpointDestination.c_str());
		output_url_prefixes.emplace_back(checkpointDestination);
	}
	if (OutputDestination)
	{
		dprintf(D_FULLDEBUG, "DoDownload: Valid output URL prefix: %s\n", OutputDestination);
		output_url_prefixes.emplace_back(OutputDestination);
	}
	std::string remaps;
	if (jobAd.EvaluateAttrString(ATTR_TRANSFER_OUTPUT_REMAPS, remaps)) {
		for (auto& list_item: StringTokenIterator(remaps, ";")) {
			auto idx = list_item.find("=");
			if (idx != std::string::npos) {
				std::string url = list_item.substr(idx + 1);
				trim(url);
				dprintf(D_FULLDEBUG, "DoDownload: Valid output URL prefix: %s\n", url.c_str());
				output_url_prefixes.emplace_back(url);
			}
		}
	}

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
		dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
		return_and_resetpriv( -1 );
	}

	filesize_t sandbox_size = 0;
	if( PeerDoesXferInfo ) {
		ClassAd xfer_info;
		if( !getClassAd(s,xfer_info) ) {
			dprintf(D_ERROR,"DoDownload: failed to receive xfer info; exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}
		xfer_info.LookupInteger(ATTR_SANDBOX_SIZE,sandbox_size);
	}

	if( !s->end_of_message() ) {
		dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
		return_and_resetpriv( -1 );
	}

	if( !final_transfer && IsServer() ) {
		SpooledJobFiles::createJobSpoolDirectory(&jobAd,desired_priv_state);
	}

	std::string outputDirectory = Iwd;
	int directory_creation_mode = 0700;
	bool all_transfers_succeeded = true;
	if( final_transfer && IsServer() ) {
		jobAd.LookupString( "OutputDirectory", outputDirectory );
		if(! outputDirectory.empty()) {
			std::filesystem::path outputPath( outputDirectory );
			if(! outputPath.has_root_path()) {
				outputDirectory = (Iwd / outputPath).string();
			}

			if(! shadow_safe_mkdir(
				outputDirectory, directory_creation_mode, desired_priv_state
			)) {
				std::string err_str;
				int the_error = errno;

				formatstr( err_str,
					"%s at %s failed to create output directory (%s): %s (errno %d)",
					get_mySubSystem()->getName(),
					s->my_ip_str(), outputDirectory.c_str(),
					strerror(the_error), the_error
				);
				dprintf(D_ALWAYS,
					"DoDownload: consuming rest of transfer and failing "
					"after encountering the following error: %s\n",
					err_str.c_str()
				);

				if( all_transfers_succeeded ) {
					download_success = false;
					try_again = false;
					hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
					hold_subcode = the_error;
					error_buf = err_str;

					all_transfers_succeeded = false;
				}
			}
		}
	}

	bool sign_s3_urls = param_boolean("SIGN_S3_URLS", true) && PeerDoesS3Urls;

		/*
		  If we want to change priv states, do it now.
		  Even if we don't transfer any files, we write a commit
		  file at the end.
		*/
	if( want_priv_change ) {
		saved_priv = set_priv( desired_priv_state );
	}

	// Not sure if we can safely add another value to `rc`, since
	// it's defined -- of all places -- in `ReliSock.h`, and we
	// really don't want the value leaking out of this function.
	bool file_transfer_plugin_timed_out   = false;
	bool file_transfer_plugin_exec_failed = false;

	// Start the main download loop. Read reply codes + filenames off a
	// socket wire, s, then handle downloads according to the reply code.
	for( int rc = 0; ; ) {
		bool log_this_transfer = true;

		TransferCommand xfer_command = TransferCommand::Unknown;
		{
			int reply;
			if( !s->code(reply) ) {
				dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}
			xfer_command = static_cast<TransferCommand>(reply);
		}
		if( !s->end_of_message() ) {
			dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}
		dprintf( D_FULLDEBUG, "FILETRANSFER: incoming file_command is %i\n", static_cast<int>(xfer_command));
		if( xfer_command == TransferCommand::Finished ) {
			break;
		}

		if ((xfer_command == TransferCommand::EnableEncryption) || (PeerDoesS3Urls && xfer_command == TransferCommand::DownloadUrl)) {
			bool cryp_ret = s->set_crypto_mode(true);
			if (!cryp_ret) {
				dprintf(D_ERROR,"DoDownload: failed to enable crypto on incoming file, exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}
		} else if (xfer_command == TransferCommand::DisableEncryption) {
			s->set_crypto_mode(false);
		} else {
			bool cryp_ret = s->set_crypto_mode(socket_default_crypto);
			if(!cryp_ret) {
				dprintf(D_ALWAYS,"DoDownload: failed to change crypto to %i on incoming file, "
					"exiting at %d\n", socket_default_crypto, __LINE__);
				return_and_resetpriv( -1 );
			}
		}

		if( !s->code(filename) ) {
			dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}
			// This check must come after we have called set_priv()
		if( !LegalPathInSandbox(filename.c_str(),outputDirectory.c_str()) ) {
			// Our peer sent us an illegal path!

			download_success = false;
			try_again = false;
			hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
			hold_subcode = EPERM;

			formatstr_cat(error_buf,
				" Attempt to write to illegal sandbox path: %s",
				filename.c_str());

			dprintf(D_ALWAYS,"DoDownload: attempt to write to illegal sandbox path by our peer %s: %s.\n",
					s->peer_description(),
					filename.c_str());

			// Just write to /dev/null and go ahead with the download.
			// This allows us to consume the rest of the downloads and
			// propagate the error message, put the job on hold, etc.
			filename = NULL_FILE;
		}

		// An old peer is sending us a renamed executable.
		// Use the real filename from the job ad
		if( ExecFile && IsClient() && !simple_init && !file_strcmp(filename.c_str(), CONDOR_EXEC)) {
			filename = ExecFile;
		}

		if( !strcmp(filename.c_str(),NULL_FILE) ) {
			fullname = filename;
		}
		else if( final_transfer || IsClient() ) {
			std::string remap_filename;
			int res = filename_remap_find(download_filename_remaps.c_str(),filename.c_str(),remap_filename,0);
			dprintf(D_FULLDEBUG, "REMAP: res is %i -> %s !\n", res, remap_filename.c_str());
			if (res == -1) {
				// there was loop in the file transfer remaps, so set a good
				// hold reason
				formatstr(error_buf, "remaps resulted in a cycle: %s", remap_filename.c_str());
				dprintf(D_ALWAYS,"REMAP: DoDownload: %s\n",error_buf.c_str());
				download_success = false;
				try_again = false;
				hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
				hold_subcode = EPERM;

					// In order for the wire protocol to remain in a well
					// defined state, we must consume the rest of the
					// file transmission without writing.
				fullname = NULL_FILE;
			}
			else if(res) {
					// If we are a client downloading the output sandbox, it makes no sense for
					// us to "download" _to_ a URL; the server sent us this in a logic error
					// unless it was simply a status report (reply == 999)
				if (IsUrl(remap_filename.c_str())) {
					if (xfer_command != TransferCommand::Other) {
						formatstr(error_buf, "Remap of output file resulted in a URL: %s", remap_filename.c_str());
						dprintf(D_ALWAYS, "REMAP: DoDownload: %s\n",error_buf.c_str());
						download_success = false;
						try_again = false;
						hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
						hold_subcode = EPERM;
						fullname = NULL_FILE;
					} else {
						// fullname is used in various error messages; keep it
						// as something reasonabel.
						formatstr(fullname,"%s%c%s",outputDirectory.c_str(),DIR_DELIM_CHAR,filename.c_str());
					}
				//
				// legit remap was found
				//
				} else if(fullpath(remap_filename.c_str())) {
					fullname = remap_filename;
				}
				else {
					// For simplicity of reasoning about the security
					// implications, don't try to create absolute directories
					// for absolute paths named in transfer_output_remaps.
					//
					// Because the `output` and `error` submit commands are
					// implemented with transfer_output_remaps, this code
					// also creates the directories they name.  This is a
					// semantic change, and one that changes a job from going
					// on hold (after running for its duration) to succeeding.
					if( param_boolean("ALLOW_TRANSFER_REMAP_TO_MKDIR",false) ) {
						std::string dirname = condor_dirname(remap_filename.c_str());
						if( strcmp(dirname.c_str(), ".") ) {
							std::string path;
                            formatstr(path, "%s%c%s", outputDirectory.c_str(), DIR_DELIM_CHAR, dirname.c_str());
#if DEBUG_OUTPUT_REMAP_MKDIR_FAILURE_REPORTING
							// So this fails on a dirA/dirB/filename remaps,
							// which seemed like the easiest way to trigger
							// the error-handling code on Linux.  (On Windows,
							// you can jsut specify illegal directory names.)
							int rv = mkdir( path.c_str(), 0700 );
							if( rv != 0 ) {
								dprintf(D_ZKM, "mkdir(%s) = %d, errno %d\n", path.c_str(), rv, errno );
							}
							if( rv != 0 && errno != EEXIST ) {
#else
							bool success = shadow_safe_mkdir(
							  path, directory_creation_mode, desired_priv_state
							  );

							if( (! success) ) {
#endif /* DEBUG_OUTPUT_REMAP_MKDIR_FAILURE_REPORTING */
								std::string err_str;
								int the_error = errno;

								formatstr( err_str,
									"%s at %s failed to create directory %s: %s (errno %d)",
									get_mySubSystem()->getName(),
									s->my_ip_str(), path.c_str(),
									strerror(the_error), the_error
								);
								dprintf(D_ALWAYS,
									"DoDownload: consuming rest of transfer and failing "
									"after encountering the following error: %s\n",
									err_str.c_str()
									);

								if( all_transfers_succeeded ) {
									download_success = false;
									try_again = false;
									hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
									hold_subcode = the_error;
									error_buf = err_str;

									all_transfers_succeeded = false;
								}
							}
						}
					}

                    formatstr(fullname,"%s%c%s",outputDirectory.c_str(),DIR_DELIM_CHAR,remap_filename.c_str());
				}
				dprintf(D_FULLDEBUG,"Remapped downloaded file from %s to %s\n",filename.c_str(),remap_filename.c_str());
			}
			else {
				// no remap found
				formatstr(fullname,"%s%c%s",outputDirectory.c_str(),DIR_DELIM_CHAR,filename.c_str());
			}
#ifdef WIN32
			// check for write permission on this file, if we are supposed to check
			if ( (fullname != NULL_FILE) && perm_obj && (perm_obj->write_access(fullname.c_str()) != 1) ) {
				// we do _not_ have permission to write this file!!
				formatstr(error_buf, "Permission denied to write file %s!",
				                   fullname.c_str());
				dprintf(D_ALWAYS,"DoDownload: %s\n",error_buf.c_str());
				download_success = false;
				try_again = false;
				hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
				hold_subcode = EPERM;

					// In order for the wire protocol to remain in a well
					// defined state, we must consume the rest of the
					// file transmission without writing.
				fullname = NULL_FILE;
			}
#endif
		} else {
			formatstr(fullname,"%s%c%s",TmpSpoolSpace.c_str(),DIR_DELIM_CHAR,filename.c_str());
		}

#ifdef HAVE_DATA_REUSE_DIR
		auto iter = std::find_if(reuse_info.begin(), reuse_info.end(),
			[&](ReuseInfo &info){return !strcmp(filename.c_str(), info.filename().c_str());});
		bool should_reuse = !reservation_id.empty() && m_reuse_dir && iter != reuse_info.end();
#endif

		if( PeerDoesGoAhead ) {
			if( !s->end_of_message() ) {
				dprintf(D_ERROR,"DoDownload: failed on eom before GoAhead: exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}

			if( !I_go_ahead_always ) {
					// The following blocks until getting the go-ahead
					// (e.g.  from the local schedd) to receive the
					// file.  It then sends a message to our peer
					// telling it to go ahead.
				if( !ObtainAndSendTransferGoAhead(xfer_queue,true,s,sandbox_size,fullname.c_str(),I_go_ahead_always) ) {
					dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
					return_and_resetpriv( -1 );
				}
			}

				// We have given permission to peer to go ahead
				// with transfer.  Now do the reverse: wait for
				// peer to tell is that it is ready to send.
			if( !peer_goes_ahead_always ) {

				if( !ReceiveTransferGoAhead(s,fullname.c_str(),true,peer_goes_ahead_always,peer_max_transfer_bytes) ) {
					dprintf(D_ERROR, "DoDownload: exiting at %d\n",__LINE__);
					return_and_resetpriv( -1 );
				}
			}

			s->decode();
		}

		UpdateXferStatus(XFER_STATUS_ACTIVE);

		filesize_t this_file_max_bytes = -1;
		filesize_t max_bytes_slack = 65535;
		if( MaxDownloadBytes < 0 ) {
			this_file_max_bytes = -1; // no limit
		}
		else if( MaxDownloadBytes + max_bytes_slack >= *total_bytes_ptr ) {

				// We have told the sender our limit, and a
				// well-behaved sender will not send more than that.
				// We give the sender a little slack, because there
				// may be minor differences in how the sender and
				// receiver account for the size of some things (like
				// proxies and what-not).  It is best if the sender
				// reaches the limit first, because that path has
				// better error handling.  If instead the receiver
				// hits its limit first, the connection is closed, and
				// the sender will not understand why.

			this_file_max_bytes = MaxDownloadBytes + max_bytes_slack - *total_bytes_ptr;
		}
		else {
			this_file_max_bytes = 0;
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
//		dprintf(D_FULLDEBUG,"TODD filetransfer DoDownload fullname=%s\n",fullname.c_str());
		start = time(NULL);

		// Setup the FileTransferStats object for this file, which we'll use
		// to gather per-transfer statistics (different from the other
		// statistics gathering which only tracks cumulative totals)
		FileTransferStats thisFileStats;
		thisFileStats.TransferFileBytes = 0;
		thisFileStats.TransferFileName = filename;
		thisFileStats.TransferProtocol = "cedar";
		thisFileStats.TransferStartTime = condor_gettimestamp_double();
		thisFileStats.TransferType = "download";

		// Create a ClassAd we'll use to store stats from a file transfer
		// plugin, if we end up using one.
		ClassAd pluginStatsAd;

		// Until we are told otherwise, assume this file transfer will not be
		// deferred until the end of the loop.
		isDeferredTransfer = false;

		if (xfer_command == TransferCommand::Other) {
			// filename already received:
			// .  verify that it is the same as FileName attribute in following classad
			// .  optimization: could be the version protocol instead
			//
			// receive the classad
			ClassAd file_info;
			if (!getClassAd(s, file_info)) {
				dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}

			// examine subcommand
			//
			TransferSubCommand subcommand;
			{
				int subcommand_int;
				if (!file_info.LookupInteger("SubCommand", subcommand_int)) {
					subcommand = TransferSubCommand::Unknown;
				} else {
					subcommand = static_cast<TransferSubCommand>(subcommand_int);
				}
			}
			// perform specified subcommand
			//
			// (this can be made a switch statement when more show up)
			//

			if(subcommand == TransferSubCommand::UploadUrl) {
				// 7 == send local file using plugin

				std::string rt_src;
				std::string rt_dst;
				std::string rt_err;
				int      rt_result = 0;
				if(!file_info.LookupInteger("Result",rt_result)) {
					rt_result = -1;
				}

				if(!file_info.LookupString("Filename", rt_src)) {
					rt_src = "<null>";
				}

				if(!file_info.LookupString("OutputDestination", rt_dst)) {
					rt_dst = "<null>";
				}

				if(!file_info.LookupString("ErrorString", rt_err)) {
					rt_err = "<null>";
				}
				bool checkpoint_url = false;
				file_info.LookupBool("CheckpointURL", checkpoint_url);

				// TODO: write to job log success/failure for each file (as a custom event?)
				dprintf(D_ALWAYS, "DoDownload: other side transferred %s to %s and got result %i\n",
				        UrlSafePrint(rt_src), UrlSafePrint(rt_dst), rt_result );

				if(rt_result == 0) {
					rc = 0;
				} else {
					// handle the error now and bypass error handling
					// that hapens further down
					rc = 0;

					// FIXME: report only the first error

					formatstr(error_buf,
						"%s at %s failed due to remote transfer hook error: %s",
						get_mySubSystem()->getName(),
						s->my_ip_str(),fullname.c_str());
					if( checkpoint_url ) {
						deferred_checkpoint_error = true;
						log_this_transfer = false;
					} else {
						download_success = false;
						try_again = false;
					}
					hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
					hold_subcode = rt_result;

					dprintf(D_ALWAYS,
						"DoDownload: consuming rest of transfer and failing "
						"after encountering the following error: %s\n",
						error_buf.c_str());
				}

				// If the starter is telling us about an upload it performed,
				// we shouldn't log anything at all -- we don't know wha the
				// right thing to log is, and we _certainly_ shouldn't log
				// the default "download" reply saying that the file was
				// transferred via CEDAR.  However, we _certainly_ shouldn't
				// log a failed transfer as a success, which I think we also
				// used to do.  *sigh*
			} else if (subcommand == TransferSubCommand::ReuseInfo) {
					// We must consume the EOM in order to send the ClassAd later.
				if (!s->end_of_message()) {
					dprintf(D_FULLDEBUG,"DoDownload: exiting at %d\n",__LINE__);
				}
				ClassAd ad;
			#ifdef HAVE_DATA_REUSE_DIR
				if (m_reuse_dir == nullptr) {
					dprintf(D_FULLDEBUG, "DoDownload: No data reuse directory available; ignoring potential reuse info.\n");
					ad.InsertAttr("Result", 1);
					rc = 0;
				} else {
					classad::Value value;
					std::string tag;
					if (!file_info.EvaluateAttr("ReuseList", value) ||
						(value.GetType() != classad::Value::SLIST_VALUE &&
						value.GetType() != classad::Value::LIST_VALUE) ||
						!file_info.EvaluateAttrString("Tag", tag))
					{
						dprintf(D_FULLDEBUG, "The reuse info ClassAd is missing attributes.\n");
						dPrintAd(D_FULLDEBUG, file_info);
						rc = 0;
					} else {
						classad_shared_ptr<classad::ExprList> exprlist;
						value.IsSListValue(exprlist);
						std::vector<std::string> retrieved_files;
						for (auto &list_entry : (*exprlist)) {
							classad::Value file_ad_value;
							if (!list_entry->Evaluate(file_ad_value)) {
								dprintf(D_FULLDEBUG, "Failed to evaluate list entry.\n");
								continue;
							}
							classad_shared_ptr<classad::ClassAd> file_ad;
							if (!file_ad_value.IsSClassAdValue(file_ad)) {
								dprintf(D_FULLDEBUG, "Failed to evaluate list entry to ClassAd.\n");
								continue;
							}
							std::string fname;
							if (!file_ad->EvaluateAttrString("FileName", fname)) {
								dprintf(D_FULLDEBUG, "List entry is missing FileName attr.\n");
								continue;
							}
							std::string checksum_type;
							if (!file_ad->EvaluateAttrString("ChecksumType", checksum_type)) {
								dprintf(D_FULLDEBUG, "List entry is missing ChecksumType attr.\n");
								continue;
							}
							std::string checksum;
							if (!file_ad->EvaluateAttrString("Checksum", checksum)) {
								dprintf(D_FULLDEBUG, "List entry is missing Checksum attr.\n");
								continue;
							}
							long long size;
							if (!file_ad->EvaluateAttrInt("Size", size)) {
								dprintf(D_FULLDEBUG, "List entry is missing Size attr.\n");
								continue;
							}
							std::string dest_fname = outputDirectory + DIR_DELIM_CHAR + fname;
							CondorError err;
							if (!m_reuse_dir->RetrieveFile(dest_fname, checksum, checksum_type, tag,
								err))
							{
								dprintf(D_FULLDEBUG, "Failed to retrieve file of size %lld from data"
									" reuse directory: %s\n", size, err.getFullText().c_str());
								reuse_info.emplace_back(fname, checksum, checksum_type,
									tag, size < 0 ? 0 : size);
								continue;
							}
							dprintf(D_FULLDEBUG, "Successfully retrieved %s from data reuse directory into job sandbox.\n", fname.c_str());
							retrieved_files.push_back(fname);
						}
						std::unique_ptr<classad::ExprList> retrieved_list(new classad::ExprList());
						for (const auto &file : retrieved_files) {
							classad::ExprTree *expr = classad::Literal::MakeString(file);
							retrieved_list->push_back(expr);
						}
						uint64_t to_retrieve = std::accumulate(reuse_info.begin(), reuse_info.end(),
							static_cast<uint64_t>(0), [](uint64_t val, const ReuseInfo &info) {return info.size() + val;});
						dprintf(D_FULLDEBUG, "There are %llu bytes to retrieve.\n",
							static_cast<unsigned long long>(to_retrieve));
						if (to_retrieve) {
							CondorError err;
							if (!m_reuse_dir->ReserveSpace(to_retrieve, 3600, tag, reservation_id,
								err))
							{
								dprintf(D_FULLDEBUG, "Failed to reserve space for data reuse:"
									" %s\n", err.getFullText().c_str());
								retrieved_files.clear();
								reuse_info.clear();
							}
							for (const auto &info : reuse_info) {
								dprintf(D_FULLDEBUG, "File we will reuse: %s\n", info.filename().c_str());
							}
						}
						ad.Insert("ReuseList", retrieved_list.release());
						rc = 0;
					}
				}
			#else
				dprintf(D_FULLDEBUG, "DoDownload: No data reuse directory available; ignoring potential reuse info.\n");
				ad.InsertAttr("Result", 1);
				rc = 0;
			#endif
				s->encode();
				if (!putClassAd(s, ad) || !s->end_of_message()) {
					dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
					return_and_resetpriv( -1 );
				}
				s->decode();
				continue;
			} else if (subcommand == TransferSubCommand::SignUrls) {
				dprintf(D_FULLDEBUG, "DoDownload: Received request to sign URLs.\n");
					// We must consume the EOM in order to send the ClassAd later.
				if (!s->end_of_message()) {
					dprintf(D_ERROR, "DoDownload: exiting at %d\n",__LINE__);
					return_and_resetpriv( -1 );
				}
				ClassAd result_ad;
				classad::Value value;
				if (!file_info.EvaluateAttr("SignList", value) ||
					(value.GetType() != classad::Value::SLIST_VALUE &&
					value.GetType() != classad::Value::LIST_VALUE))
				{
					dprintf(D_FULLDEBUG, "DoDownload: The signing URL list info in ClassAd is missing.\n");
					dPrintAd(D_FULLDEBUG, file_info);
					rc = 0;
				} else {
					classad_shared_ptr<classad::ExprList> exprlist;
					value.IsSListValue(exprlist);
					std::vector<std::string> signed_urls;
					for (auto list_entry : (*exprlist)) {
						std::string url_value;
						classad::Value value;
						if (!list_entry->Evaluate(value)) {
							dprintf(D_FULLDEBUG, "DoDownload: Failed to evaluate list entry.\n");
							signed_urls.emplace_back("");
						}
						else if (!value.IsStringValue(url_value))
						{
							dprintf(D_FULLDEBUG, "DoDownload: Failed to evaluate list entry to string.\n");
							signed_urls.emplace_back("");
						}
						else if (sign_s3_urls &&
						  (starts_with_ignore_case(url_value, "s3://") || starts_with_ignore_case(url_value, "gs://")))
						{
							bool has_good_prefix = false;
							for (const auto &prefix : output_url_prefixes) {
								if (url_value.substr(0, prefix.size()) == prefix) {
									has_good_prefix = true;
									break;
								}
							}
								// We don't deal with normalization correctly -- avoid
								// any URL that has ".." in it.
							if (url_value.find("/..") != std::string::npos) {
								has_good_prefix = false;
							}
							if (has_good_prefix) {
								dprintf(D_FULLDEBUG, "DoDownload: URL will be signed: %s.\n", url_value.c_str());
								std::string signed_url;
								CondorError err;
								if (!htcondor::generate_presigned_url(jobAd, url_value, "PUT", signed_url, err)) {
								    std::string errorMessage;
								    formatstr( errorMessage, "DoDownload: Failure when signing URL '%s': %s", url_value.c_str(), err.message() );
								    result_ad.Assign( ATTR_HOLD_REASON_CODE, FILETRANSFER_HOLD_CODE::DownloadFileError );
								    result_ad.Assign( ATTR_HOLD_REASON_SUBCODE, err.code() );
								    result_ad.Assign( ATTR_HOLD_REASON, errorMessage.c_str() );
								    dprintf( D_ALWAYS, "%s\n", errorMessage.c_str() );

									signed_urls.emplace_back("");
								} else {
									signed_urls.push_back(signed_url);
								}
							} else {
								dprintf(D_FULLDEBUG, "DoDownload: URL has invalid prefix: %s.\n", url_value.c_str());
								signed_urls.emplace_back("");
							}
						}
						else
						{
							signed_urls.push_back(url_value);
						}
					}
					classad::ExprList url_list;
					for (const auto &url : signed_urls) {
						auto expr = classad::Literal::MakeString(url);
						url_list.push_back(expr);
					}
					result_ad.Insert("SignList", url_list.Copy());
					rc = 0;
				}
				s->encode();
					// Send resulting list of signed URLs, encrypted if possible.
				classad::References encrypted_attrs{"SignList"};
				if (!putClassAd(s, result_ad, 0, NULL, &encrypted_attrs) || !s->end_of_message()) {
					dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
					return_and_resetpriv( -1 );
				}
				s->decode();
				continue;
			} else {
				// unrecongized subcommand
				dprintf(D_ALWAYS, "FILETRANSFER: unrecognized subcommand %i! skipping!\n", static_cast<int>(subcommand));
				dPrintAd(D_FULLDEBUG, file_info);

				rc = 0;
			}
		} else if (xfer_command == TransferCommand::DownloadUrl) {
			// new filetransfer command.  5 means that the next file is a
			// 3rd party transfer.  cedar will not send the file itself,
			// and instead will send the URL over the wire.  the receiving
			// side must then retreive the URL using one of the configured
			// filetransfer plugins.

			std::string URL;
			// receive the URL from the wire

			if (!s->code(URL)) {
				dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}

			if( !I_support_filetransfer_plugins ) {
				// we shouldn't get here, because a job shouldn't match to a machine that won't
				// support URL transfers if the job needs URL transfers.  but if we do get here,
				// give a nice error message.
				errstack.pushf( "FILETRANSFER", 1, "URL transfers are disabled by configuration.  Cannot transfer URL file = %s .", UrlSafePrint(URL));  // URL file in err msg
				dprintf ( D_FULLDEBUG, "FILETRANSFER: URL transfers are disabled by configuration.  Cannot transfer %s.\n", UrlSafePrint(URL));
				rc = GET_FILE_PLUGIN_FAILED;
			}

			if( all_transfers_succeeded && (rc != GET_FILE_PLUGIN_FAILED) && multifile_plugins_enabled ) {

				// Determine which plugin to invoke, and whether it supports multiple
				// file transfer.
				std::string pluginPath = DetermineFileTransferPlugin( errstack, URL.c_str(), fullname.c_str() );
				bool thisPluginSupportsMultifile = false;
				if( plugins_multifile_support.find( pluginPath ) != plugins_multifile_support.end() ) {
					thisPluginSupportsMultifile = plugins_multifile_support[pluginPath];
				}

				if( thisPluginSupportsMultifile ) {
					// Do not send the file right now!
					// Instead, add it to a deferred list, which we'll deal with
					// after the main download loop.
					dprintf( D_FULLDEBUG, "DoDownload: deferring transfer of URL %s "
						" until end of download loop.\n", UrlSafePrint(URL) );
					thisTransfer->Clear();
					thisTransfer->InsertAttr( "Url", URL );
					thisTransfer->InsertAttr( "LocalFileName", fullname );
					std::string thisTransferString;
					unparser.Unparse( thisTransferString, thisTransfer.get() );

					// Add this result to our deferred transfers map.
					if ( deferredTransfers.find( pluginPath ) == deferredTransfers.end() ) {
						deferredTransfers.insert( std::pair<std::string, std::string>( pluginPath, thisTransferString ) );
					}
					else {
						deferredTransfers[pluginPath] += thisTransferString;
					}

					isDeferredTransfer = true;
				}
			}

			if( all_transfers_succeeded && (rc != GET_FILE_PLUGIN_FAILED) && (!isDeferredTransfer) ) {
				dprintf( D_FULLDEBUG, "DoDownload: doing a URL transfer: (%s) to (%s)\n", UrlSafePrint(URL), UrlSafePrint(fullname));
				int exit_status = 0;
				TransferPluginResult result = InvokeFileTransferPlugin(errstack, exit_status, URL.c_str(), fullname.c_str(), &pluginStatsAd, LocalProxyName.c_str());
				// If transfer failed, set rc to error code that ReliSock recognizes
				switch( result ) {
					case TransferPluginResult::TimedOut:
						file_transfer_plugin_timed_out = true;
						[[fallthrough]];
					case TransferPluginResult::InvalidCredentials:
					case TransferPluginResult::Error:
						rc = GET_FILE_PLUGIN_FAILED;
						plugin_exit_code = exit_status;
						break;
					case TransferPluginResult::ExecFailed:
						file_transfer_plugin_exec_failed = true;
						break;
					case TransferPluginResult::Success:
						break;
				}
			#ifdef HAVE_DATA_REUSE_DIR
				CondorError err;
				if (result == TransferPluginResult::Success && should_reuse && !m_reuse_dir->CacheFile(fullname.c_str(), iter->checksum(),
					iter->checksum_type(), reservation_id, err))
				{
					dprintf(D_FULLDEBUG, "Failed to save file %s for reuse: %s\n", fullname.c_str(),
					err.getFullText().c_str());
						// Checksum failed; we shouldn't start the job with this file
					if (!strcmp(err.subsys(), "DataReuse") && err.code() == 11) {
						rc = -1;
					}
				}
			#endif
			}

		} else if ( xfer_command == TransferCommand::XferX509 ) {
			if ( PeerDoesGoAhead || s->end_of_message() ) {
				rc = (s->get_x509_delegation( fullname.c_str(), false, NULL ) == ReliSock::delegation_ok) ? 0 : -1;
				dprintf( D_FULLDEBUG,
				         "DoDownload: get_x509_delegation() returned %d\n",
				         rc );
				if (rc == 0) {
					// ZKM FUTURE TODO: allow this to exist outside of the job sandbox -- we may
					// need the proxy to create the sandbox itself (if execute dir is NFSv4) but
					// that will require some higher-level refactoring
					LocalProxyName = fullname;
				}
			} else {
				rc = -1;
			}
			delegation_method = 1;/* This is a delegation, unseuccessful or not */
		} else if( xfer_command == TransferCommand::Mkdir ) { // mkdir
			condor_mode_t file_mode = NULL_FILE_PERMISSIONS;
			if( !s->code(file_mode) ) {
				rc = -1;
				dprintf(D_ALWAYS,"DoDownload: failed to read mkdir mode.\n");
			}
			else {
				if (file_mode == NULL_FILE_PERMISSIONS) {
					// Don't create subdirectories with mode 0000!
					// If file_mode is still NULL_FILE_PERMISSIONS here, it
					// likely means that our peer is likely a Windows machine,
					// since Windows will always claim a mode of 0000.
					// In this case, default to mode 0700, which is a
					// conservative default, and matches what we do in
					// ReliSock::get_file().
					file_mode = (condor_mode_t) 0700;
				}
				mode_t old_umask = umask(0);
				rc = mkdir(fullname.c_str(),(mode_t)file_mode);
				umask(old_umask);
				if( rc == -1 && errno == EEXIST ) {
						// The directory name already exists.  If it is a
						// directory, just leave it alone, because the
						// user may want us to append files to an
						// existing output directory.  Otherwise, try
						// to remove it and try again.
					StatInfo st( fullname.c_str() );
					if( !st.Error() && st.IsDirectory() ) {
						dprintf(D_FULLDEBUG,"Requested to create directory but using existing one: %s\n",fullname.c_str());
						rc = 0;
					}
					else if( !strcmp(fullname.c_str(),NULL_FILE) ) {
							// we are just fast-forwarding through the
							// download, so just ignore this request
						rc = 0;
					}
					else {
						IGNORE_RETURN remove(fullname.c_str());
						old_umask = umask(0);
						rc = mkdir(fullname.c_str(),(mode_t)file_mode);
						umask(old_umask);
					}
				}
				if( rc == -1 ) {
					// handle the error now and bypass error handling
					// that hapens further down
					rc = 0;

					int the_error = errno;
					formatstr(error_buf,
						"%s at %s failed to create directory %s: %s (errno %d)",
						get_mySubSystem()->getName(),
						s->my_ip_str(),fullname.c_str(),
						strerror(the_error),the_error);
					download_success = false;
					try_again = false;
					hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
					hold_subcode = the_error;

					dprintf(D_ALWAYS,
						"DoDownload: consuming rest of transfer and failing "
						"after encountering the following error: %s\n",
						error_buf.c_str());
				}
			}
		} else if ( TransferFilePermissions ) {
			// We could create the target's parent directories, but since
			// we need to have sent them along as explicit transfer items
			// to preserve their permissions, let's just let this transfer
			// fail if the remote side screwed up.
			rc = s->get_file_with_permissions( &bytes, fullname.c_str(), false, this_file_max_bytes, &xfer_queue );
		#ifdef HAVE_DATA_REUSE_DIR
			CondorError err;
			if (rc == 0 && should_reuse && !m_reuse_dir->CacheFile(fullname.c_str(), iter->checksum(),
					iter->checksum_type(), reservation_id, err))
			{
				dprintf(D_FULLDEBUG, "Failed to save file %s for reuse: %s\n", fullname.c_str(),
					err.getFullText().c_str());
					// Checksum of downloaded file failed to match the user-provided one.
				if (!strcmp(err.subsys(), "DataReuse") && err.code() == 11) {
					rc = -1;
				}
			}
		#endif
		} else {
			// See comment about directory creation above.
			rc = s->get_file( &bytes, fullname.c_str(), false, false, this_file_max_bytes, &xfer_queue );
		}

		int the_error = errno;

		elapsed = time(NULL)-start;
		thisFileStats.TransferEndTime = condor_gettimestamp_double();
		thisFileStats.ConnectionTimeSeconds = thisFileStats.TransferEndTime - thisFileStats.TransferStartTime;

		// Report only the first error.
		if( rc < 0 && all_transfers_succeeded ) {
			all_transfers_succeeded = false;
			formatstr(error_buf, "%s at %s - |Error: receiving file %s",
			                  get_mySubSystem()->getName(),
							  s->my_ip_str(),fullname.c_str());
			download_success = false;
			if(rc == GET_FILE_OPEN_FAILED || rc == GET_FILE_WRITE_FAILED ||
					rc == GET_FILE_PLUGIN_FAILED) {
				// errno is well defined in this case, and transferred data
				// has been consumed so that the wire protocol is in a well
				// defined state

				if (rc == GET_FILE_PLUGIN_FAILED) {
					formatstr_cat(error_buf, ": %s", errstack.getFullText().c_str());
				} else {
					replace_str(error_buf, "receiving", "writing to");
					formatstr_cat(error_buf, ": (errno %d) %s",the_error,strerror(the_error));
				}

				// Since there is a well-defined errno describing what just
				// went wrong while trying to open the file, put the job
				// on hold.  Errors that are deemed to be transient can
				// be periodically released from hold.

				try_again = false;
				hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
				hold_subcode = the_error;

				// If plugin_exit_code is greater than 0, that indicates a
				// transfer plugin error. In this case set hold_subcode to the
				// plugin exit code left-shifted by 8, so we can differentiate
				// between plugin failures and regular cedar failures.

				if (plugin_exit_code > 0) {
					hold_subcode = plugin_exit_code << 8;
				}

				if( file_transfer_plugin_timed_out ) {
					hold_subcode = ETIME;
				}

				if( file_transfer_plugin_exec_failed) {
					try_again = true; // not our fault, try again elsewhere
				}

				dprintf(D_ALWAYS,
						"DoDownload: consuming rest of transfer and failing "
						"after encountering the following error: %s\n",
						error_buf.c_str());
			}
			else {
				// Assume we had some transient problem (e.g. network timeout)
				// In the current implementation of get_file(), errno is not
				// well defined in this case, so we can't report a specific
				// error message.
				try_again = true;
				hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
				hold_subcode = the_error;

				if( rc == GET_FILE_MAX_BYTES_EXCEEDED ) {
					try_again = false;
					formatstr_cat(error_buf, ": max total download bytes exceeded (max=%ld MB)",
											(long int)(MaxDownloadBytes/1024/1024));
					hold_code = CONDOR_HOLD_CODE::MaxTransferOutputSizeExceeded;
					hold_subcode = 0;
				}

				dprintf(D_ALWAYS,"DoDownload: %s\n",error_buf.c_str());

					// The wire protocol is not in a well defined state
					// at this point.  Try sending the ack message indicating
					// what went wrong, for what it is worth.
				SendTransferAck(s,download_success,try_again,hold_code,hold_subcode,error_buf.c_str());

				dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}
		}

		if ( ExecFile && !file_strcmp( condor_basename( ExecFile ), filename.c_str() ) ) {
				// We're receiving the executable, make sure execute
				// bit is set
				// TODO How should we modify the permisions of the
				//   executable? Adding any group or world permissions
				//   seems wrong. For now, behave the same way as the
				//   starter.
#if 0
			struct stat stat_buf;
			if ( stat( fullname.c_str(), &stat_buf ) < 0 ) {
				dprintf( D_ALWAYS, "Failed to stat executable %s, errno=%d (%s)\n",
						 fullname.c_str(), errno, strerror(errno) );
			} else if ( ! (stat_buf.st_mode & S_IXUSR) ) {
				stat_buf.st_mode |= S_IXUSR;
				if ( chmod( fullname.c_str(), stat_buf.st_mode ) < 0 ) {
					dprintf( D_ALWAYS, "Failed to set execute bit on %s, errno=%d (%s)\n",
							 fullname.c_str(), errno, strerror(errno) );
				}
			}
#else
			if ( chmod( fullname.c_str(), 0755 ) < 0 ) {
				dprintf( D_ALWAYS, "Failed to set execute bit on %s, errno=%d (%s)\n",
						 fullname.c_str(), errno, strerror(errno) );
			}
#endif
		}

		if ( want_fsync ) {
			struct utimbuf timewrap;

			time_t current_time = time(NULL);
			timewrap.actime = current_time;		// set access time to now
			timewrap.modtime = current_time;	// set modify time to now

			utime(fullname.c_str(),&timewrap);
		}

		if( !s->end_of_message() ) {
			return_and_resetpriv( -1 );
		}
		*total_bytes_ptr += bytes;
		thisFileStats.TransferFileBytes += static_cast<long long>(bytes);
		thisFileStats.TransferTotalBytes += static_cast<long long>(bytes);
		bytes = 0;

		numFiles++;
		if (xfer_command == TransferCommand::XferFile && rc == 0) {
			int num_cedar_files = 0;
			Info.stats.LookupInteger("CedarFilesCount", num_cedar_files);
			num_cedar_files++;
			Info.stats.InsertAttr("CedarFilesCount", num_cedar_files);
		}

		std::string container_image;
		jobAd.LookupString(ATTR_CONTAINER_IMAGE, container_image);

		if (container_image == filename) {
			Info.stats.Assign(ATTR_CONTAINER_DURATION, (time_t) thisFileStats.ConnectionTimeSeconds);
		}

		// Gather a few more statistics
		thisFileStats.TransferSuccess = download_success;

		// Merge the file transfer stats we recorded here with the stats
		// retrieved from a plugin. If we didn't use a file transfer plugin
		// this time, this ClassAd will just be empty.
		ClassAd thisFileStatsAd;
		thisFileStats.Publish(thisFileStatsAd);
		thisFileStatsAd.Update(pluginStatsAd);

		// Write stats to disk
		if( !isDeferredTransfer && log_this_transfer ) {
			RecordFileTransferStats(thisFileStatsAd);
		}

		// Get rid of compiler set-but-not-used warnings on Linux
		// Hopefully the compiler can just prune out the emitted code.
		if (delegation_method) {}
		if (elapsed) {}
	}
	// End of the main download loop

	// Release transfer queue slot after file has been put but before the
	// final transfer ACKs are done.  In the future where multifile transfers
	// plugins are used in DoDownload, this would allow DoDownload side to
	// perform external plugin-based transfers without the queue slot
	//
	xfer_queue.ReleaseTransferQueueSlot();

	// Now that we've completed the main file transfer loop, it's time to
	// transfer all files that needed a third party plugin. Iterate over the list
	// of deferred transfers, and invoke each set with the appopriate plugin.
	if ( hold_code == 0 ) {
		for ( auto it = deferredTransfers.begin(); it != deferredTransfers.end(); ++ it ) {
			int exit_status = 0;
			TransferPluginResult result = InvokeMultipleFileTransferPlugin( errstack, exit_status, it->first, it->second, LocalProxyName.c_str(), false);
			if (result == TransferPluginResult::Success) {
				/*  TODO: handle deferred files.  We may need to unparse the deferredTransfers files. */
			} else {
				dprintf( D_ALWAYS, "FILETRANSFER: Multiple file download failed: %s\n",
					errstack.getFullText().c_str() );
				download_success = false;
				hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
				hold_subcode = exit_status << 8;
				if( result == TransferPluginResult::TimedOut ) {
					hold_subcode = ETIME;
				}
				try_again = false;
				formatstr(error_buf, "%s", errstack.getFullText().c_str());
				if (result == TransferPluginResult::ExecFailed) {
					try_again = true; // not the job's fault
				}
			}
		}
	}

	// go back to the state we were in before file transfer
	s->set_crypto_mode(socket_default_crypto);

	bytesRcvd += (*total_bytes_ptr);

	// Receive final report from the sender to make sure all went well.
	bool upload_success = false;
	std::string upload_error_buf;
	bool upload_try_again = true;
	int upload_hold_code = 0;
	int upload_hold_subcode = 0;
	GetTransferAck(s,upload_success,upload_try_again,upload_hold_code,
				   upload_hold_subcode,upload_error_buf);
	if(!upload_success) {
		// Our peer had some kind of problem sending the files.

		char const *peer_ip_str = "disconnected socket";
		if(s->type() == Stream::reli_sock) {
			peer_ip_str = ((Sock *)s)->get_sinful_peer();
		}

		std::string download_error_buf;
		formatstr(download_error_buf, "%s failed to receive file(s) from %s",
						  get_mySubSystem()->getName(),peer_ip_str);
		formatstr(error_buf, "%s; %s",
						  upload_error_buf.c_str(),
						  download_error_buf.c_str());
		dprintf(D_ALWAYS,"DoDownload: %s\n",error_buf.c_str());

		download_success = false;
		SendTransferAck(s,download_success,upload_try_again,upload_hold_code,
						upload_hold_subcode,download_error_buf.c_str());

			// store full-duplex error description, because only our side
			// of the story was stored in above call to SendTransferAck
		Info.error_desc = error_buf;

		dprintf( D_ALWAYS, "DoDownload: exiting with upload errors\n" );
		return_and_resetpriv( -1 );
	}

	if( !download_success ) {
		SendTransferAck(s,download_success,try_again,hold_code,
						hold_subcode,error_buf.c_str());

		dprintf( D_ALWAYS, "DoDownload: exiting with download errors\n" );
		return_and_resetpriv( -1 );
	}

	if ( !final_transfer && IsServer() ) {
		std::string buf;
		int fd;
		// we just stashed all the files in the TmpSpoolSpace.
		// write out the commit file.

		formatstr(buf,"%s%c%s",TmpSpoolSpace.c_str(),DIR_DELIM_CHAR,COMMIT_FILENAME);
#if defined(WIN32)
		if ((fd = safe_open_wrapper_follow(buf.c_str(), O_WRONLY | O_CREAT | O_TRUNC |
			_O_BINARY | _O_SEQUENTIAL, 0644)) < 0)
#else
		if ((fd = safe_open_wrapper_follow(buf.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
#endif
		{
			dprintf(D_ALWAYS,
				"FileTransfer::DoDownload failed to write commit file\n");
			return_and_resetpriv( -1 );
		}

		// copy in list of files to remove here
		::close(fd);

		// Now actually perform the commit.
		CommitFiles();

	}

	downloadEndTime = condor_gettimestamp_double();

	// If we're uploading a checkpoint file and suppressed an error uploading
	// a URL to make sure we could clean it up later (by sending the MANIFEST
	// file to SPOOL), report the error now.
	//
	// We report "try again" for this error so that the starter doesn't put
	// the job on hold.  It would be better to inspect the plug-in's result
	// to make that decision (most failures are try-again).
	if( deferred_checkpoint_error && (hold_code != 0) ) {
		SendTransferAck(s,
		false /* failed */, true /* try again */,
		hold_code, hold_subcode,
		error_buf.c_str()
		);

		dprintf( D_ALWAYS, "DoDownload: exiting after allowing checkpoint to write its MANIFEST file.\n" );
		return_and_resetpriv( -1 );
	}

	download_success = true;
	SendTransferAck(s,download_success,try_again,hold_code,hold_subcode,NULL);

		// Log some tcp statistics about this transfer
	if (*total_bytes_ptr > 0) {
		char *stats = s->get_statistics();
		int cluster = -1;
		int proc = -1;
		jobAd.LookupInteger(ATTR_CLUSTER_ID, cluster);
		jobAd.LookupInteger(ATTR_PROC_ID, proc);

		formatstr(Info.tcp_stats, "File Transfer Download: JobId: %d.%d files: %d bytes: %lld seconds: %.2f dest: %s %s\n",
			cluster, proc, numFiles, (long long)*total_bytes_ptr, (downloadEndTime - downloadStartTime), s->peer_ip_str(), (stats ? stats : ""));
		dprintf(D_STATS, "%s", Info.tcp_stats.c_str());
	}

	return_and_resetpriv( 0 );
}

void
FileTransfer::GetTransferAck(Stream *s,bool &success,bool &try_again,int &hold_code,int &hold_subcode,std::string &error_desc)
{
	if(!PeerDoesTransferAck) {
		success = true;
		return;
	}

	s->decode();

	ClassAd ad;
	if(!getClassAd(s, ad) || !s->end_of_message()) {
		char const *ip = NULL;
		if(s->type() == Sock::reli_sock) {
			ip = ((ReliSock *)s)->get_sinful_peer();
		}
		dprintf(D_FULLDEBUG,"Failed to receive download acknowledgment from %s.\n",
				ip ? ip : "(disconnected socket)");
		success = false;
		try_again = true; // could just be a transient network problem
		return;
	}
	int result = -1;
	if(!ad.LookupInteger(ATTR_RESULT,result)) {
		std::string ad_str;
		sPrintAd(ad_str, ad);
		dprintf(D_ALWAYS,"Download acknowledgment missing attribute: %s.  Full classad: [\n%s]\n",ATTR_RESULT,ad_str.c_str());
		success = false;
		try_again = false;
		hold_code = CONDOR_HOLD_CODE::InvalidTransferAck;
		hold_subcode = 0;
		formatstr(error_desc,"Download acknowledgment missing attribute: %s",ATTR_RESULT);
		return;
	}
	if(result == 0) {
		success = true;
		try_again = false;
	}
	else if(result > 0) {
		success = false;
		try_again = true;
	}
	else {
		success = false;
		try_again = false;
	}

	if(!ad.LookupInteger(ATTR_HOLD_REASON_CODE,hold_code)) {
		hold_code = 0;
	}
	if(!ad.LookupInteger(ATTR_HOLD_REASON_SUBCODE,hold_subcode)) {
		hold_subcode = 0;
	}
	ad.LookupString(ATTR_HOLD_REASON, error_desc);
	// If this is the condor_shadow (indicated by IsServer() == true) then update
	// our file transfer stats to include those sent by the condor_starter
	ClassAd* transfer_stats = dynamic_cast<ClassAd*>(ad.Lookup("TransferStats"));
	if (transfer_stats && IsServer()) {
		Info.stats.Update(*transfer_stats);
	}
}

void
FileTransfer::SaveTransferInfo(bool success,bool try_again,int hold_code,int hold_subcode,char const *hold_reason)
{
	Info.success = success;
	Info.try_again = try_again;
	Info.hold_code = hold_code;
	Info.hold_subcode = hold_subcode;
	if( hold_reason ) {
		Info.error_desc = hold_reason;
	}
}

void
FileTransfer::SendTransferAck(Stream *s,bool success,bool try_again,int hold_code,int hold_subcode,char const *hold_reason)
{
	// Save failure information.
	SaveTransferInfo(success,try_again,hold_code,hold_subcode,hold_reason);

	if(!PeerDoesTransferAck) {
		dprintf(D_FULLDEBUG,"SendTransferAck: skipping transfer ack, because peer does not support it.\n");
		return;
	}

	ClassAd ad;
	int result;
	if(success) {
		result = 0;
	}
	else if(try_again) {
		result = 1;  //failed for transient reasons
	}
	else {
		result = -1; //failed -- do not try again (ie put on hold)
	}

	ad.Assign(ATTR_RESULT,result);
	ad.Insert("TransferStats", new ClassAd(Info.stats));
	if(!success) {
		ad.Assign(ATTR_HOLD_REASON_CODE,hold_code);
		ad.Assign(ATTR_HOLD_REASON_SUBCODE,hold_subcode);
		if(hold_reason) {
				// If we include a newline character in the hold reason, then internal schedd
				// circuit breakers will drop the whole update.
				//
				// HTCondor code shouldn't generate newlines - but potentially file transfer
				// plugins could!
			if (strchr(hold_reason, '\n')) {
				std::string hold_reason_str(hold_reason);
				replace_str(hold_reason_str, "\n", "\\n");
				ad.InsertAttr(ATTR_HOLD_REASON, hold_reason_str);
			} else {
				ad.Assign(ATTR_HOLD_REASON,hold_reason);
			}
		}
	}
	s->encode();
	if(!putClassAd(s, ad) || !s->end_of_message()) {
		char const *ip = NULL;
		if(s->type() == Sock::reli_sock) {
			ip = ((ReliSock *)s)->get_sinful_peer();
		}
		dprintf(D_ALWAYS,"Failed to send download %s to %s.\n",
		        success ? "acknowledgment" : "failure report",
		        ip ? ip : "(disconnected socket)");
	}
}

void
FileTransfer::CommitFiles()
{
	std::string buf;
	std::string newbuf;
	std::string swapbuf;
	const char *file;

	if ( IsClient() ) {
		return;
	}

	int cluster = -1;
	int proc = -1;
	jobAd.LookupInteger(ATTR_CLUSTER_ID, cluster);
	jobAd.LookupInteger(ATTR_PROC_ID, proc);

	priv_state saved_priv = PRIV_UNKNOWN;
	if( want_priv_change ) {
		saved_priv = set_priv( desired_priv_state );
	}

	Directory tmpspool( TmpSpoolSpace.c_str(), desired_priv_state );

	formatstr(buf,"%s%c%s",TmpSpoolSpace.c_str(),DIR_DELIM_CHAR,COMMIT_FILENAME);
	if ( access(buf.c_str(),F_OK) >= 0 ) {
		// the commit file exists, so commit the files.

		std::string SwapSpoolSpace;
		formatstr(SwapSpoolSpace,"%s.swap",SpoolSpace);
		bool swap_dir_ready = SpooledJobFiles::createJobSwapSpoolDirectory(&jobAd,desired_priv_state);
		if( !swap_dir_ready ) {
			EXCEPT("Failed to create %s",SwapSpoolSpace.c_str());
		}

		while ( (file=tmpspool.Next()) ) {
			// don't commit the commit file!
			if ( file_strcmp(file,COMMIT_FILENAME) == MATCH )
				continue;
			formatstr(buf,"%s%c%s",TmpSpoolSpace.c_str(),DIR_DELIM_CHAR,file);
			formatstr(newbuf,"%s%c%s",SpoolSpace,DIR_DELIM_CHAR,file);
			formatstr(swapbuf,"%s%c%s",SwapSpoolSpace.c_str(),DIR_DELIM_CHAR,file);

				// If the target name exists, move it into the swap
				// directory.  This serves two purposes:
				//   1. potentially allow rollback
				//   2. handle case of target being a non-empty directory,
				//      which cannot be overwritten by rename()
			if( access(newbuf.c_str(),F_OK) >= 0 ) {
				if ( rename(newbuf.c_str(),swapbuf.c_str()) < 0 ) {
					EXCEPT("FileTransfer CommitFiles failed to move %s to %s: %s",newbuf.c_str(),swapbuf.c_str(),strerror(errno));
				}
			}

			if ( rotate_file(buf.c_str(),newbuf.c_str()) < 0 ) {
				EXCEPT("FileTransfer CommitFiles Failed -- What Now?!?!");
			}
		}
		// TODO: remove files specified in commit file

		SpooledJobFiles::removeJobSwapSpoolDirectory(&jobAd);
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
		EXCEPT("FileTransfer::Upload called during active transfer!");
	}

	Info.duration = 0;
	Info.type = UploadFilesType;
	Info.success = true;
	Info.in_progress = true;
	Info.xfer_status = XFER_STATUS_UNKNOWN;
	Info.stats.Clear();
	TransferStart = time(NULL);

	if (blocking) {
		int status = DoUpload( &Info.bytes, (ReliSock *)s);
		Info.duration = time(NULL)-TransferStart;
		Info.success = (Info.bytes >= 0) && (status == 0);
		Info.in_progress = false;
		return Info.success;

	} else {

		ASSERT( daemonCore );

		// make a pipe to communicate with our thread
		if (!daemonCore->Create_Pipe(TransferPipe,true)) {
			dprintf(D_ALWAYS, "Create_Pipe failed in "
					"FileTransfer::Upload\n");
			return FALSE;
		}

		if (-1 == daemonCore->Register_Pipe(TransferPipe[0],
											"Upload Results",
											(PipeHandlercpp)&FileTransfer::TransferPipeHandler,
											"TransferPipeHandler",
											this)) {
			dprintf(D_ALWAYS,"FileTransfer::Upload() failed to register pipe.\n");
			return FALSE;
		}
		else {
			registered_xfer_pipe = true;
		}

		upload_info *info = (upload_info *)malloc(sizeof(upload_info));
		ASSERT( info );
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
		dprintf(D_FULLDEBUG,
				"FileTransfer: created upload transfer process with id %d\n",
				ActiveTransferTid);
		// daemonCore will free(info) when the thread exits
		TransThreadTable->insert(ActiveTransferTid, this);

		uploadStartTime = time(NULL);
	}

	return 1;
}


void
FileTransfer::ReceiveAliveMessage() {
	// We've got a alive from cedar get/put File
	// if we are in the forked child, need to pass
	// a message on our pipe up to the parent.
	static time_t lastUpdate = 0;
	time_t now = time(nullptr);

	if ((now - lastUpdate) > 1) {
		UpdateXferStatus(XFER_STATUS_ACTIVE);
		lastUpdate = now;
	}
}

bool
FileTransfer::WriteStatusToTransferPipe(filesize_t total_bytes)
{
	int n;
	bool write_failed = false;

	if(!write_failed) {
		char cmd = FINAL_UPDATE_XFER_PIPE_CMD;

		n = daemonCore->Write_Pipe( TransferPipe[1],
									&cmd,
									sizeof(cmd) );
		if(n != sizeof(cmd)) write_failed = true;
	}

	if(!write_failed) {
		n = daemonCore->Write_Pipe( TransferPipe[1],
				   (char *)&total_bytes,
				   sizeof(filesize_t) );
		if(n != sizeof(filesize_t)) write_failed = true;
	}
	if(!write_failed) {
		n = daemonCore->Write_Pipe( TransferPipe[1],
				   (char *)&Info.try_again,
				   sizeof(bool) );
		if(n != sizeof(bool)) write_failed = true;
	}
	if(!write_failed) {
		n = daemonCore->Write_Pipe( TransferPipe[1],
				   (char *)&Info.hold_code,
				   sizeof(int) );
		if(n != sizeof(int)) write_failed = true;
	}
	if(!write_failed) {
		n = daemonCore->Write_Pipe( TransferPipe[1],
				   (char *)&Info.hold_subcode,
				   sizeof(int) );
		if(n != sizeof(int)) write_failed = true;
	}
	// Classads need to be unparsed to strings to send over the wire
	// First send the length, then send the string itself
	std::string stats;
	classad::ClassAdUnParser unparser;
	unparser.Unparse(stats, &Info.stats);
	int stats_len = stats.length();
	if(!write_failed) {
		n = daemonCore->Write_Pipe( TransferPipe[1],
				   (char *)&stats_len,
				   sizeof(int) );
		if(n != sizeof(int)) write_failed = true;
	}
	if(!write_failed) {
		n = daemonCore->Write_Pipe( TransferPipe[1],
				   stats.c_str(),
				   stats_len );
		if(n != stats_len) write_failed = true;
	}
	int error_len = Info.error_desc.length();
	if(error_len) {
		error_len++; //write the null too
	}
	if(!write_failed) {
		n = daemonCore->Write_Pipe( TransferPipe[1],
				   (char *)&error_len,
				   sizeof(int) );
		if(n != sizeof(int)) write_failed = true;
	}
	if(!write_failed) {
		n = daemonCore->Write_Pipe( TransferPipe[1],
				   Info.error_desc.c_str(),
				   error_len );
		if(n != error_len) write_failed = true;
	}

	int spooled_files_len = Info.spooled_files.length();
	if(spooled_files_len) {
		spooled_files_len++; //write the null too
	}
	if(!write_failed) {
		n = daemonCore->Write_Pipe( TransferPipe[1],
				   (char *)&spooled_files_len,
				   sizeof(int) );
		if(n != sizeof(int)) write_failed = true;
	}
	if(!write_failed) {
		n = daemonCore->Write_Pipe( TransferPipe[1],
				   Info.spooled_files.c_str(),
				   spooled_files_len );
		if(n != spooled_files_len) write_failed = true;
	}

	if(write_failed) {
		dprintf(D_ALWAYS,"Failed to write transfer status to pipe (errno %d): %s\n",errno,strerror(errno));
		return false;
	}

	return true;
}

int
FileTransfer::UploadThread(void *arg, Stream *s)
{
	dprintf(D_FULLDEBUG,"entering FileTransfer::UploadThread\n");
	FileTransfer * myobj = ((upload_info *)arg)->myobj;

	if (s == NULL) {
		return 0;
	}

	filesize_t	total_bytes;
	int status = myobj->DoUpload( &total_bytes, (ReliSock *)s );
	if(!myobj->WriteStatusToTransferPipe(total_bytes)) {
		return 0;
	}
	return ( status >= 0 );
}

/**
 * This function is responsible for invoking a given multifile transfer plugin on a set of
 * files in the execution sandbox AND sending the appropriate response back to the DoDownload
 * side of the file transfer.
 *
 * This can only be called from the DoUpload context; as it will write to the provided ReliSock,
 * many assumptions are made about where it is invoking from inside DoUpload.  For example, it
 * assumes that DoUpload is responsible for the transfer header for the first file.
 *
 * The implementation consists of invoking the `InvokeMultipleFileTransferPlugin` method and
 * parsing the output as appropriate.
 *
 * For each transfer performed by the multi plugin, it will:
 *   - send a transfer header (EOM, INT/TransferCommand::Other, EOM, S/filename, EOM).
 *     Transfer header is skipped for the first file; DoUpload is supposed to do this.
 *   - Send a classad summarizing the transfer result.
 *   - EOM*.
 *
 *  * Depending on the setting of send_trailing_eom, it may skip the EOM for the
 *  very last transfer.
 *
 * - @param pluginPath: The location of the
 * - @returns: -1 on fatal error, 0 for a non-fatal error, and otherwise a fake number
 *   of bytes to use for the transfer summary.
 */
TransferPluginResult
FileTransfer::InvokeMultiUploadPlugin(const std::string &pluginPath, int &exit_code, const std::string &input, ReliSock &sock, bool send_trailing_eom, CondorError &err, long long &upload_bytes)
{
	auto result = InvokeMultipleFileTransferPlugin(err, exit_code, pluginPath, input,
		LocalProxyName.c_str(), true);

	int count = 0;
	bool classad_contents_good = true;
	for (const auto & xfer_result: pluginResultList) {
		std::string filename;
		if (!xfer_result.EvaluateAttrString("TransferFileName", filename)) {
			dprintf(D_FULLDEBUG, "DoUpload: Multi-file plugin at %s did not produce valid response; missing TransferFileName.\n", pluginPath.c_str());
			err.pushf("FILETRANSFER", 1, "Multi-file plugin at %s did not produce valid response; missing TransferFileName", pluginPath.c_str());
			classad_contents_good = false;
		}

		// Caller sends these headers for the first file only; we are responsible
		// for sending them subsequently.
		if (count) {
			// This is the trailing EOM from the last command.
			if( !sock.end_of_message() ) {
				dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
				return TransferPluginResult::Error;
			}

			if( !sock.snd_int(static_cast<int>(TransferCommand::Other), false) ) {
				dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
				return TransferPluginResult::Error;
			}
			if( !sock.end_of_message() ) {
				dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
				return TransferPluginResult::Error;
			}

			if( !sock.put(condor_basename(filename.c_str())) ) {
				dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
				return TransferPluginResult::Error;
			}
			if( !sock.end_of_message() ) {
				dprintf(D_FULLDEBUG, "DoUpload: failed on eom before GoAhead; exiting at %d\n",__LINE__);
				return TransferPluginResult::Error;
			}
		}
			// From here on out, we are mostly converting the outcome of the multifile
			// transfer plugin to the ClassAd format required by the file transfer object.

		count++;
		ClassAd file_info;
		file_info.InsertAttr("ProtocolVersion", 1);
		file_info.InsertAttr("Command", static_cast<int>(TransferCommand::Other));
		file_info.InsertAttr("SubCommand", static_cast<int>(TransferSubCommand::UploadUrl));
		// When transferring checkpoints, if an uplaod URL fails, we still
		// want to preserve the corresponding MANIFEST file so that we can
		// clean up after it, so we need to tell the shadow that's what
		// we're doing.
		if( uploadCheckpointFiles ) {
			file_info.InsertAttr("CheckpointURL", true);
		}

			// Filename is expected to be relative to the sandbox directory; if we don't
			// call condor_basename here, the shadow may see the absolute path to the execute
			// directory and flag it as illegal.
		file_info.InsertAttr("Filename", condor_basename(filename.c_str()));
		std::string output_url;
		if (!xfer_result.EvaluateAttrString("TransferUrl", output_url)) {
			dprintf(D_FULLDEBUG, "DoUpload: Multi-file plugin at %s did not produce valid response; missing TransferUrl.\n", pluginPath.c_str());
			err.pushf("FILETRANSFER", 1, "Multi-file plugin at %s did not produce valid response; missing TransferUrl", pluginPath.c_str());
			classad_contents_good = false;
		}
		file_info.InsertAttr("OutputDestination", output_url);
		bool xfer_success;
		if (!xfer_result.EvaluateAttrBool("TransferSuccess", xfer_success)) {
			dprintf(D_FULLDEBUG, "DoUpload: Multi-file plugin at %s did not produce valid response; missing TransferSuccess.\n", pluginPath.c_str());
			err.pushf("FILETRANSFER", 1, "Multi-file plugin at %s did not produce valid response; missing TransferSuccess", pluginPath.c_str());
			classad_contents_good = false;
		}
		file_info.InsertAttr("Result", xfer_success ? 0 : 1);
		if (!xfer_success) {
			std::string transfer_error;
			if (!xfer_result.EvaluateAttrString("TransferError", transfer_error)) {
				dprintf(D_FULLDEBUG, "DoUpload: Multi-file plugin at %s did not produce valid response; missing TransferError for failed transfer.\n", pluginPath.c_str());
				err.pushf("FILETRANSFER", 1, "Multi-file plugin at %s did not produce valid response; missing TransferError for failed transfer", pluginPath.c_str());
				classad_contents_good = false;
			}
			file_info.InsertAttr("ErrorString", transfer_error);
		}
		if (!putClassAd(&sock, file_info)) {
			dprintf(D_FULLDEBUG, "DoDownload: When sending upload summaries to the remote side, a socket communication failed.\n");
			return TransferPluginResult::Error;
		}
		long long bytes = 0;
		if (xfer_result.EvaluateAttrInt("TransferTotalBytes", bytes)) {
			upload_bytes += bytes;
		}
	}
	if ( send_trailing_eom && !sock.end_of_message() ) {
		dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
		return TransferPluginResult::Error;
	}

	if (!classad_contents_good) {
		return TransferPluginResult::Error;
	}

	return result;
}


bool
FileTransfer::ParseDataManifest()
{
	CondorError &err = m_reuse_info_err; err.clear(); // Aliased; got tired of long var name.
	m_reuse_info.clear();

	std::string tag;
	if (jobAd.EvaluateAttrString(ATTR_USER, tag))
	{
		dprintf(D_FULLDEBUG, "ParseDataManifest: Tag to use for data reuse: %s\n", tag.c_str());
	} else {
		tag = "";
	}

	std::string checksum_info;
	if (!jobAd.EvaluateAttrString("DataReuseManifestSHA256", checksum_info))
	{
		return true;
	}
	std::unique_ptr<FILE, fcloser> manifest(safe_fopen_wrapper_follow(checksum_info.c_str(), "r"));
	if (!manifest.get()) {
		dprintf(D_ALWAYS, "ParseDataManifest: Failed to open SHA256 manifest %s: %s.\n", checksum_info.c_str(), strerror(errno));
		err.pushf("ParseDataManifest", 1, "Failed to open SHA256 manifest %s: %s.", checksum_info.c_str(), strerror(errno));
		return false;
	}
	int lineno = 0;
	for (std::string line; readLine(line, manifest.get(), false);) {
		lineno++;
		if (!line[0] || line[0] == '\n' || line[0] == '#') {
			continue;
		}
		std::vector<std::string> sl = split(line);
		if (sl.size() == 0) {
			dprintf(D_ALWAYS, "ParseDataManifest: Invalid manifest line: %s (line #%d)\n", line.c_str(), lineno);
			err.pushf("ParseDataManifest", 2, "Invalid manifest line: %s (line #%d)", line.c_str(), lineno);
			return false;
		}
		if (sl.size() == 1) {
			dprintf(D_ALWAYS, "ParseDataManifest: Invalid manifest file line (missing name): %s (line #%d)\n", line.c_str(), lineno);
			err.pushf("ParseDataManifest", 3, "Invalid manifest file line (missing name): %s (line #%d)", line.c_str(), lineno);
			return false;
		}
		const char *cksum = sl[0].c_str();
		const char *fname = sl[1].c_str();
			// NOTE: manifest files output from sha256sum don't include a file size;
			// requiring a size here is disappointing because that means the user can't
			// use sha256sum directly.  Can be addressed in two ways:
			//   - stat()'ing the file on the submit sided, then
			//   - falling back to requiring this column (for URLs; we can't stat them).
		long long bytes_long;
		if (sl.size() == 2) {
			if (IsUrl(fname)) {
				dprintf(D_ALWAYS, "ParseDataManifest: Invalid manifest file line (missing size for URL): %s (line #%d)\n", line.c_str(), lineno);
				err.pushf("ParseDataManifest", 4, "Invalid manifest file line (missing size for URL): %s (line #%d)", line.c_str(), lineno);
				return false;
			}
			struct stat statbuf;
			if (-1 == stat(fname, &statbuf)) {
				err.pushf("ParseDataManifest", 5, "Unable to get size of file %s in data manifest: %s (line #%d)", fname, strerror(errno), lineno);
				return false;
			}
			bytes_long = statbuf.st_size;
		} else {
			const char *size_str = sl[2].c_str();
			try {
				bytes_long = std::stoll(size_str);
			} catch (...) {
				err.pushf("ParseDataManifest", 6, "Invalid size in manifest file line: %s (line #%d)", line.c_str(), lineno);
				return false;
			}
		}
		m_reuse_info.emplace_back(fname, cksum, "sha256", tag, bytes_long);
	}
	return true;
}


int
FileTransfer::DoUpload( filesize_t * total_bytes_ptr, ReliSock * s )
{
	// At the beginning of every download and every upload.
	pluginResultList.clear();

	//
	// It would be better if the checkpoint-specific function's body
	// were instead in UploadCheckpointFiles(), but that would involve
	// refactoring UploadFiles() and friends, which I don't want to do
	// right now.
	//

	if( uploadCheckpointFiles ) {
		if(! inHandleCommands ) {
			return DoCheckpointUploadFromStarter( total_bytes_ptr, s );
		} else {
			return DoCheckpointUploadFromShadow( total_bytes_ptr, s );
		}
	} else {
		return DoNormalUpload( total_bytes_ptr, s );
	}
}

int
createCheckpointManifest(
	const FileTransferList & filelist,
	int checkpointNumber,
	FileTransferItem & manifestFTI
) {
	//
	// If we're transferring a checkpoint, create a MANIFEST file from
	// the filelist and add it to the files going to the SPOOL.
	//
	// Compute the manifest list, then its hash, then write the hash and
	// the list to the MANIFEST file.
	//
	// The shadow side disables the socket time-out entirely before accepting
	// commands, so we have all the time we need to compute these checksums.
	//
	std::string manifestText;
	for( auto & fileitem : filelist ) {
		if( fileitem.isDirectory() || fileitem.isDomainSocket() ) { continue; }
		const std::string & sourceName = fileitem.srcName();

		std::string sourceHash;
		if(! compute_file_sha256_checksum( sourceName, sourceHash )) {
			dprintf( D_ALWAYS, "Failed to compute file (%s) checksum when sending checkpoint, aborting.\n", sourceName.c_str() );
			return -1;
		}
		// The '*' marks the hash as having been computed in binary mode.
		formatstr_cat( manifestText, "%s *%s\n",
			sourceHash.c_str(), sourceName.c_str()
		);
	}

	// It would be more efficient to compute the checksum from memory,
	// but writing the file to disk simplifies both this code and means
	// that the MANIFEST file isn't a special case for the actual transfer.
	std::string manifestFileName;
	formatstr( manifestFileName, "_condor_checkpoint_MANIFEST.%.4d", checkpointNumber );
	if(! htcondor::writeShortFile( manifestFileName, manifestText )) {
		dprintf( D_ALWAYS, "Failed to write manifest file when sending checkpoint, aborting.\n" );
		return -1;
	}
	std::string manifestHash;
	if(! compute_file_sha256_checksum( manifestFileName, manifestHash )) {
		dprintf( D_ALWAYS, "Failed to compute manifest (%s) checksum when sending checkpoint, aborting.\n", manifestFileName.c_str() );
		unlink( manifestFileName.c_str() );
		return -1;
	}

	// Duplicating the format of the rest of the file makes it (a) easier
	// to generate with sha256sum and (b) easier for us to parse later.
	std::string append;
	formatstr( append, "%s *%s\n",
		manifestHash.c_str(), manifestFileName.c_str()
	);
	if(! htcondor::appendShortFile( manifestFileName,  append )) {
		dprintf( D_ALWAYS, "Failed to write manifest checksum to manifest (%s) when sending checkpoint, aborting.\n", manifestFileName.c_str() );
		unlink( manifestFileName.c_str() );
		return -1;
	}

	manifestFTI.setSrcName( manifestFileName );
	manifestFTI.setFileMode( (condor_mode_t)0600 );
	manifestFTI.setFileSize( manifestText.length() + append.length() );
	return 0;
}

#define    WITH_OUTPUT_DESTINATION true
#define WITHOUT_OUTPUT_DESTINATION false
#define    WITH_OUTPUT_DESTINATION_IF_FINAL_TRANSFER (m_final_transfer_flag==1)

//
// This not a special case in DoUpload() because that function, even split
// into the two pieces of computeFileList() and updateFileList(), is too
// long, too complicated, and too hard to use or modify.  Rather than make
// it worse, I moved the checkpoint-specific parts out here, into one place.
//
int
FileTransfer::DoCheckpointUploadFromStarter( filesize_t * total_bytes_ptr, ReliSock * s ) {
	FileTransferList filelist = checkpointList;
	std::unordered_set<std::string> skip_files;
	filesize_t sandbox_size = 0;
	_ft_protocol_bits protocolState;
	DCTransferQueue xfer_queue(m_xfer_queue_contact_info);

	std::string checkpointDestination;
	char * originalOutputDestination = OutputDestination;
	if( jobAd.LookupString( "CheckpointDestination", checkpointDestination ) ) {
		OutputDestination = strdup(checkpointDestination.c_str());
		dprintf( D_FULLDEBUG, "Using %s as checkpoint destination\n", OutputDestination );
	}

	int rc = computeFileList(
	    s, filelist, skip_files, sandbox_size, xfer_queue, protocolState,
	    WITH_OUTPUT_DESTINATION
	);

	if( OutputDestination != originalOutputDestination ) {
		free(OutputDestination);
		OutputDestination = originalOutputDestination;
	}

	if( rc != 0 ) { return rc; }


	std::string manifestFileName;
	if(! checkpointDestination.empty()) {
		priv_state saved_priv = PRIV_UNKNOWN;
		if( want_priv_change ) {
			saved_priv = set_priv( desired_priv_state );
		}

		FileTransferItem manifestFTI;
		rc = createCheckpointManifest(
			filelist, this->checkpointNumber, manifestFTI
		);
		if( rc != 0 ) { return rc; }
		manifestFileName = manifestFTI.srcName();
		filelist.emplace_back(manifestFTI);

		// Additonally, the file-transfer protocol asplodes if we have
		// any directory-creation entries which point to URLs.
		for( auto i = filelist.begin(); i != filelist.end(); ) {
			if( i->isDirectory() && (! i->destUrl().empty()) ) {
				i = filelist.erase(i);
			} else {
				++i;
			}
		}

		if( saved_priv != PRIV_UNKNOWN ) {
			set_priv( saved_priv );
		}
	}


	// FIXME: startCheckpointPlugins();

	// dPrintFileTransferList( D_ALWAYS, filelist, "DoCheckpointUploadFromStarter():" );
	rc = uploadFileList(
	    s, filelist, skip_files, sandbox_size, xfer_queue, protocolState,
	    total_bytes_ptr
	);

	// FIXME: stopCheckpointPlugins(rc == 0);

	if(! checkpointDestination.empty()) {
		unlink( manifestFileName.c_str() );
	}
	return rc;
}

int
FileTransfer::DoCheckpointUploadFromShadow( filesize_t * total_bytes_ptr, ReliSock * s ) {
	FileTransferList filelist = inputList;
	std::unordered_set<std::string> skip_files;
	filesize_t sandbox_size = 0;
	_ft_protocol_bits protocolState;
	DCTransferQueue xfer_queue(m_xfer_queue_contact_info);

	filelist.insert( filelist.end(), checkpointList.begin(), checkpointList.end() );
    // dPrintFileTransferList( D_ALWAYS, filelist, "After merging checkpoint list into input list:" );


	//
	// If the user checkpoints their executable, uploadFileList() won't
	// rename it, which will cause problems when resuming after an
	// interruption.  For now, we just won't support self-modifying
	// executables.
	//
	int rc = computeFileList(
	    s, filelist, skip_files, sandbox_size, xfer_queue, protocolState,
		WITHOUT_OUTPUT_DESTINATION
	);
	// dPrintFileTransferList( D_ALWAYS, filelist, "After computeFileList():" );
	if( rc != 0 ) { return rc; }


	return uploadFileList(
	    s, filelist, skip_files, sandbox_size, xfer_queue, protocolState,
	    total_bytes_ptr
	);
}


int
FileTransfer::DoNormalUpload( filesize_t * total_bytes_ptr, ReliSock * s ) {
	FileTransferList filelist;
	std::unordered_set<std::string> skip_files;
	filesize_t sandbox_size = 0;
	_ft_protocol_bits protocolState;
	DCTransferQueue xfer_queue(m_xfer_queue_contact_info);

	if( inHandleCommands ) { filelist = this->inputList; }

	int rc = computeFileList(
	    s, filelist, skip_files, sandbox_size, xfer_queue, protocolState,
	    WITH_OUTPUT_DESTINATION_IF_FINAL_TRANSFER
	);
	if( rc != 0 ) { return rc; }

	return uploadFileList(
	    s, filelist, skip_files, sandbox_size, xfer_queue, protocolState,
	    total_bytes_ptr
	);
}

int
FileTransfer::computeFileList(
    ReliSock * s, FileTransferList & filelist,
    std::unordered_set<std::string> & skip_files,
    filesize_t & sandbox_size,
    DCTransferQueue & xfer_queue,
    _ft_protocol_bits & protocolState,
    bool should_invoke_output_plugins
) {
		// Declaration to make the return_and_reset_priv macro happy.
	std::string reservation_id;

	uploadStartTime = condor_gettimestamp_double();

	dprintf(D_FULLDEBUG,"entering FileTransfer::DoUpload\n");
	dprintf(D_FULLDEBUG,"DoUpload: Output URL plugins %s be run\n",
		should_invoke_output_plugins ? "will" : "will not");

	priv_state saved_priv = PRIV_UNKNOWN;
	if( want_priv_change ) {
		saved_priv = set_priv( desired_priv_state );
	}

	// record the state it was in when we started... the "default" state
	protocolState.socket_default_crypto = s->get_encryption();

	bool preserveRelativePaths = false;
	jobAd.LookupBool( ATTR_PRESERVE_RELATIVE_PATHS, preserveRelativePaths );

	// dPrintFileTransferList( D_ZKM, filelist, ">>> computeFileList(), before ExpandeFileTransferList():" );
	ExpandFileTransferList( FilesToSend, filelist, preserveRelativePaths );
	// dPrintFileTransferList( D_ZKM, filelist, ">>> computeFileList(), after ExpandeFileTransferList():" );

	// Presently, `inHandleCommands` will only be set on the shadow.  The conditional
	// here is abstractly, "if this side is telling the other side which URLs to download";
	// if it is, this side can take care of managing transfer slots without the other
	// side having to do anything.  See the ticket for HTCONDOR-1819.  This logic
	// currently works because this function is only called on the upload side.
	if (inHandleCommands && m_has_protected_url) {
		ExprTree * tree = jobAd.Lookup(ATTR_TRANSFER_Q_URL_IN_LIST);
		if (tree && tree->GetKind() == ClassAd::ExprTree::EXPR_LIST_NODE) {
			classad::ExprList* list = dynamic_cast<classad::ExprList*>(tree);
			for(classad::ExprList::iterator it = list->begin() ; it != list->end(); ++it ) {
				std::string files, attr;
				classad::Value item;
				if (jobAd.EvaluateExpr(*it, item, classad::Value::ALL_VALUES) && item.IsStringValue(files)) {
					if ((*it)->GetKind() == ClassAd::ExprTree::ATTRREF_NODE) {
						classad::ClassAdUnParser unparser;
						unparser.SetOldClassAd( true, true );
						unparser.Unparse(attr, *it);
					} else { /*Fail?*/ }
				} else { /*Fail?*/ }

				if (files.empty()) { continue; }
				auto pos = attr.find_first_of('_');
				if (pos == std::string::npos) { continue; }
				std::string queue = attr.substr(pos+1);
				std::vector<std::string> protectedURLs = split(files, ",");
				// We don't have to worry about order in `filelist` because we're going to sort it later.
				ExpandFileTransferList(&protectedURLs, filelist, preserveRelativePaths, queue.c_str());
			}
		}
		//dPrintFileTransferList( D_ZKM, filelist, ">>> computeFileList(), after adding protected URLs:" );
	}

	// Remove any files from the catalog that are in the ExceptionList
	auto enditer =
		std::remove_if(
				filelist.begin(),
				filelist.end(),
				[&](FileTransferItem &fti)
				{return ExceptionFiles.end() != 
					std::find(ExceptionFiles.begin(),ExceptionFiles.end(),
							decltype(ExceptionFiles)::value_type(condor_basename(fti.srcName().c_str())));});

	filelist.erase(enditer, filelist.end());

		// Calculate the sandbox size as the sum of the known file transfer items
		// (only those that are transferred via CEDAR).
	sandbox_size = std::accumulate(filelist.begin(),
		filelist.end(),
		sandbox_size,
		[](filesize_t partial_sum, FileTransferItem &item) {return partial_sum + item.fileSize();});

	s->encode();

	// tell the server if this is the final transfer or not.
	// if it is the final transfer, the server places the files
	// into the user's Iwd.  if not, the files go into SpoolSpace.
	if( !s->code(m_final_transfer_flag) ) {
		dprintf(D_ERROR,"DoUpload: exiting at %d\n",__LINE__);
		return_and_resetpriv( -1 );
	}
	if( PeerDoesXferInfo ) {
		ClassAd xfer_info;
		xfer_info.Assign(ATTR_SANDBOX_SIZE,sandbox_size);
		if( !putClassAd(s,xfer_info) ) {
			dprintf(D_ERROR,"DoUpload: failed to send xfer_info; exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}
	}
	if( !s->end_of_message() ) {
		dprintf(D_ERROR,"DoUpload: exiting at %d\n",__LINE__);
		return_and_resetpriv( -1 );
	}

	std::string tag;
	if (jobAd.EvaluateAttrString(ATTR_USER, tag))
	{
		dprintf(D_FULLDEBUG, "DoUpload: Tag to use for data reuse: %s\n", tag.c_str());
	} else {
		tag = "";
	}

	// Pre-compute various attributes about the file transfers.
	//
	// Right now, this is limited to calculating output URLs (must be done after prior
	// expansion of the transfer list); in the future, it might be a good place
	// to augment the file transfer items with checksum information.
	bool sign_s3_urls = param_boolean("SIGN_S3_URLS", true) && PeerDoesS3Urls;
		// We must pre-compute the list of URLs we need signed; the downloader-side
		// (typically the shadow...) to try and sign these.
	std::vector<std::string> s3_urls_to_sign;
	for (auto &fileitem : filelist) {
			// Pre-calculate if the uploader will be doing some uploads;
			// if so, we want to determine this now so we can sort correctly.
		if ( should_invoke_output_plugins ) {
			std::string local_output_url;
			if (OutputDestination) {
				local_output_url = OutputDestination;
				if(! ends_with(local_output_url, "/")) {
				    local_output_url += '/';
				}
				if( uploadCheckpointFiles ) {
					std::string globalJobID;
					jobAd.LookupString(ATTR_GLOBAL_JOB_ID, globalJobID);
					ASSERT(! globalJobID.empty());
					std::replace( globalJobID.begin(), globalJobID.end(),
					    '#', '_' );
					formatstr_cat( local_output_url, "%s/%.4d/",
					    globalJobID.c_str(),
					    this->checkpointNumber
					);
				}
				//
				// For whatever reason we don't just write the std{out,err}
				// logs to the filename the user requested in the sandbox,
				// we use the "download filename remaps" (despite the fact
				// that we're doing an upload) to remap them to the right
				// place; the remap is set up by the shadow in
				// initFileTransfer(), and the jic_shadow rewrites them.
				//
				// Although it makes sense to ignore the output remaps if
				// output destination is set (it avoids a lot of ugly
				// semantic questions), we know what the remap for
				// `StdoutRemapName` and `StderrRemapName` should be.
				//
				std::string outputName = fileitem.srcName();
				if(! uploadCheckpointFiles) {
					// This doesn't do anything useful if the user specified
					// an absolute path for their logs.  See HTCONDOR-1221.
					if( outputName == StdoutRemapName ) {
						jobAd.LookupString( ATTR_JOB_ORIGINAL_OUTPUT, outputName );
						local_output_url = outputName;
					} else if( outputName == StderrRemapName ) {
						jobAd.LookupString( ATTR_JOB_ORIGINAL_ERROR, outputName );
						local_output_url = outputName;
					} else {
						local_output_url += outputName;
					}
				} else {
					local_output_url += outputName;
				}
			} else {
				std::string remap_filename;
				if ((1 == filename_remap_find(download_filename_remaps.c_str(), fileitem.srcName().c_str(), remap_filename, 0)) && IsUrl(remap_filename.c_str())) {
					local_output_url = remap_filename.c_str();
				}
			}

			if (sign_s3_urls &&
			  (starts_with_ignore_case(local_output_url, "s3://") || starts_with_ignore_case(local_output_url, "gs://"))) {
				s3_urls_to_sign.push_back(local_output_url);
			}
			fileitem.setDestUrl(local_output_url);
		}
		const std::string &src_url = fileitem.srcName();
		if (sign_s3_urls && fileitem.isSrcUrl() &&
		  (strcasecmp(fileitem.srcScheme().c_str(), "s3") == 0 || strcasecmp(fileitem.srcScheme().c_str(), "gs") == 0)) {
			std::string new_src_url = "https://" + src_url.substr(5);
			dprintf(D_FULLDEBUG, "DoUpload: Will sign %s for remote transfer.\n", src_url.c_str());
			std::string signed_url;
			CondorError err;
			if (htcondor::generate_presigned_url(jobAd, src_url, "GET", signed_url, err)) {
				fileitem.setSrcName(signed_url);
			} else {
				std::string errorMessage;
				formatstr( errorMessage, "DoUpload: Failure when signing URL '%s': %s", src_url.c_str(), err.message() );
				dprintf( D_ALWAYS, "%s\n",errorMessage.c_str() );

				// While (* total_bytes_ptr) and numFiles should both be 0
				// at this point, we should probably be explicit.
				filesize_t logTCPStats = 0;
				UploadExitInfo xfer_info;
				xfer_info.setError(errorMessage, FILETRANSFER_HOLD_CODE::UploadFileError, 3)
				         .doAck(TransferAck::UPLOAD)
				         .line(__LINE__);
				return ExitDoUpload(s, protocolState.socket_default_crypto, saved_priv, xfer_queue, &logTCPStats, xfer_info);
			}
		}
	}

		// If the FTO is in the wrong state - or the peer doesn't support
		// reuse, clear out the built-up information and any relevant errors.
	if (!PeerDoesReuseInfo || m_final_transfer_flag || simple_init) {
		m_reuse_info.clear();
		m_reuse_info_err.clear();
	}


	if (!m_reuse_info.empty())
	{
		dprintf(D_FULLDEBUG, "DoUpload: Sending remote side hints about potential file reuse.\n");

			// Indicate a ClassAd-based command.
		if( !s->snd_int(static_cast<int>(TransferCommand::Other), false) || !s->end_of_message() ) {
			dprintf(D_ERROR,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}
			// Fake an empty filename.
		if (!s->put("") || !s->end_of_message()) {
			dprintf(D_ERROR,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv(-1);
		}

			// Here, we must wait for the go-ahead from the transfer peer.
		if (!ReceiveTransferGoAhead(s, "", false, protocolState.peer_goes_ahead_always, protocolState.peer_max_transfer_bytes)) {
			dprintf(D_ERROR, "DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}
			// Obtain the transfer token from the transfer queue.
		if (!ObtainAndSendTransferGoAhead(xfer_queue, false, s, sandbox_size, "", protocolState.I_go_ahead_always) ) {
			dprintf(D_ERROR, "DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}

		ClassAd file_info;
		auto sub = static_cast<int>(TransferSubCommand::ReuseInfo);
		file_info.InsertAttr("SubCommand", sub);
		file_info.InsertAttr("Tag", tag);
		std::vector<ExprTree*> info_list;
		for (auto &info : m_reuse_info) {
			classad::ClassAd *ad = new classad::ClassAd();
			ad->InsertAttr("FileName", condor_basename(info.filename().c_str()));
			ad->InsertAttr("ChecksumType", info.checksum_type());
			ad->InsertAttr("Checksum", info.checksum());
			ad->InsertAttr("Size", static_cast<long long>(info.size()));
			info_list.push_back(ad);
		}
		file_info.Insert("ReuseList", classad::ExprList::MakeExprList(info_list));
		if (!putClassAd(s, file_info) || !s->end_of_message()) {
			dprintf(D_ERROR,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv(-1);
		}
		ClassAd reuse_ad;
		s->decode();
		if (!getClassAd(s, reuse_ad)) {
			dprintf(D_ERROR,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv(-1);
		}
		if (!s->end_of_message()) {
			dprintf(D_ERROR,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv(-1);
		}
		s->encode();
		classad::Value value;
		classad_shared_ptr<classad::ExprList> exprlist;
		if (reuse_ad.EvaluateAttr("ReuseList", value) && value.IsSListValue(exprlist))
		{
			dprintf(D_FULLDEBUG, "DoUpload: Remote side sent back a list of files that were reused.\n");
			for (auto list_entry : (*exprlist)) {
				classad::Value entry_val;
				std::string fname;
				if (!list_entry->Evaluate(entry_val) || !entry_val.IsStringValue(fname)) {
					continue;
				}
				if (ExecFile && fname == "condor_exec.exe") {
					fname = ExecFile;
				}
				dprintf(D_FULLDEBUG, "DoUpload: File %s was reused.\n", fname.c_str());
				skip_files.insert(fname);
			}
		} else {
			dprintf(D_FULLDEBUG, "DoUpload: Remote side indicated there were no reused files.\n");
		}
	}

	std::unordered_map<std::string, std::string> s3_url_map;
	if (!s3_urls_to_sign.empty()) {
		dprintf(D_FULLDEBUG, "DoUpload: Requesting %zu URLs to sign.\n", s3_urls_to_sign.size());

			// Indicate a ClassAd-based command.
		if (!s->snd_int(static_cast<int>(TransferCommand::Other), false) || !s->end_of_message()) {
			dprintf(D_ERROR,"DoUpload: exiting at %d\n", __LINE__);
			return_and_resetpriv(-1);
		}
			// Fake an empty filename.
		if (!s->put("") || !s->end_of_message()) {
			dprintf(D_ERROR,"DoUpload: exiting at %d\n", __LINE__);
			return_and_resetpriv(-1);
		}

			// Here, we must wait for the go-ahead from the transfer peer.
		if (!ReceiveTransferGoAhead(s, "", false, protocolState.peer_goes_ahead_always, protocolState.peer_max_transfer_bytes)) {
			dprintf(D_ERROR, "DoUpload: exiting at %d\n", __LINE__);
			return_and_resetpriv(-1);
		}
			// Obtain the transfer token from the transfer queue.
		if (!ObtainAndSendTransferGoAhead(xfer_queue, false, s, sandbox_size, "", protocolState.I_go_ahead_always) ) {
			dprintf(D_ERROR, "DoUpload: exiting at %d\n", __LINE__);
			return_and_resetpriv(-1);
		}

		ClassAd file_info;
		auto sub = static_cast<int>(TransferSubCommand::SignUrls);
		file_info.InsertAttr("SubCommand", sub);
		std::vector<ExprTree*> info_list;
		for (auto &info : s3_urls_to_sign) {
			info_list.push_back(classad::Literal::MakeString(info));
		}
		file_info.Insert("SignList", classad::ExprList::MakeExprList(info_list));

		if (!putClassAd(s, file_info) || !s->end_of_message()) {
			dprintf(D_ERROR, "DoUpload: exiting at %d\n", __LINE__);
			return_and_resetpriv(-1);
		}
		ClassAd signed_ad;
		s->decode();
		if (!getClassAd(s, signed_ad) ||
			!s->end_of_message())
		{
			dprintf(D_ERROR, "DoUpload: exiting at %d\n", __LINE__);
			return_and_resetpriv(-1);
		}
		s->encode();

		std::string holdReason;
		if( signed_ad.LookupString( ATTR_HOLD_REASON, holdReason ) ) {
			int holdCode = FILETRANSFER_HOLD_CODE::DownloadFileError;
			signed_ad.LookupInteger( ATTR_HOLD_REASON_CODE, holdCode );

			int holdSubCode = -1;
			signed_ad.LookupInteger( ATTR_HOLD_REASON_SUBCODE, holdSubCode );

			// While (* total_bytes_ptr) and numFiles should both be 0
			// at this point, we should probably be explicit.
			filesize_t logTCPStats = 0;
			UploadExitInfo xfer_info;
			xfer_info.setError(holdReason, holdCode, holdSubCode)
			         .doAck(TransferAck::UPLOAD)
			         .line(__LINE__);
			return ExitDoUpload(s, protocolState.socket_default_crypto, saved_priv, xfer_queue, &logTCPStats, xfer_info);
		}

		classad::Value value;
		classad_shared_ptr<classad::ExprList> exprlist;
		if (signed_ad.EvaluateAttr("SignList", value) && value.IsSListValue(exprlist))
		{
			dprintf(D_FULLDEBUG, "DoUpload: Remote side sent back a list of %d URLs that were signed.\n", exprlist->size());
			size_t idx = 0;
			for (auto list_entry : (*exprlist)) {
				if (idx == s3_urls_to_sign.size()) {
					dprintf(D_FULLDEBUG, "DoUpload: WARNING - remote side sent too few results\n");
					break;
				}
				classad::Value entry_val;
				std::string signed_url;
				if (!list_entry->Evaluate(entry_val) || !entry_val.IsStringValue(signed_url)) {
					idx++;
					dprintf(D_FULLDEBUG, "DoUpload: WARNING - not a valid string entry\n");
					continue;
				}

				if (!signed_url.empty()) {
					s3_url_map.insert({s3_urls_to_sign[idx], signed_url});
				}
				idx++;
			}
		}
	}


	// dPrintFileTransferList( D_ALWAYS, filelist, ">>> computeFileList(), before duplicate removal:" );
	//
	// Remove entries which result in duplicates at the destination.
	// This is no longer necessary for correctness (because I changed
	// FileTransferItem::operator < to group rather than sort), but it's
	// still more efficient.  Some of these duplicates will have been
	// generated internally, but not all.  It's more efficient to remove
	// them all here (especially when the duplicate would only have been
	// generated by expanding a directory).  We remove duplicate directory
	// creation entries when we create the extra entries because for those,
	// we want to preserve the earlier one, rather than the later one.
	//
	std::set<std::string> names;
	// for( auto & i: filelist ) { dprintf( D_ZKM, ">>> DoUpload(), file-item before duplicate removal: %s -> %s\n", i.srcName().c_str(), i.destDir().c_str() ); }
	for( auto iter = filelist.rbegin(); iter != filelist.rend(); ++iter ) {
		auto & item = * iter;
		if( item.isDestUrl() ) { continue; }
		if( item.isDirectory() ) { continue; }

		std::string prefix = item.destDir();
		if(! prefix.empty()) { prefix += DIR_DELIM_CHAR; }
		std::string rd_path = prefix + condor_basename(item.srcName().c_str());

		if( names.insert(rd_path).second == false ) {
			// This incancation converts a reverse to a forward iterator.
			filelist.erase( (iter + 1).base() );
		} else {
			// We rename the file whose _source_ matches ExecFile, so
			// make sure to update ExecFile if the source may have changed
			// (because remote submission spooled the executable).
			if( ExecFile && rd_path == ExecFile ) {
				free( ExecFile );
				ExecFile = strdup(item.srcName().c_str());
			}
		}
	}
	// dPrintFileTransferList( D_ZKM, filelist, ">>> computeFileList(), after duplicate removal:" );

	// dPrintFileTransferList( D_ZKM, filelist, "Before stable sorting:" );
	std::stable_sort(filelist.begin(), filelist.end());
	// dPrintFileTransferList( D_ZKM, filelist, "After stable sorting:" );

	for (auto &fileitem : filelist)
	{
			// If there's a signed URL to work with, we should use that instead.
		auto iter = s3_url_map.find(fileitem.destUrl());
		if (iter != s3_url_map.end()) {
			fileitem.setDestUrl(iter->second);
		}
	}

	return_and_resetpriv(0);
}

int
FileTransfer::uploadFileList(
    ReliSock * s, const FileTransferList & filelist,
    std::unordered_set<std::string> & skip_files,
    const filesize_t & sandbox_size,
    DCTransferQueue & xfer_queue,
    _ft_protocol_bits & protocolState,
    filesize_t * total_bytes_ptr
) {
	int rc = 0;
	std::string fullname;
	filesize_t bytes = 0;

	UploadExitInfo xfer_info;
	bool is_the_executable;
	int numFiles = 0;
	int plugin_exit_code = 0;

	// If a bunch of file transfers failed strictly due to
	// PUT_FILE_OPEN_FAILED, then we keep track of the information relating to
	// the first failed one, and continue to attempt to transfer the rest in
	// the list. At the end of the transfer, the job will go on hold with the
	// information of the first failed transfer. This is to allow things like
	// corefiles and whatnot to be brought back to the spool even if the user
	// job hadn't completed writing all the files as specified in
	// transfer_output_files. These variables represent the saved state of the
	// first failed transfer. See gt #487.
	bool has_failure = false;

	int currentUploadDeferred = 0;

	// Aggregate multiple file uploads; we will upload them all at once
	std::string currentUploadPlugin;
	std::string currentUploadRequests;

	// use an error stack to keep track of failures when invoke plugins,
	// perhaps more of this can be instrumented with it later.
	CondorError errstack;
	std::string error_desc;

	std::string reservation_id;
	priv_state saved_priv = PRIV_UNKNOWN;
	if( want_priv_change ) {
		saved_priv = set_priv( desired_priv_state );
	}

	*total_bytes_ptr = 0;
	for (auto &fileitem : filelist)
	{
		auto &filename = fileitem.srcName();
		auto &dest_dir = fileitem.destDir();
			// Anything the remote side was able to reuse we do not send again.
		if (skip_files.find(filename) != skip_files.end()) {
			dprintf(D_FULLDEBUG, "Skipping file %s as it was reused.\n", filename.c_str());
			continue;
		}

		if( !dest_dir.empty() ) {
			dprintf(D_FULLDEBUG,"DoUpload: sending file %s to %s%c\n", filename.c_str(), dest_dir.c_str(), DIR_DELIM_CHAR);
		}
		else {
			dprintf(D_FULLDEBUG,"DoUpload: sending file %s\n", UrlSafePrint(filename));
		}

		if( fileitem.isSrcUrl() ) {
			if( param_boolean("ENABLE_URL_TRANSFERS", true) ) {
				// looks like a URL
				fullname = filename;
				dprintf(D_FULLDEBUG, "DoUpload: sending %s as URL.\n", UrlSafePrint(filename));
			} else {
				// A URL was requested but the sysadmin has disabled URL transfers; this
				// should have been prevented by matchmaking, so we fail this instead of
				// treating the URL as a filename.
				dprintf(D_ALWAYS, "DoUpload: WARNING - URL transfers were disabled by the sysadmin, "
					"but this transfer requires URL transfers to function; failing");
				return_and_resetpriv( -1 );
			}
		} else if( !fullpath( filename.c_str() ) ){
			// looks like a relative path
			formatstr(fullname,"%s%c%s",Iwd,DIR_DELIM_CHAR,filename.c_str());
		} else {
			// looks like an unix absolute path or a windows path
			fullname = filename;
		}

		std::string dest_filename;
		if ( ExecFile && !simple_init && (file_strcmp(ExecFile,filename.c_str())==0 )) {
			// this file is the job executable
			is_the_executable = true;
			if (PeerRenamesExecutable) {
				// Our peer is old and expects us to rename the executable
				dest_filename = CONDOR_EXEC;
			} else {
				// Strip path information and preserve the original name
				// (in case copy_to_spool gave us an ickpt filename).
				std::string cmd;
				jobAd.LookupString(ATTR_JOB_CMD, cmd);
				dest_filename = condor_basename(cmd.c_str());
			}
		} else {
			// this file is _not_ the job executable
			is_the_executable = false;

			if( !dest_dir.empty() ) {
				formatstr(dest_filename,"%s%c",dest_dir.c_str(),DIR_DELIM_CHAR);
			}

			//
			// If we're sending files from the sandbox to a URL, this code
			// does nothing.  (The remote side ignores the destination
			// filename.)  If we're commanding the sandbox to download a
			// file from a URL, we need to use the basename because we
			// don't know where in the URL the relative path starts (and
			// we don't want to write the URL to a file with the whole
			// URL as its name).
			//
			if( IsUrl( filename.c_str() ) ) {
				// If we signed the URL, we added a bunch of garbage to the
				// query string.  Strip it out to get better base name.
				auto idx = filename.find("?");
				std::string tmp_filename = filename.substr(0, idx);
				formatstr_cat(dest_filename, "%s", condor_basename(tmp_filename.c_str()));
			} else {
				// Strip any path information; if we wanted to keep it, it
				// would have been set in dest_dir.
				formatstr_cat(dest_filename, "%s", condor_basename(filename.c_str()));
			}

			dprintf(D_FULLDEBUG, "DoUpload: will transfer to filename %s.\n", dest_filename.c_str() );
		}

		// check for read permission on this file, if we are supposed to check.
		// do not check the executable, since it is likely sitting in the SPOOL
		// directory.
		//
		// also, don't check URLs
#ifdef WIN32
		if( !fileitem.isSrcUrl() && perm_obj && !is_the_executable &&
			(perm_obj->read_access(fullname.c_str()) != 1) ) {
			// we do _not_ have permission to read this file!!
			formatstr(error_desc,"error reading from %s: permission denied",fullname.c_str());
			xfer_info.setError(error_desc, FILETRANSFER_HOLD_CODE::UploadFileError, EPERM)
			         .doAck(TransferAck::BOTH)
			         .files(numFiles)
			         .line(__LINE__);
			return ExitDoUpload(s, protocolState.socket_default_crypto, saved_priv, xfer_queue, total_bytes_ptr, xfer_info);
		}
#else
		if (is_the_executable) {} // Done to get rid of the compiler set-but-not-used warnings.
#endif


		// The number 999 means we will still send the filename, and then send a
		// classad immediately following the filename, and the classad will say
		// what action to perform.  this will allow potential changes without
		// breaking the wire protocol and hopefully will be more forward and
		// backward compatible for future updates.
		//

		// default to the socket default
		TransferCommand file_command = TransferCommand::XferFile;
		TransferSubCommand file_subcommand = TransferSubCommand::Unknown;

		// find out if this file is in DontEncryptFiles
		if ( DontEncryptFiles && file_contains_withwildcard(*DontEncryptFiles, filename) ) {
			// turn crypto off for this file (actually done below)
			file_command = TransferCommand::DisableEncryption;
		}

		// now find out if this file is in EncryptFiles.  if it was
		// also in DontEncryptFiles, that doesn't matter, this will
		// override.
		if ( EncryptFiles && file_contains_withwildcard(*EncryptFiles, filename) ) {
			// turn crypto on for this file (actually done below)
			file_command = TransferCommand::EnableEncryption;
		}

		// We want to delegate the job's x509 proxy, rather than just
		// copy it.
		if ( X509UserProxy && file_strcmp( filename.c_str(), X509UserProxy ) == 0 &&
			 DelegateX509Credentials ) {

			file_command = TransferCommand::XferX509;
		}

		if ( fileitem.isSrcUrl() ) {
			file_command = TransferCommand::DownloadUrl;
		}

		std::string multifilePluginPath;
		if ( fileitem.isDestUrl() ) {
			dprintf(D_FULLDEBUG, "FILETRANSFER: Using command 999:7 for output URL destination: %s\n",
					UrlSafePrint(fileitem.destUrl()));

			// switch from whatever command we had before to new classad
			// command new classad command 999 and subcommand 7.
			//
			// 7 == invoke plugin to store file
			file_command = TransferCommand::Other;
			file_subcommand = TransferSubCommand::UploadUrl;

			if (multifile_plugins_enabled) {
				std::string pluginPath = DetermineFileTransferPlugin( errstack, fileitem.destUrl().c_str(), fullname.c_str() );
				if ( (plugins_multifile_support.find( pluginPath ) != plugins_multifile_support.end()) && \
				plugins_multifile_support[pluginPath] ) {
					multifilePluginPath = pluginPath;
				}
			}

			// If this isn't set, then we could be uploading via a plug-in
			// specified by the job, which always uses the multi-file protocol.
			if (! multifilePluginPath.empty()) {
				dprintf(D_FULLDEBUG, "Will upload output URL using multi-file plugin.\n");
			}
		}

		// Flush out any transfers if we can no longer defer the prior work we had built up.
		// We can't defer if the plugin name changed *or* we hit a transfer that doesn't
		// require a plugin at all.
		long long upload_bytes = 0;
		if (!currentUploadPlugin.empty() && (multifilePluginPath != currentUploadPlugin)) {
			dprintf (D_FULLDEBUG, "DoUpload: Can't defer any longer; executing multifile plugin for multiple transfers.\n");
			int exit_code = 0;
			TransferPluginResult result = InvokeMultiUploadPlugin(currentUploadPlugin, exit_code, currentUploadRequests, *s, true, errstack, upload_bytes);
			if (result != TransferPluginResult::Success) {
				formatstr_cat(error_desc, ": %s", errstack.getFullText().c_str());
				if (!has_failure) {
					xfer_info.setError(error_desc,
					            FILETRANSFER_HOLD_CODE::UploadFileError,
					            exit_code << 8
					).line(__LINE__);
				}
			}
			currentUploadPlugin = "";
			currentUploadRequests = "";
			currentUploadDeferred = 0;
		}

		bool fail_because_mkdir_not_supported = false;
		bool fail_because_symlink_not_supported = false;
		if( fileitem.isDirectory() && ! fileitem.isDestUrl() ) {
			if( fileitem.isSymlink() ) {
				fail_because_symlink_not_supported = true;
				dprintf(D_ALWAYS,"DoUpload: attempting to transfer symlink %s which points to a directory.  This is not supported.\n", filename.c_str());
			}
			else if( PeerUnderstandsMkdir ) {
				file_command = TransferCommand::Mkdir;
			}
			else {
				fail_because_mkdir_not_supported = true;
				dprintf(D_ALWAYS,"DoUpload: attempting to transfer directory %s, but the version of Condor we are talking to is too old to support that!\n",
						filename.c_str());
			}
		}

		dprintf ( D_FULLDEBUG, "FILETRANSFER: outgoing file_command is %i for %s\n",
			static_cast<int>(file_command), UrlSafePrint(filename) );

			// Frustratingly, we cannot skip the header of the first transfer command
			// if we are defering uploads as we may have to acquire a transfer token below.
			// The protocol also requires us to acquire a transfer token AFTER the filename
			// is sent; hence, we cannot simply reorder the logic.
			//
			// Because we send the header now, `InvokeMultiUploadPlugin` does not for the first
			// transfer command.
		bool no_defer_header = multifilePluginPath.empty() || !currentUploadDeferred;
		if (no_defer_header) {
			if( !s->snd_int(static_cast<int>(file_command), false) ) {
				dprintf(D_ERROR,"DoUpload: exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}
			if( !s->end_of_message() ) {
				dprintf(D_ERROR,"DoUpload: exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}
		}

		// now enable the crypto decision we made; if we are sending a URL down the pipe
		// (potentially embedding an authorization itself), ensure we encrypt.
		if (file_command == TransferCommand::EnableEncryption || (PeerDoesS3Urls && (file_command == TransferCommand::DownloadUrl))) {
			bool cryp_ret = s->set_crypto_mode(true);
			if (!cryp_ret) {
				dprintf(D_ERROR,"DoUpload: failed to enable crypto on outgoing file, exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}

		} else if (file_command == TransferCommand::DisableEncryption) {
			s->set_crypto_mode(false);
		}
		else {
			bool cryp_ret = s->set_crypto_mode(protocolState.socket_default_crypto);
			if (!cryp_ret) {
				dprintf(D_ERROR,"DoUpload: failed to set default crypto on outgoing file, exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}
		}

		// for command 999, this string must equal the Attribute "Filename" in
		// the classad that follows.  it wouldn't really need to be sent here
		// but is more wire-compatible with older versions if we do.
		//
		// should we send a protocol version string instead?  or some other token
		// like 'CLASSAD'?
		//
		if( no_defer_header && !s->put(dest_filename.c_str()) ) {
			dprintf(D_ERROR,"DoUpload: exiting at %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}

		if( PeerDoesGoAhead ) {
			if( no_defer_header && !s->end_of_message() ) {
				dprintf(D_ERROR, "DoUpload: failed on eom before GoAhead; exiting at %d\n",__LINE__);
				return_and_resetpriv( -1 );
			}

			if( !protocolState.peer_goes_ahead_always ) {
					// Now wait for our peer to tell us it is ok for us to
					// go ahead and send data.
				if( !ReceiveTransferGoAhead(s,fullname.c_str(),false,protocolState.peer_goes_ahead_always,protocolState.peer_max_transfer_bytes) ) {
					dprintf(D_ERROR, "DoUpload: exiting at %d\n",__LINE__);
					return_and_resetpriv( -1 );
				}
			}

			if( !protocolState.I_go_ahead_always ) {
					// Now tell our peer when it is ok for us to read data
					// from disk for sending.
				if( !ObtainAndSendTransferGoAhead(xfer_queue,false,s,sandbox_size,fullname.c_str(),protocolState.I_go_ahead_always) ) {
					dprintf(D_ERROR, "DoUpload: exiting at %d\n",__LINE__);
					return_and_resetpriv( -1 );
				}
			}

			s->encode();
		}
		// Multifile uploads imply we execute multiple commands at once; although we can lie to the other side,
		// maintenance of the state becomes quite complex.  Hence, we defer uploads only when the protocol
		// is completely asynchronous.
		//
		// NOTE: if we ever want to reacquire the token (or acquire an alternate token for non-CEDAR transfers),
		// then this would provide a natural synchronization point.
		bool can_defer_uploads = !PeerDoesGoAhead || (protocolState.peer_goes_ahead_always && protocolState.I_go_ahead_always);

		UpdateXferStatus(XFER_STATUS_ACTIVE);

		filesize_t this_file_max_bytes = -1;
		filesize_t effective_max_upload_bytes = MaxUploadBytes;
		bool using_peer_max_transfer_bytes = false;
		if( protocolState.peer_max_transfer_bytes >= 0 && (protocolState.peer_max_transfer_bytes < effective_max_upload_bytes || effective_max_upload_bytes < 0) ) {
				// For superior error handling, it is best for the
				// uploading side to know about the downloading side's
				// max transfer byte limit.  This prevents the
				// uploading side from trying to send more than the
				// maximum, which would cause the downloading side to
				// close the connection, which would cause the
				// uploading side to assume there was a communication
				// error rather than an intentional stop.
			effective_max_upload_bytes = protocolState.peer_max_transfer_bytes;
			using_peer_max_transfer_bytes = true;
			dprintf(D_FULLDEBUG,"DoUpload: changing maximum upload MB from %ld to %ld at request of peer.\n",
					(long int)(effective_max_upload_bytes >= 0 ? effective_max_upload_bytes/1024/1024 : effective_max_upload_bytes),
					(long int)(protocolState.peer_max_transfer_bytes/1024/1024));
		}
		if( effective_max_upload_bytes < 0 ) {
			this_file_max_bytes = -1; // no limit
		}
		else if( effective_max_upload_bytes >= *total_bytes_ptr ) {
			this_file_max_bytes = effective_max_upload_bytes - *total_bytes_ptr;
		}
		else {
			this_file_max_bytes = 0;
		}

		if ( file_command == TransferCommand::Other) {
			// new-style, send classad

			ClassAd file_info;
			file_info.Assign("ProtocolVersion", 1);
			file_info.Assign("Command", static_cast<int>(file_command));
			file_info.Assign("SubCommand", static_cast<int>(file_subcommand));

			// only one subcommand at the moment: 7
			//
			// 7 is "Report to shadow the final status of invoking a transfer
			// hook to move the output file"

			if(file_subcommand == TransferSubCommand::UploadUrl) {
				// make the URL out of Attr OutputDestination and filename
				std::string source_filename;
				source_filename = Iwd;
				source_filename += DIR_DELIM_CHAR;
				source_filename += filename;

				const std::string &local_output_url = fileitem.destUrl();
				// Potentially execute the multifile plugin.  Note all the error handling
				// occurs outside this gigantic if block - we must carefully set `rc` for it
				// to work correctly.
				if (!multifilePluginPath.empty()) {
					currentUploadPlugin = multifilePluginPath;

					classad::ClassAdUnParser unparser;
					ClassAd xfer_ad;
					xfer_ad.InsertAttr( "Url", local_output_url );
					xfer_ad.InsertAttr( "LocalFileName", fullname );
					std::string xfer_str;
					unparser.Unparse( xfer_str, &xfer_ad );

					currentUploadRequests += xfer_str;
					currentUploadDeferred ++;

					// If we cannot defer uploads, we must execute the plugin now -- with one file.
					if (!can_defer_uploads) {
						dprintf (D_FULLDEBUG, "DoUpload: Can't defer uploads; executing multifile plugin for multiple transfers.\n");
						long long upload_bytes = 0;
						int exit_code = 0;
						TransferPluginResult result = InvokeMultiUploadPlugin(currentUploadPlugin, exit_code, currentUploadRequests, *s, false, errstack, upload_bytes);
						if( result == TransferPluginResult::Success ) {
							currentUploadPlugin = "";
							currentUploadRequests = "";
							currentUploadDeferred = 0;
							rc = 0;
						} else {
							// We haven't used `exit_code` yet, but we need
							// it to compute the hold reason subcode.  It's
							// not clear if this early exit is required for
							// wire-protocol compability or if the original
							// implementation just didn't want to deal with
							// this hopefully rare case.
							dprintf( D_ALWAYS, "InvokeMultiUploadPlugin() failed (%d); plugin exit code was %d.\n", (int)result, exit_code );
							return_and_resetpriv( -1 );
						}
					} else {
						rc = 0;
					}
				} else {
					// actually invoke the plugin.  this could block indefinitely.
					ClassAd pluginStatsAd;
					dprintf (D_FULLDEBUG, "DoUpload: calling IFTP(fn,U): fn\"%s\", U\"%s\"\n", UrlSafePrint(source_filename), UrlSafePrint(local_output_url));
					dprintf (D_FULLDEBUG, "LocalProxyName: %s\n", LocalProxyName.c_str());
					int exit_code = 0;
					TransferPluginResult result = InvokeFileTransferPlugin(errstack, exit_code, source_filename.c_str(), local_output_url.c_str(), &pluginStatsAd, LocalProxyName.c_str());
					dprintf (D_FULLDEBUG, "DoUpload: IFTP(fn,U): fn\"%s\", U\"%s\" returns %i\n", UrlSafePrint(source_filename), UrlSafePrint(local_output_url), rc);

					// report the results:
					file_info.Assign("Filename", source_filename);
					file_info.Assign("OutputDestination", local_output_url);

					// If failed, put the ErrStack into the classad
					if (result != TransferPluginResult::Success) {
						file_info.Assign("ErrorString", errstack.getFullText());
						plugin_exit_code = exit_code;
						rc = GET_FILE_PLUGIN_FAILED;
					} else {
						plugin_exit_code = 0;
						rc = 0;
					}
					file_info.Assign( "Result", rc );

					// it's all assembled, so send the ad using stream s.
					// don't end the message, it's done below.
					// Always encrypt the URL as it might contain an authorization.
					const classad::References encrypted_attrs{"OutputDestination"};
					if(!putClassAd(s, file_info, 0, NULL, &encrypted_attrs)) {
						dprintf(D_ERROR,"DoDownload: exiting at %d\n",__LINE__);
						return_and_resetpriv( -1 );
					}

					//
					// This comment used to read 'compute the size of what we sent',
					// but obviously the wire format and the string format of
					// ClassAds are not the same and can't be expected to be the
					// same length.  Since the size will be wrong anyway, simplify
					// future security audits but not printing the private attrs.
					//
					std::string junkbuf;
					sPrintAd(junkbuf, file_info);
					bytes = junkbuf.length();
				}
			} else {
				dprintf( D_ALWAYS, "DoUpload: invalid subcommand %i, skipping %s.",
					static_cast<int>(file_subcommand), UrlSafePrint(filename));
				bytes = 0;
				rc = 0;
			}
		} else if ( file_command == TransferCommand::XferX509 ) {
			if ( (PeerDoesGoAhead || s->end_of_message()) ) {
				time_t expiration_time = GetDesiredDelegatedJobCredentialExpiration(&jobAd);
				rc = s->put_x509_delegation( &bytes, fullname.c_str(), expiration_time, NULL );
				dprintf( D_FULLDEBUG,
				         "DoUpload: put_x509_delegation() returned %d\n",
				         rc );
			} else {
				rc = -1;
			}
		} else if (file_command == TransferCommand::DownloadUrl) {
			// send the URL and that's it for now.
			// TODO: this should probably be a classad
			if(!s->code(fullname)) {
				dprintf( D_FULLDEBUG, "DoUpload: failed to send fullname: %s\n", UrlSafePrint(fullname));
				rc = -1;
			} else {
				dprintf( D_FULLDEBUG, "DoUpload: sent fullname and NO eom: %s\n", UrlSafePrint(fullname));
				rc = 0;
			}

			// on the sending side, we don't know how many bytes the actual
			// file was, since we aren't the ones downloading it.  to find out
			// the length, we'd have to make a connection to some server (via a
			// plugin, for which no API currently exists) and ask it, and i
			// don't want to add that latency.
			//
			// instead we add the length of the URL itself, since that's what
			// we sent.
			bytes = fullname.length();

		} else if( file_command == TransferCommand::Mkdir ) { // mkdir
			// the only data sent is the file_mode.
			bytes = sizeof( fileitem.fileMode() );

			if( !s->put( fileitem.fileMode() ) ) {
				rc = -1;
				dprintf(D_ALWAYS,"DoUpload: failed to send mkdir mode\n");
			}
			else {
				rc = 0;
			}
		} else if( fail_because_mkdir_not_supported || fail_because_symlink_not_supported ) {
			if( TransferFilePermissions ) {
				rc = s->put_file_with_permissions( &bytes, NULL_FILE );
			}
			else {
				rc = s->put_file( &bytes, NULL_FILE );
			}
			if( rc == 0 ) {
				rc = PUT_FILE_OPEN_FAILED;
				errno = EISDIR;
			}
		} else if ( TransferFilePermissions ) {
			rc = s->put_file_with_permissions( &bytes, fullname.c_str(), this_file_max_bytes, &xfer_queue );
		} else {
			rc = s->put_file( &bytes, fullname.c_str(), 0, this_file_max_bytes, &xfer_queue );
		}
		if( rc < 0 ) {
			int hold_code = FILETRANSFER_HOLD_CODE::UploadFileError;
			int hold_subcode = errno;
			formatstr(error_desc,"|Error: sending file %s",UrlSafePrint(fullname));
			if((rc == PUT_FILE_OPEN_FAILED) || (rc == PUT_FILE_PLUGIN_FAILED) || (rc == PUT_FILE_MAX_BYTES_EXCEEDED)) {

				// If plugin_exit_code is greater than 0, that indicates a
				// transfer plugin error. In this case set hold_subcode to the
				// plugin exit code left-shifted by 8, so we can differentiate
				// between plugin failures and regular cedar failures.

				if (plugin_exit_code > 0) {
					hold_subcode = plugin_exit_code << 8;
				}

				if (rc == PUT_FILE_OPEN_FAILED) {
					// In this case, put_file() has transmitted a zero-byte
					// file in place of the failed one. This means there is an
					// ack waiting for us to read back which we do at the end of
					// the while loop.

					replace_str(error_desc,"sending","reading from");
					formatstr_cat(error_desc,": (errno %d) %s",hold_subcode,strerror(hold_subcode));
					if( fail_because_mkdir_not_supported ) {
						formatstr_cat(error_desc, "- Remote condor version is too old to transfer directories.");
					}
					if( fail_because_symlink_not_supported ) {
						formatstr_cat(error_desc, "- Transfer of symlinks to directories is not supported.");
					}
				} else if ( rc == PUT_FILE_MAX_BYTES_EXCEEDED ) {
					StatInfo this_file_stat(fullname.c_str());
					filesize_t this_file_size = this_file_stat.GetFileSize();
					formatstr_cat(error_desc, ": max total %s bytes exceeded (max=%ld MB, this file=%ld MB)",
											 using_peer_max_transfer_bytes ? "download" : "upload",
											 (long int)(effective_max_upload_bytes/1024/1024),
											 (long int)(this_file_size/1024/1024));
					hold_code = using_peer_max_transfer_bytes ? CONDOR_HOLD_CODE::MaxTransferOutputSizeExceeded : CONDOR_HOLD_CODE::MaxTransferInputSizeExceeded;
					hold_subcode = 0;
				} else {
					// add on the error string from the errstack used
					formatstr_cat(error_desc, ": %s", errstack.getFullText().c_str());
				}

				// We'll continue trying to transfer the rest of the files
				// in question, but we'll record the information we need from
				// the first failure. Notice that this means we won't know
				// the complete set of files which failed to transfer back
				// but have become zero length files on the submit side.
				// We'd need to append those failed files to some kind of an
				// attribute in the job ad representing this failure. That
				// is not currently implemented....

				if (!has_failure) {
					has_failure = true;
					xfer_info.setError(error_desc, hold_code, hold_subcode)
					         .line(__LINE__);
				}
			}
			else {
				// We can't currently tell the different between other
				// put_file() errors that will generate an ack error
				// report, and those that are due to a genuine
				// disconnect between us and the receiver.  Therefore,
				// always try reading the download ack.
				// The stream _from_ us to the receiver is in an undefined
				// state.  Some network operation may have failed part
				// way through the transmission, so we cannot expect
				// the other side to be able to read our upload ack.
				xfer_info.setError(error_desc, hold_code, hold_subcode)
				         .doAck(TransferAck::DOWNLOAD)
				         .retry()
				         .files(numFiles)
				         .line(__LINE__);
				// for the more interesting reasons why the transfer failed,
				// we can try again and see what happens.
				return ExitDoUpload(s, protocolState.socket_default_crypto, saved_priv, xfer_queue, total_bytes_ptr, xfer_info);

			}
		}

		if( !currentUploadDeferred && !s->end_of_message() ) {
			dprintf(D_ERROR,"DoUpload: socket communication failure; exiting at line %d\n",__LINE__);
			return_and_resetpriv( -1 );
		}

		*total_bytes_ptr += bytes;
		numFiles++;

		if (!fileitem.isSrcUrl() && !fileitem.isDestUrl()) {
			int num_cedar_files = 0;
			Info.stats.LookupInteger("CedarFilesCount", num_cedar_files);
			num_cedar_files++;
			Info.stats.InsertAttr("CedarFilesCount", num_cedar_files);
		}

			// The spooled files list is used to generate
			// SpooledOutputFiles, which replaces TransferOutputFiles
			// when pulling output from the spool directory.  We don't
			// really know here whether the other side is writing to the
			// spool or not, but we generate the list just in case.
			//
			// If we transfer directories, only include the directory
			// in the spooled files list, not its contents.  Also skip
			// the stdout/stderr files, since those are handled
			// separately when building the list of files to transfer.

		if( dest_filename.find(DIR_DELIM_CHAR) == std::string::npos &&
			dest_filename != condor_basename(JobStdoutFile.c_str()) &&
			dest_filename != condor_basename(JobStderrFile.c_str()) &&
			(file_command != TransferCommand::Other || file_subcommand != TransferSubCommand::UploadUrl) )
		{
			Info.addSpooledFile( dest_filename.c_str() );
		}
	}
	// Release transfer queue slot if we haven't sent a protected URL for
	// the remote side to download. Currently the remote side (likely starter)
	// collects all passed URLs for download and then downloads post main loop
	// We don't need the transfer queue slot if there is no URL using protected
	// resources.
	if (!m_has_protected_url) { xfer_queue.ReleaseTransferQueueSlot(); }

	// Clear out the multi-upload queue; we must do the error handling locally if it fails.
	long long upload_bytes = 0;
	if (!currentUploadRequests.empty()) {
		dprintf (D_FULLDEBUG, "DoUpload: Executing deferred multifile plugin for multiple transfers.\n");
		int exit_code = 0;
		TransferPluginResult result = InvokeMultiUploadPlugin(currentUploadPlugin, exit_code, currentUploadRequests, *s, true, errstack, upload_bytes);
		if (result != TransferPluginResult::Success) {
			formatstr_cat(error_desc, ": %s", errstack.getFullText().c_str());
			if (!has_failure) {
				has_failure = true;
				xfer_info.setError(error_desc,
				                   FILETRANSFER_HOLD_CODE::UploadFileError,
				                   exit_code << 8
				).line(__LINE__);
			}
		}
		*total_bytes_ptr += upload_bytes;
	}

	// If we had an error when parsing the data manifest, it occurred far too early for us to
	// send a reasonable error back to the queue.  Hence, we delay looking at the error object until now.
	if (!m_reuse_info_err.empty()) {
		formatstr_cat(error_desc, ": %s", m_reuse_info_err.getFullText().c_str());
		if (!has_failure) {
			has_failure = true;
			xfer_info.setError(error_desc, FILETRANSFER_HOLD_CODE::UploadFileError, 2)
			         .line(__LINE__);
		}
	}

	if (!has_failure) { xfer_info.noError().success(); }
	xfer_info.doAck(TransferAck::BOTH)
	         .files(numFiles);

	uploadEndTime = condor_gettimestamp_double();

	return ExitDoUpload(s, protocolState.socket_default_crypto, saved_priv, xfer_queue, total_bytes_ptr, xfer_info);
}

void
FileTransfer::setTransferQueueContactInfo(char const *contact) {
	m_xfer_queue_contact_info = TransferQueueContactInfo(contact);
}

bool
FileTransfer::ObtainAndSendTransferGoAhead(DCTransferQueue &xfer_queue,bool downloading,Stream *s,filesize_t sandbox_size,char const *full_fname,bool &go_ahead_always)
{
	bool result;
	bool try_again = true;
	int hold_code = 0;
	int hold_subcode = 0;
	std::string error_desc;

	result = DoObtainAndSendTransferGoAhead(xfer_queue,downloading,s,sandbox_size,full_fname,go_ahead_always,try_again,hold_code,hold_subcode,error_desc);

	if( !result ) {
		SaveTransferInfo(false,try_again,hold_code,hold_subcode,error_desc.c_str());
		if( error_desc.length() ) {
			dprintf(D_ALWAYS,"%s\n",error_desc.c_str());
		}
	}
	return result;
}

std::string
FileTransfer::GetTransferQueueUser()
{
	std::string user;
	ClassAd *job = GetJobAd();
	if( job ) {
		std::string user_expr;
		if( param(user_expr,"TRANSFER_QUEUE_USER_EXPR","strcat(\"Owner_\",Owner)") ) {
			ExprTree *user_tree = NULL;
			if( ParseClassAdRvalExpr( user_expr.c_str(), user_tree ) == 0 && user_tree ) {
				classad::Value val;
				const char *str = NULL;
				if ( EvalExprToString(user_tree,job,NULL,val) && val.IsStringValue(str) )
				{
					user = str;
				}
				delete user_tree;
			}
		}
	}
	return user;
}

bool
FileTransfer::DoObtainAndSendTransferGoAhead(DCTransferQueue &xfer_queue,bool downloading,Stream *s,filesize_t sandbox_size,char const *full_fname,bool &go_ahead_always,bool &try_again,int &hold_code,int &hold_subcode,std::string &error_desc)
{
	ClassAd msg;
	int go_ahead = GO_AHEAD_UNDEFINED;
	int alive_interval = 0;
	time_t last_alive = time(NULL);
		//extra time to reserve for sending msg to our file xfer peer
	const int alive_slop = 20;
	int min_timeout = 300;

	std::string queue_user = GetTransferQueueUser();

	s->decode();
	if( !s->get(alive_interval) || !s->end_of_message() ) {
		formatstr(error_desc, "ObtainAndSendTransferGoAhead: failed on alive_interval before GoAhead");
		return false;
	}

	if( Sock::get_timeout_multiplier() > 0 ) {
		min_timeout *= Sock::get_timeout_multiplier();
	}

	int timeout = alive_interval;
	if( timeout < min_timeout ) {
		timeout = min_timeout;

			// tell peer the new timeout
		msg.Assign(ATTR_TIMEOUT,timeout);
			// GO_AHEAD_UNDEFINED just means that our peer should keep waiting
		msg.Assign(ATTR_RESULT,go_ahead);

		s->encode();
		if( !putClassAd(s, msg) || !s->end_of_message() ) {
			formatstr(error_desc, "Failed to send GoAhead new timeout message.");
		}
	}
	ASSERT( timeout > alive_slop );
	timeout -= alive_slop;

	// Don't bother to request a transfer queue slot for
	// small-enough sandboxes.
	long int min_required_to_transfer = param_integer(
		"BYTES_REQUIRED_TO_QUEUE_FOR_TRANSFER", 100 * 1024 * 1024
	);
	if( sandbox_size <= min_required_to_transfer ) {
		dprintf( D_ALWAYS, "Not entering transfer queue because sandbox (%ld) is too small (<= %ld).\n", sandbox_size, min_required_to_transfer );
		go_ahead = GO_AHEAD_ALWAYS;
	} else {
		if( !xfer_queue.RequestTransferQueueSlot(downloading,sandbox_size,full_fname,m_jobid.c_str(),queue_user.c_str(),timeout,error_desc) )
		{
			go_ahead = GO_AHEAD_FAILED;
		}
	}

	bool first_poll = true;
	while(1) {
		if( go_ahead == GO_AHEAD_UNDEFINED ) {
			timeout = alive_interval - (time(NULL) - last_alive) - alive_slop;
			if( timeout < min_timeout ) timeout = min_timeout;
			if( first_poll ) {
					// Use a short timeout on the first time, so we quickly report
					// that the transfer is queued, if it is.
				timeout = 5;
			}
			bool pending = true;
			if( xfer_queue.PollForTransferQueueSlot(timeout,pending,error_desc) )
			{
				// In the current version of HTCondor, the file transfer
				// queue slot lasts as long as the TCP connection does.
				// Hence, there is no need to keep checking for GoAhead
				// for each file; just let 'em rip.
				go_ahead = GO_AHEAD_ALWAYS;
			}
			else if( !pending ) {
				go_ahead = GO_AHEAD_FAILED;
			}
		}

		char const *ip = s->peer_ip_str();
		char const *go_ahead_desc = "";
		if( go_ahead < 0 ) go_ahead_desc = "NO ";
		if( go_ahead == GO_AHEAD_UNDEFINED ) go_ahead_desc = "PENDING ";

		dprintf( go_ahead < 0 ? D_ALWAYS : D_FULLDEBUG,
				 "Sending %sGoAhead for %s to %s %s%s.\n",
				 go_ahead_desc,
				 ip ? ip : "(null)",
				 downloading ? "send" : "receive",
				 UrlSafePrint(full_fname),
				 (go_ahead == GO_AHEAD_ALWAYS) ? " and all further files":"");

		s->encode();
		msg.Assign(ATTR_RESULT,go_ahead); // go ahead
		if( downloading ) {
			msg.Assign(ATTR_MAX_TRANSFER_BYTES,MaxDownloadBytes);
		}
		if( go_ahead < 0 ) {
				// tell our peer what exactly went wrong
			msg.Assign(ATTR_TRY_AGAIN,try_again);
			msg.Assign(ATTR_HOLD_REASON_CODE,hold_code);
			msg.Assign(ATTR_HOLD_REASON_SUBCODE,hold_subcode);
			if( error_desc.length() ) {
				msg.Assign(ATTR_HOLD_REASON,error_desc.c_str());
			}
		}
		if( !putClassAd(s, msg) || !s->end_of_message() ) {
			formatstr(error_desc, "Failed to send GoAhead message.");
			try_again = true;
			return false;
		}
		last_alive = time(NULL);

		if( go_ahead != GO_AHEAD_UNDEFINED ) {
			break;
		}

		UpdateXferStatus(XFER_STATUS_QUEUED);
	}

	if( go_ahead == GO_AHEAD_ALWAYS ) {
		go_ahead_always = true;
	}

	return go_ahead > 0;
}

bool
FileTransfer::ReceiveTransferGoAhead(
	Stream *s,
	char const *fname,
	bool downloading,
	bool &go_ahead_always,
	filesize_t &peer_max_transfer_bytes)
{
	bool try_again = true;
	int hold_code = 0;
	int hold_subcode = 0;
	std::string error_desc;
	bool result;
	int alive_interval;
	int old_timeout;
	const int slop_time = 20; // extra time to wait when alive_interval expires
	const int min_alive_interval = 300;

	// How frequently peer should tell us that it is still alive while
	// we are waiting for GoAhead.  Note that the peer may respond
	// with its own specification of timeout if it does not agree with
	// ours.  This is an important issue, because our peer may need to
	// talk to some other service (i.e. the schedd) before getting
	// back to us.

	alive_interval = clientSockTimeout;
	if( alive_interval < min_alive_interval ) {
		alive_interval = min_alive_interval;
	}
	old_timeout = s->timeout(alive_interval + slop_time);

	result = DoReceiveTransferGoAhead(s,fname,downloading,go_ahead_always,peer_max_transfer_bytes,try_again,hold_code,hold_subcode,error_desc,alive_interval);

	s->timeout( old_timeout );

	if( !result ) {
		SaveTransferInfo(false,try_again,hold_code,hold_subcode,error_desc.c_str());
		if( error_desc.length() ) {
			dprintf(D_ALWAYS,"%s\n",error_desc.c_str());
		}
	}

	return result;
}

bool
FileTransfer::DoReceiveTransferGoAhead(
	Stream *s,
	char const *fname,
	bool downloading,
	bool &go_ahead_always,
	filesize_t &peer_max_transfer_bytes,
	bool &try_again,
	int &hold_code,
	int &hold_subcode,
	std::string &error_desc,
	int alive_interval)
{
	int go_ahead = GO_AHEAD_UNDEFINED;

	s->encode();

	if( !s->put(alive_interval) || !s->end_of_message() ) {
		error_desc = "DoReceiveTransferGoAhead: failed to send alive_interval";
		return false;
	}

	s->decode();

	while(1) {
		ClassAd msg;
		if( !getClassAd(s, msg) || !s->end_of_message() ) {
			char const *ip = s->peer_ip_str();
			formatstr(error_desc, "Failed to receive GoAhead message from %s.",
							   ip ? ip : "(null)");

			return false;
		}

		go_ahead = GO_AHEAD_UNDEFINED;
		if(!msg.LookupInteger(ATTR_RESULT,go_ahead)) {
			std::string msg_str;
			sPrintAd(msg_str, msg);
			formatstr(error_desc, "GoAhead message missing attribute: %s.  "
							   "Full classad: [\n%s]",
							   ATTR_RESULT,msg_str.c_str());
			try_again = false;
			hold_code = CONDOR_HOLD_CODE::InvalidTransferGoAhead;
			hold_subcode = 1;
			return false;
		}

		filesize_t mtb = peer_max_transfer_bytes;
		if( msg.LookupInteger(ATTR_MAX_TRANSFER_BYTES,mtb) ) {
			peer_max_transfer_bytes = mtb;
		}

		if( go_ahead == GO_AHEAD_UNDEFINED ) {
				// This is just an "alive" message from our peer.
				// Keep looping.

			int new_timeout = -1;
			if( msg.LookupInteger(ATTR_TIMEOUT,new_timeout) &&
				new_timeout != -1)
			{
				// our peer wants a different timeout
				s->timeout(new_timeout);
				dprintf(D_FULLDEBUG,"Peer specified different timeout "
				        "for GoAhead protocol: %d (for %s)\n",
						new_timeout, UrlSafePrint(fname));
			}

			dprintf(D_FULLDEBUG,"Still waiting for GoAhead for %s.\n",fname);
			UpdateXferStatus(XFER_STATUS_QUEUED);
			continue;
		}

		if(!msg.LookupBool(ATTR_TRY_AGAIN,try_again)) {
			try_again = true;
		}

		if(!msg.LookupInteger(ATTR_HOLD_REASON_CODE,hold_code)) {
			hold_code = 0;
		}
		if(!msg.LookupInteger(ATTR_HOLD_REASON_SUBCODE,hold_subcode)) {
			hold_subcode = 0;
		}
		msg.LookupString(ATTR_HOLD_REASON, error_desc);

		break;
	}

	if( go_ahead <= 0 ) {
		return false;
	}

	if( go_ahead == GO_AHEAD_ALWAYS ) {
		go_ahead_always = true;
	}

	dprintf(D_FULLDEBUG,"Received GoAhead from peer to %s %s%s.\n",
			downloading ? "receive" : "send",
			UrlSafePrint(fname),
			go_ahead_always ? " and all further files" : "");

	return true;
}

int
FileTransfer::ExitDoUpload(ReliSock *s, bool socket_default_crypto, priv_state saved_priv, DCTransferQueue & xfer_queue, const filesize_t *total_bytes_ptr, UploadExitInfo& xfer_info)
{
	int rc = xfer_info.upload_success ? 0 : -1;
	bool download_success = false;
	std::string error_buf;
	std::string download_error_buf;


	dprintf(D_FULLDEBUG, "DoUpload: exiting at %d\n", xfer_info.exit_line);
	dprintf(D_FULLDEBUG, "Transfer exit info: %s\n", xfer_info.displayStr().c_str());

	if( saved_priv != PRIV_UNKNOWN ) {
		_set_priv(saved_priv, __FILE__, xfer_info.exit_line, 1);
	}

	bytesSent += *total_bytes_ptr;

	if(xfer_info.checkAck(TransferAck::UPLOAD)) {
		// peer is still expecting us to send a file command
		if(!PeerDoesTransferAck && !xfer_info.upload_success) {
			// We have no way to tell the other side that something has
			// gone wrong other than slamming the connection without
			// sending the final file command 0.  Therefore, send nothing.
		}
		else {
			// no more files to send
			s->snd_int(static_cast<int>(TransferCommand::Finished), TRUE);

			// go back to the state we were in before file transfer
			s->set_crypto_mode(socket_default_crypto);

			std::string error_desc_to_send;
			if(!xfer_info.upload_success) {
				formatstr(error_desc_to_send, "%s at %s failed to send file(s) to %s",
										   get_mySubSystem()->getName(),
										   s->my_ip_str(),
										   s->get_sinful_peer());
				if (!xfer_info.error_desc.empty()) {
					formatstr_cat(error_desc_to_send,": %s", xfer_info.error_desc.c_str());
				}
			}
			SendTransferAck(s, xfer_info.upload_success, xfer_info.try_again,
			                xfer_info.hold_code, xfer_info.hold_subcode,
			                error_desc_to_send.c_str());
		}
	} else {
		// go back to the state we were in before file transfer
		s->set_crypto_mode(socket_default_crypto);
	}

	// Now find out whether there was an error on the receiver's
	// (i.e. downloader's) end, such as failure to write data to disk.
	// If we have already failed to communicate with the receiver
	// for reasons that are likely to be transient network issues
	// (e.g. timeout writing), then ideally do_download_ack would be false,
	// and we will skip this step.
	if(xfer_info.checkAck(TransferAck::DOWNLOAD)) {
		GetTransferAck(s, download_success, xfer_info.try_again,
		               xfer_info.hold_code, xfer_info.hold_subcode,
		               download_error_buf);
		if(!download_success) {
			rc = -1;
		}
	}

	// Final release of the transfer token in case we held onto it
	// because we passed a protected URL for download
	xfer_queue.ReleaseTransferQueueSlot();

	if(rc != 0) {
		char const *receiver_ip_str = s->get_sinful_peer();
		if(!receiver_ip_str) {
			receiver_ip_str = "disconnected socket";
		}

		formatstr(error_buf, "%s at %s failed to send file(s) to %s",
						  get_mySubSystem()->getName(),
						  s->my_ip_str(),receiver_ip_str);
		if(!xfer_info.error_desc.empty()) {
			formatstr_cat(error_buf, ": %s", xfer_info.error_desc.c_str());
		}

		if(!download_error_buf.empty()) {
			formatstr_cat(error_buf, "; %s", download_error_buf.c_str());
		}

		if(xfer_info.try_again) {
			dprintf(D_ALWAYS,"DoUpload: %s\n", error_buf.c_str());
		}
		else {
			dprintf(D_ALWAYS,"DoUpload: (Condor error code %d, subcode %d) %s\n",
			                 xfer_info.hold_code, xfer_info.hold_subcode, error_buf.c_str());
		}
	}

	// Record error information so it can be copied back through
	// the transfer status pipe and/or observed by the caller
	// of Upload().
	Info.success = rc == 0;
	Info.try_again = xfer_info.try_again;
	Info.hold_code = xfer_info.hold_code;
	Info.hold_subcode = xfer_info.hold_subcode;
	Info.error_desc = error_buf;

		// Log some tcp statistics about this transfer
	if (*total_bytes_ptr > 0) {
		int cluster = -1;
		int proc = -1;
		jobAd.LookupInteger(ATTR_CLUSTER_ID, cluster);
		jobAd.LookupInteger(ATTR_PROC_ID, proc);

		char *stats = s->get_statistics();
		formatstr(Info.tcp_stats, "File Transfer Upload: JobId: %d.%d files: %d bytes: %lld seconds: %.2f dest: %s %s\n",
		          cluster, proc, xfer_info.xfered_files, (long long)*total_bytes_ptr, (uploadEndTime - uploadStartTime),
		          s->peer_ip_str(), (stats ? stats : ""));
		dprintf(D_STATS, "%s", Info.tcp_stats.c_str());
	}

	return rc;
}

void
FileTransfer::stopServer()
{
	abortActiveTransfer();
	if (TransKey) {
		// remove our key from the hash table
		if ( TranskeyTable ) {
			std::string key(TransKey);
			TranskeyTable->remove(key);
			if ( TranskeyTable->getNumElements() == 0 ) {
				// if hash table is empty, delete table as well
				delete TranskeyTable;
				TranskeyTable = NULL;
			}
		}
		// and free the key as well
		free(TransKey);
		TransKey = NULL;
	}
}

void
FileTransfer::abortActiveTransfer()
{
	if( ActiveTransferTid != -1 ) {
		ASSERT( daemonCore );
		dprintf(D_ALWAYS,"FileTransfer: killing active transfer %d\n",ActiveTransferTid);
		daemonCore->Kill_Thread(ActiveTransferTid);
		TransThreadTable->remove(ActiveTransferTid);
		ActiveTransferTid = -1;
	}
}

int
FileTransfer::Suspend() const
{
	int result = TRUE;	// return TRUE if there currently is no thread

	if (ActiveTransferTid != -1 ) {
		ASSERT( daemonCore );
		result = daemonCore->Suspend_Thread(ActiveTransferTid);
	}

	return result;
}

int
FileTransfer::Continue() const
{
	int result = TRUE;	// return TRUE if there currently is no thread

	if (ActiveTransferTid != -1 ) {
		ASSERT( daemonCore );
		result = daemonCore->Continue_Thread(ActiveTransferTid);
	}

	return result;
}


void
FileTransfer::addOutputFile( const char* filename )
{
	if( !file_contains(OutputFiles, filename) ) {
		OutputFiles.emplace_back(filename);
	}
}

void
FileTransfer::addFailureFile( const char* filename )
{
	if( !file_contains(FailureFiles, filename) ) {
		FailureFiles.emplace_back(filename);
	}
}

bool
FileTransfer::addFileToExceptionList( const char* filename )
{
	if (ExceptionFiles.end() != 
			std::find(ExceptionFiles.begin(), ExceptionFiles.end(), decltype(ExceptionFiles)::value_type(filename))) {
		return true;
	}
	ExceptionFiles.emplace_back(filename);
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
		// The sender tells the receiver whether they're delegating or
		// copying credential files, so it's ok for them to have different
		// values for DelegateX509Credentials.
	if ( peer_version.built_since_version(6,7,19) &&
		 param_boolean( "DELEGATE_JOB_GSI_CREDENTIALS", true ) ) {
		DelegateX509Credentials = true;
	} else {
		DelegateX509Credentials = false;
	}
	if ( peer_version.built_since_version(6,7,20) ) {
		PeerDoesTransferAck = true;
	}
	else {
		PeerDoesTransferAck = false;
		dprintf(D_FULLDEBUG,
			"FileTransfer: peer (version %d.%d.%d) does not support "
			"transfer ack.  Will use older (unreliable) protocol.\n",
			peer_version.getMajorVer(),
			peer_version.getMinorVer(),
			peer_version.getSubMinorVer());
	}
	if( peer_version.built_since_version(6,9,5) ) {
		PeerDoesGoAhead = true;
	}
	else {
		PeerDoesGoAhead = false;
	}

	if( peer_version.built_since_version(7,5,4) ) {
		PeerUnderstandsMkdir = true;
	}
	else {
		PeerUnderstandsMkdir = false;
	}

	if ( peer_version.built_since_version(7,6,0) ) {
		TransferUserLog = false;
	} else {
		TransferUserLog = true;
	}

	if( peer_version.built_since_version(8,1,0) ) {
		PeerDoesXferInfo = true;
	}
	else {
		PeerDoesXferInfo = false;
	}

	PeerDoesReuseInfo = peer_version.built_since_version(8,9,4);
	PeerDoesS3Urls = peer_version.built_since_version(8,9,4);
	PeerRenamesExecutable = ! peer_version.built_since_version(10, 6, 0);
	PeerKnowsProtectedURLs = peer_version.built_since_version(23, 1, 0);
}


// will take a filename and look it up in our internal catalog.  returns
// true if found and false if not.  also updates the parameters mod_time
// and filesize if they are not null.
bool FileTransfer::LookupInFileCatalog(const char *fname, time_t *mod_time, filesize_t *filesize) {
	CatalogEntry *entry = 0;
	std::string fn = fname;
	if (last_download_catalog->lookup(fn, entry) == 0) {
		// hashtable return code zero means found (!?!)

		// update if passed in
		if (mod_time) {
			*mod_time = entry->modification_time;
		}

		// update if passed in
		if (filesize) {
			*filesize = entry->filesize;
		}

		// we return true, as in 'yes, we found it'
		return true;
	} else {
		// not found
		return false;
	}
}


// normally, we want to build our catalog (last_download_catalog) on the Iwd
// that we already have.  but to support all modes of operation, we can also
// accept a different directory, and a different catalog to put them into.
//
// we take a pointer to this catalog pointer so we can correctly delete and
// recreate it with new. (i prefer this over pass by reference because it is
// explicit from the call site.)  by default, we simply set this pointer to
// our own *last_download_catalog.
//
// also, if spool_time is non-zero, set all modification times to that time.
// this is necessary for now, until we store a persistent copy of the catalog
// somewhere (job ad, or preferably in a file in the spool dir itself).
bool FileTransfer::BuildFileCatalog(time_t spool_time, const char* iwd, FileCatalogHashTable **catalog) {

	if (!iwd) {
		// by default, use the one in this intantiation
		iwd = Iwd;
	}

	if (!catalog) {
		// by default, use the one in this intantiation
		catalog = &last_download_catalog;
	}

	if (*catalog) {
		// iterate through catalog and free memory of CatalogEntry s.
		CatalogEntry *entry_pointer;

		(*catalog)->startIterations();
		while((*catalog)->iterate(entry_pointer)) {
			delete entry_pointer;
		}
		delete (*catalog);
	}

	// If we're going to stick a prime number in here, then let's make it
	// big enough that the chains are decent sized. Suppose you might
	// have 50,000 files. In the case for 997 buckets and even distribution,
	// the chains would be ~50 entries long. Good enough.
	(*catalog) = new FileCatalogHashTable(hashFunction);

	/* If we've decided not to use a file catalog, then leave it empty. */
	if (m_use_file_catalog == false) {
		/* just leave the catalog empty. */
		return true;
	}

	// now, iterate the directory and put the relavant info into the catalog.
	// this currently excludes directories, and only stores the modification
	// time and filesize.  if you were to add hashes, signatures, etc., that
	// would go here.
	//
	// also note this information is not sufficient to notice a byte changing
	// in a file and the file being backdated, since neither modification time
	// nor filesize change in that case.
	//
	// furthermore, if spool_time was specified, we set filesize to -1 as a
	// flag for special behavior in FindChangedFiles and set all file
	// modification times to spool_time.  this essentially builds a catalog
	// that mimics old behavior.
	//
	// make sure this iteration is done as the actual owner of the directory,
	// as it may not be world-readable.
	// desired_priv_state indicates which priv state that is.
	Directory file_iterator(iwd, desired_priv_state);
	const char * f = NULL;
	while( (f = file_iterator.Next()) ) {
		if (!file_iterator.IsDirectory()) {
			CatalogEntry *tmpentry = 0;
			tmpentry = new CatalogEntry;
			if (spool_time) {
				// -1 for filesize is a special flag for old behavior.
				// when checking a file to see if it is new, if the filesize
				// is -1 then the file date must be newer (not just different)
				// than the stored modification date. (see FindChangedFiles)
				tmpentry->modification_time = spool_time;
				tmpentry->filesize = -1;
			} else {
				tmpentry->modification_time = file_iterator.GetModifyTime();
				tmpentry->filesize = file_iterator.GetFileSize();
			}
			std::string fn = f;
			(*catalog)->insert(fn, tmpentry);
		}
	}

	// always, succeed
	return true;
}

void FileTransfer::setSecuritySession(char const *session_id) {
	free(m_sec_session_id);
	m_sec_session_id = NULL;
	m_sec_session_id = session_id ? strdup(session_id) : NULL;
}

// Determines the third-party plugin needed for a file transfer.
// Looks at both source and destination to determine which one contains a URL,
// then extracts the method (ie. http, ftp) and uses it to lookup plugin.
std::string FileTransfer::DetermineFileTransferPlugin( CondorError &error, const char* source, const char* dest ) {

	char *URL = NULL;
	std::string plugin;

	// First, check the destination to see if it looks like a URL.
	// If not, source must be the URL.
	if( IsUrl( dest ) ) {
		URL = const_cast<char*>(dest);
		dprintf( D_FULLDEBUG, "FILETRANSFER: DFT: using destination to determine "
			"plugin type: %s\n", UrlSafePrint(dest) );
	}
	else {
		URL = const_cast<char*>(source);
		dprintf( D_FULLDEBUG, "FILETRANSFER: DFT: using source to determine "
			"plugin type: %s\n", UrlSafePrint(source) );
	}

	// Find the type of transfer
	auto method = getURLType( URL, true );

	// we now (as of 8.9.8) defer building the table until we actually
	// need it.  if the job had custom plugins the table is already built.
	// but if not we need to build it now.
	if ( !plugin_table ) {
		// this function always succeeds (sigh) but we can capture the errors
		dprintf(D_VERBOSE, "FILETRANSFER: Building full plugin table to look for %s.\n", method.c_str());
		if(-1 == InitializeSystemPlugins(error, false)) {
			return "";
		}
	}

	// Hashtable returns zero if found.
	if ( plugin_table->lookup( method, plugin ) ) {
		// no plugin for this type!!!
		dprintf ( D_FULLDEBUG, "FILETRANSFER: plugin for type %s not found!\n", method.c_str() );
		return "";
	}

	return plugin;
}


TransferPluginResult
FileTransfer::InvokeFileTransferPlugin(CondorError &e, int &exit_status, const char* source, const char* dest, ClassAd* plugin_stats, const char* proxy_filename) {

	// detect which plugin to invoke
	const char *URL = NULL;

	// first, check the dest to see if it looks like a URL.  if not, source must
	// be the URL.
	if(IsUrl(dest)) {
		URL = dest;
		dprintf(D_FULLDEBUG, "FILETRANSFER: IFT: using destination to determine plugin type: %s\n", UrlSafePrint(dest));
	} else {
		URL = source;
		dprintf(D_FULLDEBUG, "FILETRANSFER: IFT: using source to determine plugin type: %s\n", UrlSafePrint(source));
	}

	// find the type of transfer
	const char* colon = strchr(URL, ':');

	if (!colon) {
		// in theory, this should never happen -- then sending side should only
		// send URLS after having checked this.  however, trust but verify.
		e.pushf("FILETRANSFER", 1, "Specified URL does not contain a ':' (%s)", URL);
		return TransferPluginResult::Error;
	}

	// Find the type of transfer
	auto method = getURLType( URL, true );

	// we now (as of 8.9.8) defer building the table until we actually
	// need it.  if the job had custom plugins the table is already built.
	// but if not we need to build it now.
	if ( !plugin_table ) {
		// this function always succeeds (sigh) but we can capture the errors
		dprintf(D_VERBOSE, "FILETRANSFER: Building full plugin table to look for %s.\n", method.c_str());
		if(-1 == InitializeSystemPlugins(e, false)) {
			return TransferPluginResult::Error;
		}
	}

	// look up the method in our hash table
	std::string plugin;

	// hashtable returns zero if found.
	if (plugin_table->lookup(method, plugin)) {
		// no plugin for this type!!!
		e.pushf("FILETRANSFER", 1, "FILETRANSFER: plugin for type %s not found!", method.c_str());
		dprintf (D_FULLDEBUG, "FILETRANSFER: plugin for type %s not found!\n", method.c_str());
		return TransferPluginResult::Error;
	}


	// prepare environment for the plugin
	Env plugin_env;

	// start with this environment
	plugin_env.Import();

	// Add any credential directory.
	if (!m_cred_dir.empty()) {
		plugin_env.SetEnv( "_CONDOR_CREDS", m_cred_dir.c_str() );
	}

	// add x509UserProxy if it's defined
	if (proxy_filename && *proxy_filename) {
		plugin_env.SetEnv("X509_USER_PROXY",proxy_filename);
		dprintf(D_FULLDEBUG, "FILETRANSFER: setting X509_USER_PROXY env to %s\n", proxy_filename);
	}

	if (!m_job_ad.empty()) {
		plugin_env.SetEnv("_CONDOR_JOB_AD", m_job_ad.c_str());
		dprintf(D_FULLDEBUG, "FILETRANSFER: setting runtime job ad to %s\n", m_job_ad.c_str());
	}
	if (!m_machine_ad.empty()) {
		plugin_env.SetEnv("_CONDOR_MACHINE_AD", m_machine_ad.c_str());
		dprintf(D_FULLDEBUG, "FILETRANSFER: setting runtime machine ad to %s\n", m_machine_ad.c_str());
	}

	// prepare args for the plugin
	ArgList plugin_args;
	plugin_args.AppendArg(plugin);
	plugin_args.AppendArg(source);
	plugin_args.AppendArg(dest);
	dprintf(D_FULLDEBUG, "FileTransfer::InvokeFileTransferPlugin: %s %s %s\n", plugin.c_str(), UrlSafePrint(source), UrlSafePrint(dest));

	// determine if we want to run the plugin with root priv (if available).
	// if so, drop_privs should be false.  the default is to drop privs.
	bool drop_privs = !param_boolean("RUN_FILETRANSFER_PLUGINS_WITH_ROOT", false);

	//
	// Invoke the plug-in.
	//
	MyPopenTimer p_timer;
	p_timer.start_program(
		plugin_args,
		false,
		& plugin_env,
		drop_privs
	);

	int rc = 0;
	int timeout = param_integer( "MAX_FILE_TRANSFER_PLUGIN_LIFETIME", 72000 );
	if ( ! p_timer.wait_for_exit(timeout, & rc)) {
		p_timer.close_program(1); // send TERM, wait 1 second, then KILL
		rc = p_timer.exit_status();
	}

	bool exit_by_signal;
	TransferPluginResult result;

	if( p_timer.was_timeout() ) {
		exit_status = ETIME;
		exit_by_signal = TRUE;

		result = TransferPluginResult::TimedOut;

		dprintf( D_ALWAYS, "FILETRANSFER: plugin %s was killed after running for %d seconds.\n", plugin.c_str(), timeout );
	} else if( p_timer.exit_status() == MYPCLOSE_EX_STATUS_UNKNOWN ) {
		exit_status    = -1; // don't know, assume -1 for exit code
		exit_by_signal = false;

		result = TransferPluginResult::Error;

		dprintf( D_ALWAYS, "FILETRANSFER: plugin %s exit status unknown, assuming -1.\n", plugin.c_str() );
	} else {
		exit_status    = WEXITSTATUS(rc);
		exit_by_signal = WIFSIGNALED(rc);

		// We document that exit code 2 might mean something in the future
		// but for now, treat non-zero codes as errors.
		switch (exit_status) {
			case 0:
				result = TransferPluginResult::Success;
				break;
			default:
				result = TransferPluginResult::Error;
				break;
		}
		if (exit_by_signal) {
			result = TransferPluginResult::Error;
		}

		dprintf (D_ALWAYS, "FILETRANSFER: plugin returned %i exit_by_signal: %d\n", exit_status, exit_by_signal);
	}

	// Capture stdout from the plugin and dump it to the stats file
	char * output = p_timer.output().Detach();

	char * token = strtok( output, "\r\n" );
	while( token != nullptr ) {
		// Does this need a newline?  It used to get one.
		if( !plugin_stats->Insert( token ) ) {
			dprintf (D_ALWAYS, "FILETRANSFER: error importing statistic %s\n", token);
		}

		token = strtok(nullptr, "\r\n" );
	}

	free(output);

	plugin_stats->InsertAttr("PluginExitCode", exit_status);
	plugin_stats->InsertAttr("PluginExitBySignal", exit_by_signal);
	dprintf (D_ALWAYS, "FILETRANSFER: plugin %s returned %i exit_by_signal: %d\n", plugin.c_str(), exit_status, exit_by_signal);

	// there is a unique issue when invoking plugins as root where shared
	// libraries defined as relative to $ORIGIN in the RUNPATH will not
	// be loaded for security reasons.  in this case the dynamic loader
	// exits with 127 before even calling main() in the plugin.
	//
	// if we suspect this is the case, let's print a hint since it's
	// otherwise very difficult to understand what is happening and why
	// this failed.
	if (!drop_privs && exit_status == 127) {
		dprintf (D_ALWAYS, "FILETRANSFER: ERROR!  You are invoking plugins as root because "
			"you have RUN_FILETRANSFER_PLUGINS_WITH_ROOT set to TRUE.  However, some of "
			"the shared libraries in your plugin are likely paths that are relative to "
			"$ORIGIN, and then dynamic library loader refuses to load those for security "
			"reasons.  Run 'ldd' on your plugin and move needed libraries to a system "
			"location controlled by root. Good luck!\n");
	}

	// If the plugin did not return successfully, report the error and return
	if (result != TransferPluginResult::Success) {
		if (result == TransferPluginResult::TimedOut) {
			e.pushf( "FILETRANSFER", 1,
				"File transfer plugin %s timed out after %d seconds.",
				plugin.c_str(), timeout
			);

			return TransferPluginResult::TimedOut;
		}

		std::string errorMessage;
		std::string transferUrl;
		if (!plugin_stats->LookupString("TransferError", errorMessage)) {
			errorMessage = "File transfer plugin " + plugin +
				" exited unexpectedly without producing an error message ";
		}
		plugin_stats->LookupString("TransferUrl", transferUrl);
		if (exit_by_signal) {
			e.pushf("FILETRANSFER", 1, "exit by signal %d from %s. |Error: %s ( URL file = %s )|",  // URL file in err msg
					WTERMSIG(rc), plugin.c_str(), errorMessage.c_str(), UrlSafePrint(transferUrl));
		} else {
			e.pushf("FILETRANSFER", 1, "non-zero exit (%i) from %s. |Error: %s ( URL file = %s )|",  // URL file in err msg
					exit_status, plugin.c_str(), errorMessage.c_str(), UrlSafePrint(transferUrl));
		}
		return TransferPluginResult::Error;
	}

	return result;
}


const std::vector< ClassAd > &
FileTransfer::getPluginResultList() {
    return pluginResultList;
}


// Similar to FileTransfer::InvokeFileTransferPlugin, modified to transfer
// multiple files in a single plugin invocation.
// Returns 0 on success, error code >= 1 on failure.
TransferPluginResult
FileTransfer::InvokeMultipleFileTransferPlugin( CondorError &e, int &exit_status,
			const std::string &plugin_path, const std::string &transfer_files_string,
			const char* proxy_filename, bool do_upload ) {

	ArgList plugin_args;
	CondorClassAdFileIterator adFileIter;
	FILE* input_file;
	FILE* output_file;
	std::string input_filename;
	std::string output_filename;
	std::string plugin_name;

	// Prepare environment for the plugin
	Env plugin_env;
	plugin_env.Import();

	// Add any credential directory.
	if (!m_cred_dir.empty()) {
		plugin_env.SetEnv( "_CONDOR_CREDS", m_cred_dir.c_str() );
	}

	// Add x509UserProxy if it's defined
	if ( proxy_filename && *proxy_filename ) {
		plugin_env.SetEnv( "X509_USER_PROXY",proxy_filename );
		dprintf( D_FULLDEBUG, "FILETRANSFER: setting X509_USER_PROXY env to %s\n",
				proxy_filename );
	}
	if (!m_job_ad.empty()) {
		plugin_env.SetEnv("_CONDOR_JOB_AD", m_job_ad.c_str());
		dprintf(D_FULLDEBUG, "FILETRANSFER: setting runtime job ad to %s\n", m_job_ad.c_str());
	}
	if (!m_machine_ad.empty()) {
		plugin_env.SetEnv("_CONDOR_MACHINE_AD", m_machine_ad.c_str());
		dprintf(D_FULLDEBUG, "FILETRANSFER: setting runtime machine ad to %s\n", m_machine_ad.c_str());
	}


	// Determine if we want to run the plugin with root priv (if available).
	// If so, drop_privs should be false.  the default is to drop privs.
	bool drop_privs = !param_boolean( "RUN_FILETRANSFER_PLUGINS_WITH_ROOT", false );
	if (plugins_from_job.find(plugin_path) != plugins_from_job.end()) { drop_privs = true; }

	// Lookup the initial working directory
	std::string iwd;
	if ( jobAd.LookupString( ATTR_JOB_IWD, iwd ) != 1) {
		dprintf( D_ALWAYS, "FILETRANSFER InvokeMultipleFileTransferPlugin: "
					"Job Ad did not have an IWD! Aborting.\n" );
		return TransferPluginResult::Error;
	}

	// Create an input file for the plugin.
	// Input file consists of the transfer_files_string data (list of classads)
	// which we'll save to a temporary file in the working directory.
	plugin_name = plugin_path.substr( plugin_path.find_last_of("/\\") + 1 );
	input_filename = iwd + "/." + plugin_name + ".in";
	input_file = safe_fopen_wrapper( input_filename.c_str(), "w" );
	if (input_file == nullptr) {
		dprintf( D_ALWAYS, "FILETRANSFER InvokeMultipleFileTransferPlugin: "
			"Could not open %s for writing (%s, errno=%d), aborting\n",
			input_filename.c_str(), strerror(errno), errno );
		return TransferPluginResult::Error;
	}
	int fputs_error  = fputs(transfer_files_string.c_str(), input_file);
	if (fputs_error == EOF) {
		dprintf( D_ALWAYS, "FILETRANSFER InvokeMultipleFileTransferPlugin: "
			"Could not write to file %s (%s, errno=%d), aborting file transfer\n",
			input_filename.c_str(), strerror(errno), errno);
		std::ignore = fclose(input_file);
		return TransferPluginResult::Error;
	}

	int fclose_error = fclose(input_file);
	if (fclose_error == EOF) {
		dprintf( D_ALWAYS, "FILETRANSFER InvokeMultipleFileTransferPlugin: "
			"Could not close file %s (%s, errno=%d), aborting file transfer\n",
			input_filename.c_str(), strerror(errno), errno);
		return TransferPluginResult::Error;
	}


	output_filename = iwd + "/." + plugin_name + ".out";

	// Pre-allocate the output file.  Filling it with spaces avoids
	// both potential cross-platform fallocate() issues and potential
	// sparse allocation.
	output_file = safe_fopen_wrapper( output_filename.c_str(), "w" );
	if( output_file == nullptr ) {
		dprintf( D_ALWAYS, "FILETRANSFER InvokeMultipleFileTransferPlugin: "
			"Could not open %s for writing (%s, errno=%d), aborting\n",
			output_filename.c_str(), strerror(errno), errno );
		return TransferPluginResult::Error;
	}
	const char sixty_four_spaces[] =
		"                                                                ";
	for( unsigned i = 0; i < ((16 * 1204)/64); ++i ) {
		if( fputs( sixty_four_spaces, output_file ) == EOF ) {
			dprintf( D_ALWAYS, "FILETRANSFER InvokeMultipleFileTransferPlugin: "
				"Failed to preallocate output file (fputs() failed), aborting\n" );
			return TransferPluginResult::Error;
		}
	}
	int rv = fclose(output_file);
	if( rv != 0 ) {
		dprintf( D_ALWAYS, "FILETRANSFER InvokeMultipleFileTransferPlugin: "
			"Failed to preallocate output file (fclose() failed), aborting\n" );
		return TransferPluginResult::Error;
	}
	output_file = nullptr;


	// Prepare args for the plugin
	plugin_args.AppendArg( plugin_path.c_str() );
	plugin_args.AppendArg( "-infile" );
	plugin_args.AppendArg( input_filename.c_str() );
	plugin_args.AppendArg( "-outfile" );
	plugin_args.AppendArg( output_filename.c_str() );
	if (do_upload) {
		plugin_args.AppendArg( "-upload" );
	}

	if (IsFulldebug(D_ALWAYS)) {
		std::string arglog;
		plugin_args.GetArgsStringForLogging(arglog);
		// note - test_curl_plugin.py depends on seeing the word 'invoking' in the starter log.
		dprintf( D_FULLDEBUG, "FILETRANSFER: invoking: %s \n", arglog.c_str() );
	}

	bool want_stderr = param_boolean("REDIRECT_FILETRANSFER_PLUGIN_STDERR_TO_STDOUT", true);
	MyPopenTimer p_timer;
	int plugin_exec_result = p_timer.start_program(
		plugin_args,
		want_stderr,
		& plugin_env,
		drop_privs
	);

	if (plugin_exec_result != 0) {
		exit_status = errno;
		std::string message;

		formatstr(message, "FILETRANSFER: Failed to execute %s: %s", plugin_path.c_str(), strerror(errno));
		dprintf(D_ALWAYS, "%s\n", message.c_str());
		e.pushf("FILETRANSFER", 1, "%s", message.c_str());
		return TransferPluginResult::ExecFailed;
	}

	int rc = 0;
	int timeout = param_integer( "MAX_FILE_TRANSFER_PLUGIN_LIFETIME", 72000 );
	if ( ! p_timer.wait_for_exit( timeout, & rc )) {
		p_timer.close_program( 1 );
		rc = p_timer.exit_status();
	}

	TransferPluginResult result;

	if( p_timer.was_timeout() ) {
		exit_status = ETIME;
		result = TransferPluginResult::TimedOut;

		dprintf( D_ERROR, "FILETRANSFER: plugin %s was killed after running for %d seconds.\n", plugin_path.c_str(), timeout );
	} else if( p_timer.exit_status() == MYPCLOSE_EX_STATUS_UNKNOWN ) {
		exit_status    = -1; // pick a value for exit status
		result = TransferPluginResult::Error;

		dprintf( D_ERROR, "FILETRANSFER: plugin %s exit status unknown, assuming -1.\n", plugin_path.c_str() );
	} else {
		exit_status    = WEXITSTATUS(rc);

		// We document that exit code 2 might mean something in the future
		// but for now, treat non-zero codes as errors.
		switch (exit_status) {
			case 0:
				result = TransferPluginResult::Success;
				break;
			default:
				result = TransferPluginResult::Error;
				break;
		}

		bool exit_by_signal = WIFSIGNALED(rc);
		if (exit_by_signal) {
			result = TransferPluginResult::Error;
		}

		dprintf (D_ERROR, "FILETRANSFER: plugin %s returned %i exit_by_signal: %d\n", plugin_path.c_str(), exit_status, exit_by_signal);
	}

	// load and parse a config knob that tells us with what cat and verbosity we should log the plugin output
	int log_output = -1; // < 0 is don't log
	auto_free_ptr log_level;
	if (result == TransferPluginResult::Success) {
		log_level.set(param("LOG_FILETRANSFER_PLUGIN_STDOUT_ON_SUCCESS"));
	} else {
		log_level.set(param("LOG_FILETRANSFER_PLUGIN_STDOUT_ON_FAILURE"));
	}
	if (log_level) {
		int cat_and_verb = 0;
		if (parse_debug_cat_and_verbosity(log_level, cat_and_verb)) {
			log_output = cat_and_verb;
		}
	}

	// if the transfer plugin had any output, and we are configured to log that output, do so now
	auto_free_ptr outbuf(p_timer.output().Detach());
	if (outbuf && log_output >= 0) {
		const int trunate_output_to = 1024*16; // the max we are willing to put in a single dprintf message
		if (p_timer.output_size() > trunate_output_to) {
			// if output is excessive, just show the last  16k
			char * p = outbuf.ptr() + (p_timer.output_size() - trunate_output_to);
			dprintf (log_output, "FILETRANSFER: plugin %s exit=%d had %d bytes of stdout. last 16KB : %s\n",
				plugin_path.c_str(), exit_status, p_timer.output_size(), p);
		} else {
			dprintf (log_output, "FILETRANSFER: plugin %s exit=%d stdout: %s\n",
				plugin_path.c_str(), exit_status, outbuf.ptr());
		}
	}
	// TODO: forward the transfer plugin output to the shadow
	outbuf.clear();


	// there is a unique issue when invoking plugins as root where shared
	// libraries defined as relative to $ORIGIN in the RUNPATH will not
	// be loaded for security reasons.  in this case the dynamic loader
	// exits with 127 before even calling main() in the plugin.
	//
	// if we suspect this is the case, let's print a hint since it's
	// otherwise very difficult to understand what is happening and why
	// this failed.
	if ( !drop_privs && exit_status == 127 ) {
		dprintf (D_ALWAYS, "FILETRANSFER: ERROR!  You are invoking plugins as root because "
			"you have RUN_FILETRANSFER_PLUGINS_WITH_ROOT set to TRUE.  However, some of "
			"the shared libraries in your plugin are likely paths that are relative to "
			"$ORIGIN, and then dynamic library loader refuses to load those for security "
			"reasons.  Run 'ldd' on your plugin and move needed libraries to a system "
			"location controlled by root. Good luck!\n");
	}

	// Is there a good reason we weren't doing this before?
	std::string contents;
	if( IsFulldebug(D_FULLDEBUG) ) {
		if( htcondor::readShortFile( output_filename, contents )) {
			dprintf( D_FULLDEBUG, "Plugin output: '%s'\n", contents.c_str() );
		}
	}

	// Output stats regardless of success or failure
	output_file = safe_fopen_wrapper( output_filename.c_str(), "r" );
	if ( output_file == nullptr ) {
		dprintf( D_ALWAYS, "FILETRANSFER: Unable to open %s output file "
			"%s.\n", plugin_path.c_str(), output_filename.c_str() );
		e.pushf( "FILETRANSFER", 1, "|Error: file transfer plugin %s exited with code %i, "
			"unable to open output file %s", plugin_path.c_str(), exit_status, output_filename.c_str() );
		return TransferPluginResult::Error;
	}
	if ( !adFileIter.begin( output_file, false, CondorClassAdFileParseHelper::Parse_new )) {
		dprintf( D_ALWAYS, "FILETRANSFER: Failed to iterate over file transfer output.\n" );
		return TransferPluginResult::Error;
	}
	else {
		int num_ads = 0;
		pluginResultList.emplace_back();
		for( ;
				adFileIter.next( pluginResultList[num_ads] ) > 0;
				++num_ads, pluginResultList.emplace_back() ) {
			ClassAd & this_file_stats_ad = pluginResultList[num_ads];

			this_file_stats_ad.InsertAttr( "PluginExitCode", exit_status );
			RecordFileTransferStats( this_file_stats_ad );

			// If this classad represents a failed transfer, produce an error
			bool transfer_success = false;
			std::string error_message;
			std::string transfer_url;
			this_file_stats_ad.LookupString( "TransferUrl", transfer_url );
			if ( !this_file_stats_ad.LookupBool( "TransferSuccess", transfer_success ) ) {
				error_message = "File transfer plugin " + plugin_path +
					" exited without producing a TransferSuccess result ";
				e.pushf( "FILETRANSFER", 1, "non-zero exit (%i) from %s. |Error: %s (%s)|",
					exit_status, plugin_path.c_str(), error_message.c_str(), transfer_url.c_str() );
			} else if ( !transfer_success ) {
				if (!this_file_stats_ad.LookupString("TransferError", error_message)) {
					error_message = "File transfer plugin " + plugin_path +
						" exited unexpectedly without producing an error message ";
				}
				e.pushf( "FILETRANSFER", 1, "non-zero exit (%i) from %s. |Error: %s ( URL file = %s )|",   // URL file in err msg
					exit_status, plugin_path.c_str(), error_message.c_str(), UrlSafePrint(transfer_url) );
			}

			SendPluginOutputAd( this_file_stats_ad );
		}
		// The loop terminates when next() doesn't fill in the new ad.
		pluginResultList.resize(num_ads);

		if ( num_ads == 0 && result != TransferPluginResult::TimedOut ) {
			dprintf( D_ALWAYS, "FILETRANSFER: No valid classads in file transfer output.\n" );
			e.pushf( "FILETRANSFER", 1, "|Error: file transfer plugin %s exited with code %i, "
				"no valid classads in output file %s", plugin_path.c_str(), exit_status, output_filename.c_str() );
			return TransferPluginResult::Error;
		}

	}
	fclose(output_file);

	// if we got to here with a failure, but no message, add a generic message
	if (e.empty() && result != TransferPluginResult::Success) {
		if (result == TransferPluginResult::TimedOut) {
			e.pushf( "FILETRANSFER", 1,
				"File transfer plugin %s timed out after %d seconds.",
				plugin_path.c_str(), timeout
			);
		} else {
			e.pushf("FILETRANSFER", 1, "File transfer plugin %s failed unexpectedly with exit code %i, "
				"did not report a TransferError message.", plugin_path.c_str(), exit_status);
		}
	}

	return result;
}

int FileTransfer::RecordFileTransferStats( ClassAd &stats ) {

	// this log is meant to be kept in the condor LOG directory, so switch to
	// the correct priv state to manipulate files in that dir.
	priv_state saved_priv = set_condor_priv();

	// Read name of statistics file from params
	std::string stats_file_path;
	if (!param( stats_file_path, "FILE_TRANSFER_STATS_LOG" )) {
		return 1;
	}

	// First, check for an existing statistics file.
	struct stat stats_file_buf;
	int rc = stat( stats_file_path.c_str(), &stats_file_buf );
	if( rc == 0 ) {
		// If it already exists and is larger than 5 Mb, copy the contents
		// to a .old file.
		if( stats_file_buf.st_size > 5'000'000 ) {
			std::string stats_file_old_path = stats_file_path;
			stats_file_old_path += ".old";
			// TODO: Add a lock to prevent two starters from rotating the log
			// at the same time.
			if (rotate_file(stats_file_path.c_str(), stats_file_old_path.c_str()) != 0) {
				dprintf(D_ALWAYS, "FileTransfer failed to rotate %s to %s\n", stats_file_path.c_str(), stats_file_old_path.c_str());
			}
		}
	}

	// Add some new job-related statistics that were not available from
	// the file transfer plugin.
	int cluster_id;
	jobAd.LookupInteger( ATTR_CLUSTER_ID, cluster_id );
	stats.Assign( "JobClusterId", cluster_id );

	int proc_id;
	jobAd.LookupInteger( ATTR_PROC_ID, proc_id );
	stats.Assign( "JobProcId", proc_id );

	std::string owner;
	jobAd.LookupString( ATTR_OWNER, owner );
	stats.Assign( "JobOwner", owner );

	// Output statistics to file
	std::string stats_string;
	std::string stats_output = "***\n";
	sPrintAd( stats_string, stats );
	stats_output += stats_string;

	FILE* stats_file = safe_fopen_wrapper( stats_file_path.c_str(), "a" );
	if( !stats_file ) {
		dprintf( D_ALWAYS, "FILETRANSFER: failed to open statistics file %s with"
			" error %d (%s)\n", stats_file_path.c_str(), errno, strerror(errno) );
	}
	else {
		int stats_file_fd = fileno( stats_file );
		if ( write( stats_file_fd, stats_output.c_str(), stats_output.length() ) == -1 ) {
			dprintf( D_ALWAYS, "FILETRANSFER: failed to write to statistics file %s with"
				" error %d (%s)\n", stats_file_path.c_str(), errno, strerror(errno) );
		}
		fclose( stats_file );
	}

	// back to previous priv state
	set_priv(saved_priv);

	// In addition to the log file, we also want to save stats in our FileTransferInfo object
	std::string protocol;
	if (stats.LookupString("TransferProtocol", protocol)) {
		// Do not record cedar stats here, only plugins
		if (protocol != "cedar") {
			upper_case(protocol);
			std::string attr_count = protocol + "FilesCount";
			std::string attr_size = protocol + "SizeBytes";

			int num_files = 0;
			Info.stats.LookupInteger(attr_count, num_files);
			num_files++;
			Info.stats.InsertAttr(attr_count, num_files);

			long long this_size_bytes;
			if (stats.LookupInteger("TransferTotalBytes", this_size_bytes)) {
				long long prev_size_bytes;
				if (!Info.stats.LookupInteger(attr_size, prev_size_bytes)) {
					prev_size_bytes = 0;
				}
				Info.stats.InsertAttr(attr_size, prev_size_bytes + this_size_bytes);
			}
		}
	}

	return 0;
}

void FileTransfer::DoPluginConfiguration() {
	// see if they are explicitly disabled
	if (param_boolean("ENABLE_URL_TRANSFERS", true)) {
		I_support_filetransfer_plugins = true;
	} else {
		dprintf(D_FULLDEBUG, "FILETRANSFER: transfer plugins are disabled by config.\n");
		I_support_filetransfer_plugins = false;
	}

	// we should also check to see if multi-file transfers have been
	// explicitly disabled.
	if (param_boolean("ENABLE_MULTIFILE_TRANSFER_PLUGINS", true)) {
		multifile_plugins_enabled = true;
	} else {
		dprintf(D_FULLDEBUG, "FILETRANSFER: multi-file transfers are disabled by config.\n");
		multifile_plugins_enabled = false;
	}
}

std::string FileTransfer::GetSupportedMethods(CondorError &e) {
	std::string method_list;

	DoPluginConfiguration();

	// build plugin table if we haven't done so
	if (!plugin_table) {
		if(-1 == InitializeSystemPlugins(e, true)) {
			return "";
		}
	}

	// iterate plugin_table if it existssrc
	if (plugin_table) {
		std::string junk;
		std::string method;

		plugin_table->startIterations();
		while(plugin_table->iterate(method, junk)) {
			// add comma if needed
			if (!(method_list.empty())) {
				method_list += ",";
			}
			method_list += method;
		}
		if( I_support_S3 ) {
			// method_list must contain at least "https".
			method_list += ",s3,gs";
		}
	}
	return method_list;
}

int FileTransfer::AddJobPluginsToInputFiles(const ClassAd &job, CondorError &e, std::vector<std::string> &infiles) const {

	if ( ! I_support_filetransfer_plugins ) {
		return 0;
	}

	std::string job_plugins;
	if ( ! job.LookupString(ATTR_TRANSFER_PLUGINS, job_plugins)) {
		return 0;
	}

	StringTokenIterator plugins(job_plugins, ";");
	for (const char * plug = plugins.first(); plug != NULL; plug = plugins.next()) {
		const char * equals = strchr(plug, '=');
		if (equals) {
			// add the plugin to the front of the input files list
			std::string plugin_path(equals + 1);
			trim(plugin_path);
			if (! file_contains(infiles, plugin_path)) {
				infiles.insert(infiles.begin(), plugin_path);
			}
		} else {
			dprintf(D_ALWAYS, "FILETRANSFER: AJP: no '=' in " ATTR_TRANSFER_PLUGINS " definition '%s'\n", plug);
			e.pushf("FILETRANSFER", 1, "AJP: no '=' in " ATTR_TRANSFER_PLUGINS" definition '%s'", plug);
		}
	}

	return 0;
}

int FileTransfer::InitializeJobPlugins(const ClassAd &job, CondorError &e)
{
	if ( ! I_support_filetransfer_plugins ) {
		return 0;
	}

	std::string job_plugins;
	if ( ! job.LookupString(ATTR_TRANSFER_PLUGINS, job_plugins)) {
		return 0;
	}

	// start with the system table
	if (-1 == InitializeSystemPlugins(e, false)) {
		return -1;
	}

	// process the user plugins
	StringTokenIterator plugins(job_plugins, ";");
	for (const char * plug = plugins.first(); plug != NULL; plug = plugins.next()) {
		const char * equals = strchr(plug, '=');
		if (equals) {
			std::string methods(plug, equals - plug);

			// use the file basename as the plugin name, so that when we invoke it
			// we will invoke the copy in the input sandbox
			std::string plugin_path(equals + 1);
			trim(plugin_path);
			std::string plugin(condor_basename(plugin_path.c_str()));

			std::string dummy;
			InsertPluginMappings(methods, plugin, false, dummy);
			plugins_multifile_support[plugin] = true;
			plugins_from_job[plugin.c_str()] = true;
			multifile_plugins_enabled = true;
		} else {
			dprintf(D_ALWAYS, "FILETRANSFER: IJP: no '=' in " ATTR_TRANSFER_PLUGINS " definition '%s'\n", plug);
			e.pushf("FILETRANSFER", 1, "IJP: no '=' in " ATTR_TRANSFER_PLUGINS " definition '%s'", plug);
		}
	}

	return 0;
}


int FileTransfer::InitializeSystemPlugins(CondorError &e, bool enable_testing) {

	// don't leak even if Initialize gets called more than once
	if (plugin_table) {
		delete plugin_table;
		plugin_table = NULL;
	}
	plugin_ads.clear();

	// see if this is explicitly disabled
	if (!I_support_filetransfer_plugins) {
		return -1;
	}

	// plugin_table is a member variable
	plugin_table = new PluginHashTable(hashFunction);

	// even if we do not have any plugins, we still need to set up the
	// table so any user plugins can be added.  plugin_table should not
	// be NULL after this function exits.
	auto_free_ptr plugin_list_string(param("FILETRANSFER_PLUGINS"));

	for (const auto & path : StringTokenIterator(plugin_list_string)) {
		// TODO: plugin must be an absolute path (win and unix)
		SetPluginMappings( e, path.c_str(), enable_testing );
	}

	// If we have an https plug-in, this version of HTCondor also supports S3.
	std::string method, junk;
	plugin_table->startIterations();
	while( plugin_table->iterate( method, junk ) ) {
		if( method == "https" ) {
			I_support_S3 = true;
		}
	}

	return 0;
}

// query a plugins for the -classad output, capture that into the plugin_ads vector.
// Also look at some attributes of the ad and update the URL to plugin map
// and the plugins_multifile_support map.
void
FileTransfer::SetPluginMappings( CondorError &e, const char* path, bool enable_testing )
{

	ArgList args;
	args.AppendArg(path);
	args.AppendArg("-classad");

	const int timeout = 20; // max time to allow the plugin to run

	MyPopenTimer pgm;

	// start_program returns 0 on success, -1 on "already started", and errno otherwise
	if (pgm.start_program(args, false) != 0) {
		std::string message;
		formatstr(message, "FILETRANSFER: Failed to execute %s -classad: %s skipping", path, strerror(errno));
		dprintf(D_ALWAYS, "%s\n", message.c_str());
		e.pushf("FILETRANSFER", 1, "%s", message.c_str());
		return;
	}

	if ( ! pgm.wait_and_close(timeout) || pgm.output_size() <= 0) {
		int error = pgm.error_code();
		if ( ! error) error = 1;
		dprintf( D_ALWAYS, "FILETRANSFER: No output from %s -classad, ignoring\n", path );
		e.pushf("FILETRANSFER", error, "No output from %s -classad, ignoring", path );
		return;
	}

	ClassAd & ad = plugin_ads.emplace_back();

	std::string line;
	while (pgm.output().readLine(line)) {
		trim(line);
		if (line.empty() || line.front() == '#') continue;
		if ( ! ad.Insert(line.c_str())) {
			dprintf( D_ALWAYS, "FILETRANSFER: Failed to insert '%s' into ClassAd, "
				"ignoring invalid plugin\n", line.c_str());
			e.pushf("FILETRANSFER", 1, "Received invalid input '%s', ignoring", line.c_str() );
			plugin_ads.pop_back();
			return;
		}
	}

	if (ad.size() == 0) {
		dprintf( D_ALWAYS,
					"FILETRANSFER: \"%s -classad\" did not produce any output, ignoring\n",
					path );
		e.pushf("FILETRANSFER", 1, "\"%s -classad\" did not produce any output, ignoring", path );
		plugin_ads.pop_back();
		return;
	}

	ad.Assign("Path", path);

	// TODO: verify that plugin type is FileTransfer
	// e.pushf("FILETRANSFER", 1, "\"%s -classad\" is not plugin type FileTransfer, ignoring", path );

	// extract the info we care about
	std::string methods, failed_methods;
	bool this_plugin_supports_multifile = false;
	if ( ad.LookupBool( "MultipleFileSupport", this_plugin_supports_multifile ) ) {
		plugins_multifile_support[path] = this_plugin_supports_multifile;
	}

	// Before adding mappings, make sure that if multifile plugins are disabled,
	// this is not a multifile plugin.
	if ( multifile_plugins_enabled || !this_plugin_supports_multifile ) {
		if (ad.LookupString( "SupportedMethods", methods)) {
			InsertPluginMappings( methods, path, enable_testing, failed_methods);

			// Additionally, if the plug-in report a proxy for any of its
			// supported methods, record that, too.
			for( const auto & method : StringTokenIterator(methods) ) {
				std::string attr = method + "_proxy";

				std::string proxy;
				if( ad.LookupString( attr, proxy ) ) {
					proxy_by_method[method] = proxy;
				}
			}
		}
	}

	if ( ! failed_methods.empty()) { ad.Assign("FailedMethods", failed_methods); }

	return;
}


void
FileTransfer::InsertPluginMappings(const std::string& methods, const std::string& p, bool enable_testing, std::string & failed_methods)
{
	for (auto & m : StringTokenIterator(methods)) {
		if (enable_testing && !TestPlugin(m, p)) {
			dprintf(D_FULLDEBUG, "FILETRANSFER: protocol \"%s\" not handled by \"%s\" due to failed test\n", m.c_str(), p.c_str());
			if ( ! failed_methods.empty()) failed_methods += ",";
			failed_methods += m;
			continue;
		}
		dprintf(D_FULLDEBUG, "FILETRANSFER: protocol \"%s\" handled by \"%s\"\n", m.c_str(), p.c_str());
		if ( plugin_table->insert(m, p, true) != 0 ) {
			dprintf(D_FULLDEBUG, "FILETRANSFER: error adding protocol \"%s\" to plugin table, ignoring\n", m.c_str());
		}
	}
}

namespace {

class AutoDeleteDirectory {
public:
	AutoDeleteDirectory(std::string dir, ClassAd *ad) : m_dirname(dir), m_ad(ad) {}

	~AutoDeleteDirectory() {
		if (m_dirname.empty()) {return;}

		dprintf(D_FULLDEBUG, "FILETRANSFER: Cleaning up directory %s.\n", m_dirname.c_str());
		Directory dir_obj(m_dirname.c_str());
		if (!dir_obj.Remove_Entire_Directory()) {
			dprintf(D_ALWAYS, "FILETRANSFER: Failed to remove directory %s contents.\n", m_dirname.c_str());
			return;
		}
		if (-1 == rmdir(m_dirname.c_str())) {
			dprintf(D_ALWAYS, "FILETRANSFER: Failed to remove directory %s: %s (errno=%d).\n",
				m_dirname.c_str(), strerror(errno), errno);
		}

		if (m_ad) {
			m_ad->Delete("Iwd");
		}
	}

private:
	std::string m_dirname;
	ClassAd *m_ad;
};

}

bool
FileTransfer::TestPlugin(const std::string &method, const std::string &plugin)
{
	const std::string test_url_param = std::string(method) + "_test_url";
	std::string test_url;
	if (!param(test_url, test_url_param.c_str())) {
		dprintf(D_FULLDEBUG, "FILETRANSFER: no test url defined for method %s.\n", method.c_str());
		return true;
	}

#ifdef WIN32
	dprintf(D_ALWAYS,
		"WARNING - FILETRANSFER: test url defined for method %s in config, but this is not supported on Windows OS.\n",
		method.c_str());
	return true;
#else
	// If we are running as a test starter, we may not have Iwd set appropriately.
	// In this case, create an execute directory.
	std::string iwd, directory;
	if (!jobAd.EvaluateAttrString("Iwd", iwd)) {
		std::string execute_dir;
		if (!param(execute_dir, "EXECUTE")) {
			dprintf(D_ALWAYS, "FILETRANSFER: EXECUTE configuration variable not set; cannot test plugin.\n");
			return false;
		}
		auto test_dir = execute_dir + "/test_file_transfer.XXXXXX";
		std::unique_ptr<char, decltype(&free)> test_dir_template(strdup(test_dir.c_str()), &free);
		{
			TemporaryPrivSentry sentry_mkdir((PRIV_CONDOR_FINAL == get_priv()) ? PRIV_CONDOR_FINAL : PRIV_CONDOR);
			auto result = mkdtemp(test_dir_template.get());
			if (!result) {
				dprintf(D_ALWAYS, "FILETRANSFER: Failed to create temporary test directory %s: %s (errno=%d).\n",
					test_dir_template.get(), strerror(errno), errno);
				return false;
			}
			directory = std::string(result);
		}
		if (user_ids_are_inited()) {
			TemporaryPrivSentry sentry_mkdir((PRIV_CONDOR_FINAL == get_priv()) ? PRIV_CONDOR_FINAL : PRIV_ROOT);
			if (0 != chown(directory.c_str(), get_user_uid(), get_user_gid())) {
				dprintf(D_ALWAYS, "FILETRANSFER: Failed to chown temporary test directory %s to user UID %d: %s (errno=%d).\n",
					directory.c_str(), get_user_uid(), strerror(errno), errno);
				return false;
			}
		}
		iwd = directory;
		jobAd.InsertAttr("Iwd", directory);
	}
	AutoDeleteDirectory dir_delete(directory, &jobAd);

	auto fullname = iwd + DIR_DELIM_CHAR + "test_file";

	classad::ClassAd testAd;
	testAd.InsertAttr("Url", test_url);
	testAd.InsertAttr("LocalFileName", fullname);
	std::string testAdString;
	classad::ClassAdUnParser unparser;
	unparser.Unparse(testAdString, &testAd);

	CondorError err;
	int exit_code = 0;
	auto result = InvokeMultipleFileTransferPlugin(err, exit_code, plugin, testAdString, nullptr, false);
	if (result != TransferPluginResult::Success) {
		dprintf(D_ALWAYS, "FILETRANSFER: Test URL %s download failed by plugin %s: %s\n",
			test_url.c_str(), plugin.c_str(), err.getFullText().c_str());
		return false;
	}
	dprintf(D_ALWAYS, "FILETRANSFER: Successfully downloaded test URL %s using plugin %s.\n",
		test_url.c_str(), plugin.c_str());
	return true;
#endif
}

bool
FileTransfer::ExpandFileTransferList( std::vector<std::string> *input_list, FileTransferList &expanded_list, bool preserveRelativePaths, const char* queue )
{
	bool rc = true;
	std::set<std::string> pathsAlreadyPreserved;

	if( !input_list ) {
		return true;
	}

	// if this exists and is in the list do it first
	if (X509UserProxy && contains(*input_list, X509UserProxy)) {
		if( !ExpandFileTransferList( X509UserProxy, "", Iwd, -1, expanded_list, preserveRelativePaths, SpoolSpace, pathsAlreadyPreserved, queue ) ) {
			rc = false;
		}
	}

	// then process the rest of the list
	for (auto& path: *input_list) {
		// skip the proxy if it's defined -- we dealt with it above.
		// everything else gets expanded.  this if would short-circuit
		// true if X509UserProxy is not defined, but i made it explicit.
		if(!X509UserProxy || (X509UserProxy && strcmp(path.c_str(), X509UserProxy) != 0)) {
			if( !ExpandFileTransferList( path.c_str(), "", Iwd, -1, expanded_list, preserveRelativePaths, SpoolSpace, pathsAlreadyPreserved, queue ) ) {
				rc = false;
			}
		}
	}


	// For testing.
	if( param_boolean( "TEST_HTCONDOR_993", false ) ) {
		for( auto & path : pathsAlreadyPreserved ) {
			dprintf( D_ALWAYS, "path cache includes: '%s'\n", path.c_str() );
		}

		std::string dir;
		for( auto & fti : expanded_list ) {
			if( fti.isDirectory() ) {
				dir = fti.destDir();
				if(! dir.empty()) { dir += DIR_DELIM_CHAR; }
				dir += condor_basename( fti.srcName().c_str() );
				dprintf( D_ALWAYS, "directory list includes: '%s'\n", dir.c_str() );
			}
		}
	}

	return rc;
}

std::vector<std::string>
split_path( const char * src_path ) {
	// Fill a stack with path components from right to left.
	std::string dir, file;
	std::string path( src_path );
	std::vector< std::string > splitPath;
	// dprintf( D_ZKM, ">>> initial path-to-preserve = %s\n", path.c_str() );
	while( filename_split( path.c_str(), dir, file ) ) {
		// dprintf( D_ZKM, ">>> found trailing path-component %s\n", file.c_str() );
		splitPath.emplace_back( file );
		path = path.substr( 0, path.length() - file.length() - 1 );
		// dprintf( D_ZKM, ">>> proceeding with path-to-preserve = %s\n", path.c_str() );
	}
	// dprintf( D_ZKM, ">>> found root path-component %s\n", file.c_str() );
	splitPath.emplace_back( file );

	return splitPath;
}

bool
FileTransfer::ExpandParentDirectories( const char * src_path, const char * iwd, FileTransferList &expanded_list, const char * SpoolSpace, std::set<std::string> & pathsAlreadyPreserved ) {
	// dprintf( D_ALWAYS, ">>> ExpandParentDirectories( %s, %s, ...)\n", src_path, iwd );

	std::vector< std::string > splitPath = split_path(src_path);

	// Empty the stack to add directories from the root down.  Note
	// that the "parent" directory is always empty, because src_path
	// is relative to iwd, not dest_dir.
	std::string parent;
	while( splitPath.size() != 0 ) {
		std::string partialPath = parent;
		if( partialPath.length() > 0 ) {
			partialPath += DIR_DELIM_CHAR;
		}
		partialPath += splitPath.back(); splitPath.pop_back();

		if( pathsAlreadyPreserved.find( partialPath ) == pathsAlreadyPreserved.end() ) {
			if(! ExpandFileTransferList( partialPath.c_str(), parent.c_str(), iwd, 0, expanded_list, false, SpoolSpace, pathsAlreadyPreserved )) {
				return false;
			}

			// If partialPath is not a directory, don't add it to
			// this list.  This should keep this list nice and small.

			// Refactor this common-with-EFTL() code.
			std::string full_path;
			if( !fullpath( partialPath.c_str() ) ) {
				full_path = iwd;
				if( full_path.length() > 0 ) {
					full_path += DIR_DELIM_CHAR;
				}
			}
			full_path += partialPath;

			// We know this will succeed because it already did in EFTL().
			StatInfo st( full_path.c_str() );
			if( st.IsDirectory() ) {
				pathsAlreadyPreserved.insert(partialPath);
			}
		}
		parent = partialPath;
	}

	return true;
}

bool
FileTransfer::ExpandFileTransferList( char const *src_path, char const *dest_dir, char const *iwd, int max_depth, FileTransferList &expanded_list, bool preserveRelativePaths, char const *SpoolSpace, std::set<std::string> & pathsAlreadyPreserved, const char* queue )
{
	ASSERT( src_path );
	ASSERT( dest_dir );
	ASSERT( iwd );

	// dprintf( D_ZKM, ">>> EFTL( %s, %s, %s, %d, ..., %d )\n", src_path, dest_dir, iwd, max_depth, preserveRelativePaths );

		// To simplify error handling, we always want to include an
		// entry for the specified path, except two cases which are
		// handled later on by removing the entry we add here.
	expanded_list.push_back( FileTransferItem() );
	FileTransferItem &file_xfer_item = expanded_list.back();

	file_xfer_item.setSrcName( src_path );
	file_xfer_item.setDestDir( dest_dir );

	if (queue) { file_xfer_item.setXferQueue(std::string(queue)); }

	if( IsUrl(src_path) ) {
		return true;
	}

	std::string full_src_path;
	if( !fullpath( src_path ) ) {
		full_src_path = iwd;
		if( full_src_path.length() > 0 ) {
			full_src_path += DIR_DELIM_CHAR;
		}
	}
	full_src_path += src_path;

	// dprintf( D_ZKM, ">>> Calling stat(%s)\n", full_src_path.c_str() );
	StatInfo st( full_src_path.c_str() );
	if( st.Error() != 0 ) {
		return false;
	}

		// TODO: somehow deal with cross-platform file modes.
		// For now, ignore modes on windows.
#ifndef WIN32
	file_xfer_item.setFileMode( (condor_mode_t)st.GetMode() );
#endif

	size_t srclen = file_xfer_item.srcName().length();
	bool trailing_slash = srclen > 0 && IS_ANY_DIR_DELIM_CHAR(src_path[srclen-1]);

	file_xfer_item.setSymlink( st.IsSymlink() );
	file_xfer_item.setDomainSocket( st.IsDomainSocket() );
	file_xfer_item.setDirectory( st.IsDirectory() );

		// If this file is a domain socket, we don't want to send it but it's
		// also not an error. Remove the entry from the list and return true.
	if( file_xfer_item.isDomainSocket() ) {
		dprintf(D_FULLDEBUG, "FILETRANSFER: File %s is a domain socket, excluding "
			"from transfer list\n", UrlSafePrint(full_src_path) );
		expanded_list.pop_back();
		return true;
	}

	if( !file_xfer_item.isDirectory() ) {
		file_xfer_item.setFileSize(st.GetFileSize());

		if( preserveRelativePaths && (! fullpath(file_xfer_item.srcName().c_str())) ) {
			std::string dirname = condor_dirname( file_xfer_item.srcName().c_str() );

			if( strcmp( dirname.c_str(), "." ) != 0 ) {
				file_xfer_item.setDestDir( dirname );

				// This is really clumsy.  Where's std::contains(container, value)?
				if( pathsAlreadyPreserved.find( dirname ) == pathsAlreadyPreserved.end() ) {
					// dprintf( D_ZKM, "Creating '%s' for '%s'\n", dirname.c_str(), file_xfer_item.srcName().c_str() );

					// ExpandParentDirectories() adds this back in the correct place.
					expanded_list.pop_back();

					// dprintf( D_ZKM, ">>> expanding parent directories of named file %s\n", UrlSafePrint(src_path) );
					// N.B.: This isn't an infinite loop because
					// ExpandParentDirectories() calls ExpandFileTransferList()
					// with preserveRelativePaths turned off -- the whole point
					// of it being to generate paths one level at a time.
					// dprintf( D_ZKM, ">>> [c] ExpandParentDirectories( %s, %s )\n", src_path, iwd );
					if(! ExpandParentDirectories( src_path, iwd, expanded_list, SpoolSpace, pathsAlreadyPreserved )) {
						return false;
					}
				}
			}
		}

		// dprintf( D_ZKM, ">>> file added: %s in %s\n", file_xfer_item.srcName().c_str(), file_xfer_item.destDir().c_str() );
		return true;
	}

		// do not follow symlinks to directories unless we are just
		// fetching the contents of the directory
	if( !trailing_slash && file_xfer_item.isSymlink() ) {
			// leave it up to our caller to decide if this is an error
		return true;
	}

	if( max_depth == 0 ) {
			// do not scan the contents of the directory
		return true; // this is not considered an error
	}
	if( max_depth > 0 ) {
		max_depth--;
	}

	//
	// We're going to transfer the contents of the directory named by src_path.
	//
	// If that name has a trailing slash, we don't transfer the directory, and
	// its contents will go where the directory would have.
	//
	// Otherwise, we transfer the directory, and its contents go in it, but we
	// only transfer its parent directories if we're preserving relative paths.
	//
	// Determine where the contents of the directory will be going, and make
	// sure that the directories in that path have been added as file transfer
	// items (otherwise, the remote side won't know what permissions to set).
	//

	// dprintf( D_ZKM, ">>> transferring contents of directory %s\n", src_path );
	std::string destination = dest_dir;

	if( trailing_slash ) {
		// dprintf( D_ZKM, ">>> detected trailing slash.\n" );
		expanded_list.pop_back();
	} else {
		if( destination.length() > 0 ) { destination += DIR_DELIM_CHAR; }

		if(! preserveRelativePaths) {
			// dprintf( D_ZKM, ">>> not preserving relative path.\n" );
			destination += condor_basename(src_path);
		} else {
			if(! fullpath(src_path)) {
				// dprintf( D_ZKM, ">>> preserving relative path of relative path %s\n", src_path );

				if( destination.length() > 0 ) { destination += DIR_DELIM_CHAR; }
				destination += src_path;

				if( pathsAlreadyPreserved.find( src_path ) == pathsAlreadyPreserved.end() ) {
					// dprintf( D_ZKM, "Creating '%s' for '%s'\n", src_path, src_path );

					// ExpandParentDirectories() adds this back in the correct place.
					expanded_list.pop_back();

					// dprintf( D_ZKM, ">>> [b] ExpandParentDirectories( %s, %s, ..., ... )\n", src_path, iwd );
					if(! ExpandParentDirectories( src_path, iwd, expanded_list, SpoolSpace, pathsAlreadyPreserved )) {
						return false;
					}
				}
			} else {
				//
				// The only absolute paths we want to treat as relative paths
				// are absolute paths into the SPOOL directory.  Everything
				// else should be transferred as usual.
				//

                // SpoolSpace is not under user control; if the admin screws
                // up this setting, it's OK to assert and die.  (Setting
                // SPOOL to a relative path should have already caused
                // failures before getting here.)
				ASSERT( SpoolSpace == NULL || fullpath(SpoolSpace) );
				if( SpoolSpace != nullptr && starts_with(src_path, SpoolSpace) ) {
					const char * relative_path = &src_path[strlen(SpoolSpace)];
					if( IS_ANY_DIR_DELIM_CHAR(relative_path[0]) ) { ++relative_path; }
					// dprintf( D_ZKM, ">>> preserving relative path of directory (%s) in SPOOL (as %s)\n", src_path, relative_path );

					if( pathsAlreadyPreserved.find( relative_path ) == pathsAlreadyPreserved.end() ) {
						// dprintf( D_ZKM, "Creating '%s' for '%s'\n", relative_path, src_path );

						// ExpandParentDirectories() adds this back in the correct place.
						expanded_list.pop_back();

						// ExpandParentDirectories() needs its first argument to
						// be the path relative to SPOOL, and its second argument
						// needs to be SPOOL (to parallel the non-SPOOL use).
						// dprintf( D_ZKM, ">>> [a] ExpandParentDirectories( %s, %s, ..., ... )\n", relative_path, SpoolSpace );
						if(! ExpandParentDirectories( relative_path, SpoolSpace, expanded_list, SpoolSpace, pathsAlreadyPreserved )) {
							return false;
						}
					}

					//
					// relative_path is relative to SpoolSpace, not the
					// destination, which means we can't use it to set
					// destination.  Instead, see if destination is a prefix
					// to relative_path, and correct if it is.
					//
					ASSERT(! fullpath(destination.c_str()));

					if( starts_with( relative_path, destination ) ) {
						relative_path = &relative_path[destination.length()];
						if( IS_ANY_DIR_DELIM_CHAR(relative_path[0]) ) { ++relative_path; }
					}

					if(  (! destination.empty())
					  && (! IS_ANY_DIR_DELIM_CHAR(destination[destination.length() - 1]))
					  ) {
						destination += DIR_DELIM_CHAR;
					}
					destination += relative_path;
				} else {
					destination += condor_basename(src_path);
				}

			}
		}
	}

	//
	// Transfer the contents of the directory.
	//
	// dprintf( D_ZKM, ">>> transferring contents of directory %s to %s\n", src_path, destination.c_str() );

	Directory dir( &st );
	dir.Rewind();

	bool rc = true;
	char const *file_in_dir;

	while( (file_in_dir=dir.Next()) != NULL ) {

		std::string file_full_path = src_path;
		if( !trailing_slash ) {
			file_full_path += DIR_DELIM_CHAR;
		}
		file_full_path += file_in_dir;

		// dprintf( D_ZKM, ">>> calling ExpandFileTransferList( %s, %s, %s, ... )\n", file_full_path.c_str(), destination.c_str(), iwd );
		if( !ExpandFileTransferList( file_full_path.c_str(), destination.c_str(), iwd, max_depth, expanded_list, preserveRelativePaths, SpoolSpace, pathsAlreadyPreserved ) ) {
			rc = false;
		}
	}

	return rc;
}

bool
FileTransfer::ExpandInputFileList( char const *input_list, char const *iwd, std::string &expanded_list, std::string &error_msg )
{
	bool result = true;
	for (auto& path: StringTokenIterator(input_list)) {
		bool needs_expansion = false;

		size_t pathlen = path.length();
		bool trailing_slash = pathlen > 0 && path[pathlen-1] == DIR_DELIM_CHAR;

		if( trailing_slash && !IsUrl(path.c_str()) ) {
			needs_expansion = true;
		}

		if( !needs_expansion ) {
				// We intentionally avoid the need to stat any of the entries
				// that don't need to be expanded in case stat is expensive.
			if (!expanded_list.empty()) {
				expanded_list += ',';
			}
			expanded_list += path;
		}
		else {
			FileTransferList filelist;
			// N.B.: It's only safe to flatten relative paths here because
			// this code never calls destDir().
			//
			// This implicitly assumes that nothing in the input file list is in SPOOL.
			std::set<std::string> pap;
			if( !ExpandFileTransferList( path.c_str(), "", iwd, 1, filelist, false, "", pap ) ) {
				formatstr_cat(error_msg, "Failed to expand '%s' in transfer input file list. ", path.c_str());
				result = false;
			}
			FileTransferList::iterator filelist_it;
			for( filelist_it = filelist.begin();
				 filelist_it != filelist.end();
				 filelist_it++ )
			{
				if (!expanded_list.empty()) {
					expanded_list += ',';
				}
				expanded_list += filelist_it->srcName();
			}
		}
	}
	return result;
}

bool
FileTransfer::ExpandInputFileList( ClassAd *job, std::string &error_msg ) {

		// If we are spooling input files, input directories that end
		// in a slash must be expanded to list their contents so that
		// when the schedd rewrites ATTR_TRANSFER_INPUT_FILES, it can
		// correctly represent the contents of the spool, without
		// requiring the schedd to iterate through the spool directory
		// to see what files are there.  Alternatively, when spooling
		// input, we could ignore trailing slashes and preserve the
		// source directory in the spool, with its contents inside of
		// it.  However, this could lead to name collisions if
		// something else in the spool has the same name as the directory.
		//
		// Ideally, we would just leave this up to the file transfer
		// object during the actual transfer, similarly to how
		// ATTR_SPOOLED_OUTPUT_FILES works.  However, given the way
		// the job state is managed, that is not an easy task.  If the
		// job submission client (e.g. condor_submit) were to rewrite
		// the file list after transferring the files, it would need
		// to keep the job on hold until it reconnects to the schedd
		// to modify the job, rather than having the schedd modify and
		// release the job in the reaper of the spooling operation.
		// So unless we rewire that, we need to pre-process the input
		// file list during the job submission, before spooling files.

	std::string input_files;
	if( job->LookupString(ATTR_TRANSFER_INPUT_FILES,input_files) != 1 )
	{
		return true; // nothing to do
	}

	std::string iwd;
	if( job->LookupString(ATTR_JOB_IWD,iwd) != 1 )
	{
		formatstr(error_msg, "Failed to expand transfer input list because no IWD found in job ad.");
		return false;
	}

	std::string expanded_list;
	if( !FileTransfer::ExpandInputFileList(input_files.c_str(),iwd.c_str(),expanded_list,error_msg) )
	{
		return false;
	}

	if( expanded_list != input_files ) {
		dprintf(D_FULLDEBUG,"Expanded input file list: %s\n",expanded_list.c_str());
		job->Assign(ATTR_TRANSFER_INPUT_FILES,expanded_list.c_str());
	}
	return true;
}

bool
FileTransfer::LegalPathInSandbox(char const *path,char const *sandbox) {
	bool result = true;

	ASSERT( path );
	ASSERT( sandbox );

	std::string buf = path;
	canonicalize_dir_delimiters( buf );
	path = buf.c_str();

	if( fullpath(path) ) {
		return false;
	}

		// now we want to make sure there are no references to ".."
	char *pathbuf = strdup( path );
	char *dirbuf = strdup( path );
	char *filebuf = strdup( path );

	ASSERT( pathbuf );
	ASSERT( dirbuf );
	ASSERT( filebuf );

	bool more = true;
	while( more ) {
		more = filename_split( pathbuf, dirbuf, filebuf );

		if( strcmp(filebuf,"..") == 0 ) {
			result = false;
			break;
		}

		strcpy(pathbuf,dirbuf);
	}

	free( pathbuf );
	free( dirbuf );
	free( filebuf );

	return result;
}

void FileTransfer::FileTransferInfo::addSpooledFile(char const *name_in_spool)
{
	if (!spooled_files.empty()) {
		spooled_files += ',';
	}
	spooled_files += name_in_spool;
}


time_t
GetDesiredDelegatedJobCredentialExpiration(ClassAd *job)
{
	if ( !param_boolean( "DELEGATE_JOB_GSI_CREDENTIALS", true ) ) {
		return 0;
	}

	time_t expiration_time = 0;
	int lifetime = -1;
	if( job ) {
		job->LookupInteger(ATTR_DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME,lifetime);
	}
	if( lifetime < 0 ) {
		lifetime = param_integer("DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME", 3600*24, 0);
	}
	if( lifetime ) {
		expiration_time = time(NULL) + lifetime;
	}
	return expiration_time;
}

time_t
GetDelegatedProxyRenewalTime(time_t expiration_time)
{
	if( expiration_time == 0 ) {
		return 0;
	}
	if ( !param_boolean( "DELEGATE_JOB_GSI_CREDENTIALS", true ) ) {
		return 0;
	}

	time_t now = time(NULL);
	time_t lifetime = expiration_time - now;
	double lifetime_frac = param_double( "DELEGATE_JOB_GSI_CREDENTIALS_REFRESH", 0.25,0,1);
	return now + (time_t)floor(lifetime*lifetime_frac);
}

void
GetDelegatedProxyRenewalTime(ClassAd *jobAd)
{
	GetDelegatedProxyRenewalTime(GetDesiredDelegatedJobCredentialExpiration(jobAd));
}

bool
FileTransfer::outputFileIsSpooled(char const *fname) {
	if(fname) {
		if( !fullpath(fname) ) {
			if( Iwd && SpoolSpace && strcmp(Iwd,SpoolSpace)==0 ) {
				return true;
			}
		}
		else if( SpoolSpace && strncmp(fname,SpoolSpace,strlen(SpoolSpace))==0 ) {
			return true;
		}
	}
	return false;
}

ClassAd*
FileTransfer::GetJobAd() {
	return &jobAd;
}

void
FileTransfer::setMaxUploadBytes(filesize_t _MaxUploadBytes)
{
	MaxUploadBytes = _MaxUploadBytes;
}

void
FileTransfer::setMaxDownloadBytes(filesize_t _MaxDownloadBytes)
{
	MaxDownloadBytes = _MaxDownloadBytes;
}

//
// There are two differences between this and ExpandParentDirectories():
//   (1) this function doesn't check if the source's parent directories
//       exist (they won't, because they're coming for URLs); and
//   (2) the source _must_ be a file (and won't be expanded if it's a
//       directory, because we can't do that for URLs).  This is the reason
//       we don't call ExpandFileTransferList() to add the directory entries.
// We may refactor these two functions together at some point (probably
// cleanest to do with callbacks), but until then, if you find a bug in one
// function, it's probably in the other, too.  If you change one function,
// you probably need to change the other one, too.
//
void
FileTransfer::addSandboxRelativePath(
	const std::string & source,
	const std::string & destination,
	FileTransferList & ftl,
	std::set< std::string > & pathsAlreadyPreserved
) {
	std::vector< std::string > splitPath = split_path( destination.c_str() );

	std::string parent;
	while( splitPath.size() > 1 ) {
		std::string partialPath = parent;
		if( partialPath.length() > 0 ) {
			partialPath += DIR_DELIM_CHAR;
		}
		partialPath += splitPath.back(); splitPath.pop_back();

		if( pathsAlreadyPreserved.find( partialPath ) == pathsAlreadyPreserved.end() ) {
			FileTransferItem fti;
			fti.setSrcName( partialPath.c_str() );
			fti.setDestDir( parent.c_str() );
			fti.setDirectory( true );
			// dprintf( D_ALWAYS, "addSandboxRelativePath(%s, %s): %s -> %s\n", source.c_str(), destination.c_str(), partialPath.c_str(), parent.c_str() );
			ftl.emplace_back( fti );

			pathsAlreadyPreserved.insert( partialPath );
		}

		parent = partialPath;
	}

	FileTransferItem fti;
	fti.setSrcName( source );
	// At some point, we'd like to be able to store target _name_, too.
	fti.setDestDir( condor_dirname( destination.c_str() ) );
	ftl.emplace_back(fti);
}

void
FileTransfer::addCheckpointFile(
  const std::string & source, const std::string & destination,
  std::set< std::string > & pathsAlreadyPreserved
) {
	addSandboxRelativePath( source, destination, this->checkpointList, pathsAlreadyPreserved );
}

void
FileTransfer::addInputFile(
  const std::string & source, const std::string & destination,
  std::set< std::string > & pathsAlreadyPreserved
) {
	addSandboxRelativePath( source, destination, this->inputList, pathsAlreadyPreserved );
}

