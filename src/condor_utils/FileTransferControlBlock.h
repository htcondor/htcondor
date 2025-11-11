#ifndef   _CONDOR_FILETRANSFERCONTROLBLOCK_H
#define   _CONDOR_FILETRANSFERCONTROLBLOCK_H

// #include <string>
// #include "classad/classad.h"

class FileTransferControlBlock {
	public:

		FileTransferControlBlock();
		FileTransferControlBlock( ClassAd * jobAd );

		bool streamOutput() { return STREAM_OUTPUT; }
		bool streamError() { return STREAM_ERROR; }
		bool preserveRelativePaths() { return PRESERVE_RELATIVE_PATHS; }

		bool hasJobIWD() { return eval_JOB_IWD; }
		const std::string & getJobIWD() { return JOB_IWD; }
		void setJobIWD( const std::string & iwd ) {
			eval_JOB_IWD = true;
			JOB_IWD = iwd;
		}
		// This is moronic, but required by FileTransfer::TestPlugin().
		void unsetJobIWD() {
			eval_JOB_IWD = false;
			JOB_IWD.clear();
		}

		bool hasOwner() { return eval_OWNER; }
		const std::string & getOwner() { return OWNER; }

		bool hasTransferInputFiles() { return eval_TRANSFER_INPUT_FILES; }
		const std::string & getTransferInputFiles() { return TRANSFER_INPUT_FILES; }

		bool hasDataReuseManifestSHA256() { return eval_DataReuseManifestSHA256; }
		const std::string & getDataReuseManifestSHA256() { return DataReuseManifestSHA256; }

		bool hasUlogFile() { return eval_ULOG_FILE; }
		const std::string & getUlogFile() { return ULOG_FILE; }

		bool hasTransferKey() { return eval_TRANSFER_KEY; }
		const std::string & getTransferKey() { return TRANSFER_KEY; }
		void setTransferKey( const std::string & tk ) {
			eval_TRANSFER_KEY = true;
			TRANSFER_KEY = tk;
		};

		bool hasTransferSocket() { return eval_TRANSFER_SOCKET; }
		const std::string & getTransferSocket() { return TRANSFER_SOCKET; }
		void setTransferSocket( const std::string & ts ) {
			eval_TRANSFER_SOCKET = true;
			TRANSFER_SOCKET = ts;
		}

		bool hasTransferIntermediateFiles() { return eval_TRANSFER_INTERMEDIATE_FILES; }
		const std::string & getTransferIntermediateFiles() { return TRANSFER_INTERMEDIATE_FILES; }
		void setTransferIntermediateFiles( const std::string & tif ) {
			eval_TRANSFER_INTERMEDIATE_FILES = true;
			TRANSFER_INTERMEDIATE_FILES = tif;
		}

		bool hasX509UserProxy() { return eval_X509_USER_PROXY; }
		const std::string & getX509UserProxy() { return X509_USER_PROXY; }

		bool hasCheckpointFiles() { return eval_CHECKPOINT_FILES; }
		const std::string & getCheckpointFiles() { return CHECKPOINT_FILES; }

		bool hasCheckpointDestination() { return eval_JOB_CHECKPOINT_DESTINATION; }
		const std::string & getCheckpointDestination() { return JOB_CHECKPOINT_DESTINATION; }

		bool hasTransferOutputRemaps() { return eval_TRANSFER_OUTPUT_REMAPS; }
		const std::string & getTransferOutputRemaps() { return TRANSFER_OUTPUT_REMAPS; }

		bool hasOutputDirectory() { return eval_OutputDirectory; }
		const std::string & getOutputDirectory() { return OutputDirectory; }

		const std::string & getContainerImage() { return CONTAINER_IMAGE; }

		int getClusterID() { return CLUSTER_ID; }
		int getProcID() { return PROC_ID; }

		bool hasUser() { return eval_USER; }
		const std::string & getUser() { return USER; }

		const std::string & getJobCmd() { return JOB_CMD; }
		const std::string & getGlobalJobID() { return GLOBAL_JOB_ID; }

		const std::string & getJobOriginalOutput() { return JOB_ORIGINAL_OUTPUT; }
		const std::string & getJobOriginalError() { return JOB_ORIGINAL_ERROR; }

		// This is a hack; see below.
		bool hasTransferQInputListAttr() { return eval_TRANSFER_Q_URL_IN_LIST; }
		bool isTransferQInputAttrAList() { return list_TRANSFER_Q_URL_IN_LIST; }
		// This is doubly a hack, but maintains value semantics.
		classad::ExprList * getTransferQInputList() { return & TRANSFER_Q_URL_IN_LIST; }

		bool hasPublicInputFiles() { return eval_PUBLIC_INPUT_FILES; }
		const std::string & getPublicInputFiles() { return PUBLIC_INPUT_FILES; }

		bool hasJobInput() { return eval_JOB_INPUT; }
		const std::string & getJobInput() { return JOB_INPUT; }

		bool hasOutputDestination() { return eval_OUTPUT_DESTINATION; }
		const std::string & getOutputDestination() { return OUTPUT_DESTINATION; }

		bool getTransferExecutable() { return TRANSFER_EXECUTABLE; }

		bool hasJobOrigCmd() { return eval_JOB_ORIG_CMD; }
		const std::string & getJobOrigCmd() { return JOB_ORIG_CMD; }

		bool hasJobOutput() { return eval_JOB_OUTPUT; }
		const std::string & getJobOutput() { return JOB_OUTPUT; }

		bool hasJobError() { return eval_JOB_ERROR; }
		const std::string & getJobError() { return JOB_ERROR; }

		bool hasSpooledOutputFiles() { return eval_SPOOLED_OUTPUT_FILES; }
		const std::string & getSpooledOutputFiles() { return SPOOLED_OUTPUT_FILES; }

		bool hasTransferOutputFiles() { return eval_TRANSFER_OUTPUT_FILES; }
		const std::string & getTransferOutputFiles() { return TRANSFER_OUTPUT_FILES; }

		bool hasEncryptInputFiles() { return eval_ENCRYPT_INPUT_FILES; }
		const std::string & getEncryptInputFiles() { return ENCRYPT_INPUT_FILES; }

		bool hasEncryptOutputFiles() { return eval_ENCRYPT_OUTPUT_FILES; }
		const std::string & getEncryptOutputFiles() { return ENCRYPT_OUTPUT_FILES; }

		bool hasDontEncryptInputFiles() { return eval_DONT_ENCRYPT_INPUT_FILES; }
		const std::string & getDontEncryptInputFiles() { return DONT_ENCRYPT_INPUT_FILES; }

		bool hasDontEncryptOutputFiles() { return eval_DONT_ENCRYPT_OUTPUT_FILES; }
		const std::string & getDontEncryptOutputFiles() { return DONT_ENCRYPT_OUTPUT_FILES; }

		bool hasFailureFiles() { return eval_FAILURE_FILES; }
		const std::string & getFailureFiles() { return FAILURE_FILES; }

		int getStageInFinish() { return STAGE_IN_FINISH; }

		bool hasNTDomain() { return eval_NT_DOMAIN; }
		const std::string & getNTDomain() { return NT_DOMAIN; }

	protected:
		// For the first pass, let's just be brain-damaged and just store
		// everything that the code originally looked up.  ALL_CAPITAL
		// data members have had their ATTR_ prefices removed to protect
		// the innocent.
		bool			STREAM_OUTPUT {false};
		bool			STREAM_ERROR {false};
		bool			PRESERVE_RELATIVE_PATHS {false};
		bool			TRANSFER_EXECUTABLE {true};

		int				CLUSTER_ID {-1};
		int				PROC_ID {-1};
		int				STAGE_IN_FINISH {0};

		std::string		X509_USER_PROXY;
		std::string 	CHECKPOINT_FILES;
		std::string		JOB_CHECKPOINT_DESTINATION;
		std::string		OutputDirectory;
		std::string		TRANSFER_OUTPUT_REMAPS;
		std::string		CONTAINER_IMAGE;
		std::string		USER;
		std::string		DataReuseManifestSHA256;
		std::string		JOB_CMD;
		std::string		JOB_IWD;
		std::string		OWNER;
		std::string		TRANSFER_INPUT_FILES;
		std::string		ULOG_FILE;
		std::string		GLOBAL_JOB_ID;
		std::string		JOB_ORIGINAL_OUTPUT;
		std::string		JOB_ORIGINAL_ERROR;
		std::string		PUBLIC_INPUT_FILES;
		std::string		JOB_INPUT;
		std::string		OUTPUT_DESTINATION;
		std::string		JOB_ORIG_CMD;
		std::string		SPOOLED_OUTPUT_FILES;
		std::string		TRANSFER_OUTPUT_FILES;
		std::string		JOB_OUTPUT;
		std::string		JOB_ERROR;
		std::string		ENCRYPT_INPUT_FILES;
		std::string		ENCRYPT_OUTPUT_FILES;
		std::string		DONT_ENCRYPT_INPUT_FILES;
		std::string		DONT_ENCRYPT_OUTPUT_FILES;
		std::string		FAILURE_FILES;
		std::string		NT_DOMAIN;

		// Some attributes are evaluated rather than looked up.
		bool			eval_TRANSFER_OUTPUT_REMAPS {false};
		bool			eval_USER {false};
		bool			eval_DataReuseManifestSHA256 {false};
		bool			eval_JOB_IWD {false};

		// Some attributes look-ups are checked.
		bool			eval_OWNER {false};
		bool			eval_TRANSFER_INPUT_FILES  {false};
		bool			eval_ULOG_FILE {false};
		bool			eval_X509_USER_PROXY {false};
		bool			eval_CHECKPOINT_FILES {false};
		bool			eval_JOB_CHECKPOINT_DESTINATION {false};
		bool			eval_PUBLIC_INPUT_FILES {false};
		bool			eval_JOB_INPUT {false};
		bool			eval_OUTPUT_DESTINATION {false};
		bool			eval_OutputDirectory {false};
		bool			eval_JOB_ORIG_CMD {false};
		bool			eval_SPOOLED_OUTPUT_FILES {false};
		bool			eval_TRANSFER_OUTPUT_FILES {false};
		bool			eval_JOB_OUTPUT {false};
		bool			eval_JOB_ERROR {false};
		bool			eval_ENCRYPT_INPUT_FILES {false};
		bool			eval_ENCRYPT_OUTPUT_FILES {false};
		bool			eval_DONT_ENCRYPT_INPUT_FILES {false};
		bool			eval_DONT_ENCRYPT_OUTPUT_FILES {false};
		bool			eval_FAILURE_FILES {false};
		bool			eval_NT_DOMAIN {false};


		// For clarity, I'll store attributes stored in the job but which
		// don't originate there in a separate list.
		std::string		TRANSFER_KEY;
		bool			eval_TRANSFER_KEY {false};

		std::string		TRANSFER_SOCKET;
		bool			eval_TRANSFER_SOCKET {false};

		std::string		TRANSFER_INTERMEDIATE_FILES;
		bool			eval_TRANSFER_INTERMEDIATE_FILES {false};

		// Aspirational: an actual interface.
		// std::map< std::string, std::vector< std::string > > protectedFiles;
		bool				eval_TRANSFER_Q_URL_IN_LIST {false};
		bool				list_TRANSFER_Q_URL_IN_LIST {false};
		classad::ExprList	TRANSFER_Q_URL_IN_LIST;
};

#endif /* _CONDOR_FILETRANSFERCONTROLBLOCK_H */
